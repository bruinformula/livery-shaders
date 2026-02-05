#include <iostream>
#include <string>
#include <filesystem>

#include <OSL/oslquery.h>
#include <OSL/oslcomp.h>

#include <MaterialXCore/Document.h>
#include <MaterialXCore/Util.h>

#include <MaterialXFormat/File.h>
#include <MaterialXFormat/Util.h>

#include <MaterialXGenShader/GenContext.h>
#include <MaterialXGenShader/Shader.h>

#include <MaterialXGenOsl/OslShaderGenerator.h>
#include <MaterialXGenOsl/OslSyntax.h>

namespace osl = OSL;
namespace mx = MaterialX;
namespace oiio = OIIO;

struct InputArgs {
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
        if (token == "--libraryPath") {
            if (nextToken.empty()) {
                std::cerr << "Expected another token following command-line option: " << token << std::endl; 
                return FAILURE;
            }
            if (libraryPath.empty()) {
                libraryPath = nextToken;
                return SUCCESS_AND_BUMP;
            } else {
                std::cerr << "libraryPath is already set!" << std::endl;
                return FAILURE;
            }
        } else if (token == "--outputPath") {
            if (nextToken.empty()) {
                std::cerr << "Expected another token following command-line option: " << token << std::endl; 
                return FAILURE;
            }
            if (outputPath.empty()) {
                outputPath = nextToken;
                return SUCCESS_AND_BUMP;
            } else {
                std::cerr << "outputPath is already set!" << std::endl;
                return FAILURE;
            }
        } else if (token == "--oslInclude") {
            if (nextToken.empty()) {
                std::cerr << "Expected another token following command-line option: " << token << std::endl; 
                return FAILURE;
            }
            if (oslInclude.empty()) {
                oslInclude = nextToken;
                return SUCCESS_AND_BUMP;
            } else {
                std::cerr << "oslInclude is already set!" << std::endl;
                return FAILURE;
            }
        } else if (token == "--help") {
            std::cout << "Usage: ./main --libraryPath <path> --outputPath <path> --oslInclude <path>" << std::endl;
            return EXIT; // Indicate that no further processing is needed
        } else {
            std::cout << "Unrecognized command-line option: " << token << std::endl;
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

bool compileOSL(const std::string& oslSourceCode, const std::string& oslFileName, const mx::FilePath& outputDir, const OslCompileOptions& options) {
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

    if (options.writeSourceToDisk) {
        // Compile from the source file
        compiler.compile(oslFilePath.asString(), oslCompilerArgs);
    } else {
        // Compile directly from the string buffer
        std::string osoBuffer;
        compiler.compile_buffer(oslSourceCode, osoBuffer, oslCompilerArgs, std::string_view(), oslFilePath.asString());

        std::ofstream osoFile;
        osoFile.open(osoFilePath.asString());
        osoFile << osoBuffer;
        osoFile.close();
    }

    std::cout << "Compiled " << oslFilePath.getBaseName() << " -> " << osoFilePath.getBaseName() << std::endl;

    return true;
}

int main(int argc, char* const argv[]) {
    std::vector<std::string> tokens;

    for (int i = 1; i < argc; i++) {
        tokens.emplace_back(argv[i]);
    }

    InputArgs inputArgs;
    for (size_t i = 0; i < tokens.size(); i++) {
        const std::string& token = tokens[i];
        const std::string& nextToken = i + 1 < tokens.size() ? tokens[i + 1] : mx::EMPTY_STRING;
        InputArgs::ParseResult parseResult = inputArgs.parse(token, nextToken);

        if (parseResult == InputArgs::EXIT) {
            return 0;
        }
        if (parseResult == InputArgs::FAILURE) {
            return 1;
        }
        if (parseResult == InputArgs::SUCCESS_AND_BUMP) {
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

    for (int i = 0; i < files.size(); i++) {
        const std::filesystem::path& oslFileName = files[i].filename();
        const mx::FilePath outputDir = inputArgs.outputPath;

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

            //std::cout << "OSL File Content:\n" << oslFileContent << std::endl;

            compileOSL(oslFileContent, oslFileName.string(), outputDir, options);

        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    return 0;
}
