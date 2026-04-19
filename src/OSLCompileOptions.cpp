#include <string>
#include <vector>
#include <filesystem>
#include <cctype>
#include <fstream>
#include <sstream>
#include <iostream>

#include <OSL/oslquery.h>
#include <OSL/oslconfig.h>
#include <OSL/oslcomp.h>

#include "OSLCompileOptions.h"

namespace osl = OSL;
namespace oiio = OpenImageIO;

// Osl compile options
OslCompileOptions::ParseResult OslCompileOptions::parse(const std::string& token, const std::string& nextToken) {
    switch (hashString(token.c_str())) {
        case hashString("-h"):
        case hashString("--help"): {
            std::cout << oslcArgOptions << std::endl;
            return SUCCESS;
        }
        case hashString("-o"): {
            outputDirectory = nextToken;
            return SUCCESS_CONSUME_NEXT;
        }
        case hashString("-v"): {
            oslCompilerArgs.push_back("-v");
            return SUCCESS;
        }
        case hashString("-q"): {
            oslCompilerArgs.push_back("-q");
            return SUCCESS;
        }
        case hashString("-d"): {
            oslCompilerArgs.push_back("-d");
            return SUCCESS;
        }
        case hashString("-embed-source"): {
            oslCompilerArgs.push_back("-embed-source");
            return SUCCESS;
        }
        case hashString("-Werror"): {
            oslCompilerArgs.push_back("-Werror");
            return SUCCESS;
        }
        case hashString("-O0"): {
            oslCompilerArgs.push_back("-O0");
            return SUCCESS;
        }
        case hashString("-O1"): {
            oslCompilerArgs.push_back("-O1");
            return SUCCESS;
        }
        case hashString("-O2"): {
            oslCompilerArgs.push_back("-O2");
            return SUCCESS;
        }
        case hashString("-E"): {
            oslCompilerArgs.push_back("-E");
            return SUCCESS;
        }
        case hashString("-buffer"): {
            oslCompilerArgs.push_back("-buffer");
            return SUCCESS;
        }
        case hashString("-MD"): {
            oslCompilerArgs.push_back("-MD");
            return SUCCESS;
        }
        case hashString("-MMD"): {
            oslCompilerArgs.push_back("-MMD");
            return SUCCESS;
        }
        case hashString("-M"): {
            oslCompilerArgs.push_back("-M");
            return SUCCESS;
        }
        case hashString("-MM"): {
            oslCompilerArgs.push_back("-MM");
            return SUCCESS;
        }
        case hashString("-MF"): {
            if (nextToken.empty()) goto expectOption;
            oslCompilerArgs.push_back("-MF ");
            oslCompilerArgs.push_back(nextToken);
            return SUCCESS_CONSUME_NEXT;
        }
        case hashString("-MT"): {
            if (nextToken.empty()) goto expectOption;
            oslCompilerArgs.push_back("-MT ");
            oslCompilerArgs.push_back(nextToken);
            return SUCCESS_CONSUME_NEXT;
        }
        default: { // handle -I, -D, -U prefix options
            if (token.substr(0, 2) == "-I") {
                std::string path = token.substr(2);
                if (path.empty() && !nextToken.empty()) {
                    path = nextToken;
                    oslCompilerArgs.push_back("-I");
                    oslCompilerArgs.push_back(path);
                    return SUCCESS_CONSUME_NEXT;
                } else if (!path.empty()) {
                    oslCompilerArgs.push_back("-I");
                    oslCompilerArgs.push_back(path);
                    return SUCCESS;
                }
                goto expectOption;
            } else if (token.substr(0, 2) == "-D") {
                oslCompilerArgs.push_back(token);
                return SUCCESS;
            } else if (token.substr(0, 2) == "-U") {
                oslCompilerArgs.push_back(token);
                return SUCCESS;
            }
            
            std::cout << "Unrecognized command-line option: " << token << std::endl;
            return FAILURE;
        }
    }

    expectOption: {
        std::cerr << "Expected another token following command-line option: " << token << std::endl;
        return FAILURE;
    }
}

bool OslCompileOptions::verify() const {
    return true;
}

bool compileAndWriteOSL(
    const fs::path& oslFilePath, 
    const OslCompileOptions& options
) {

    std::ifstream oslFileInput(oslFilePath);
    std::stringstream buffer;
    buffer << oslFileInput.rdbuf();
    std::string oslFileContent = buffer.str();
    oslFileInput.close();

    oiio::ErrorHandler errorHandler;
    osl::OSLCompiler compiler(&errorHandler);

    std::string osoBuffer;
    compiler.compile_buffer(oslFileContent, osoBuffer, options.oslCompilerArgs, std::string_view(), oslFilePath.string());

    osl::OSLQuery osoQuery;
    osoQuery.open_bytecode(osoBuffer);
    
    std::string shadername = osoQuery.shadername().c_str();
    fs::path namedOsoFilePath = (options.outputDirectory / shadername);
    namedOsoFilePath.replace_extension("oso");

    std::ofstream osoFile;
    osoFile.open(namedOsoFilePath);
    osoFile << osoBuffer;
    osoFile.close();

    return true;
}