#include <iostream>
#include <string>
#include <sstream>
#include <fstream>

#include <MaterialXCore/Document.h>
#include <MaterialXFormat/File.h>
#include <MaterialXFormat/Util.h>

#include "GLSLStubs.h"
#include "OSLCompiler.h"

namespace mx = MaterialX;

std::string materialXTypeToGLSL(const std::string& mtlxType) {

    switch (hashString(mtlxType)) {
        case hashString("vector3"):
        case hashString("color3"): {
            return "vec3";
        }
        case hashString("float"): {
            return "float";
        }
        case hashString("string"): // use int to placehold for strings 
        case hashString("integer"): {
            return "int";
        }
        case hashString("boolean"): {
            return "bool";
        }
        case hashString("BSDF"): {
            return "BSDF";
        }
        case hashString("matrix44"): {
            return "mat4";
        }
        // Array types
        case hashString("stringarray"): // use int to placehold for strings 
        case hashString("integerarray"): {
            return "int[]";
        }
        case hashString("floatarray"): {
            return "float[]";
        }
        case hashString("vector3array"):
        case hashString("color3array"): {
            return "vec3[]";
        }

    }
    
    return mtlxType; // return struct name
}

std::string generateGLSLStructsFromTypeDefs(mx::DocumentPtr& typeDefDoc) {
    std::stringstream glslStructs;
    
    for (mx::TypeDefPtr typeDef : typeDefDoc->getTypeDefs()) {
        std::string typeName = typeDef->getName();
        
        // the members will be empty if it isn't a struct
        auto members = typeDef->getMembers();
        if (members.empty()) continue;
        
        glslStructs << "struct " << typeName << "\n{\n";
        
        for (mx::MemberPtr member : members) {
            std::string memberName = member->getName();
            std::string memberType = member->getType();
            std::string glslType = materialXTypeToGLSL(memberType);
            
            glslStructs << "    " << glslType << " " << memberName << ";\n";
        }
        
        glslStructs << "};\n\n";
    }
    
    return glslStructs.str();
}

bool generateGLSLStub(
    const std::string& oslFileName,
    const std::string& shaderName,
    const mx::NodeDefPtr& nodeDef,
    const mx::FilePath& outputDir,
    mx::DocumentPtr& nodeDefDoc,
    mx::DocumentPtr& typeDefDoc
) {
    mx::FilePath glslFilePath = outputDir / oslFileName;
    glslFilePath.removeExtension();
    glslFilePath.addExtension("glsl");

    try {
        std::ofstream glslFile;
        glslFile.open(glslFilePath.asString());
        
        glslFile << "// " << nodeDef->getNodeString() << " Stub\n\n";
        
        bool hasBSDFOutput = false;
        for (auto output : nodeDef->getActiveOutputs()) {
            if (output->getType() == "BSDF") {
                hasBSDFOutput = true;
                break;
            }
        }
        
        if (hasBSDFOutput) {
            glslFile << "#include \"lib/mx_closure_type.glsl\"\n";
            glslFile << "#include \"lib/mx_microfacet_specular.glsl\"\n\n";
        }
        
        // build struct definitions based on the typedefs 
        std::string structDefs = generateGLSLStructsFromTypeDefs(typeDefDoc);
        if (!structDefs.empty()) {
            glslFile << "// Struct definitions\n";
            glslFile << structDefs;
        }
        
        glslFile << "void " << shaderName << "("; // function signature
        
        // Add ClosureData as first parameter if this is a BSDF function
        bool firstParam = true;
        if (hasBSDFOutput) {
            glslFile << "ClosureData closureData";
            firstParam = false;
        }
        
        // Add inputs
        for (auto input : nodeDef->getActiveInputs()) {
            if (!firstParam) glslFile << ", ";
            firstParam = false;
            
            std::string mtlxType = input->getType();
            std::string glslType = materialXTypeToGLSL(mtlxType);
            std::string paramName = input->getName();
            
            glslFile << glslType << " " << paramName;
        }
        
        // Add outputs
        for (auto output : nodeDef->getActiveOutputs()) {
            if (!firstParam) glslFile << ", ";
            firstParam = false;
            
            std::string mtlxType = output->getType();
            std::string glslType = materialXTypeToGLSL(mtlxType);
            std::string paramName = output->getName();
            
            // BSDF outputs should be inout, not out
            if (mtlxType == "BSDF") {
                glslFile << "inout " << glslType << " " << paramName;
            } else {
                glslFile << "out " << glslType << " " << paramName;
            }
        }
        
        // function body
        glslFile << ")\n";
        glslFile << "{\n";
        glslFile << "    return;\n";
        glslFile << "}\n";
        
        glslFile.close();
        return true;
    }
    catch (std::exception& e) {
        std::cerr << "Exception during GLSL stub generation: " << e.what() << std::endl;
        return false;
    }
}
