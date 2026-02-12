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

#include <MaterialXGenGlsl/GlslShaderGenerator.h>
#include <MaterialXGenGlsl/GlslSyntax.h>

#include "OSLCompiler.h"

namespace mx = MaterialX;
namespace osl = OSL;
namespace oiio = OpenImageIO;

std::vector<mx::FilePath> findFiles(const mx::FilePath& rootDir, const std::string& extension, bool maintainRelativePath) {
    std::vector<mx::FilePath> results;
    
    std::filesystem::path fsRoot(rootDir.asString());
    if (!std::filesystem::exists(fsRoot))
        return results;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(fsRoot)) {
        if (entry.is_regular_file() && entry.path().extension() == extension) {
            if (maintainRelativePath) {
                std::filesystem::path rel = std::filesystem::relative(entry.path(), fsRoot);
                results.emplace_back(mx::FilePath(rel.string()));
            } else {
                results.emplace_back(mx::FilePath(entry.path().string()));
            }
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

std::string unescapeString(const std::string& input) {
    std::string result;
    result.reserve(input.size());
    
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '\\' && i + 1 < input.size()) {
            switch (input[i + 1]) {
                case 'n': result += '\n'; i++; break;
                case 't': result += '\t'; i++; break;
                case 'r': result += '\r'; i++; break;
                case '\\': result += '\\'; i++; break;
                case '"': result += '"'; i++; break;
                case '\'': result += '\''; i++; break;
                default: result += input[i]; break;
            }
        } else {
            result += input[i];
        }
    }
    
    return result;
}

std::vector<std::string> OslCompileOptions::getArgs(const mx::FilePath& osoFilePath) const {
    // build up a vector of compiler arguments
    std::vector<std::string> oslCompilerArgs;

    oslCompilerArgs.emplace_back("-o");
    oslCompilerArgs.emplace_back(osoFilePath);

    if (verboseMode) oslCompilerArgs.emplace_back("-v");
    if (quietMode) oslCompilerArgs.emplace_back("-q");
    if (debugMode) oslCompilerArgs.emplace_back("-d");
    if (embedSource) oslCompilerArgs.emplace_back("-embed-source");
    if (warningsAsErrors) oslCompilerArgs.emplace_back("-Werror");

    switch (optimizationLevel) {
        case Optimization::None:
            oslCompilerArgs.emplace_back("-O0");
            break;
        case Optimization::Size:
            oslCompilerArgs.emplace_back("-O1");
            break;
        case Optimization::Performance:
            oslCompilerArgs.emplace_back("-O2");
            break;
    }

    for (const std::string& def : definePreprocessors) {
        oslCompilerArgs.emplace_back("-D" + def);
    }

    for (const std::string& undef : undefinePreprocessors) {
        oslCompilerArgs.emplace_back("-U" + undef);
    }

    for (mx::FilePath p : oslIncludePath) {
        oslCompilerArgs.emplace_back("-I" + p.asString() + "");
    }
    
    return oslCompilerArgs;
}

// Utility Functions
std::string materialXTypeToString(
    MaterialXType type, 
    const std::string& structName
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
    const osl::OSLQuery* oslQuery, 
    const std::string& paramName
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
                result = unescapeString(param.sdefault[0].c_str());
                for (size_t i = 1; i < std::min((size_t)param.type.arraylen, param.sdefault.size()); ++i) {
                    result += ", " + unescapeString(param.sdefault[i].c_str());
                }
            }
        } else if (param.type.elementtype().aggregate == osl::TypeDesc::VEC3) {
            // Vector3/Color3/Normal3/Point3
            size_t elementsPerVec = 3;
            size_t numVecs = param.type.arraylen > 0 ? param.type.arraylen : (param.fdefault.size() / elementsPerVec);
            if (param.fdefault.size() >= elementsPerVec) {
                for (size_t vec = 0; vec < numVecs && (vec * elementsPerVec + 2) < param.fdefault.size(); ++vec) {
                    if (vec > 0) result += ", ";
                    result += std::to_string(param.fdefault[vec * elementsPerVec]) + "," +
                              std::to_string(param.fdefault[vec * elementsPerVec + 1]) + "," +
                              std::to_string(param.fdefault[vec * elementsPerVec + 2]);
                }
            }
        }
    }
    // Handle scalar types
    else if (param.type == osl::TypeDesc::STRING) {
        result = param.sdefault.empty() ? "" : unescapeString(param.sdefault[0].c_str());
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
    osl::OSLQuery* osoQuery
) {
    // we compile the shader to get attributes and metadata
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

    std::vector<std::string> oslCompilerArgs = options.getArgs(osoFilePath);

    for (mx::FilePath p : options.oslIncludePath) {        
        if (options.writeSourceToDisk) {

            auto headerFiles = findFiles(p, ".h", true);

            for (auto header = headerFiles.begin(); header != headerFiles.end(); ++header) {
                mx::FilePath fullHeaderPath = p / *header;
                std::ifstream headerInput(fullHeaderPath.asString());
                if (!headerInput.is_open()) {
                    std::cerr << "Failed to open OSL include header for reading: " << header->asString() << std::endl;
                    continue;
                }

                std::stringstream buffer;
                buffer << headerInput.rdbuf();
                std::string headerSourceCode = buffer.str();
                headerInput.close();

                mx::FilePath headerOutputPath = outputDir / *header;
                
                // header directories are created if they don't exist
                mx::FilePath parentPath = headerOutputPath.getParentPath();
                if (!parentPath.isEmpty() && !parentPath.exists()) {
                    parentPath.createDirectory();
                }
                
                std::ofstream headerFile;
                headerFile.open(headerOutputPath.asString());
                headerFile << headerSourceCode;
                headerFile.close();
            }
        }
    }

    for (const std::string& def : options.definePreprocessors) {
        oslCompilerArgs.emplace_back("-D" + def);
    }

    oiio::ErrorHandler errorHandler;
    osl::OSLCompiler compiler(&errorHandler);

    std::string osoBuffer;
    compiler.compile_buffer(oslSourceCode, osoBuffer, oslCompilerArgs, std::string_view(), oslFilePath.asString());

    if (osoQuery != nullptr) {
        osoQuery->open_bytecode(osoBuffer);
    }

    if (options.writeByteCodeToDisk) {
        std::ofstream osoFile;
        osoFile.open(osoFilePath.asString());
        osoFile << osoBuffer;
        osoFile.close();
    }

    return true;
}