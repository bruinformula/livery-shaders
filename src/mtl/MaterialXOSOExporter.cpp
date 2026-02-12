#include <iostream>
#include <ostream>
#include <string>
#include <vector>
#include <unordered_set>

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

#include "OSLCompiler.h"

namespace mx = MaterialX;

const std::string argOptions =
    " MaterialXOSOExporter -- Compiles a bunch of MaterialX documents into OSL btyecode \n"
    " Options: \n"
    "    --mtlxMaterialsPath        Path to the directory containing OSL shader files.  All .osl files in this directory and its subdirectories will be processed. \n"
    "    --osoOutputPath            Path to the output directory where generated MaterialX documents and OSL shaders will be written. \n"
    "    --oslIncludePath           OSL Include Path\n"
    "    --oslDefine [NAME=VALUE]   Define a preprocessor macro to be used during OSL compilation.  Can be specified multiple times to define multiple macros.\n"
    "    --writeOSLSource           Write OSL source files to disk\n"
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
    mx::FilePath mtlxMaterialsPath;
    mx::FilePath osoOutputPath;
    mx::FilePath oslIncludePath;
    std::vector<std::string> oslDefine;

    mx::FileSearchPath searchPath;
    std::vector<mx::FilePath> libraryFolders;

    bool writeOSLSource = false;

    ParseResult parse(const std::string& token, const std::string& nextToken) {
        // yes managed to get goto in here
        if (token == "--mtlxMaterialsPath") {
            if (nextToken.empty()) goto expectOption;
            if (!mtlxMaterialsPath.isEmpty()) goto alreadySet;
            
            mtlxMaterialsPath = nextToken;
            return SUCCESS_AND_BUMP;
        } else if (token == "--osoOutputPath") {
            if (nextToken.empty()) goto expectOption;
            if (!osoOutputPath.isEmpty()) goto alreadySet;

            osoOutputPath = nextToken;
            return SUCCESS_AND_BUMP;
        } else if (token == "--path") {
            if (nextToken.empty()) goto expectOption;
            searchPath.append(mx::FileSearchPath(nextToken));
            return SUCCESS_AND_BUMP;
        } else if (token == "--oslIncludePath") {
            if (nextToken.empty()) goto expectOption;
            if (!oslIncludePath.isEmpty()) goto alreadySet;

            oslIncludePath = nextToken;
            return SUCCESS_AND_BUMP;
        } else if (token == "--library") {
            if (nextToken.empty()) goto expectOption;
            
            libraryFolders.push_back(nextToken);
            return SUCCESS_AND_BUMP;
        } else if (token == "--oslDefine") {
            if (nextToken.empty()) goto expectOption;
            oslDefine.push_back(nextToken);
            return SUCCESS_AND_BUMP;
        } else if (token == "--writeOSLSource") {
            writeOSLSource = true;
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
        if (mtlxMaterialsPath.isEmpty()) {
            std::cerr << "mtlxMaterialsPath is not set!" << std::endl;
            return false;
        }
        if (osoOutputPath.isEmpty()) {
            std::cerr << "osoOutputPath is not set!" << std::endl;
            return false;
        }

        if (!osoOutputPath.exists() || !osoOutputPath.isDirectory()) {
            osoOutputPath.createDirectory();

            if (!osoOutputPath.exists() || !osoOutputPath.isDirectory()) {
                std::cerr << "Failed to find and/or create the provided output oso path:"
                        << osoOutputPath.asString() << std::endl;
                return 1;
            }
        }

        return true;
    }

};

void edgePrintout(
    mx::Edge edge
) {
    std::cout << "    - Upstream Element to: " << edge.getDownstreamElement()->getName() << std::endl;
    std::cout << "    - Connecting Element: " << edge.getConnectingElement()->getName() << std::endl;
    std::cout << "    - Upstream Port: " << edge.getUpstreamElement()->getName() << std::endl;

    for (size_t j = 0; j < edge.getUpstreamElement()->getUpstreamEdgeCount(); j++) {
        mx::Edge upstreamEdge = edge.getUpstreamElement()->getUpstreamEdge(j);
        edgePrintout(upstreamEdge);
    }
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
    
    mx::FileSearchPath oslRendererIncludePaths;
    oslRendererIncludePaths.append(inputArgs.oslIncludePath);
    oslRendererIncludePaths.append(inputArgs.searchPath.find("libraries/stdlib/genosl/include"));

    mx::ShaderGeneratorPtr oslShaderGen = mx::OslShaderGenerator::create();

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
            std::cout << "File " << file.asString() << " contains:" << std::endl;
            std::cout << "  Materials: " << doc->getMaterialNodes().size() << std::endl;
            std::cout << "  Nodes: " << doc->getNodes().size() << std::endl;
            doc->importLibrary(librariesDoc);
            materialDocuments.push_back(doc);
            std::cout << "Loaded: " << file.asString() << std::endl;
        } catch (std::exception& e) {
            std::cerr << "Failed to load MaterialX file " << file.asString() << ": " << e.what() << std::endl;
            return 1;
        }
    }
    
    oslShaderGen->registerTypeDefs(librariesDoc);

    // Setup the context of the OSL shader generator.
    mx::GenContext context(oslShaderGen);
    context.registerSourceCodeSearchPath(inputArgs.searchPath);
    context.getOptions().addUpstreamDependencies = true;
    context.getOptions().fileTextureVerticalFlip = false;
    context.getOptions().oslImplicitSurfaceShaderConversion = false;

    OslCompileOptions options;
    options.oslIncludePath = oslRendererIncludePaths;
    options.writeSourceToDisk = inputArgs.writeOSLSource;
    options.writeByteCodeToDisk = true;
    options.definePreprocessors = inputArgs.oslDefine;
    options.embedSource = false;
    options.optimizationLevel = OslCompileOptions::Performance;

    std::unordered_set<std::string> materialNames;

    for (const mx::DocumentPtr& doc : materialDocuments) {
        std::cout << "Document has " << doc->getMaterialNodes().size() << " material nodes." << std::endl;

        for (mx::NodePtr materialNode : doc->getMaterialNodes()) {
            const std::string oslFileName = materialNode->getName();

            std::cout << "Generating OSL shader for material: " << oslFileName << std::endl;

            if (materialNames.contains(oslFileName)) {
                std::cerr << "Duplicate material name found: " << oslFileName << ". Material names must be unique!" << std::endl;
                return 1;
            }

            materialNames.insert(oslFileName);

            try {
                mx::ShaderPtr shader = oslShaderGen->generate(
                    "complete_material", 
                    materialNode, 
                    context
                );
                
                compileOSLToBytecode(
                    shader->getSourceCode(), 
                    oslFileName, 
                    inputArgs.osoOutputPath, 
                    options
                );
                            
            } catch (mx::Exception& e) {
                std::cerr << "Error: " << e.what() << std::endl;
                return 1;
            }
        }
    }
    return 0;
}
