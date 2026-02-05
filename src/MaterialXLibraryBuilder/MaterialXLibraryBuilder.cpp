#include <iostream>
#include <string>
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

namespace osl = OSL;
namespace mx = MaterialX;
namespace oiio = OIIO;

struct CommandLineArgs {
    enum ParseResult {
        SUCCESS,
        SUCCESS_AND_BUMP,
        FAILURE,
        EXIT
    };
    std::string oslInclude;

    std::string libraryPath;
    std::string outputPath;


    ParseResult parse(const std::string_view& token, const std::string_view& nextToken) {
        // yes managed to get goto in here
        if (token == "--libraryPath") {
            if (nextToken.empty()) goto expectOption;
            if (!libraryPath.empty()) goto alreadySet;
            
            libraryPath = nextToken;
            return SUCCESS_AND_BUMP;
        } else if (token == "--outputPath") {
            if (nextToken.empty()) goto expectOption;
            if (!outputPath.empty()) goto alreadySet;

            outputPath = nextToken;
            return SUCCESS_AND_BUMP;
        } else if (token == "--oslInclude") {
            if (nextToken.empty()) goto expectOption;
            if (!oslInclude.empty()) goto alreadySet;
            
            oslInclude = nextToken;
            return SUCCESS_AND_BUMP;
        } else if (token == "--help") {
            std::cout << "Usage: ./main --libraryPath <path> --outputPath <path> --oslInclude <path>" << std::endl;
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
        if (libraryPath.empty()) {
            std::cerr << "libraryPath is not set!" << std::endl;
            return false;
        }
        if (outputPath.empty()) {
            std::cerr << "outputPath is not set!" << std::endl;
            return false;
        }
        return true;
    }

};

std::vector<std::filesystem::path> findFiles(const std::filesystem::path& rootDir, const std::string_view& extension) {
    std::vector<std::filesystem::path> results;
    if (!std::filesystem::exists(rootDir))
        return results;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(rootDir)) {
        if (entry.is_regular_file() && entry.path().extension() == extension) {
            results.push_back(entry.path());
        }
    }
    return results;
}

std::string toSnakeCase(const std::string& input) {
    std::string out;
    out.reserve(input.size());

    for (size_t i = 0; i < input.size(); ++i) {
        char c = input[i];

        if (std::isupper(c)) {
            if (i != 0 && out.back() != '_') {
                out += '_';
            }
            out += std::tolower(c);
        } else {
            out += c;
        }
    }

    return out;
}

class ExceptionCompileError : public mx::Exception {
public:
    ExceptionCompileError(const std::string& msg, const mx::StringVec& errorLog = mx::StringVec()) :
        Exception(msg),
        _errorLog(errorLog)
    {
    }

    ExceptionCompileError(const ExceptionCompileError& e) :
        Exception(e),
        _errorLog(e._errorLog)
    {
    }

    ExceptionCompileError& operator=(const ExceptionCompileError& e)
    {
        Exception::operator=(e);
        _errorLog = e._errorLog;
        return *this;
    }

    const mx::StringVec& errorLog() const
    {
        return _errorLog;
    }

private:
    mx::StringVec _errorLog;
};

class OslCompileOptions {
public:
    OslCompileOptions() = default;
    ~OslCompileOptions() = default;

    mx::FilePath oslCompilerPath;
    mx::FileSearchPath oslIncludePath;
    bool writeSourceToDisk = true;
};

enum class MaterialXType {
    String,
    Integer,
    Float,
    Color3,
    Vector3,
    Matrix44,
    Matrix33,
    IntegerArray,
    FloatArray,
    StringArray,
    Color3Array,
    Vector3Array,
    BSDF,
    Struct,
    Unknown
};

std::string materialXTypeToString(
    MaterialXType type, 
    const std::string& structName = ""
) {
    switch (type) {
        case MaterialXType::String: return "string";
        case MaterialXType::Integer: return "integer";
        case MaterialXType::Float: return "float";
        case MaterialXType::Color3: return "color3";
        case MaterialXType::Vector3: return "vector3";
        case MaterialXType::Matrix44: return "matrix44";
        case MaterialXType::Matrix33: return "matrix33";
        case MaterialXType::IntegerArray: return "integerarray";
        case MaterialXType::FloatArray: return "floatarray";
        case MaterialXType::StringArray: return "stringarray";
        case MaterialXType::Color3Array: return "color3array";
        case MaterialXType::Vector3Array: return "vector3array";
        case MaterialXType::BSDF: return "BSDF";
        case MaterialXType::Struct: return structName;
        case MaterialXType::Unknown: 
        default: return "unknown";
    }
}

std::string parseOSLParameterValue(
    const osl::OSLQuery::Parameter& param, 
    const osl::OSLQuery* oslQuery = nullptr, 
    const std::string& paramName = ""
) {
    std::string result = "";

    // Handle struct types
    if (param.isstruct && oslQuery && !paramName.empty()) {
        std::vector<std::string> fieldValues;
        for (const auto& fieldName : param.fields) {
            std::string fullFieldName = paramName + "." + fieldName.c_str();
            const osl::OSLQuery::Parameter* fieldParam = oslQuery->getparam(fullFieldName);
            if (fieldParam) {
                std::string fieldValue = parseOSLParameterValue(*fieldParam);
                fieldValues.push_back(fieldValue);
            }
        }
        // Join field values with commas
        for (size_t i = 0; i < fieldValues.size(); ++i) {
            if (i > 0) result += ", ";
            result += fieldValues[i];
        }
        return result;
    }

    // Handle array types
    if (param.type.arraylen > 0 || param.type.is_array()) { // is_array() for unsized arrays
        if (param.type.elementtype() == osl::TypeDesc::INT) {
            // Integer array
            if (!param.idefault.empty()) {
                result = std::to_string(param.idefault[0]);
                for (size_t i = 1; i < std::min((size_t)param.type.arraylen, param.idefault.size()); ++i) {
                    result += ", " + std::to_string(param.idefault[i]);
                }
            }
        } else if (param.type.elementtype() == osl::TypeDesc::FLOAT) {
            // Float array
            if (!param.fdefault.empty()) {
                result = std::to_string(param.fdefault[0]);
                for (size_t i = 1; i < std::min((size_t)param.type.arraylen, param.fdefault.size()); ++i) {
                    result += ", " + std::to_string(param.fdefault[i]);
                }
            }
        } else if (param.type.elementtype() == osl::TypeDesc::STRING) {
            // String array
            if (!param.sdefault.empty()) {
                result = "\"" + std::string(param.sdefault[0].c_str()) + "\"";
                for (size_t i = 1; i < std::min((size_t)param.type.arraylen, param.sdefault.size()); ++i) {
                    result += ", \"" + std::string(param.sdefault[i].c_str()) + "\"";
                }
            }
        } else if (param.type.elementtype().aggregate == osl::TypeDesc::VEC3) {
            // Vector3/Color3/Normal3/Point3
            size_t elementsPerVec = 3;
            size_t numVecs = param.type.arraylen > 0 ? param.type.arraylen : (param.fdefault.size() / elementsPerVec);
            if (param.fdefault.size() >= elementsPerVec) {
                for (size_t vec = 0; vec < numVecs && (vec * elementsPerVec + 2) < param.fdefault.size(); ++vec) {
                    if (vec > 0) result += " ";
                    result += std::to_string(param.fdefault[vec * elementsPerVec]) + "," +
                              std::to_string(param.fdefault[vec * elementsPerVec + 1]) + "," +
                              std::to_string(param.fdefault[vec * elementsPerVec + 2]);
                }
            }
        }
    }
    // Handle scalar types
    else if (param.type == osl::TypeDesc::STRING) {
        result = param.sdefault.empty() ? "" : param.sdefault[0].c_str();
    } else if (param.type == osl::TypeDesc::INT) {
        result = param.idefault.empty() ? "0" : std::to_string(param.idefault[0]);
    } else if (param.type == osl::TypeDesc::FLOAT) {
        result = param.fdefault.empty() ? "0.0" : std::to_string(param.fdefault[0]);
    } 
    // Handle vector types
    else if (param.type.aggregate == osl::TypeDesc::VEC3 &&
               (param.type.vecsemantics == osl::TypeDesc::COLOR ||
                param.type.vecsemantics == osl::TypeDesc::POINT ||
                param.type.vecsemantics == osl::TypeDesc::VECTOR ||
                param.type.vecsemantics == osl::TypeDesc::NORMAL)) {
        if (param.fdefault.size() >= 3) {
            result = std::to_string(param.fdefault[0]) + ", " +
                     std::to_string(param.fdefault[1]) + ", " +
                     std::to_string(param.fdefault[2]);
        }
    }
    // Handle matrix types
    else if (param.type.aggregate == osl::TypeDesc::MATRIX44) {
        if (param.fdefault.size() >= 16) {
            result = "";
            for (int i = 0; i < 16; ++i) {
                if (i > 0) result += ", ";
                result += std::to_string(param.fdefault[i]);
            }
        }
    } else if (param.type.aggregate == osl::TypeDesc::MATRIX33) {
        if (param.fdefault.size() >= 9) {
            result = "";
            for (int i = 0; i < 9; ++i) {
                if (i > 0) result += ", ";
                result += std::to_string(param.fdefault[i]);
            }
        }
    }

    return result;
}

std::string parseOSLParameterType(const osl::OSLQuery::Parameter& param) {
    const osl::TypeDesc& oslType = param.type;
    MaterialXType type = MaterialXType::Unknown;

    // Handle struct 
    if (oslType == osl::TypeDesc::UNKNOWN && param.isstruct) {
        type = MaterialXType::Struct;
    }

    // Handle closures  
    if (oslType == osl::TypeDesc::PTR) {
        type = MaterialXType::BSDF;
    }

    // Handle array types
    if (oslType.arraylen > 0 || oslType.is_array()) {
        if (oslType.elementtype() == osl::TypeDesc::INT) {
            type = MaterialXType::IntegerArray;
        } else if (oslType.elementtype() == osl::TypeDesc::FLOAT) {
            type = MaterialXType::FloatArray;
        } else if (oslType.elementtype() == osl::TypeDesc::STRING) {
            type = MaterialXType::StringArray;
        } else if (oslType.elementtype().aggregate == osl::TypeDesc::VEC3) {
            if (oslType.elementtype().vecsemantics == osl::TypeDesc::COLOR) {
                type = MaterialXType::Color3Array;
            } else if (oslType.elementtype().vecsemantics == osl::TypeDesc::POINT ||
                       oslType.elementtype().vecsemantics == osl::TypeDesc::VECTOR ||
                       oslType.elementtype().vecsemantics == osl::TypeDesc::NORMAL) {
                type = MaterialXType::Vector3Array;
            }
        }
    }

    // Handle scalar types
    if (oslType == osl::TypeDesc::STRING) {
        type = MaterialXType::String;
    } else if (oslType == osl::TypeDesc::INT) {
        type = MaterialXType::Integer;
    } else if (oslType == osl::TypeDesc::FLOAT) {
        type = MaterialXType::Float;
    }
    
    // Handle vector types
    if (oslType.aggregate == osl::TypeDesc::VEC3) {
        if (oslType.vecsemantics == osl::TypeDesc::COLOR) {
            type = MaterialXType::Color3;
        } else if (oslType.vecsemantics == osl::TypeDesc::POINT ||
                   oslType.vecsemantics == osl::TypeDesc::VECTOR ||
                   oslType.vecsemantics == osl::TypeDesc::NORMAL) {
            type = MaterialXType::Vector3;
        }
    }
    
    // Handle matrix types
    if (oslType.aggregate == osl::TypeDesc::MATRIX44) {
        type = MaterialXType::Matrix44;
    } else if (oslType.aggregate == osl::TypeDesc::MATRIX33) {
        type = MaterialXType::Matrix33;
    }

    std::string structName = param.isstruct ? param.structname.c_str() : "";
    return materialXTypeToString(type, structName);
}


bool compileOSLToBytecode(
    const std::string& oslSourceCode, 
    const std::string& oslFileName, 
    const mx::FilePath& outputDir, 
    const OslCompileOptions& options,
    osl::OSLQuery& osoQuery
) {
    mx::FilePath oslFilePath = outputDir / oslFileName;
    oslFilePath.removeExtension();
    oslFilePath.addExtension("osl");

    if (options.writeSourceToDisk) {
        std::ofstream oslFile;
        oslFile.open(oslFilePath);
        oslFile << oslSourceCode;
        oslFile.close();
    }

    mx::FilePath osoFilePath = outputDir / oslFileName;
    osoFilePath.removeExtension();
    osoFilePath.addExtension("oso");

    // build up a vector of compiler arguments
    std::vector<std::string> oslCompilerArgs;
    oslCompilerArgs.emplace_back("-o");
    oslCompilerArgs.emplace_back(osoFilePath);
    for (mx::FilePath p : options.oslIncludePath) {
        oslCompilerArgs.emplace_back("-I" + p.asString() + "");
    }

    oiio::ErrorHandler errorHandler;
    osl::OSLCompiler compiler(&errorHandler);

    std::string osoBuffer;
    compiler.compile_buffer(oslSourceCode, osoBuffer, oslCompilerArgs, std::string_view(), oslFilePath.asString());
    osoQuery.open_bytecode(osoBuffer);

    std::ofstream osoFile;
    osoFile.open(osoFilePath.asString());
    osoFile << osoBuffer;
    osoFile.close();

    return true;
}

bool createMaterialXDefinitions(
    osl::OSLQuery& osoQuery,
    const std::string& oslFileName,
    mx::DocumentPtr& nodeDefMtlxDoc,
    mx::DocumentPtr& implMtlxDoc,
    mx::DocumentPtr& typeDefMtlxDoc,
    const mx::FilePath& outputDir
) {
    mx::FilePath oslFilePath = outputDir / oslFileName;
    oslFilePath.removeExtension();
    oslFilePath.addExtension("osl");

    mx::FilePath osoFilePath = outputDir / oslFileName;
    osoFilePath.removeExtension();
    osoFilePath.addExtension("oso");

    std::string shaderName = osoQuery.shadername().c_str();
    std::string nodeName = toSnakeCase(shaderName);

    mx::NodeDefPtr nodeDef = nodeDefMtlxDoc->addNodeDef(
        "NG_" + nodeName,
        "",
        nodeName
    );
    if (!nodeDef) {
        std::cerr << "Failed to create NodeDef for node: " << nodeName << std::endl;
        return false;
    }

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

        std::string paramType;

        if (param->isclosure) {
            paramType = "BSDF";
        } else {
            paramType = parseOSLParameterType(*param);
        }

        mx::ElementPtr element;
        if (param->isoutput) {
            element = nodeDef->addOutput(paramName, paramType);
        } else {
            element = nodeDef->addInput(paramName, paramType);
        }

        //Add Defaults 
        std::string defaultValue = parseOSLParameterValue(*param, &osoQuery, paramName);

        element->setAttribute("value", defaultValue);

        //Add Metadata
        for (auto metadata = param->metadata.begin(); metadata != param->metadata.end(); ++metadata) {
            std::string attrib = metadata->name.c_str();

            std::string value = parseOSLParameterValue(*metadata);
            
            element->setAttribute(attrib, value);
        }

    }

    std::string validationErrors;

    if (!implMtlxDoc->validate(&validationErrors)) {
        std::cerr << "MaterialX document validation failed after adding NodeDef: " << validationErrors << std::endl;
        return false;
    }

    try {
        std::string implName = "IM_" + nodeName;
        auto impl = implMtlxDoc->addImplementation(implName);

        if (!impl) {
            std::cerr << "Failed to create Implementation for node: " << nodeName << std::endl;
            return false;
        }

        impl->setNodeDef(nodeDef);
        impl->setFile(osoFilePath);
        impl->setFunction(shaderName);
        impl->setTarget("genosl");
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
        const std::string_view& token = tokens[i];
        const std::string_view& nextToken = i + 1 < tokens.size() ? tokens[i + 1] : mx::EMPTY_STRING;
        CommandLineArgs::ParseResult parseResult = inputArgs.parse(token, nextToken);

        if (parseResult == CommandLineArgs::EXIT) {
            return 0;
        }
        if (parseResult == CommandLineArgs::FAILURE) {
            return 1;
        }
        if (parseResult == CommandLineArgs::SUCCESS_AND_BUMP) {
            i++;
        }
    }

    if (!inputArgs.verify()) {
        return 1;
    }

    mx::FilePath outputMtlxImplementationPath(inputArgs.outputPath);
    if (!outputMtlxImplementationPath.exists() || !outputMtlxImplementationPath.isDirectory()) {
        outputMtlxImplementationPath.createDirectory();

        if (!outputMtlxImplementationPath.exists() || !outputMtlxImplementationPath.isDirectory()) {
            std::cerr << "Failed to find and/or create the provided output mtlx path:"
                      << outputMtlxImplementationPath.asString() << std::endl;
            return 1;
        }
    }

    mx::FilePath oslIncludePath(inputArgs.oslInclude);
    if (!oslIncludePath.exists() || !oslIncludePath.isDirectory())
    {
        std::cerr << "The provided path to the OSL includes is not valid: " << oslIncludePath.asString() << std::endl;
        return 1;
    }

    //get std::vector of paths to all .osl files in libraryPath
    auto files = findFiles(inputArgs.libraryPath, ".osl");
    
    mx::FileSearchPath oslRendererIncludePaths;
    oslRendererIncludePaths.append(oslIncludePath);

    OslCompileOptions options;
    options.oslIncludePath = oslRendererIncludePaths;
    options.writeSourceToDisk = false;

    mx::DocumentPtr implMtlxDoc = mx::createDocument();
    mx::DocumentPtr typeDefMtlxDoc = mx::createDocument();
    mx::DocumentPtr nodeDefMtlxDoc = mx::createDocument();

    const mx::FilePath outputDir = inputArgs.outputPath;

    for (int i = 0; i < files.size(); i++) {
        const std::filesystem::path& oslFileName = files[i].filename();

        try {
            // open and read the osl file
            std::ifstream oslFileInput(files[i]);
            if (!oslFileInput.is_open()) {
                throw std::runtime_error("Failed to open the OSL file for reading: " + files[i].string());
            }

            std::stringstream buffer;
            buffer << oslFileInput.rdbuf();
            std::string oslFileContent = buffer.str();
            oslFileInput.close();

            //compileOSL(oslFileContent, oslFileName.string(), nodeDefMtlxDoc, implMtlxDoc, typeDefMtlxDoc, outputDir, options);
            osl::OSLQuery osoQuery;
            
            compileOSLToBytecode(oslFileContent, oslFileName, outputDir, options, osoQuery);
    
            createMaterialXDefinitions(osoQuery, oslFileName, nodeDefMtlxDoc, implMtlxDoc, typeDefMtlxDoc, outputDir);
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    mx::FilePath outputNodeDefFilePath = outputDir / "autolib_defs.mtlx";
    mx::FilePath outputImplFilePath = outputDir / "autolib_genosl_impl.mtlx";

    nodeDefMtlxDoc->importLibrary(typeDefMtlxDoc); // add the typdefs to the end of the file

    mx::writeToXmlFile(nodeDefMtlxDoc, outputNodeDefFilePath);
    mx::writeToXmlFile(implMtlxDoc, outputImplFilePath);

    return 0;
}
