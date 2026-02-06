#pragma once 

#include <string>
#include <vector>

#include <MaterialXCore/Document.h>
#include <MaterialXFormat/File.h>
#include <MaterialXFormat/Util.h>

namespace mx = MaterialX;

std::vector<mx::FilePath> findFiles(const mx::FilePath& rootDir, const std::string& extension, bool maintainRelativePath = false);

std::string toSnakeCase(const std::string& input);

constexpr uint32_t hashString(std::string_view s) {
    uint32_t h = 2166136261u;
    for (char c : s)
        h = (h ^ c) * 16777619u;
    return h;
}

std::string unescapeString(const std::string& input);

std::string materialXTypeToGLSL(const std::string& mtlxType);

std::string generateGLSLStructsFromTypeDefs(mx::DocumentPtr& typeDefDoc);

enum class MaterialXType {
    String,
    Integer,
    Float,
    Color3,
    Vector3,
    Matrix44,
    Matrix33,
    IntegerArray,
    FloatArray,
    StringArray,
    Color3Array,
    Vector3Array,
    BSDF,
    Struct,
    Unknown
};

bool generateGLSLStub(
    const std::string& oslFileName,
    const std::string& shaderName,
    const mx::NodeDefPtr& nodeDef,
    const mx::FilePath& outputDir,
    mx::DocumentPtr& nodeDefDoc,
    mx::DocumentPtr& typeDefDoc
);
