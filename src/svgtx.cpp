#include <string>
#include <vector>
#include <filesystem>
#include <string_view>
#include <iostream>
#include <regex>
#include <fstream>
#include <execution>
#include <numeric> 

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
    "    --output, -o    <baseName>   Output .tx file (optional, defaults to input filename with .tx extension)\n"
    "    --flatten, -f                Flatten all layers into a single texture (optional, default is to bake each layer separately)\n"
    "    --width, -w     <int>        Set output texture resolution (optional, defaults to SVG dimensions)\n"
    "    --verbose, -v                Print verbose output during processing\n"
    "    --help, -h                   Print this message\n";

static bool verbose = false;

struct LibraryArgumentHandler : public ArgumentHandler {

    fs::path inputFile;
    std::optional<std::string> outputBaseName;
    int outputWidth = 0;
    bool flatten = false;
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
                outputBaseName = nextToken;
                return SUCCESS_CONSUME_NEXT;
            }
            case (hashString("-f")):
            case (hashString("--flatten")): {
                flatten = true;
                return SUCCESS;
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
            case (hashString("-v")):
            case (hashString("--verbose")): {
                verbose = true;
                return SUCCESS;
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

        if (outputBaseName.has_value() && outputBaseName->back() == '/') {
            std::cerr << "Output base name cannot end with a slash: " << outputBaseName.value() << std::endl;
            return false;
        }
        return true;
    }
};

struct SvgLayer {
    std::string id;
    std::string label;
};

std::vector<SvgLayer> extractLayers(const fs::path& svgPath) {
    std::vector<SvgLayer> layers;
    
    std::ifstream file(svgPath);
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    
    // Match <g> tags that look like Inkscape layers
    // Layers have inkscape:groupmode="layer"
    std::regex layerRegex(R"(<g[^>]*inkscape:groupmode=\"layer\"[^>]*>)");
    std::regex idRegex(R"(id=\"([^\"]+)\")");
    std::regex labelRegex(R"(inkscape:label=\"([^\"]+)\")");
    
    auto begin = std::sregex_iterator(content.begin(), content.end(), layerRegex);
    auto end   = std::sregex_iterator();
    
    for (auto it = begin; it != end; ++it) {
        std::string tag = it->str();
        SvgLayer layer;
        
        std::smatch idMatch, labelMatch;
        if (std::regex_search(tag, idMatch, idRegex))
            layer.id = idMatch[1];
        if (std::regex_search(tag, labelMatch, labelRegex))
            layer.label = labelMatch[1];
        
        if (!layer.id.empty())
            layers.push_back(layer);
    }
    
    return layers;
}

size_t findTagEnd(const std::string& content, size_t tagStart) {
    size_t pos = tagStart + 1;
    bool inQuote = false;
    char quoteChar = 0;
    
    while (pos < content.size()) {
        char c = content[pos];
        
        if (inQuote) {
            if (c == quoteChar)
                inQuote = false;
        } else {
            if (c == '"' || c == '\'') {
                inQuote = true;
                quoteChar = c;
            } else if (c == '>') {
                return pos;
            }
        }
        pos++;
    }
    return std::string::npos;
}

std::string isolateLayer(const std::string content, const std::string& targetId) {
    std::string result;
    result.reserve(content.size());

    size_t pos = 0;
    while (pos < content.size()) {
        size_t tagStart = content.find('<', pos);
        if (tagStart == std::string::npos) {
            result += content.substr(pos);
            break;
        }

        result += content.substr(pos, tagStart - pos);

        size_t tagEnd = findTagEnd(content, tagStart);
        if (tagEnd == std::string::npos) {
            result += content.substr(tagStart);
            break;
        }

        std::string tag = content.substr(tagStart, tagEnd - tagStart + 1);

        // Check for <g followed by whitespace (newline, space, tab)
        bool isG = tag.size() >= 2 && tag[0] == '<' && tag[1] == 'g' 
                && (tag.size() == 2 || std::isspace((unsigned char)tag[2]));

        if (isG) {
            bool isLayerGroup = tag.find("inkscape:groupmode=\"layer\"") != std::string::npos
                             || tag.find("inkscape:groupmode='layer'")  != std::string::npos;

            if (isLayerGroup) {
                bool isTarget = tag.find("id=\"" + targetId + "\"") != std::string::npos
                             || tag.find("id='" + targetId + "'")   != std::string::npos;

                if (verbose) {
                    std::cout << "  Found layer group: isTarget=" << isTarget << " id=" << targetId << std::endl;
                }

                if (!isTarget) {
                    size_t stylePos = tag.find("style=\"");
                    if (stylePos != std::string::npos) {
                        tag.insert(stylePos + 7, "display:none;");
                    } else {
                        tag.insert(tag.size() - 1, " style=\"display:none\"");
                    }
                }
            }
        }

        result += tag;
        pos = tagEnd + 1;
    }

    return result;
}

using ImagePtr = std::unique_ptr<NSVGimage, decltype(&nsvgDelete)>;
using RastPtr = std::unique_ptr<NSVGrasterizer, decltype(&nsvgDeleteRasterizer)>;

struct LayerJob {
    const SvgLayer& layer;
    const std::string& svg;
    float scale;
    int w;
    int h;
    size_t totalLayers;
    const LibraryArgumentHandler& args;
};

bool writeLayer(const LayerJob& job, const std::string& svg, std::mutex& coutMutex) {
    const SvgLayer& layer = job.layer;

    std::vector<unsigned char> img(job.w * job.h * 4, 0);

    if (verbose) {
        std::cout << "Rasterizing layer: " << layer.label << std::endl;
    }

    std::vector<char> svgBuf;
    if (job.args.flatten) {
        svgBuf = std::vector<char>(svg.begin(), svg.end());
    } else {
        const std::string svgLayer = isolateLayer(svg, layer.id);
        svgBuf = std::vector<char>(svgLayer.begin(), svgLayer.end());
    }

    svgBuf.push_back('\0');

    ImagePtr layerImage(nullptr, nsvgDelete);
    layerImage.reset(nsvgParse(svgBuf.data(), "px", 96.0f));
    if (!layerImage) {
        std::cerr << "Could not parse layer: " << layer.label << std::endl;
        return false;
    }

    auto srgbToLinear = [](float c) -> float {
        if (c <= 0.04045f)
            return c / 12.92f;
        return std::pow((c + 0.055f) / 1.055f, 2.4f);
    };

    RastPtr rast(nullptr, nsvgDeleteRasterizer);
    rast.reset(nsvgCreateRasterizer());
    if (!rast) {
        std::cerr << "Could not init rasterizer." << std::endl;
        return false;
    }

    nsvgRasterize(rast.get(), layerImage.get(), 0, 0, job.scale, img.data(), job.w, job.h, job.w * 4);

    std::vector<half> imgHalf(job.w * job.h * 4);
    for (int i = 0; i < job.w * job.h; i++) {
        imgHalf[i*4+0] = half(srgbToLinear(img[i*4+0] / 255.0f));
        imgHalf[i*4+1] = half(srgbToLinear(img[i*4+1] / 255.0f));
        imgHalf[i*4+2] = half(srgbToLinear(img[i*4+2] / 255.0f));
        imgHalf[i*4+3] = half(img[i*4+3] / 255.0f);
    }

    OIIO::ImageSpec srcSpec(job.w, job.h, 4, OIIO::TypeDesc::HALF);
    srcSpec.attribute("oiio:ColorSpace", "Linear");
    srcSpec.channelnames = {"R", "G", "B", "A"};
    srcSpec.alpha_channel = 3;

    OIIO::ImageBuf srcBuf(srcSpec);
    srcBuf.set_pixels(OIIO::ROI::All(), OIIO::TypeDesc::HALF, imgHalf.data());

    // Build output filename per layer: filename_layername.tx
    std::string layerName = layer.label.empty() ? layer.id : layer.label;
    // Replace spaces with underscores for filename
    std::string safeName = layerName;
    std::replace(safeName.begin(), safeName.end(), ' ', '_');

    std::string outputBaseName;
    if (job.args.outputBaseName.has_value()) {
        outputBaseName = job.args.outputBaseName.value();
    } else {
        outputBaseName = job.args.inputFile.stem();
    }

    fs::path layerOutFile;
    if (job.args.flatten && job.totalLayers > 0) {
        layerOutFile = outputBaseName;
    } else {
        layerOutFile = outputBaseName + "_" + safeName;
    }

    layerOutFile.replace_extension("tx");

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

    if (verbose) {
        std::cout << "Writing: " << layerOutFile << std::endl;
    }

    bool ok = OIIO::ImageBufAlgo::make_texture(
        OIIO::ImageBufAlgo::MakeTxTexture,
        srcBuf,
        layerOutFile.string(),
        configSpec
    );

    if (!ok) {
        std::lock_guard<std::mutex> lock(coutMutex);
        std::cerr << "make_texture failed: " << OIIO::geterror() << std::endl;
        return false;
    }

    if (verbose) {
        std::lock_guard<std::mutex> lock(coutMutex);
        std::cout << "Wrote: " << layerOutFile << std::endl;
    }
    return true;
}

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

    ImagePtr image(nullptr, nsvgDelete);

    fs::path absFile = fs::absolute(args.inputFile);
    if (verbose) {
        std::cout << "Parsing " << absFile << std::endl;
    }

    image.reset(nsvgParseFromFile(absFile.c_str(), "px", 96.0f));
    if (!image) {
        std::cerr << "Could not open SVG image\n";
        return 1;
    }

    if (image->width == 0 || image->height == 0) {
        std::cerr << "SVG has no intrinsic size: " << args.inputFile << std::endl;
        return 1;
    }

    float scale;
    if (args.outputWidth > 0) {
        scale = (float)args.outputWidth / image->width;
    } else {
        scale = 1.0f;
    }

    int w = (int)std::round(image->width  * scale);
    int h = (int)std::round(image->height * scale);

    if (verbose) {
        std::cout << "Output size: " << w << "x" << h << std::endl;
    }

    std::vector<SvgLayer> layers;
    
    if (!args.flatten) {
        layers = extractLayers(absFile);
    } else {
        layers.push_back({"", ""});
    }

    std::ifstream file(absFile);
    std::string svg((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    std::vector<LayerJob> layerJobs;

    for (size_t li = 0; li < layers.size(); li++) {
        layerJobs.push_back({
            layers[li],
            svg,
            scale,
            w,
            h,
            layers.size(),
            args
        });
    }

    std::mutex coutMutex;
    std::atomic<int> nextJob{0};
    int total = (int)layerJobs.size();
    int numThreads = std::min((int)std::thread::hardware_concurrency(), total);

    std::vector<std::thread> threads;

    for (int t = 0; t < numThreads; t++) {
        threads.emplace_back([&]() {
            while (true) {
                int i = nextJob.fetch_add(1);
                if (i >= total) break;
                
                bool success = writeLayer(layerJobs[i], svg, coutMutex);
                if (!success) {
                    std::lock_guard<std::mutex> lock(coutMutex);
                    std::cerr << "Failed to write layer: " << layerJobs[i].layer.label << std::endl;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    return 0;
}