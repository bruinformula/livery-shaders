#include <iostream>
#include <fstream>
#include <ostream>
#include <string>
#include <vector>
#include <unordered_set>

#include <MaterialXCore/Document.h>
#include <MaterialXCore/Util.h>
#include <MaterialXCore/Interface.h>

#include <MaterialXFormat/File.h>
#include <MaterialXFormat/Util.h>


#include "OSLCompiler.h"

namespace mx = MaterialX;

const std::string argOptions =
    " MaterialXNodeQuery -- Retrives all instances and values of a particular type \n"
    " Options: \n"
    "    --node                     The name of the nodedef to be queried.  All nodes in the provided MaterialX files matching this nodedef type will be included in the output. \n"
    "    --mtlxMaterialsPath        Path to the directory containing OSL shader files.  All .osl files in this directory and its subdirectories will be processed. \n"
    "    --csvOutputPath            Path to the output directory where CSV files will be written. \n"
    "    --path                     Specify an additional data search path location (e.g. '/projects/MaterialX').  This absolute path will be queried when locating data libraries, XInclude references, and referenced images.\n"
    "    --library                  Specify an additional data library folder (e.g. 'vendorlib', 'studiolib').  This relative path will be appended to each location in the data search path when loading data libraries.\n"
    "    --help                     Prints this message\n";

struct NodeQueryCommandLineArgs : public CommandLineArgs {

    mx::FilePath mtlxMaterialsPath;
    mx::FilePath csvOutputPath;

    std::string interestingNode;

    mx::FileSearchPath searchPath;
    std::vector<mx::FilePath> libraryFolders;

    ParseResult parse(const std::string& token, const std::string& nextToken) override {
        // yes managed to get goto in here
        if (token == "--mtlxMaterialsPath") {
            if (nextToken.empty()) goto expectOption;
            if (!mtlxMaterialsPath.isEmpty()) goto alreadySet;
            
            mtlxMaterialsPath = nextToken;
            return SUCCESS_CONSUME_NEXT;
        } else if (token == "--csvOutputPath") {
            if (nextToken.empty()) goto expectOption;
            if (!csvOutputPath.isEmpty()) goto alreadySet;

            csvOutputPath = nextToken;
            return SUCCESS_CONSUME_NEXT;
        } else if (token == "--node") {
            if (nextToken.empty()) goto expectOption;
            if (!interestingNode.empty()) goto alreadySet;

            interestingNode = nextToken;
            return SUCCESS_CONSUME_NEXT;
        } else if (token == "--path") {
            if (nextToken.empty()) goto expectOption;
            searchPath.append(mx::FileSearchPath(nextToken));
            return SUCCESS_CONSUME_NEXT;
        } else if (token == "--library") {
            if (nextToken.empty()) goto expectOption;
            
            libraryFolders.push_back(nextToken);
            return SUCCESS_CONSUME_NEXT;
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
        if (mtlxMaterialsPath.isEmpty()) {
            std::cerr << "mtlxMaterialsPath is not set!" << std::endl;
            return false;
        }
        if (csvOutputPath.isEmpty()) {
            std::cerr << "csvOutputPath is not set!" << std::endl;
            return false;
        }
        if (interestingNode.empty()) {
            std::cerr << "node is not set!" << std::endl;
            return false;
        }

        if (!csvOutputPath.exists() || !csvOutputPath.isDirectory()) {
            csvOutputPath.createDirectory();

            if (!csvOutputPath.exists() || !csvOutputPath.isDirectory()) {
                std::cerr << "Failed to find and/or create the provided output csv path:"
                        << csvOutputPath.asString() << std::endl;
                return 1;
            }
        }

        return true;
    }

};

int main(int argc, char* const argv[]) {
    std::vector<std::string> tokens;

    for (int i = 1; i < argc; i++) {
        tokens.emplace_back(argv[i]);
    }

    NodeQueryCommandLineArgs inputArgs;
    for (size_t i = 0; i < tokens.size(); i++) {
        const std::string& token = tokens[i];
        const std::string& nextToken = i + 1 < tokens.size() ? tokens[i + 1] : mx::EMPTY_STRING;
        CommandLineArgs::ParseResult parseResult = inputArgs.parse(token, nextToken);

        switch (parseResult) {
            case CommandLineArgs::SUCCESS:
                break;
            case CommandLineArgs::SUCCESS_CONSUME_NEXT:
                i++;
                break;
            case CommandLineArgs::FAILURE:
                return 1;
            case CommandLineArgs::EXIT:
                return 0;
        }
    }

    if (!inputArgs.verify()) {
        return 1;
    }

    // Append the standard library folder, giving it a lower precedence than user-supplied libraries.
    inputArgs.searchPath = mx::getDefaultDataSearchPath();
    
    const char* mtlxEnvPath = std::getenv("MATERIALX_SEARCH_PATH");

    if (mtlxEnvPath) {
        //std::cout << "DEBUG: MATERIALX_SEARCH_PATH=" << mtlxEnvPath << std::endl;
        inputArgs.searchPath.append(mx::FileSearchPath(mtlxEnvPath));
    }
    
    inputArgs.libraryFolders.push_back("libraries");

    //get std::vector of paths to all .mtlx files in mtlxMaterialsPath
    std::vector<mx::FilePath> files = findFiles(inputArgs.mtlxMaterialsPath, ".mtlx");
    
    std::cout << "Found " << files.size() << " MaterialX files in " << inputArgs.mtlxMaterialsPath.asString() << std::endl;
    for (const auto& file : files) {
        std::cout << "  - " << file.asString() << std::endl;
    }

    mx::DocumentPtr librariesDoc = mx::createDocument();

    try {
        for (auto folder : inputArgs.libraryFolders) {
            std::cout << "Library folder: " << folder.asString() << std::endl;
        }
        mx::loadLibraries(inputArgs.libraryFolders, inputArgs.searchPath, librariesDoc);
        std::cout << "Loaded standard libraries, found " << librariesDoc->getNodeDefs().size() << " NodeDefs" << std::endl;
    } catch (std::exception& e) {
        std::cerr << "Failed to load standard data libraries: " << e.what() << std::endl;
        return 1;
    }
    
    std::vector<mx::DocumentPtr> materialDocuments;
    
    for (const mx::FilePath& file : files) {
        try {
            mx::DocumentPtr doc = mx::createDocument();
            mx::readFromXmlFile(doc, file, inputArgs.searchPath);
            //std::cout << "File " << file.asString() << " contains:" << std::endl;
            //std::cout << "  Materials: " << doc->getMaterialNodes().size() << std::endl;
            //std::cout << "  Nodes: " << doc->getNodes().size() << std::endl;
            doc->importLibrary(librariesDoc);
            materialDocuments.push_back(doc);
            //std::cout << "Loaded: " << file.asString() << std::endl;
        } catch (std::exception& e) {
            std::cerr << "Failed to load MaterialX file " << file.asString() << ": " << e.what() << std::endl;
            return 1;
        }
    }

    std::unordered_set<std::string> materialNames;

    // First pass: find the nodedef and collect input names
    const std::string& interestingNode = inputArgs.interestingNode;
    mx::NodeDefPtr nodeDef = nullptr;
    
    for (const mx::DocumentPtr& doc : materialDocuments) {
        for (mx::NodeDefPtr nd : doc->getNodeDefs()) {
            if (nd->getNodeString() == interestingNode) {
                nodeDef = nd;
                break;
            }
        }
        if (nodeDef) break;
    }

    if (!nodeDef) {
        std::cerr << "No NodeDef found for: " << interestingNode << std::endl;
        return 1;
    }

    // collect input names for CSV header
    std::vector<std::string> inputNames;
    for (mx::InputPtr defInput : nodeDef->getInputs()) {
        inputNames.push_back(defInput->getName());
    }

    // open CSV file for writing
    mx::FilePath csvFilePath = inputArgs.csvOutputPath / (interestingNode + "_node_query.csv");
    std::ofstream csvFile(csvFilePath.asString());
    
    if (!csvFile.is_open()) {
        std::cerr << "Failed to open CSV file for writing: " << csvFilePath.asString() << std::endl;
        return 1;
    }

    // CSV header
    csvFile << "Node Name,Source File";
    for (const std::string& inputName : inputNames) {
        csvFile << "," << inputName;
    }
    csvFile << std::endl;

    int nodeCount = 0;
    for (size_t docIdx = 0; docIdx < materialDocuments.size(); docIdx++) {
        const mx::DocumentPtr& doc = materialDocuments[docIdx];
        const mx::FilePath& sourceFile = files[docIdx];

        for (const mx::NodePtr& node : doc->getNodes()) {
            if (node->getCategory() == interestingNode) {
                nodeCount++;
                std::cout << "Found node of interest: " << node->getName() << " in " << sourceFile.getBaseName() << std::endl;

                //csvFile << node->getName() << "," << sourceFile.asString(); // complete path
                csvFile << node->getName() << "," << sourceFile.getBaseName(); // write node name and source file as first 2 columns

                // get all inputs
                for (mx::InputPtr defInput : nodeDef->getInputs()) {
                    const std::string& name = defInput->getName();
                    mx::InputPtr instanceInput = node->getInput(name);

                    std::string value;
                    bool isDefault = false;

                    if (instanceInput && instanceInput->hasValue()) {
                        value = instanceInput->getValueString();
                    } else if (defInput->hasValue()) {
                        value = defInput->getValueString();
                        isDefault = true;
                    } else {
                        value = "";
                    }

                    // escape value for CSV
                    if (value.find(',') != std::string::npos || value.find('"') != std::string::npos) {
                        // replace " with ""
                        size_t pos = 0;
                        while ((pos = value.find('"', pos)) != std::string::npos) {
                            value.replace(pos, 1, "\"\"");
                            pos += 2;
                        }
                        value = "\"" + value + "\"";
                    }

                    csvFile << "," << value;

                    std::cout << "  Input: " << name << " = ";
                    if (instanceInput && instanceInput->hasValue()) {
                        std::cout << instanceInput->getValueString();
                    } else if (defInput->hasValue()) {
                        std::cout << defInput->getValueString() << " (default)";
                    } else {
                        std::cout << "(no value)";
                    }
                    std::cout << std::endl;
                }

                csvFile << std::endl;
            }
        }
    }

    csvFile.close();
    std::cout << "\nWrote " << nodeCount << " node instances to " << csvFilePath.asString() << std::endl;
    
    return 0;
}
