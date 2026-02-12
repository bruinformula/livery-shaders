#include <iostream>
#include <string>
#include <vector>

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

const std::string argOptions =
    " MaterialXLibraryBuilder -- Generates MaterialX bindings for a bunch of OSL shaders\n"
    " Options: \n"
    "    --oslLibraryPath           Path to the directory containing OSL shader files.  All .osl files in this directory and its subdirectories will be processed. \n"
    "    --libraryOutputPath        Path to the output directory where generated MaterialX documents and OSL shaders will be written. \n"
    "    --skipWritingOSLSource     Skip generating Only generate OSL source files to the libraryOutputPath \n"
    "    --skipWritingMtlxHeaders   Skip generating MaterialX implementation files to the libraryOutputPath. \n"
    "    --oslIncludePath           OSL Include Path\n"
    "    --oslDefine [NAME=VALUE]   Define a preprocessor macro to be used during OSL compilation.  Can be specified multiple times to define multiple macros.\n"
    "    --path                     Specify an additional data search path location (e.g. '/projects/MaterialX').  This absolute path will be queried when locating data libraries, XInclude references, and referenced images.\n"
    "    --library                  Specify an additional data library folder (e.g. 'vendorlib', 'studiolib').  This relative path will be appended to each location in the data search path when loading data libraries.\n"
    "    --help                     Prints this message\n";

struct CommandLineArgs {
    enum ParseResult {
        SUCCESS,
        SUCCESS_AND_BUMP,
        FAILURE,
        EXIT
    };
    mx::FilePath oslIncludePath;
    mx::FilePath oslLibraryPath;
    mx::FilePath libraryOutputPath;
        std::vector<std::string> oslDefine;


    bool skipWritingOSLSource;
    bool skipWritingMtlxHeaders;

    ParseResult parse(const std::string& token, const std::string& nextToken) {
        // yes managed to get goto in here
        if (token == "--oslLibraryPath") {
            if (nextToken.empty()) goto expectOption;
            if (!oslLibraryPath.isEmpty()) goto alreadySet;
            
            oslLibraryPath = mx::FilePath(nextToken);
            return SUCCESS_AND_BUMP;
        } else if (token == "--libraryOutputPath") {
            if (nextToken.empty()) goto expectOption;
            if (!libraryOutputPath.isEmpty()) goto alreadySet;

            libraryOutputPath = mx::FilePath(nextToken);
            return SUCCESS_AND_BUMP;
        } else if (token == "--oslIncludePath") {
            if (nextToken.empty()) goto expectOption;
            if (!oslIncludePath.isEmpty()) goto alreadySet;
            
            oslIncludePath = mx::FilePath(nextToken);
            return SUCCESS_AND_BUMP;
        } else if (token == "--oslDefine") {
            if (nextToken.empty()) goto expectOption;
            oslDefine.push_back(nextToken);
            return SUCCESS_AND_BUMP;
        } else if (token == "--skipWritingOSLSource") {
            skipWritingOSLSource = true;
            return SUCCESS;
        } else if (token == "--skipWritingMtlxHeaders") {
            skipWritingMtlxHeaders = true;
            return SUCCESS;
        } else if (token == "--help") {
            std::cout << argOptions << std::endl;
            return EXIT;
        } else {
            std::cout << "Unrecognized command-line option: " << token << std::endl;
            return FAILURE;
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

    //enforces any additional rules about the input arguments
    bool verify() {
        if (oslLibraryPath.isEmpty()) {
            std::cerr << "oslLibraryPath is not set!" << std::endl;
            return false;
        }
        if (libraryOutputPath.isEmpty()) {
            std::cerr << "libraryOutputPath is not set!" << std::endl;
            return false;
        }

        // Output Path
        if (!libraryOutputPath.exists() || !libraryOutputPath.isDirectory()) {
            libraryOutputPath.createDirectory();

            if (!libraryOutputPath.exists() || !libraryOutputPath.isDirectory()) {
                std::cerr << "Failed to find and/or create the provided output mtlx path:"
                        << libraryOutputPath.asString() << std::endl;
                return false;
            }
        }

        if (!oslIncludePath.exists() || !oslIncludePath.isDirectory()) {
            std::cerr << "The provided path to the OSL includes is not valid: " << oslIncludePath.asString() << std::endl;
            return false;
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

bool createMaterialXDefinitions(
    osl::OSLQuery& osoQuery,
    const std::string& oslFileName,
    mx::DocumentPtr& nodeDefMtlxDoc,
    mx::DocumentPtr& implMtlxDoc,
    mx::DocumentPtr& typeDefMtlxDoc,
    const mx::FilePath& outputDir,
    MaterialXDefinitionOptions& mtlxDefinitionOptions
) {
    //mx::FilePath oslFilePath = outputDir / oslFileName;
    mx::FilePath oslFilePath = oslFileName;
    oslFilePath.removeExtension();
    oslFilePath.addExtension("osl");

    //mx::FilePath osoFilePath = outputDir / oslFileName;
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

    //std::cout << osoQuery.nparams() << " parameters found for shader: " << shaderName << std::endl;

    for (auto param = osoQuery.begin(); param != osoQuery.end(); ++param) {

        // add the struct if it doesn't exist
        if (param->isstruct) { 
            std::string typeName = param->structname.c_str();
            std::string paramName = param->name.c_str();
            
            mx::TypeDefPtr type = typeDefMtlxDoc->getTypeDef(typeName);

            if (!type) { // if type does exist, add it
                type = typeDefMtlxDoc->addTypeDef(typeName);

                // the struct fields get spit out as "struct.fieldname"
                for (auto field = param->fields.begin(); field != param->fields.end(); ++field) {
                    std::string fieldName = field->c_str();
                    std::string fullFieldName = paramName + "." + fieldName;

                    const osl::OSLQuery::Parameter* fieldParam = osoQuery.getparam(fullFieldName);
                    std::string fieldType = parseOSLParameterType(*fieldParam);

                    mx::MemberPtr member = type->addMember(fieldName);
                    member->setType(fieldType);
                }
            }
        }

        // skip struct members. they are returned as params in the function signature (e.g., "mv1.x", "mv2.y")
        std::string paramName = param->name.c_str();
        if (paramName.find('.') != std::string::npos) {
            continue;
        }

        std::string paramType = parseOSLParameterType(*param);

        mx::ElementPtr element;
        if (param->isoutput) {
            element = nodeDef->addOutput(paramName, paramType);
        } else {
            element = nodeDef->addInput(paramName, paramType);
        }

        //Add Defaults 
        std::string defaultValue = parseOSLParameterValue(*param, &osoQuery, paramName);

        if (paramType != "BSDF") {
            element->setAttribute("value", defaultValue);
        }

        //Add Metadata
        for (auto metadata = param->metadata.begin(); metadata != param->metadata.end(); ++metadata) {

            std::string attributeName = metadata->name.c_str();
            std::string attributeType = parseOSLParameterType(*metadata);
            std::string attributeValue = parseOSLParameterValue(*metadata);
            
            // Unescape string attributes
            if (attributeType == "string") {
                attributeValue = unescapeString(attributeValue);
            }

            switch (hashString(attributeName)) {
                case hashString("name"):  { // name is just the name in the shader
                    if (!mtlxDefinitionOptions.implicitAssignmentWarning) break;
                    std::cout << "name is determined by the name of the shader function signature. Skipping" << std::endl;
                    continue; // skip setting this attribute
                }
                case hashString("type"): {
                    if (!mtlxDefinitionOptions.implicitAssignmentWarning) break;
                    std::cout << "type is determined from the OSL parameter type and cannot be overridden by metadata. Skipping." << std::endl;
                    continue; // skip setting this attribute
                }
                case hashString("value"): {
                    if (!mtlxDefinitionOptions.implicitAssignmentWarning) break;
                    std::cout << "value is determined from the OSL parameter type and cannot be overridden by metadata. Skipping." << std::endl;
                    continue; // skip setting this attribute
                }
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
                        std::cerr << "Warning: colorspace can only be used with color3inputs, parameter " << paramName << " is " << paramType << std::endl;
                    }
                    break;
                }
                case hashString("unittype"): {
                   if (!mtlxDefinitionOptions.typeMismatchWarning) break;
                    if (paramType != "float" && paramType != "vector3" && paramType != "filename") { // TODO: need to ensure filename
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

    std::cout << "Compiled " << oslFilePath.getBaseName() << " -> " << osoFilePath.getBaseName() << std::endl;

    return true;
}

int main(int argc, char* const argv[]) {
    std::vector<std::string> tokens;

    for (int i = 1; i < argc; i++) {
        tokens.emplace_back(argv[i]);
    }

    CommandLineArgs inputArgs;
    for (size_t i = 0; i < tokens.size(); i++) {
        const std::string& token = tokens[i];
        const std::string& nextToken = i + 1 < tokens.size() ? tokens[i + 1] : mx::EMPTY_STRING;
        CommandLineArgs::ParseResult parseResult = inputArgs.parse(token, nextToken);

        switch (parseResult) {
            case CommandLineArgs::SUCCESS:
                break;
            case CommandLineArgs::SUCCESS_AND_BUMP:
                i++;
                break;
            case CommandLineArgs::FAILURE:
                return 1;
            case CommandLineArgs::EXIT:
                return 0;
        }
    }

    if (!inputArgs.verify()) {
        std::cerr << "Input argument verification failed." << std::endl;
        return 1;
    }

    //get std::vector of paths to all .osl files in oslLibraryPath
    std::vector<mx::FilePath> files = findFiles(inputArgs.oslLibraryPath, ".osl");
    
    mx::FileSearchPath oslRendererIncludePaths;
    oslRendererIncludePaths.append(inputArgs.oslIncludePath);

    OslCompileOptions options;
    options.oslIncludePath = oslRendererIncludePaths;
    options.writeSourceToDisk = !inputArgs.skipWritingOSLSource;
    options.definePreprocessors = inputArgs.oslDefine;

    // shader metadata is defined in the shader entry 
    // SOLO_SHADER enables shader entry points 
    options.definePreprocessors.emplace_back("SOLO_SHADER");

    MaterialXDefinitionOptions mtlxDefinitionOptions;

    mx::DocumentPtr implMtlxDoc = mx::createDocument();
    mx::DocumentPtr typeDefMtlxDoc = mx::createDocument();
    mx::DocumentPtr nodeDefMtlxDoc = mx::createDocument();

    for (int i = 0; i < files.size(); i++) {
        const std::string oslFileName = files[i].getBaseName();

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
            
            compileOSLToBytecode(oslFileContent, oslFileName, inputArgs.libraryOutputPath, options, &osoQuery);
    
            createMaterialXDefinitions(osoQuery, oslFileName, nodeDefMtlxDoc, implMtlxDoc, typeDefMtlxDoc, inputArgs.libraryOutputPath, mtlxDefinitionOptions);
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    mx::FilePath outputNodeDefFilePath = inputArgs.libraryOutputPath / "autolib_defs.mtlx";
    mx::FilePath outputImplFilePath = inputArgs.libraryOutputPath / "autolib_genosl_impl.mtlx";

    nodeDefMtlxDoc->importLibrary(typeDefMtlxDoc); // add the typdefs to the end of the file

    if (!inputArgs.skipWritingMtlxHeaders) {
        mx::writeToXmlFile(nodeDefMtlxDoc, outputNodeDefFilePath);
        mx::writeToXmlFile(implMtlxDoc, outputImplFilePath);
    }


    return 0;
}
