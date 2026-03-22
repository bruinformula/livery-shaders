#include <MaterialXFormat/XmlIo.h>
#include <MaterialXGenShader/Library.h>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
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
    "    --arnold-impl              Add Arnold implementation. \n"
    "    --genosl-impl              Add Arnold implementation. \n"
    "    --genglsl-dummy-impl       Add genglsl dummy implementation. \n"
    "    --path                     Specify an additional data search path location (e.g. '/projects/MaterialX').  This absolute path will be queried when locating data libraries, XInclude references, and referenced images.\n"
    "    --library                  Specify an additional data library folder (e.g. 'vendorlib', 'studiolib').  This relative path will be appended to each location in the data search path when loading data libraries.\n"
    "    --help                     Prints this message\n";

class MaterialXDefinitionOptions {
public:
    MaterialXDefinitionOptions() = default;
    ~MaterialXDefinitionOptions() = default;

    bool unknownAttributeWarning = false;
    bool typeMismatchWarning = true;
    bool implicitAssignmentWarning = true;
    bool writeArnoldImpl = false;
    bool writeGenOSLImpl = false;
    bool writeGenGLSLDummy = false;
};

struct LibraryArgumentHandler : public ArgumentHandler {
    mx::FilePath oslLibraryPath;
    mx::FilePath mtlxNodeGraphsPath;
    mx::FilePath libraryOutputPath;
    mx::FilePath copyFilesOfPath;
    bool skipWritingMtlxHeaders = false;
    MaterialXDefinitionOptions mtlxOptions;

    OslCompileOptions oslCompileOptions;
    
    ParseResult parse(const std::string& token, const std::string& nextToken) override {
        switch (hashString(token.c_str())) {
            case hashString("--oslLibraryPath"): {
                if (nextToken.empty()) goto expectOption;
                if (!oslLibraryPath.isEmpty()) goto alreadySet;
                oslLibraryPath = mx::FilePath(nextToken);
                return SUCCESS_CONSUME_NEXT;
            }
            case hashString("--copyFilesOfPath"): {
                if (nextToken.empty()) goto expectOption;
                if (!copyFilesOfPath.isEmpty()) goto alreadySet;
                copyFilesOfPath = mx::FilePath(nextToken);
                return SUCCESS_CONSUME_NEXT;
            }
            case hashString("--mtlxNodeGraphsPath"): {
                if (nextToken.empty()) goto expectOption;
                if (!mtlxNodeGraphsPath.isEmpty()) goto alreadySet;
                mtlxNodeGraphsPath = mx::FilePath(nextToken);
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
            case hashString("--arnold-impl"): {
                mtlxOptions.writeArnoldImpl = true;
                return SUCCESS;
            }
            case hashString("--genosl-impl"): {
                mtlxOptions.writeGenOSLImpl = true;
                return SUCCESS;
            }
            case hashString("--genglsl-dummy-impl"): {
                mtlxOptions.writeGenGLSLDummy = true;
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

        bool atLeastOne = mtlxOptions.writeArnoldImpl || mtlxOptions.writeGenOSLImpl;

        if (!atLeastOne) {
            std::cerr << "Must specify a gen target (eg: genosl)" << std::endl;
            return false;
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

    if (osoQuery.shadername().empty()) {
        std::cerr << "OSLQuery is empty for file: " << oslFileName << std::endl;
        return false;
    }

    std::string nodeName = osoQuery.shadername().c_str();

    std::string nodeDefName;

    // there is some Arnold bug where nodes must be resgistered with the ARNOLD_ or there'll be a string_view something something error 
    if (mtlxDefinitionOptions.writeArnoldImpl) {
        nodeDefName = "ARNOLD_ND_" + nodeName;
    } else {
        nodeDefName = "ND_" + nodeName;
    }

    mx::NodeDefPtr nodeDef = nodeDefMtlxDoc->addNodeDef(
        nodeDefName,
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
                    std::cerr << "Warning: inherit attribute must be string type, got " << attributeType << " for nodedef " << nodeName << std::endl;
                }
                break;
            }

            case hashString("nodegroup"): {
                if (!mtlxDefinitionOptions.typeMismatchWarning) break;
                if (attributeType != "string") {
                    std::cerr << "Warning: nodegroup attribute must be string type, got " << attributeType << " for nodedef " << nodeName << std::endl;
                }
                break;
            }

            case hashString("version"): {
                if (!mtlxDefinitionOptions.typeMismatchWarning) break;
                if (attributeType != "string") {
                    std::cerr << "Warning: version attribute must be string type formatted as " << "major[.minor], got " << attributeType << " for nodedef " << nodeName << std::endl;
                }
                break;
            }

            case hashString("isdefaultversion"): {
                if (!mtlxDefinitionOptions.typeMismatchWarning) break;
                if (attributeType != "integer") {
                    std::cerr << "Warning: isdefaultversion must be boolean (integer), got " << attributeType << " for nodedef " << nodeName << std::endl;
                }
                
                if (attributeValue == "1") {
                    attributeValue = "true";
                } else if (attributeValue == "0") {
                    attributeValue = "false";
                } else {
                    std::cerr << "Warning: isdefaultversion should be boolean (1 or 0), got " << attributeValue << " for nodedef " << nodeName << std::endl;
                }

                break;
            }

            case hashString("target"): {
                if (!mtlxDefinitionOptions.typeMismatchWarning) break;
                if (attributeType != "stringarray") {
                    std::cerr << "Warning: target attribute must be stringarray type, got " << attributeType << " for nodedef " << nodeName << std::endl;
                }
                break;
            }
            case hashString("uiname"): {
                if (!mtlxDefinitionOptions.typeMismatchWarning) break;
                if (attributeType != "string") {
                    std::cerr << "Warning: uiname attribute must be string type, got " << attributeType << " for nodedef " << nodeName << std::endl;
                }
                break;
            }
            case hashString("internalgeomprops"): {
                if (!mtlxDefinitionOptions.typeMismatchWarning) break;
                if (attributeType != "stringarray") {
                    std::cerr << "Warning: internalgeomprops must be stringarray type, got " << attributeType << " for nodedef " << nodeName << std::endl;
                }
                break;
            }

            case hashString("doc"):
            case hashString("help"):
            default: {
                if (!mtlxDefinitionOptions.unknownAttributeWarning) break;
                std::cout << "Warning: Unknown attribute '" << attributeName << "' for nodedef " << nodeName << std::endl;
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
        std::string implBaseName;

        if (mtlxDefinitionOptions.writeArnoldImpl) {
            implBaseName = "ARNOLD_IM_" + nodeName;
        } else {
            implBaseName = "IM_" + nodeName;
        }

        // OSL Implementation
        if (mtlxDefinitionOptions.writeGenOSLImpl) {
            std::string implNameOSL = implBaseName + nodeName + "_genosl";
            auto implOSL = implMtlxDoc->addImplementation(implNameOSL);

            if (!implOSL) {
                std::cerr << "Failed to create Implementation for node: " << nodeName << std::endl;
                return false;
            }

            implOSL->setNodeDef(nodeDef);
            implOSL->setFile(oslFilePath);
            implOSL->setFunction(nodeName);
            implOSL->setTarget("genosl");
        }
        
        // Arnold Implementation (registered through ARNOLD_PLUGIN_PATH)
        if (mtlxDefinitionOptions.writeArnoldImpl) {
            std::string implNameArnold = implBaseName + nodeName + "_arnold";
            auto implArnold = implMtlxDoc->addImplementation(implNameArnold);

            if (!implArnold) {
                std::cerr << "Failed to create Implementation for node: " << nodeName << std::endl;
                return false;
            }

            implArnold->setNodeDef(nodeDef);
            implArnold->setTarget("arnold");
        }

        // GLSL Stub Implementation
        if (mtlxDefinitionOptions.writeGenGLSLDummy) {
            mx::FilePath glslFilePath = oslFileName;
            glslFilePath.removeExtension();
            glslFilePath.addExtension("glsl");
            
            if (!generateGLSLStub(oslFileName, nodeName, nodeDef, outputDir, nodeDefMtlxDoc, typeDefMtlxDoc)) {
                std::cerr << "Failed to generate GLSL stub for node: " << nodeName << std::endl;
            } else {
                std::string implNameGLSL = implBaseName + nodeName + "_genglsl";
                auto implGLSL = implMtlxDoc->addImplementation(implNameGLSL);

                if (!implGLSL) {
                    std::cerr << "Failed to create GLSL Implementation for node: " << nodeName << std::endl;
                    return false;
                }

                implGLSL->setNodeDef(nodeDef);
                implGLSL->setFile(glslFilePath);
                implGLSL->setFunction(nodeName);
                implGLSL->setTarget("genglsl");
            }
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

    mx::DocumentPtr implMtlxDoc = mx::createDocument();
    mx::DocumentPtr typeDefMtlxDoc = mx::createDocument();
    mx::DocumentPtr nodeDefMtlxDoc = mx::createDocument();

    mx::FilePath osoOutputPath = inputArgs.libraryOutputPath / "oso";

    if (!osoOutputPath.exists()) {
        std::filesystem::create_directory(osoOutputPath.asString());
    }
            
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

            compileOSLToBytecode(
                oslFileContent, 
                oslFileName, 
                inputArgs.libraryOutputPath, 
                osoOutputPath, 
                inputArgs.oslCompileOptions, 
                &osoQuery
            );
    
            createMaterialXDefinitions(
                osoQuery, 
                oslFileName, 
                relativeOslPath, 
                nodeDefMtlxDoc, 
                implMtlxDoc, 
                typeDefMtlxDoc, 
                inputArgs.libraryOutputPath, 
                inputArgs.mtlxOptions
            );
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    mx::DocumentPtr nodeGraphMtlxDoc = mx::createDocument();

    if (!inputArgs.mtlxNodeGraphsPath.isEmpty()) {
        std::vector<mx::FilePath> files = findFiles(inputArgs.mtlxNodeGraphsPath, ".mtlx");

        for (const mx::FilePath& file : files) {
            try {
                mx::DocumentPtr nodeGraphDoc = mx::createDocument();
                mx::readFromXmlFile(nodeGraphDoc, file);

                for (const mx::NodeGraphPtr& nodeGraph : nodeGraphDoc->getNodeGraphs()) {

                    if (!nodeGraph->getNodeDefString().empty()) continue;

                    const std::string graphName = nodeGraph->getName();

                    // Guard against duplicates across multiple files
                    if (nodeGraphMtlxDoc->getNodeDef("ND_" + graphName)) {
                        std::cout << "Skipping duplicate: " << graphName << std::endl;
                        continue;
                    }

                    // Call on nodeGraphMtlxDoc — it internally calls addNodeGraph + addNodeDef
                    // on itself, and copyContentFrom(nodeGraph) works cross-document
                    // No copy loop needed — NG_ and ND_ land directly in nodeGraphMtlxDoc
                    mx::NodeDefPtr nodeDef = nodeGraphMtlxDoc->addNodeDefFromGraph(
                        nodeGraph,
                        "ND_" + graphName,
                        graphName,
                        "NG_" + graphName
                    );

                    if (!nodeDef) {
                        std::cerr << "Failed to create nodedef for: " << graphName << std::endl;
                        continue;
                    }

                    mx::NodeDefPtr sourceNodeDef = nodeGraph->getNodeDef();
                    if (sourceNodeDef && sourceNodeDef->hasNodeGroup()) {
                        nodeDef->setNodeGroup(sourceNodeDef->getNodeGroup());
                    }

                    //std::cout << "Registered: " << nodeDef->getName() << std::endl;
                }

            } catch (const std::exception& e) {
                std::cerr << "Failed to read: " << e.what() << std::endl;
                return 1;
            }
        }
    }
    
    mx::FilePath outputNodeDefFilePath = inputArgs.libraryOutputPath / "autolib_defs.mtlx";
    mx::FilePath outputImplFilePath = inputArgs.libraryOutputPath / "autolib_genosl_impl.mtlx";

    nodeDefMtlxDoc->importLibrary(typeDefMtlxDoc);
    nodeDefMtlxDoc->importLibrary(nodeGraphMtlxDoc);

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

    if (!inputArgs.copyFilesOfPath.isEmpty()) {
        fs::path sourcePath(inputArgs.copyFilesOfPath.asString());
        fs::path outputRoot(inputArgs.libraryOutputPath.asString());

        for (const fs::directory_entry& entry : fs::recursive_directory_iterator(sourcePath)) {
            if (!entry.is_regular_file()) continue;

            try {
                fs::path relativePath = fs::relative(entry.path(), sourcePath);
                fs::path outputFile = outputRoot / relativePath;

                fs::create_directories(outputFile.parent_path());
                fs::copy_file(entry.path(), outputFile, fs::copy_options::overwrite_existing);
            } catch (const std::exception& e) {
                std::cerr << "Failed to copy file " << entry.path() << ": " << e.what() << std::endl;
                return 1;
            }
        }
    }

    return 0;
}