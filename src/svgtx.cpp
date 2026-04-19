#include <string>
#include <vector>
#include <filesystem>
#include <string_view>
#include <iostream>

#include <OpenImageIO/half.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/texture.h>

#define NANOSVG_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvg.h"
#include "nanosvgrast.h"

#include "ArgumentHandler.h"

namespace fs = std::filesystem;

const std::string svgBakerArgOptions = 
    " bakesvg - Bakes an svg into a texture file\n"
    " Options:\n"
    "    --input, -i     <file>       Add input SVG file\n"
    "    --output, -o    <file>       Output .tx file (optional, defaults to input filename with .tx extension)\n"
    "    --width, -w     <int>        Set output texture resolution (optional, defaults to SVG dimensions)\n"
    "    --help, -h                   Print this message\n";

struct LibraryArgumentHandler : public ArgumentHandler {

    fs::path inputFile;
    std::optional<fs::path> outputFile;
    int outputWidth = 0;

    ParseResult parse(const std::string& token, const std::string& nextToken) override {
        switch (hashString(token.c_str())) {
            case (hashString("-i")):
            case (hashString("--input")): {
                if (nextToken.empty()) goto expectOption;
                inputFile = nextToken;
                return SUCCESS_CONSUME_NEXT;
            }
            case (hashString("-o")):
            case (hashString("--output")): {
                if (nextToken.empty()) goto expectOption;
                outputFile = nextToken;
                return SUCCESS_CONSUME_NEXT;
            }
            case (hashString("-w")):
            case (hashString("--width")): {
                if (nextToken.empty()) goto expectOption;
                try {
                    outputWidth = std::stoi(nextToken);
                } catch (const std::exception& e) {
                    std::cerr << "Invalid width value: " << nextToken << std::endl;
                    return FAILURE;
                }
                return SUCCESS_CONSUME_NEXT;
            }
            case (hashString("-h")):
            case (hashString("--help")): {
                std::cout << svgBakerArgOptions << std::endl;
                return EXIT;
            }
            default: {
                std::cerr << "Unknown command-line option: " << token << std::endl;
                return FAILURE;
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
        if (!fs::exists(inputFile)) {
            std::cerr << "Input file does not exist: " << inputFile.string() << std::endl;
            return false;
        }

        if (outputFile.has_value() && outputFile->extension() != ".tx") {
            std::cerr << "Output file must have .tx extension: " << outputFile->string() << std::endl;
            return false;
        }
        return true;
    }
};

int main(int argc, char* const argv[]) {
    std::vector<std::string> tokens;

    for (int i = 1; i < argc; i++) {
        tokens.emplace_back(argv[i]);
    }

    LibraryArgumentHandler args;

    for (size_t i = 0; i < tokens.size(); i++) {
        const std::string& token = tokens[i];
        const std::string& nextToken = i + 1 < tokens.size() ? tokens[i + 1] : "";
        ArgumentHandler::ParseResult parseResult = args.parse(token, nextToken);

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

    if (!args.verify()) {
        std::cerr << "Input argument verification failed." << std::endl;
        return 1;
    }   
            
    NSVGimage *image = NULL;
    NSVGrasterizer *rast = NULL;
    unsigned char* img = NULL;

    auto clean = [&]() -> void {
        if (rast) nsvgDeleteRasterizer(rast);
        if (image) nsvgDelete(image);
        if (img) free(img);
    };

    std::cout << "parsing " << args.inputFile << std::endl;
    fs::path absFile = fs::absolute(args.inputFile);
    image = nsvgParseFromFile(absFile.c_str(), "px", 96.0f);
    if (image == NULL) {
        std::cout << "Could not open SVG image." << std::endl;
        clean();
        return 1;
    }

    if (image->width == 0 || image->height == 0) {
        std::cerr << "SVG has no intrinsic size: " << args.inputFile << std::endl;
        clean();
        return 1;
    }

    float scale;
    if (args.outputWidth > 0) {
        scale = (float)args.outputWidth / image->width;
    } else {
        scale = 1.0f; // use intrinsic size
    }

    int w = (int)std::round(image->width  * scale);
    int h = (int)std::round(image->height * scale);

    rast = nsvgCreateRasterizer();
    if (rast == NULL) {
        std::cout << "Could not init rasterizer." << std::endl;
        clean();
        return 1;
    }

    img = (unsigned char*)calloc(w * h * 4, sizeof(unsigned char));
    if (img == NULL) {
        std::cout << "Could not alloc image buffer." << std::endl;
        clean();
        return 1;
    }

    std::cout << "rasterizing image:" << w << " x " << h << std::endl;
    nsvgRasterize(rast, image, 0, 0, scale, img, w, h, w * 4);

    {
        auto srgbToLinear = [](float c) -> float {
            if (c <= 0.04045f)
                return c / 12.92f;
            return std::pow((c + 0.055f) / 1.055f, 2.4f);
        };

        std::vector<half> imgHalf(w * h * 4);
        for (int i = 0; i < w * h; i++) {
            imgHalf[i*4 + 0] = half(srgbToLinear(img[i*4 + 0] / 255.0f));
            imgHalf[i*4 + 1] = half(srgbToLinear(img[i*4 + 1] / 255.0f));
            imgHalf[i*4 + 2] = half(srgbToLinear(img[i*4 + 2] / 255.0f));
            imgHalf[i*4 + 3] = half(img[i*4 + 3] / 255.0f);
        }

        OIIO::ImageSpec srcSpec(w, h, 4, OIIO::TypeDesc::HALF);
        srcSpec.attribute("oiio:ColorSpace", "Linear");
        srcSpec.channelnames = {"R", "G", "B", "A"};
        srcSpec.alpha_channel = 3;

        OIIO::ImageBuf srcBuf(srcSpec, imgHalf.data());

        OIIO::ImageSpec configSpec;
        configSpec.attribute("maketx:forcefloat", 0); 
        configSpec.attribute("maketx:filtername", "lanczos3");
        configSpec.attribute("maketx:opaquedetect", 1);
        configSpec.attribute("maketx:unpremult", 1);
        configSpec.attribute("maketx:updatemode", 0);
        configSpec.attribute("maketx:monochrome_detect", 1);
        configSpec.attribute("maketx:fixnan", "box3");
        configSpec.attribute("compression", "dwaa");             
        configSpec.attribute("oiio:ColorSpace", "Linear");

        fs::path outFileName;
        if (args.outputFile.has_value()) {
            outFileName = args.outputFile.value();
        } else {
            outFileName = args.inputFile.filename();
            outFileName.replace_extension("tx");
        }

        bool ok = OIIO::ImageBufAlgo::make_texture(
            OIIO::ImageBufAlgo::MakeTxTexture,
            srcBuf,
            outFileName.string(),
            configSpec,
            &std::cout
        );

        if (!ok) {
            std::cerr << "make_texture failed for " << outFileName << ": " << OIIO::geterror() << std::endl;
            clean();
            return 1;
        }

        std::cout << "Successfully wrote optimized .tx: " << outFileName << std::endl;
    }

    return 0;
}