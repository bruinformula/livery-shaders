#pragma once 

#include <string>

#include <MaterialXCore/Document.h>
#include <MaterialXFormat/File.h>
#include <MaterialXCore/Definition.h>

namespace mx = MaterialX;

std::string unescapeString(const std::string& input);

std::string materialXTypeToGLSL(const std::string& mtlxType);

std::string generateGLSLStructsFromTypeDefs(mx::DocumentPtr& typeDefDoc);

bool generateGLSLStub(
    const std::string& oslFileName,
    const std::string& shaderName,
    const mx::NodeDefPtr& nodeDef,
    const mx::FilePath& outputDir,
    mx::DocumentPtr& nodeDefDoc,
    mx::DocumentPtr& typeDefDoc
);
