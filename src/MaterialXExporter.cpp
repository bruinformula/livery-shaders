#include <MaterialXCore/Element.h>
#include <iostream>
#include <ostream>
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

#include "OSLCompiler.h"

namespace mx = MaterialX;

struct CommandLineArgs {
    enum ParseResult {
        SUCCESS,
        SUCCESS_AND_BUMP,
        FAILURE,
        EXIT
    };
    mx::FilePath oslIncludePath;
    mx::FilePath libraryPath;
    mx::FilePath outputPath;

    mx::FileSearchPath searchPath;
    std::vector<mx::FilePath> libraryFolders;

    ParseResult parse(const std::string& token, const std::string& nextToken) {
        // yes managed to get goto in here
        if (token == "--libraryPath") {
            if (nextToken.empty()) goto expectOption;
            if (!libraryPath.isEmpty()) goto alreadySet;
            
            libraryPath = nextToken;
            return SUCCESS_AND_BUMP;
        } else if (token == "--outputPath") {
            if (nextToken.empty()) goto expectOption;
            if (!outputPath.isEmpty()) goto alreadySet;

            outputPath = nextToken;
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
        } else if (token == "--help") {
            std::cout << "Usage: ./main --libraryPath <path> --outputPath <path> " << std::endl;
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
        if (libraryPath.isEmpty()) {
            std::cerr << "libraryPath is not set!" << std::endl;
            return false;
        }
        if (outputPath.isEmpty()) {
            std::cerr << "outputPath is not set!" << std::endl;
            return false;
        }

        if (!outputPath.exists() || !outputPath.isDirectory()) {
            outputPath.createDirectory();

            if (!outputPath.exists() || !outputPath.isDirectory()) {
                std::cerr << "Failed to find and/or create the provided output oso path:"
                        << outputPath.asString() << std::endl;
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

    //get std::vector of paths to all .mtlx files in libraryPath
    std::vector<mx::FilePath> files = findFiles(inputArgs.libraryPath, ".mtlx");
    
    std::cout << "Found " << files.size() << " MaterialX files in " << inputArgs.libraryPath.asString() << std::endl;
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
            std::cout << "  After importing libraries - NodeDefs: " << doc->getNodeDefs().size() << std::endl;
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
    options.writeSourceToDisk = true;
    options.writeByteCodeToDisk = true;

    std::cout << "Processing " << materialDocuments.size() << " material documents." << std::endl;
    
    for (const mx::DocumentPtr& doc : materialDocuments) {
        std::cout << "Document has " << doc->getMaterialNodes().size() << " material nodes." << std::endl;
        
        for (mx::NodePtr materialNode : doc->getMaterialNodes()) {
            const std::string oslFileName = materialNode->getName();

            std::cout << "Generating OSL shader for material: " << oslFileName << std::endl;

            try {
                mx::ShaderPtr shader = oslShaderGen->generate(
                    "complete_material", 
                    materialNode, 
                    context
                );
                
                compileOSLToBytecode(
                    shader->getSourceCode(), 
                    oslFileName, 
                    inputArgs.outputPath, 
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
