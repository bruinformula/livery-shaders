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
    "    --path                     Specify an additional data search path location (e.g. '/projects/MaterialX').  This absolute path will be queried when locating data libraries, XInclude references, and referenced images.\n"
    "    --library                  Specify an additional data library folder (e.g. 'vendorlib', 'studiolib').  This relative path will be appended to each location in the data search path when loading data libraries.\n"
    "    --help                     Prints this message\n";

struct OsoExporterArgumentHandler : public ArgumentHandler {
    mx::FilePath mtlxMaterialsPath;
    mx::FilePath osoOutputPath;
    mx::FileSearchPath searchPath;
    std::vector<mx::FilePath> libraryFolders;
    
    OslCompileOptions oslCompileOptions;
    
    ParseResult parse(const std::string& token, const std::string& nextToken) override {
        switch (hashString(token.c_str())) {
            case hashString("--mtlxMaterialsPath"): {
                if (nextToken.empty()) goto expectOption;
                if (!mtlxMaterialsPath.isEmpty()) goto alreadySet;
                
                mtlxMaterialsPath = nextToken;
                return SUCCESS_CONSUME_NEXT;
            }
            case hashString("--osoOutputPath"): {
                if (nextToken.empty()) goto expectOption;
                if (!osoOutputPath.isEmpty()) goto alreadySet;
                osoOutputPath = nextToken;
                return SUCCESS_CONSUME_NEXT;
            }
            case hashString("--path"): {
                if (nextToken.empty()) goto expectOption;
                searchPath.append(mx::FileSearchPath(nextToken));
                return SUCCESS_CONSUME_NEXT;
            }
            case hashString("--library"): {
                if (nextToken.empty()) goto expectOption;
                libraryFolders.push_back(nextToken);
                return SUCCESS_CONSUME_NEXT;
            }
            case hashString("--help"): {
                std::cout << argOptions << std::endl;
                std::cout << "\n" << oslcArgOptions << std::endl;
                return EXIT;
            }
            default: {
                // Try parsing as OSL compile option
                ParseResult oslResult = oslCompileOptions.parse(token, nextToken);
                if (oslResult == SUCCESS || oslResult == SUCCESS_CONSUME_NEXT) {
                    return oslResult;
                } else if (oslResult == FAILURE) {
                    // OSL compiler already printed error message
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
        if (mtlxMaterialsPath.isEmpty()) {
            std::cerr << "mtlxMaterialsPath is not set!" << std::endl;
            return false;
        }
        if (osoOutputPath.isEmpty()) {
            std::cerr << "osoOutputPath is not set!" << std::endl;
            return false;
        }
        
        // output Path
        if (!osoOutputPath.exists() || !osoOutputPath.isDirectory()) {
            osoOutputPath.createDirectory();
            if (!osoOutputPath.exists() || !osoOutputPath.isDirectory()) {
                std::cerr << "Failed to find and/or create the provided output oso path: "
                          << osoOutputPath.asString() << std::endl;
                return false;
            }
        }
        
        // validate materials path 
        if (!mtlxMaterialsPath.exists()) {
            std::cerr << "The provided MaterialX materials path does not exist: " << mtlxMaterialsPath.asString() << std::endl;
            return false;
        }
        
        // validate search paths 
        for (const auto& path : searchPath) {
            if (!path.exists()) {
                std::cerr << "Search path does not exist: " << path.asString() << std::endl;
                return false;
            }
        }
        
        // validate library paths
        for (const auto& libFolder : libraryFolders) {
            bool foundInSearchPath = false;
            for (const auto& searchPathEntry : searchPath) {
                mx::FilePath fullPath = searchPathEntry / libFolder;
                if (fullPath.exists() && fullPath.isDirectory()) {
                    foundInSearchPath = true;
                    break;
                }
            }
            if (!foundInSearchPath && !searchPath.isEmpty()) {
                std::cerr << "Warning: Library folder not found in any search path: " 
                          << libFolder.asString() << std::endl;
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

    OsoExporterArgumentHandler inputArgs;

    inputArgs.oslCompileOptions.writeSourceToDisk = false;
    inputArgs.oslCompileOptions.writeByteCodeToDisk = true;
    inputArgs.oslCompileOptions.embedSource = false;
    inputArgs.oslCompileOptions.optimizationLevel = OslCompileOptions::Performance;

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
        return 1;
    }

    // Append the standard library folder, giving it a lower precedence than user-supplied libraries.
    inputArgs.searchPath.append(mx::getDefaultDataSearchPath());
    
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
    
    inputArgs.oslCompileOptions.oslIncludePath.append(inputArgs.searchPath.find("libraries/stdlib/genosl/include"));

    std::cout << inputArgs.oslCompileOptions.oslIncludePath.size() << " OSL include paths:" << std::endl;

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
    //context.getOptions().shaderInterfaceType = mx::ShaderInterfaceType::SHADER_INTERFACE_REDUCED;

    for (const mx::FilePath& p : inputArgs.oslCompileOptions.oslIncludePath) {
        context.registerSourceCodeSearchPath(p);
    }

    std::unordered_set<std::string> materialNames;

    /*
    for (const mx::DocumentPtr& doc : materialDocuments) {
        std::cout << "Document has " << doc->getMaterialNodes().size() << " material nodes." << std::endl;
        for (mx::NodePtr materialNode : doc->getMaterialNodes()) {
            const std::string oslFileName = materialNode->getName();
            std::cout << "Generating OSL shader for material: " << oslFileName << std::endl;
            
            std::cout << "  Material node category: " << materialNode->getCategory() << std::endl;
            std::cout << "  Material node type: " << materialNode->getType() << std::endl;
            
            if (materialNames.contains(oslFileName)) {
                std::cerr << "Duplicate material name found: " << oslFileName << ". Material names must be unique!" << std::endl;
                return 1;
            }
            materialNames.insert(oslFileName);
            
            std::cout << "  Material has " << materialNode->getInputs().size() << " inputs:" << std::endl;
            for (mx::InputPtr input : materialNode->getInputs()) {
                std::cout << "    - Input: " << input->getName() 
                        << ", Type: " << input->getType() 
                        << ", HasValue: " << (input->hasValue() ? "yes" : "no");
                if (input->hasNodeName()) {
                    std::cout << ", NodeName: " << input->getNodeName();
                }
                std::cout << std::endl;
            }
            
            std::cout << "  Checking child nodes in document:" << std::endl;
            for (mx::NodePtr node : doc->getNodes()) {
                std::cout << "    - Node: " << node->getName() 
                        << ", Category: " << node->getCategory() 
                        << ", Type: " << node->getType() << std::endl;
                
                for (mx::InputPtr input : node->getInputs()) {
                    std::cout << "      Input: " << input->getName() 
                            << ", Type: " << input->getType() << std::endl;
                }
                
                std::cout << "      Outputs: " << node->getOutputs().size() << std::endl;
                for (mx::OutputPtr output : node->getOutputs()) {
                    std::cout << "        Output: " << output->getName() 
                            << ", Type: " << output->getType() << std::endl;
                }
                
                mx::NodeDefPtr nodeDef = node->getNodeDef();
                if (nodeDef) {
                    std::cout << "      NodeDef found: " << nodeDef->getName() 
                            << ", Type: " << nodeDef->getType() << std::endl;
                    std::cout << "      NodeDef outputs: " << nodeDef->getOutputs().size() << std::endl;
                    for (mx::OutputPtr defOutput : nodeDef->getOutputs()) {
                        std::cout << "        DefOutput: " << defOutput->getName() 
                                << ", Type: " << defOutput->getType() << std::endl;
                    }
                    
                    // Check if implementation exists
                    mx::InterfaceElementPtr impl = nodeDef->getImplementation();
                    if (impl) {
                        std::cout << "      Implementation found: " << impl->getName() << std::endl;
                    } else {
                        std::cout << "      WARNING: No implementation found for NodeDef!" << std::endl;
                    }
                } else {
                    std::cout << "      WARNING: No NodeDef found for node '" 
                            << node->getName() << "' (category: " 
                            << node->getCategory() << ")" << std::endl;
                }
            }
            
            // check registered type syntaxes in the shader generator
            std::cout << "\n  Checking OSL shader generator type syntax:" << std::endl;
            const mx::Syntax& oslSyntax = oslShaderGen->getSyntax();
            std::cout << "    Registered type syntaxes:" << std::endl;
            for (const auto& typeSyntax : oslSyntax.getTypeSyntaxes()) {
                std::cout << "      - " << typeSyntax->getName() << std::endl;
            }
            
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
                    inputArgs.oslCompileOptions
                );
            } catch (mx::Exception& e) {
                std::cerr << "Error: " << e.what() << std::endl;
                std::cerr << "  This error occurred while processing material: " << oslFileName << std::endl;
                return 1;
            }
        }
    }
    */
    
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
                    inputArgs.osoOutputPath,
                    inputArgs.oslCompileOptions
                );
                            
            } catch (mx::Exception& e) {
                std::cerr << "Error: " << e.what() << std::endl;
                return 1;
            }
        }
    }
    
    return 0;
}
