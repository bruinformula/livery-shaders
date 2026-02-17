#include <MaterialXGenShader/Library.h>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <regex>
#include <filesystem>

#include <OSL/oslquery.h>
#include <OSL/oslcomp.h>

#include <MaterialXCore/Document.h>
#include <MaterialXCore/Util.h>
#include <MaterialXCore/Interface.h>

#include <MaterialXFormat/File.h>
#include <MaterialXFormat/Util.h>

#include <MaterialXGenShader/GenContext.h>
#include <MaterialXGenShader/Shader.h>

#include <MaterialXGenOsl/OslShaderGenerator.h>
#include <MaterialXGenOsl/OslSyntax.h>

#include "GLSLStubs.h"
#include "OSLCompiler.h"

namespace osl = OSL;
namespace mx = MaterialX;
namespace fs = std::filesystem;

const std::string argOptions =
    " MaterialXLibraryBuilder -- Generates MaterialX bindings for a bunch of OSL shaders\n"
    " Options: \n"
    "    --oslLibraryPath           Path to the directory containing OSL shader files.  All .osl files in this directory and its subdirectories will be processed. \n"
    "    --libraryOutputPath        Path to the output directory where generated MaterialX documents and OSL shaders will be written. \n"
    "    --skipWritingMtlxHeaders   Skip generating MaterialX implementation files to the libraryOutputPath. \n"
    "    --autolib-include-rewrite  Rewrite #include paths in header files to point to flattened OSL files in root directory. \n"
    "    --path                     Specify an additional data search path location (e.g. '/projects/MaterialX').  This absolute path will be queried when locating data libraries, XInclude references, and referenced images.\n"
    "    --library                  Specify an additional data library folder (e.g. 'vendorlib', 'studiolib').  This relative path will be appended to each location in the data search path when loading data libraries.\n"
    "    --help                     Prints this message\n";

struct LibraryArgumentHandler : public ArgumentHandler {
    mx::FilePath oslLibraryPath;
    mx::FilePath libraryOutputPath;
    bool skipWritingMtlxHeaders = false;
    bool autolibIncludeRewrite = false;
    
    OslCompileOptions oslCompileOptions;
    
    ParseResult parse(const std::string& token, const std::string& nextToken) override {
        switch (hashString(token.c_str())) {
            case hashString("--oslLibraryPath"): {
                if (nextToken.empty()) goto expectOption;
                if (!oslLibraryPath.isEmpty()) goto alreadySet;
                oslLibraryPath = mx::FilePath(nextToken);
                return SUCCESS_CONSUME_NEXT;
            }
            case hashString("--libraryOutputPath"): {
                if (nextToken.empty()) goto expectOption;
                if (!libraryOutputPath.isEmpty()) goto alreadySet;
                libraryOutputPath = mx::FilePath(nextToken);
                return SUCCESS_CONSUME_NEXT;
            }
            case hashString("--skipWritingMtlxHeaders"): {
                skipWritingMtlxHeaders = true;
                return SUCCESS;
            }
            case hashString("--autolib-include-rewrite"): {
                autolibIncludeRewrite = true;
                return SUCCESS;
            }
            case hashString("--help"): {
                std::cout << argOptions << std::endl;
                std::cout << "\n" << oslcArgOptions << std::endl;
                return EXIT;
            }
            default: { // try parsing as OSL compile option
                ParseResult oslResult = oslCompileOptions.parse(token, nextToken);
                if (oslResult == SUCCESS || oslResult == SUCCESS_CONSUME_NEXT) {
                    return oslResult;
                } else if (oslResult == FAILURE) { // already printed error message
                    return FAILURE;
                } else {
                    std::cout << "Unrecognized command-line option: " << token << std::endl;
                    return FAILURE;
                }
            }
        }
        
        alreadySet: {
            std::cerr << token << " is already set!" << std::endl;
            return FAILURE;
        }
        expectOption: {
            std::cerr << "Expected another token following command-line option: " << token << std::endl; 
            return FAILURE;
        }
    }
    
    // enforce additional rules about the input arguments
    bool verify() {
        if (oslLibraryPath.isEmpty()) {
            std::cerr << "oslLibraryPath is not set!" << std::endl;
            return false;
        }
        if (libraryOutputPath.isEmpty()) {
            std::cerr << "libraryOutputPath is not set!" << std::endl;
            return false;
        }
        
        // output Path
        if (!libraryOutputPath.exists() || !libraryOutputPath.isDirectory()) {
            libraryOutputPath.createDirectory();
            if (!libraryOutputPath.exists() || !libraryOutputPath.isDirectory()) {
                std::cerr << "Failed to find and/or create the provided output mtlx path: "
                          << libraryOutputPath.asString() << std::endl;
                return false;
            }
        }
        
        // validate osl library path
        if (!oslLibraryPath.exists() || !oslLibraryPath.isDirectory()) {
            std::cerr << "The provided path to the OSL library is not valid: " << oslLibraryPath.asString() << std::endl;
            return false;
        }
        
        // validate osl include paths
        for (const auto& includePath : oslCompileOptions.oslIncludePath) {
            if (!includePath.exists() || !includePath.isDirectory()) {
                std::cerr << "The provided OSL include path is not valid: " 
                          << includePath.asString() << std::endl;
                return false;
            }
        }
        
        return true;
    }
};

class MaterialXDefinitionOptions {
public:
    MaterialXDefinitionOptions() = default;
    ~MaterialXDefinitionOptions() = default;

    bool unknownAttributeWarning = false;
    bool typeMismatchWarning = true;
    bool implicitAssignmentWarning = true;
};

std::string getRelativePathToRoot(const mx::FilePath& headerPath) {
    fs::path path(headerPath.asString());
    
    size_t depth = 0;
    for (const auto& part : path) {
        if (part != "." && part != path.filename()) {
            depth++;
        }
    }
    
    std::string prefix;
    prefix.reserve(depth * 3); // reserve some extra space
    for (size_t i = 0; i < depth; i++) {
        prefix.append("../");
    }
    
    return prefix;
}

bool rewriteHeaderIncludePaths(const mx::FilePath& headerFilePath, const mx::FilePath& relativeHeaderPath) {
    fs::path fsHeaderPath(headerFilePath.asString());
    
    std::ifstream headerInput(fsHeaderPath);
    if (!headerInput.is_open()) {
        std::cerr << "Failed to open header file for rewriting: " << headerFilePath.asString() << std::endl;
        return false;
    }
    
    std::stringstream buffer;
    buffer << headerInput.rdbuf();
    std::string headerContent = buffer.str();
    headerInput.close();
    
    std::string relativePrefix = getRelativePathToRoot(relativeHeaderPath);
    
    // regex to match #include "path/to/file.osl" and capture just the filename
    std::regex includeRegex("#include\\s+\"(?:[^\"]*/)?([\\w]+\\.osl)\"");
    
    // replace with #include "../filename.osl" (or appropriate depth)
    std::string modifiedContent;
    std::string::const_iterator searchStart(headerContent.cbegin());
    std::smatch match;
    
    while (std::regex_search(searchStart, headerContent.cend(), match, includeRegex)) {
        modifiedContent.append(searchStart, searchStart + match.position());
        
        std::string filename = match[1].str();
        modifiedContent.append("#include \"");
        modifiedContent.append(relativePrefix);
        modifiedContent.append(filename);
        modifiedContent.append("\"");
        
        searchStart += match.position() + match.length();
    }
    
    modifiedContent.append(searchStart, headerContent.cend());
    
    std::ofstream headerOutput(fsHeaderPath);
    if (!headerOutput.is_open()) {
        std::cerr << "Failed to open header file for writing: " << headerFilePath.asString() << std::endl;
        return false;
    }
    
    headerOutput << modifiedContent;
    headerOutput.close();
    
    return true;
}

bool createMaterialXDefinitions(
    osl::OSLQuery& osoQuery,
    const std::string& oslFileName,
    const mx::FilePath& relativeOslPath,
    mx::DocumentPtr& nodeDefMtlxDoc,
    mx::DocumentPtr& implMtlxDoc,
    mx::DocumentPtr& typeDefMtlxDoc,
    const mx::FilePath& outputDir,
    MaterialXDefinitionOptions& mtlxDefinitionOptions
) {
    // OSL files are referenced from root
    mx::FilePath oslFilePath = oslFileName;
    oslFilePath.removeExtension();
    oslFilePath.addExtension("osl");

    mx::FilePath osoFilePath = oslFileName;
    osoFilePath.removeExtension();
    osoFilePath.addExtension("oso");

    if (osoQuery.shadername().empty()) {
        std::cerr << "OSLQuery is empty for file: " << oslFileName << std::endl;
        return false;
    }
    
    std::string shaderName = osoQuery.shadername().c_str();
    std::string nodeName = toSnakeCase(shaderName);

    mx::NodeDefPtr nodeDef = nodeDefMtlxDoc->addNodeDef(
        "ND_" + nodeName,
        "",
        nodeName
    );

    if (!nodeDef) {
        std::cerr << "Failed to create NodeDef for node: " << nodeName << std::endl;
        return false;
    }

    //Add Shader Metadata
    for (auto metadata = osoQuery.metadata().begin(); metadata != osoQuery.metadata().end(); ++metadata) {

        std::string attributeName = metadata->name.c_str();
        std::string attributeType = parseOSLParameterType(*metadata);
        std::string attributeValue = parseOSLParameterValue(*metadata);
        
        // Unescape string attributes
        if (attributeType == "string") {
            attributeValue = unescapeString(attributeValue);
        }

        std::string paramType = metadata->type.c_str();
        std::string paramName = metadata->name.c_str();

        switch (hashString(attributeName)) {
            case hashString("name"): {
                if (!mtlxDefinitionOptions.implicitAssignmentWarning) break;
                std::cout << "name is the unique identifier of the nodedef and should not be reassigned via metadata. Skipping." << std::endl;
                continue;
            }
            case hashString("node"): {
                if (!mtlxDefinitionOptions.implicitAssignmentWarning) break;
                std::cout << "node is defined by the shader function being exported and cannot be overridden. Skipping." << std::endl;
                continue;
            }

            case hashString("inherit"): {
                if (!mtlxDefinitionOptions.typeMismatchWarning) break;
                if (attributeType != "string") {
                    std::cerr << "Warning: inherit attribute must be string type, got " << attributeType << " for nodedef " << shaderName << std::endl;
                }
                break;
            }

            case hashString("nodegroup"): {
                if (!mtlxDefinitionOptions.typeMismatchWarning) break;
                if (attributeType != "string") {
                    std::cerr << "Warning: nodegroup attribute must be string type, got " << attributeType << " for nodedef " << shaderName << std::endl;
                }
                break;
            }

            case hashString("version"): {
                if (!mtlxDefinitionOptions.typeMismatchWarning) break;
                if (attributeType != "string") {
                    std::cerr << "Warning: version attribute must be string type formatted as " << "major[.minor], got " << attributeType << " for nodedef " << shaderName << std::endl;
                }
                break;
            }

            case hashString("isdefaultversion"): {
                if (!mtlxDefinitionOptions.typeMismatchWarning) break;
                if (attributeType != "integer") {
                    std::cerr << "Warning: isdefaultversion must be boolean (integer), got " << attributeType << " for nodedef " << shaderName << std::endl;
                }
                
                if (attributeValue == "1") {
                    attributeValue = "true";
                } else if (attributeValue == "0") {
                    attributeValue = "false";
                } else {
                    std::cerr << "Warning: isdefaultversion should be boolean (1 or 0), got " << attributeValue << " for nodedef " << shaderName << std::endl;
                }

                break;
            }

            case hashString("target"): {
                if (!mtlxDefinitionOptions.typeMismatchWarning) break;
                if (attributeType != "stringarray") {
                    std::cerr << "Warning: target attribute must be stringarray type, got " << attributeType << " for nodedef " << shaderName << std::endl;
                }
                break;
            }
            case hashString("uiname"): {
                if (!mtlxDefinitionOptions.typeMismatchWarning) break;
                if (attributeType != "string") {
                    std::cerr << "Warning: uiname attribute must be string type, got " << attributeType << " for nodedef " << shaderName << std::endl;
                }
                break;
            }
            case hashString("internalgeomprops"): {
                if (!mtlxDefinitionOptions.typeMismatchWarning) break;
                if (attributeType != "stringarray") {
                    std::cerr << "Warning: internalgeomprops must be stringarray type, got " << attributeType << " for nodedef " << shaderName << std::endl;
                }
                break;
            }

            case hashString("doc"):
            case hashString("help"):
            default: {
                if (!mtlxDefinitionOptions.unknownAttributeWarning) break;
                std::cout << "Warning: Unknown attribute '" << attributeName << "' for nodedef " << shaderName << std::endl;
                break;
            }
        }

        // Assuming the verification passed the attribute will be added
        nodeDef->setAttribute(attributeName, attributeValue);
    }

    for (auto param = osoQuery.begin(); param != osoQuery.end(); ++param) {

        // add the struct if it doesn't exist
        if (param->isstruct) { 
            std::string typeName = param->structname.c_str();
            std::string paramName = param->name.c_str();
            
            mx::TypeDefPtr type = typeDefMtlxDoc->getTypeDef(typeName);

            if (!type) { // if type doesn't exist, add it
                type = typeDefMtlxDoc->addTypeDef(typeName);

                // TODO:
                // for some types like mtx integer aren't rewritten as int in the generated osl struct
                
                for (auto field = param->fields.begin(); field != param->fields.end(); ++field) {
                    std::string fieldName = field->c_str();
                    std::string fullFieldName = paramName + "." + fieldName;

                    const osl::OSLQuery::Parameter* fieldParam = osoQuery.getparam(fullFieldName);

                    if (fieldParam == nullptr) {
                        std::cerr << "Failed to find OSL parameter for struct field: " << fullFieldName << std::endl;
                        continue;
                    }
                    
                    std::string fieldType = parseOSLParameterType(*fieldParam);

                    mx::MemberPtr member = type->addMember(fieldName);
                    member->setType(fieldType);
                }
            }
        }

        // skip struct members
        std::string paramName = param->name.c_str();
        if (paramName.find('.') != std::string::npos) {
            continue;
        }

        std::string defaultValue = parseOSLParameterValue(*param, &osoQuery, paramName);
        std::string paramType = parseOSLParameterType(*param);

        for (auto metadata = param->metadata.begin(); metadata != param->metadata.end(); ++metadata) {
            std::string attributeName = metadata->name.c_str();
            std::string attributeType = parseOSLParameterType(*metadata);
            std::string attributeValue = parseOSLParameterValue(*metadata);

            bool attributeTypeIsString = attributeType == "string";

            switch (hashString(attributeName)) {
                case hashString("name"):  { 
                    if (!mtlxDefinitionOptions.implicitAssignmentWarning) break;
                    paramName = attributeValue;
                    std::cout << "name is determined by the name of the shader function signature. Skipping" << std::endl;
                    continue;
                }
                case hashString("type"): {
                    switch (hashString(attributeValue)) { 
                        case hashString( "filename"): {
                            // allow filename attributes to be specified as strings 
                            // in metadata since that's more intuitive 
                            // TODO: actaully figure what the path semantics are for this 
                            if (attributeTypeIsString) {
                                paramType = "filename";
                            }
                            continue;
                        }
                        default: {
                            if (!mtlxDefinitionOptions.implicitAssignmentWarning) break;
                            std::cout << "type is determined from the OSL parameter type and cannot be overridden by metadata. Skipping." << std::endl;
                            continue;
                        }
                    }
                }
                case hashString("value"): {
                    if (!mtlxDefinitionOptions.implicitAssignmentWarning) break;
                    std::cout << "value is determined from the OSL parameter type and cannot be overridden by metadata. Skipping." << std::endl;
                    continue;
                }
            }
        }

        mx::ElementPtr element;

        if (param->isoutput) {
            element = nodeDef->addOutput(paramName, paramType);
        } else {
            element = nodeDef->addInput(paramName, paramType);
        }

        if (paramType != "BSDF") {
            element->setAttribute("value", defaultValue);
        }

        //Add Metadata
        for (auto metadata = param->metadata.begin(); metadata != param->metadata.end(); ++metadata) {

            std::string attributeName = metadata->name.c_str();
            std::string attributeType = parseOSLParameterType(*metadata);
            std::string attributeValue = parseOSLParameterValue(*metadata);
            
            if (attributeType == "string") {
                attributeValue = unescapeString(attributeValue);
            }

            switch (hashString(attributeName)) {
                case hashString("uniform"): {
                    if (!mtlxDefinitionOptions.typeMismatchWarning) break;
                    if (attributeType != "integer") {
                        std::cerr << "Warning: uniform attribute must be boolean (integer), got " << attributeType << " for parameter " << paramName << std::endl;
                    }
                    break;
                }
                case hashString("defaultgeomprop"): {
                    if (!mtlxDefinitionOptions.typeMismatchWarning) break;
                    /* do i even need these
                    switch (hashString(attributeValue)) {
                        "Pobject"
                        "Nobject"
                        "Tobject"
                        "Bobject"
                        "Pworld"
                        "Nworld"
                        "Tworld"
                        "Bworld"	
                        "UV0"
                    }
                    */

                    if (paramType != "vector3") {
                        std::cerr << "Warning: defaultgeomprop can only be used with vector3 inputs, parameter " << paramName << " is " << paramType << std::endl;
                    }
                    break;
                }
                case hashString("enum"): {
                    if (!mtlxDefinitionOptions.typeMismatchWarning) break;
                    if (attributeType != "stringarray") {
                        std::cerr << "Warning: enum attribute should be stringarray type, got " << attributeType << " for parameter " << paramName << std::endl;
                    }
                    break;
                }
                case hashString("enumvalues"): {
                    if (!mtlxDefinitionOptions.typeMismatchWarning) break;
                    std::string expectedType = paramType + "array";
                    if (attributeType != expectedType) {
                        std::cerr << "Warning: enumvalues should be " << expectedType << " type, got " << attributeType << " for parameter " << paramName << std::endl;
                    }
                    break;
                }
                case hashString("colorspace"): {
                    if (!mtlxDefinitionOptions.typeMismatchWarning) break;
                    if (paramType != "color3") {
                        std::cerr << "Warning: colorspace can only be used with color3 inputs, parameter " << paramName << " is " << paramType << std::endl;
                    }
                    break;
                }
                case hashString("unittype"): {
                   if (!mtlxDefinitionOptions.typeMismatchWarning) break;
                    if (paramType != "float" && paramType != "vector3" && paramType != "filename") {
                        std::cerr << "Warning: unittype can only be used with float, vector, or filename inputs, parameter " << paramName << " is " << paramType << std::endl;
                    }
                    break;
                }
                case hashString("uiname"): {
                    if (!mtlxDefinitionOptions.typeMismatchWarning) break;
                    if (attributeType != "string") {
                        std::cerr << "Warning: uiname should be string type, got " << attributeType << " for parameter " << paramName << std::endl;
                    }
                    break;
                }
                case hashString("uifolder"): {
                    if (!mtlxDefinitionOptions.typeMismatchWarning) break;
                    if (attributeType != "string") {
                        std::cerr << "Warning: uifolder should be string type, got " << attributeType << " for parameter " << paramName << std::endl;
                    }
                    break;
                }
                case hashString("uimin"):
                case hashString("uimax"):
                case hashString("uisoftmin"):
                case hashString("uisoftmax"):
                case hashString("uistep"): {
                    if (!mtlxDefinitionOptions.typeMismatchWarning) break;
                    if (paramType != "integer" && paramType != "float" && paramType != "color3" && paramType != "vector3") {
                        std::cerr << "Warning: " << attributeName << " can only be used with integer, float, color3, or vector3 inputs, parameter " << paramName << " is " << paramType << std::endl;
                    } else if (attributeType != paramType) {
                        std::cerr << "Warning: " << attributeName << " must be the same type as the parameter. Expected " << paramType << ", got " << attributeType << " for parameter " << paramName << std::endl;
                    }
                    break;
                }
                case hashString("unit"): 
                case hashString("doc"): 
                case hashString("hint"): {
                    break;
                }
                default: {
                    if (!mtlxDefinitionOptions.unknownAttributeWarning) break;
                    std::cout << "Warning: Unknown attribute '" << attributeName << "' for parameter " << paramName << std::endl;
                    
                    break;
                }
            }

            // Assuming the verification passed the attribute will be added
            element->setAttribute(attributeName, attributeValue);
        }

    }

    std::string validationErrors;

    if (!implMtlxDoc->validate(&validationErrors)) {
        std::cerr << "MaterialX document validation failed after adding NodeDef: " << validationErrors << std::endl;
        return false;
    }

    try {
        // OSL Implementation
        std::string implNameOSL = "IM_" + nodeName + "_genosl";
        auto implOSL = implMtlxDoc->addImplementation(implNameOSL);

        if (!implOSL) {
            std::cerr << "Failed to create Implementation for node: " << nodeName << std::endl;
            return false;
        }

        implOSL->setNodeDef(nodeDef);
        implOSL->setFile(oslFilePath);
        implOSL->setFunction(shaderName);
        implOSL->setTarget("genosl");

        // GLSL Stub Implementation
        mx::FilePath glslFilePath = oslFileName;
        glslFilePath.removeExtension();
        glslFilePath.addExtension("glsl");
        
        if (!generateGLSLStub(oslFileName, shaderName, nodeDef, outputDir, nodeDefMtlxDoc, typeDefMtlxDoc)) {
            std::cerr << "Failed to generate GLSL stub for node: " << nodeName << std::endl;
        } else {
            std::string implNameGLSL = "IM_" + nodeName + "_genglsl";
            auto implGLSL = implMtlxDoc->addImplementation(implNameGLSL);

            if (!implGLSL) {
                std::cerr << "Failed to create GLSL Implementation for node: " << nodeName << std::endl;
                return false;
            }

            implGLSL->setNodeDef(nodeDef);
            implGLSL->setFile(glslFilePath);
            implGLSL->setFunction(shaderName);
            implGLSL->setTarget("genglsl");
        }
    } catch (ExceptionCompileError& exc) {
        std::cerr << "Uh oh! There was error for the following node: "
                    << nodeDef->getName() << std::endl;
        std::cerr << exc.what() << std::endl;

        for (const std::string& error : exc.errorLog()) {
            std::cerr << error << std::endl;
        }
    }

    if (!implMtlxDoc->validate(&validationErrors)) {
        std::cerr << "MaterialX document validation failed after adding Implementation: " << validationErrors << std::endl;
        return false;
    }

    constexpr bool debug = false;

    if (debug) {
        std::cout << "Registered " << oslFilePath.getBaseName() << " as " << nodeDef->getName() << std::endl;
        for (const auto& input : nodeDef->getInputs()) {
            std::cout << "  Input: " << input->getType() << " " << input->getName() << std::endl;
        }
        
        for (const auto& output : nodeDef->getOutputs()) {
            std::cout << "  Output: " << output->getType() << " " << output->getName() << std::endl;
        }
    }

    return true;
}

void dumpElement(mx::ElementPtr elem, int indent = 0) {
    std::string padding(indent, ' ');

    std::cout << padding
              << elem->getCategory()
              << " name=\"" << elem->getName() << "\""
              << " path=\"" << elem->getNamePath() << "\"";

    // only TypedElement has getType()
    if (auto typed = elem->asA<mx::TypedElement>()) {
        if (!typed->getType().empty())
            std::cout << " type=\"" << typed->getType() << "\"";
    }

    // only ValueElement has getValueString()
    if (auto valueElem = elem->asA<mx::ValueElement>()) {
        if (valueElem->hasValue())
            std::cout << " value=\"" << valueElem->getValueString() << "\"";
    }

    std::cout << std::endl;

    if (auto node = elem->asA<mx::Node>()) {
        for (auto input : node->getInputs()) {
            std::cout << padding << "  INPUT "
                      << input->getName()
                      << " type=" << input->getType();

            if (input->getConnectedOutput()) {
                std::cout << " -> "
                          << input->getConnectedOutput()->getNamePath();
            } else if (input->hasValue()) {
                std::cout << " value="
                          << input->getValueString();
            }

            std::cout << std::endl;
        }

        for (auto output : node->getOutputs()) {
            std::cout << padding << "  OUTPUT "
                      << output->getName()
                      << " type=" << output->getType()
                      << std::endl;
        }
    }

    for (auto child : elem->getChildren()) {
        dumpElement(child, indent + 2);
    }
}

int main(int argc, char* const argv[]) {
    std::vector<std::string> tokens;

    for (int i = 1; i < argc; i++) {
        tokens.emplace_back(argv[i]);
    }

    LibraryArgumentHandler inputArgs;
    inputArgs.oslCompileOptions.writeSourceToDisk = true;

    for (size_t i = 0; i < tokens.size(); i++) {
        const std::string& token = tokens[i];
        const std::string& nextToken = i + 1 < tokens.size() ? tokens[i + 1] : mx::EMPTY_STRING;
        ArgumentHandler::ParseResult parseResult = inputArgs.parse(token, nextToken);

        switch (parseResult) {
            case ArgumentHandler::SUCCESS:
                break;
            case ArgumentHandler::SUCCESS_CONSUME_NEXT:
                i++;
                break;
            case ArgumentHandler::FAILURE:
                return 1;
            case ArgumentHandler::EXIT:
                return 0;
        }
    }

    if (!inputArgs.verify()) {
        std::cerr << "Input argument verification failed." << std::endl;
        return 1;
    }

    //get std::vector of paths to all .osl files in oslLibraryPath
    std::vector<mx::FilePath> files = findFiles(inputArgs.oslLibraryPath, ".osl");

    // shader metadata is defined in the shader entry 
    // SOLO_SHADER enables shader entry points 
    inputArgs.oslCompileOptions.definePreprocessors.emplace_back("SOLO_SHADER");

    MaterialXDefinitionOptions mtlxDefinitionOptions;

    mx::DocumentPtr implMtlxDoc = mx::createDocument();
    mx::DocumentPtr typeDefMtlxDoc = mx::createDocument();
    mx::DocumentPtr nodeDefMtlxDoc = mx::createDocument();

    for (int i = 0; i < files.size(); i++) {
        const std::string oslFileName = files[i].getBaseName();
        
        // Get relative path from oslLibraryPath using filesystem utilities
        fs::path fullPath(files[i].asString());
        fs::path basePath(inputArgs.oslLibraryPath.asString());
        fs::path relativePath = fs::relative(fullPath, basePath);
        mx::FilePath relativeOslPath(relativePath.string());

        try {
            // open and read the osl file
            std::ifstream oslFileInput(files[i].asString());
            if (!oslFileInput.is_open()) {
                throw std::runtime_error("Failed to open the OSL file for reading: " + files[i].asString());
            }

            std::stringstream buffer;
            buffer << oslFileInput.rdbuf();
            std::string oslFileContent = buffer.str();
            oslFileInput.close();

            osl::OSLQuery osoQuery;
            
            compileOSLToBytecode(oslFileContent, oslFileName, inputArgs.libraryOutputPath, inputArgs.oslCompileOptions, &osoQuery);
    
            createMaterialXDefinitions(osoQuery, oslFileName, relativeOslPath, nodeDefMtlxDoc, implMtlxDoc, typeDefMtlxDoc, inputArgs.libraryOutputPath, mtlxDefinitionOptions);
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    // The headers in the autolib just point to shaders in src
    // After duplicated shaders are flattened to root, the includes 
    // in the headers need to be rewritten to point to the new location of the osl files.
    if (inputArgs.autolibIncludeRewrite) {
        for (const auto& includePath : inputArgs.oslCompileOptions.oslIncludePath) {
            auto headerFiles = findFiles(includePath, ".h", true);
            
            for (const auto& relativeHeaderPath : headerFiles) {
                mx::FilePath outputHeaderPath = inputArgs.libraryOutputPath / relativeHeaderPath;
                
                if (outputHeaderPath.exists()) {
                    rewriteHeaderIncludePaths(outputHeaderPath, relativeHeaderPath);
                }
            }
        }
    }

    mx::FilePath outputNodeDefFilePath = inputArgs.libraryOutputPath / "autolib_defs.mtlx";
    mx::FilePath outputImplFilePath = inputArgs.libraryOutputPath / "autolib_genosl_impl.mtlx";

    nodeDefMtlxDoc->importLibrary(typeDefMtlxDoc);

    constexpr bool debug = false;

    if (debug) {
        for (auto elem : nodeDefMtlxDoc->traverseTree()) {
            //dumpElement(elem);
        }

        for (auto elem : implMtlxDoc->traverseTree()) {
            //dumpElement(elem);
        }

        for (auto elem : nodeDefMtlxDoc->traverseTree()) {
            if (auto valueElem = elem->asA<mx::ValueElement>()) {
                if (valueElem->hasValue()) {
                    std::cout << valueElem->getNamePath()
                            << " type=" << valueElem->getType()
                            << " value=\"" << valueElem->getValueString()
                            << "\"" << std::endl;
                }
                valueElem->validate();
            }
        }
    }

    
    std::string nodeDefMessage;
    bool isNodeDefValid = nodeDefMtlxDoc->validate(&nodeDefMessage);

    if (!isNodeDefValid) {
        std::cerr << "Node Def Validation failed:\n" << nodeDefMessage << std::endl;
        return 1;
    }

    std::string implMessage;
    bool isImplValid = implMtlxDoc->validate(&implMessage);

    if (!isImplValid) {
        std::cerr << "Implementation Validation failed:\n" << implMessage << std::endl;
        return 1;
    }
    
    if (!inputArgs.skipWritingMtlxHeaders) {
        mx::writeToXmlFile(nodeDefMtlxDoc, outputNodeDefFilePath);
        mx::writeToXmlFile(implMtlxDoc, outputImplFilePath);
    }

    return 0;
}