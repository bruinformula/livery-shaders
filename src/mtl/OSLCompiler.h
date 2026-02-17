#pragma once 

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

#include <MaterialXGenGlsl/GlslShaderGenerator.h>
#include <MaterialXGenGlsl/GlslSyntax.h>

namespace mx = MaterialX;
namespace osl = OSL;

const std::string oslcArgOptions =
    " Open Shading Language Compiler\n"
    " Options:\n"
    "    --help                 Print this message\n"
    "    -v                     Verbose mode\n"
    "    -q                     Quiet mode\n"
    "    -Ipath                 Add path to the #include search path\n"
    "    -Dsym[=val]            Define preprocessor symbol\n"
    "    -Usym                  Undefine preprocessor symbol\n"
    "    -O0, -O1, -O2          Set optimization level (default=1)\n"
    "    -d                     Debug mode\n"
    "    -E                     Only preprocess the input and output to stdout\n"
    "    -Werror                Treat all warnings as errors\n"
    "    -embed-source          Embed preprocessed source in the oso file\n"
    "    -buffer                (debugging) Force compile from buffer\n"
    "    -MD, -MMD              Write a depfile containing headers used, to a file\n"
    "    -M, -MM                Like -MD, but write depfile to stdout\n"
    "    -MF filename           Specify the name of the depfile to output (for -MD, -MMD)\n"
    "    -MT target             Specify a custom dependency target name for -M...\n"
    "    --writeOSLSource       Write OSL source to disk\n"
    "    --writeByteCode        Write compiled bytecode to disk\n";

// Utilities
std::vector<mx::FilePath> findFiles(const mx::FilePath& rootDir, const std::string& extension, bool maintainRelativePath = false);

std::string toSnakeCase(const std::string& input);

constexpr uint32_t hashString(std::string_view s) {
    uint32_t h = 2166136261u;
    for (char c : s)
        h = (h ^ c) * 16777619u;
    return h;
}

// MaterialX Utilities
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

std::string materialXTypeToString(
    MaterialXType type, 
    const std::string& structName = ""
);

std::string parseOSLParameterValue(
    const osl::OSLQuery::Parameter& param, 
    const osl::OSLQuery* oslQuery = nullptr, 
    const std::string& paramName = ""
);

std::string parseOSLParameterType(const osl::OSLQuery::Parameter& param);

// Compiler Utilities
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

    ExceptionCompileError& operator=(const ExceptionCompileError& e) {
        Exception::operator=(e);
        _errorLog = e._errorLog;
        return *this;
    }

    const mx::StringVec& errorLog() const {
        return _errorLog;
    }

private:
    mx::StringVec _errorLog;
};

struct ArgumentHandler {

    enum ParseResult {
        SUCCESS,
        SUCCESS_CONSUME_NEXT,
        FAILURE,
        EXIT
    };

    virtual ParseResult parse(const std::string& token, const std::string& nextToken) = 0;
};

struct OslCompileOptions : public ArgumentHandler {
    // Standard Options
    bool verboseMode = false;
    bool quietMode = false;
    bool debugMode = false;
    bool embedSource = false;
    bool warningsAsErrors = false;
    bool preprocessOnly = false;
    bool forceBuffer = false;
    bool writeMD = false;
    bool writeMMD = false;
    bool writeM = false;
    bool writeMM = false;
    std::string depfileName;
    std::string depfileTarget;
    mx::FileSearchPath oslIncludePath;
    std::vector<std::string> definePreprocessors;
    std::vector<std::string> undefinePreprocessors;
    
    enum Optimization {
        None,        // O0
        Size,        // O1
        Performance  // O2
    };
    Optimization optimizationLevel = Optimization::Size;
    
    // Other Options
    bool writeSourceToDisk = true;
    bool writeByteCodeToDisk = false;
    
    std::vector<std::string> getArgs(const mx::FilePath& osoFilePath) const;
    
    ParseResult parse(const std::string& token, const std::string& nextToken) override;
};

bool compileOSLToBytecode(
    const std::string& oslSourceCode, 
    const std::string& oslFileName, 
    const mx::FilePath& outputDir, 
    const OslCompileOptions& options,
    osl::OSLQuery* osoQuery = nullptr
);