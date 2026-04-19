#include <string>
#include <vector>
#include <filesystem>
#include <exception>
#include <string_view>
#include <iostream>

#include "OSLCompileOptions.h"
#include "ArgumentHandler.h"

namespace fs = std::filesystem;

struct LibraryArgumentHandler : public ArgumentHandler {

    OslCompileOptions oslCompileOptions;
    std::vector<fs::path> inputDirs;
    
    ParseResult parse(const std::string& token, const std::string& nextToken) override {
        switch (hashString(token.c_str())) {
            case (hashString("-h")):
            case (hashString("--help")): {
                std::cout << oslcArgOptions << std::endl;
                return EXIT;
            }
            case (hashString("-i")): {
                if (nextToken.empty()) goto expectOption;
                inputDirs.push_back(nextToken);
                return SUCCESS_CONSUME_NEXT;
            }
            default: {
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
    
    bool verify() const override {
        return true;
    }
};

std::vector<fs::path> findFiles(const fs::path& rootDir, const std::string& extension, bool maintainRelativePath = false) {
    std::vector<fs::path> results;
    
    if (!std::filesystem::exists(rootDir))
        return results;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(rootDir)) {
        if (entry.is_regular_file() && entry.path().extension() == extension) {
            if (maintainRelativePath) {
                std::filesystem::path rel = std::filesystem::relative(entry.path(), rootDir);
                results.emplace_back(fs::path(rel.string()));
            } else {
                results.emplace_back(fs::path(entry.path().string()));
            }
        }
    }
    
    return results;
}

int main(int argc, char* const argv[]) {
    std::vector<std::string> tokens;

    for (int i = 1; i < argc; i++) {
        tokens.emplace_back(argv[i]);
    }

    LibraryArgumentHandler inputArgs;

    for (size_t i = 0; i < tokens.size(); i++) {
        const std::string& token = tokens[i];
        const std::string& nextToken = i + 1 < tokens.size() ? tokens[i + 1] : "";
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

    const OslCompileOptions& options = inputArgs.oslCompileOptions;

    //get std::vector of paths to all .osl files in oslLibraryPath
    std::vector<fs::path> files;
    for (const fs::path& dir : inputArgs.inputDirs) {
        auto foundFiles = findFiles(dir, ".osl");
        files.insert(files.end(), foundFiles.begin(), foundFiles.end());
    }
            
    for (const fs::path& file : files) {       
        try {
            compileAndWriteOSL(
                file, 
                options
            );
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    return 0;
}