#pragma once 

#include <string>
#include <vector>
#include <filesystem>

#include "ArgumentHandler.h"

namespace fs = std::filesystem;

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
    "    -MT target             Specify a custom dependency target name for -M...\n";

struct OslCompileOptions : public ArgumentHandler {

    fs::path outputDirectory;
    std::vector<std::string> oslCompilerArgs;
    
    std::vector<std::string> getArgs(const fs::path& osoFilePath) const;
    
    ParseResult parse(const std::string& token, const std::string& nextToken) override;

    bool verify() const override;
};

bool compileAndWriteOSL(
    const fs::path& oslFilePath, 
    const OslCompileOptions& options
);