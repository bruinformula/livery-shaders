#include <algorithm>
#include <atomic>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <tinyxml2.h>

#include <msdfgen.h>
#include <msdfgen-ext.h>

#include <OpenImageIO/half.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imageio.h>


namespace fs = std::filesystem;
namespace tx2 = tinyxml2;
using namespace msdfgen;

const char* svgtxArgOptions = 
    " svgtx - Bake SVG layers into MTSDF .tx textures \n"
    " Options:\n"
    "    --input, -i     <file>       Input SVG file\n"
    "    --output, -o    <baseName>   Output base name (optional, defaults to input stem)\n"
    "    --flatten, -f                Bake all layers into one texture (optional)\n"
    "    --width, -w     <int>        Output width in pixels (height auto-scaled)\n"
    "    --pxrange       <float>      SDF pixel range (optional, default: 4)\n"
    "    --angle         <float>      Corner angle threshold in radians (optional, default: 3)\n"
    "    --seed          <uint>       Edge-coloring seed (optional, default: 0)\n"
    "    --verbose, -v                Verbose output\n"
    "    --help, -h                   Print this message\n";

static bool verbose = false;

struct Args {

    fs::path inputFile;
    std::optional<std::string> outputBaseName;
    int outputWidth = 0;
    bool flatten = false;
    double pxRange = 4.0;
    double angleThreshold = 3.0;
    unsigned long long seed = 0;

    enum ParseResult { OK, OK_CONSUME, FAIL, EXIT };

    ParseResult parse(const std::string& token, const std::string& nextToken) {
        if (token == "-i" || token == "--input") {
            if (nextToken.empty()) goto expectOption;
            inputFile = nextToken;
            return OK_CONSUME;
        } else if (token == "-o" || token == "--output") {
            if (nextToken.empty()) goto expectOption;
            outputBaseName = nextToken;
            return OK_CONSUME;
        } else if (token == "-w" || token == "--width") {
            if (nextToken.empty()) goto expectOption;
            try {
                outputWidth = std::stoi(nextToken);
            } catch (const std::exception&) {
                std::cerr << "Invalid width value: " << nextToken << std::endl;
                return FAIL;
            }
            return OK_CONSUME;
        } else if (token == "--pxrange") {
            if (nextToken.empty()) goto expectOption;
            try {
                pxRange = std::stod(nextToken);
            } catch (const std::exception&) {
                std::cerr << "Invalid pxrange value: " << nextToken << std::endl;
                return FAIL;
            }
            return OK_CONSUME;
        } else if (token == "--angle") {
            if (nextToken.empty()) goto expectOption;
            try {
                angleThreshold = std::stod(nextToken);
            } catch (const std::exception&) {
                std::cerr << "Invalid angle value: " << nextToken << std::endl;
                return FAIL;
            }
            return OK_CONSUME;
        } else if (token == "--seed") {
            if (nextToken.empty()) goto expectOption;
            try {
                seed = std::stoull(nextToken);
            } catch (const std::exception&) {
                std::cerr << "Invalid seed value: " << nextToken << std::endl;
                return FAIL;
            }
            return OK_CONSUME;
        } else if (token == "-f" || token == "--flatten") {
            flatten = true;
            return OK;
        } else if (token == "-v" || token == "--verbose") {
            verbose = true;
            return OK;
        } else if (token == "-h" || token == "--help") {
            std::cout << svgtxArgOptions << std::endl;
            return EXIT;
        } else {
            std::cerr << "Unknown command-line option: " << token << std::endl;
            return FAIL;
        }

        expectOption: {
            std::cerr << "Option " << token << " requires an argument." << std::endl;
            return FAIL;
        }
    }

    bool verify() const {
        if (inputFile.empty()) {
            std::cerr << "No input file provided.\n";
            return false;
        }

        if (!fs::exists(inputFile)) {
            std::cerr << "Input file does not exist: " << inputFile.string() << std::endl;
            return false;
        }

        if (pxRange <= 0) {
            std::cerr << "pxrange must be > 0\n";
            return false;
        }

        if (outputBaseName.has_value() && outputBaseName->back() == '/') {
            std::cerr << "Output base name cannot end with a slash: " << outputBaseName.value() << std::endl;
            return false;
        }

        return true;
    }
};

// 2-D affine transform  [a c e]   x' = a*x + c*y + e
//                       [b d f]   y' = b*x + d*y + f
struct Mat2D {
    double a=1,b=0,c=0,d=1,e=0,f=0;

    void applyTo(double& x, double& y) const {
        double nx = a*x + c*y + e;
        double ny = b*x + d*y + f;
        x=nx; y=ny;
    }

    // Compose: apply *this first, then m
    Mat2D concat(const Mat2D& m) const {
        return { m.a*a + m.c*b,  m.b*a + m.d*b,
                 m.a*c + m.c*d,  m.b*c + m.d*d,
                 m.a*e + m.c*f + m.e,
                 m.b*e + m.d*f + m.f };
    }

    bool isIdentity() const { return a==1&&b==0&&c==0&&d==1&&e==0&&f==0; }
};

static void skipSpComma(const char*& p) {
    while (*p==' '||*p=='\t'||*p=='\r'||*p=='\n'||*p==',') ++p;
}
static bool tryDouble(const char*& p, double& v) {
    skipSpComma(p);
    char* end;
    v = strtod(p, &end);
    if (end==p) return false;
    p = end; return true;
}

static Mat2D parseSvgTransform(const char* str) {
    Mat2D result;
    if (!str || !*str) return result;
    const char* p = str;
    while (*p) {
        skipSpComma(p);
        if (!*p) break;
        const char* ns = p;
        while (*p && *p!='(') ++p;
        if (!*p) break;
        std::string name(ns, p);
        ++p; // skip '('
        double v[6]={0,0,0,0,0,0}; int n=0;
        while (*p && *p!=')' && n<6) { 
            if (!tryDouble(p,v[n++])) break; 
        }
        if (*p==')') ++p;

        Mat2D m;
        if (name=="matrix" && n==6) {
            m = {v[0],v[1],v[2],v[3],v[4],v[5]};
        } else if (name=="translate") {
            m = {1,0,0,1, v[0], n>=2?v[1]:0.0};
        } else if (name=="scale") {
            double sx=v[0], sy=(n>=2?v[1]:v[0]);
            m = {sx,0,0,sy,0,0};
        } else if (name=="rotate") {
            double ang=v[0]*M_PI/180.0, cs=cos(ang), sn=sin(ang);
            if (n>=3) {
                double ox=v[1], oy=v[2];
                m = {cs,sn,-sn,cs, ox-ox*cs+oy*sn, oy-ox*sn-oy*cs};
            } else {
                m = {cs,sn,-sn,cs,0,0};
            }
        } else if (name=="skewX") {
            m = {1,0,tan(v[0]*M_PI/180.0),1,0,0};
        } else if (name=="skewY") {
            m = {1,tan(v[0]*M_PI/180.0),0,1,0,0};
        }
        result = result.concat(m);
    }
    return result;
}

static void applyTransformToShape(Shape& shape, const Mat2D& m) {
    if (m.isIdentity()) return;
    for (auto& contour : shape.contours) {
        for (auto& edge : contour.edges) {
            switch (edge->type()) {
                case LinearSegment::EDGE_TYPE: {
                    auto* s = static_cast<LinearSegment*>(&*edge);
                    m.applyTo(s->p[0].x, s->p[0].y);
                    m.applyTo(s->p[1].x, s->p[1].y);
                    break;
                }
                case QuadraticSegment::EDGE_TYPE: {
                    auto* s = static_cast<QuadraticSegment*>(&*edge);
                    for (auto& p : s->p) m.applyTo(p.x, p.y);
                    break;
                }
                case CubicSegment::EDGE_TYPE: {
                    auto* s = static_cast<CubicSegment*>(&*edge);
                    for (auto& p : s->p) m.applyTo(p.x, p.y);
                    break;
                }
            }
        }
    }
}

// Primitive -> path-d string used only for rect/circle/ellipse/polygon
static const double kappa = 0.5522847498;

static std::string rectToD(
    double x, 
    double y, 
    double w, 
    double h,                 
    double rx=0, 
    double ry=0
) {
    if (rx<=0 && ry<=0) {
        std::ostringstream s;
        s << "M " << x     << " " << y
          << " H " << x+w
          << " V " << y+h
          << " H " << x
          << " Z";
        return s.str();
    }
    if (rx<=0) rx=ry;
    if (ry<=0) ry=rx;
    rx=std::min(rx,w*0.5); ry=std::min(ry,h*0.5);
    double kx=kappa*rx, ky=kappa*ry;
    std::ostringstream s;
    s << "M "  << x+rx    << " " << y
      << " H " << x+w-rx
      << " C " << x+w-rx+kx << " " << y      << " " << x+w << " " << y+ry-ky << " " << x+w << " " << y+ry
      << " V " << y+h-ry
      << " C " << x+w     << " " << y+h-ry+ky << " " << x+w-rx+kx << " " << y+h << " " << x+w-rx << " " << y+h
      << " H " << x+rx
      << " C " << x+rx-kx << " " << y+h      << " " << x << " " << y+h-ry+ky << " " << x << " " << y+h-ry
      << " V " << y+ry
      << " C " << x       << " " << y+ry-ky  << " " << x+rx-kx << " " << y << " " << x+rx << " " << y
      << " Z";
    return s.str();
}

static std::string circleToD(double cx, double cy, double r) {
    double k=kappa*r;
    std::ostringstream s;
    s << "M " << cx+r << " " << cy
      << " C " << cx+r << " " << cy+k << " " << cx+k << " " << cy+r << " " << cx   << " " << cy+r
      << " C " << cx-k << " " << cy+r << " " << cx-r << " " << cy+k << " " << cx-r << " " << cy
      << " C " << cx-r << " " << cy-k << " " << cx-k << " " << cy-r << " " << cx   << " " << cy-r
      << " C " << cx+k << " " << cy-r << " " << cx+r << " " << cy-k << " " << cx+r << " " << cy
      << " Z";
    return s.str();
}

static std::string ellipseToD(double cx, double cy, double rx, double ry) {
    double kx=kappa*rx, ky=kappa*ry;
    std::ostringstream s;
    s << "M "  << cx+rx << " " << cy
      << " C " << cx+rx << " " << cy+ky << " " << cx+kx << " " << cy+ry << " " << cx    << " " << cy+ry
      << " C " << cx-kx << " " << cy+ry << " " << cx-rx << " " << cy+ky << " " << cx-rx << " " << cy
      << " C " << cx-rx << " " << cy-ky << " " << cx-kx << " " << cy-ry << " " << cx    << " " << cy-ry
      << " C " << cx+kx << " " << cy-ry << " " << cx+rx << " " << cy-ky << " " << cx+rx << " " << cy
      << " Z";
    return s.str();
}

static std::string polygonToD(const char* pts, bool close) {
    std::ostringstream s;
    const char* p = pts;
    bool first = true;
    while (*p) {
        double x, y;
        if (!tryDouble(p,x) || !tryDouble(p,y)) break;
        s << (first ? "M " : "L ") << x << " " << y << "\n";
        first = false;
    }
    if (!first && close) s << "Z";
    return s.str();
}

// Returns a path-d string for supported primitive elements, or empty string for <path>.
// Caller handles <path> directly via its "d" attribute.
static std::string primitiveToPathD(const tx2::XMLElement* e) {
    const char* tag = e->Name();
    if (!tag) return {};

    if (!strcmp(tag,"rect")) {
        double w=e->DoubleAttribute("width"), h=e->DoubleAttribute("height");
        if (w>0&&h>0)
            return rectToD(e->DoubleAttribute("x"), e->DoubleAttribute("y"), w, h,
                           e->DoubleAttribute("rx"), e->DoubleAttribute("ry"));
    } else if (!strcmp(tag,"circle")) {
        double r=e->DoubleAttribute("r");
        if (r>0) return circleToD(e->DoubleAttribute("cx"), e->DoubleAttribute("cy"), r);
    } else if (!strcmp(tag,"ellipse")) {
        double rx=e->DoubleAttribute("rx"), ry=e->DoubleAttribute("ry");
        if (rx>0&&ry>0) return ellipseToD(e->DoubleAttribute("cx"), e->DoubleAttribute("cy"), rx, ry);
    } else if (!strcmp(tag,"polygon")) {
        const char* pts=e->Attribute("points");
        if (pts) return polygonToD(pts,true);
    } else if (!strcmp(tag,"polyline")) {
        const char* pts=e->Attribute("points");
        if (pts) return polygonToD(pts,false);
    }
    return {};
}

// Recursively collect shapes.
static void collectShapes(
    const tx2::XMLElement* elem,
    const Mat2D& parentTx,
    const std::string& targetId,
    bool flatMode,
    bool collecting,
    Shape& shape,
    double snapRange
) {
    if (!elem) return;
    const char* tag = elem->Name();
    if (!tag) return;

    // Skip non-drawable metadata
    if (!strcmp(tag,"defs")   || !strcmp(tag,"metadata") ||
        !strcmp(tag,"title")  || !strcmp(tag,"desc")     ||
        !strcmp(tag,"sodipodi:namedview")) return;

    // Accumulate transform
    const Mat2D myTx  = parseSvgTransform(elem->Attribute("transform"));
    const Mat2D curTx = parentTx.concat(myTx);

    // Determine whether this is an Inkscape layer
    bool isLayer = false;
    const char* gm = elem->Attribute("inkscape:groupmode");
    if (gm && !strcmp(gm,"layer")) isLayer=true;

    // Determine collecting state
    bool nowCollecting = collecting;
    if (!flatMode && isLayer) {
        const char* id = elem->Attribute("id");
        nowCollecting = (id && targetId==id);
    }

    // Emit geometry from this element if collecting
    if (flatMode || nowCollecting) {
        const char* pathD = nullptr;
        std::string syntheticD;

        if (!strcmp(tag,"path")) {
            pathD = elem->Attribute("d");
        } else {
            syntheticD = primitiveToPathD(elem);
            if (!syntheticD.empty()) pathD = syntheticD.c_str();
        }

        if (pathD && *pathD) {
            // Build a temporary shape from this path/primitive, then transform it.
            // buildShapeFromSvgPath handles all relative/absolute, arcs, S/T reflection, etc.
            Shape tmp;
            tmp.setYAxisOrientation(Y_UPWARD);
            buildShapeFromSvgPath(tmp, pathD, snapRange);

            applyTransformToShape(tmp, curTx);

            // Move contours into the main shape
            for (auto& c : tmp.contours)
                shape.contours.push_back(std::move(c));
        }
    }

    // Recurse into children
    for (auto* c=elem->FirstChildElement(); c; c=c->NextSiblingElement())
        collectShapes(c, curTx, targetId, flatMode, nowCollecting, shape, snapRange);
}

// Layer discovery
struct SvgLayer { 
    std::string id, label;
};

static void findLayers(const tx2::XMLElement* e, std::vector<SvgLayer>& out) {
    if (!e) return;
    const char* gm = e->Attribute("inkscape:groupmode");
    if (gm && !strcmp(gm,"layer")) {
        SvgLayer L;
        if (const char* id  = e->Attribute("id"))             L.id    = id;
        if (const char* lbl = e->Attribute("inkscape:label")) L.label = lbl;
        else L.label = L.id;
        if (!L.id.empty()) out.push_back(L);
    }
    for (auto* c=e->FirstChildElement(); c; c=c->NextSiblingElement())
        findLayers(c, out);
}

static bool parseViewBox(
    const tx2::XMLElement* svg,           
    double& x, 
    double& y, 
    double& w, 
    double& h
) {
    if (const char* vb = svg->Attribute("viewBox")) {
        if (sscanf(vb," %lf %lf %lf %lf",&x,&y,&w,&h)==4 && w>0&&h>0)
            return true;
    }
    x = 0; y = 0;
    w = svg->DoubleAttribute("width");
    h = svg->DoubleAttribute("height");
    return w > 0 && h > 0;
}

// Fit shape into bitmap with pxRange pixels of SDF margin.
static void autoFrame(
    const Shape& shape, 
    int w, 
    int h, 
    double pxRange,                  
    Vector2& scale, 
    Vector2& translate, 
    Range& range
) {
    Shape::Bounds b = shape.getBounds();
    if (b.l >= b.r || b.b >= b.t) b = {0,0,1,1};

    double margin = pxRange * 0.5;
    double fw = w - 2.0*margin;
    double fh = h - 2.0*margin;
    if (fw<=0) fw=1;
    if (fh<=0) fh=1;

    double dx=b.r-b.l, dy=b.t-b.b;
    double s;
    if (dx*fh < dy*fw) { // height-limited
        s = fh/dy;
        translate.set(0.5*(fw/fh*dy - dx) - b.l, -b.b);
    } else { // width-limited
        s = fw/dx;
        translate.set(-b.l, 0.5*(fh/fw*dx - dy) - b.b);
    }
    scale = s;
    translate.x += margin/s;
    translate.y += margin/s;
    range = Range(pxRange/s);
}

// Build shape, generate MTSDF, write .tx
static bool bakeShape(
    const tx2::XMLElement* svgRoot,
    const std::string& targetLayerId,
    bool flatMode,
    const std::string& outPath,
    double vbW, double vbH,
    double snapRange,
    int width,
    double pxRange,
    double angleThreshold,
    unsigned long long seed
) {
    Shape shape;
    shape.setYAxisOrientation(Y_UPWARD);

    collectShapes(svgRoot, Mat2D{}, targetLayerId, flatMode, flatMode, shape, snapRange);

    if (shape.contours.empty()) {
        std::cerr << "No contours for " << outPath << " (empty layer?) – skipping.\n";
        return false;
    }

    if (!shape.validate()) {
        std::cerr << "Invalid geometry for " << outPath << "\n";
        return false;
    }
    if (shape.edgeCount() == 0) {
        std::cerr << "No edges for " << outPath << " – skipping.\n";
        return false;
    }

    shape.normalize();

    // Output dimensions: preserve SVG aspect ratio so all layers share canvas size
    int height = width;
    if (vbW>0 && vbH>0)
        height = std::max(1, (int)std::round(width * vbH / vbW));

    if (verbose)
        std::cout << "  " << width << "x" << height
                  << "  edges=" << shape.edgeCount() << "\n";

    // Edge colouring
    edgeColoringSimple(shape, angleThreshold, seed);

    // Auto-frame: always fit the actual shape bounds
    Vector2 sc, tr;
    Range rng;
    autoFrame(shape, width, height, pxRange, sc, tr, rng);
    SDFTransformation xform(Projection(sc, tr), DistanceMapping(rng));

    // At this point sign (inside/outside) is determined from contour winding alone,
    // which may be wrong for SVG paths with arbitrary winding directions.
    Bitmap<float,4> mtsdf(width, height);
    {
        MSDFGeneratorConfig genCfg;
        genCfg.errorCorrection.mode = ErrorCorrectionConfig::DISABLED;
        generateMTSDF(mtsdf, shape, xform, genCfg);
    }
 
    // Rasterises the shape with even-odd fill rule and flips any pixel whose sign disagrees with the raster fill.
    distanceSignCorrection(mtsdf, shape, xform, 0.5f, FILL_ODD);

    {
        MSDFGeneratorConfig errCfg;
        errCfg.errorCorrection.distanceCheckMode = ErrorCorrectionConfig::DO_NOT_CHECK_DISTANCE;
        msdfErrorCorrection(mtsdf, shape, xform, errCfg);
    }

    // OIIO ImageBuf
    OIIO::ImageSpec spec(width, height, 4, OIIO::TypeDesc::HALF); // oiio handles conversion from float to half?
    spec.attribute("oiio:ColorSpace", "Linear");
    spec.channelnames = {"R","G","B","A"};
    spec.alpha_channel = 3;

    OIIO::ImageBuf buf(spec);
    for (int y=0; y<height; ++y) {
        for (int x=0; x<width; ++x) {
            const float* px = mtsdf(x, y);
            float rgba[4] = {px[0], px[1], px[2], px[3]};
            buf.setpixel(x, y, 0, rgba, 4);
        }
    }

    // Write .tx
    OIIO::ImageSpec cfg;
    cfg.attribute("maketx:forcefloat",        0);
    cfg.attribute("maketx:filtername",        "lanczos3");
    cfg.attribute("maketx:opaquedetect",      0);   // alpha = SDF, not opacity
    cfg.attribute("maketx:unpremult",         0);
    cfg.attribute("maketx:updatemode",        0);
    cfg.attribute("maketx:monochrome_detect", 0);
    cfg.attribute("maketx:fixnan",            "box3");
    cfg.attribute("compression",              "dwaa");
    cfg.attribute("oiio:ColorSpace",          "Linear");

    bool ok = OIIO::ImageBufAlgo::make_texture(
        OIIO::ImageBufAlgo::MakeTxTexture, buf, outPath, cfg);

    if (!ok)
        std::cerr << "make_texture failed for " << outPath
                  << ": " << OIIO::geterror() << "\n";
    else if (verbose)
        std::cout << "  Wrote: " << outPath << "\n";

    return ok;
}

int main(int argc, char* const argv[]) {
    Args args;
    std::vector<std::string> tokens;
    for (int i=1; i<argc; ++i) tokens.emplace_back(argv[i]);

    for (size_t i=0; i<tokens.size(); ++i) {
        const std::string& next = (i+1<tokens.size()) ? tokens[i+1] : std::string{};
        auto r = args.parse(tokens[i], next);
        if (r==Args::FAIL) return 1;
        if (r==Args::EXIT) return 0;
        if (r==Args::OK_CONSUME) ++i;
    }
    if (!args.verify()) return 1;

    // Parse SVG
    tx2::XMLDocument doc;
    if (doc.LoadFile(args.inputFile.string().c_str()) != tx2::XML_SUCCESS) {
        std::cerr << "Failed to parse " << args.inputFile << "\n"; return 1;
    }
    tx2::XMLElement* svgRoot = doc.FirstChildElement("svg");
    if (!svgRoot) { std::cerr << "No <svg> root element.\n"; return 1; }

    double vbX=0, vbY=0, vbW=0, vbH=0;
    parseViewBox(svgRoot, vbX, vbY, vbW, vbH);

    int width = args.outputWidth;
    if (width<=0) width = (vbW>0) ? (int)std::round(vbW) : 512;

    // Endpoint snap range proportional to diagonal
    double snapRange;
    if (vbW>0&&vbH>0) {
        snapRange = (1.0/16384.0) * sqrt(vbW*vbW + vbH*vbH);
    } else {
        snapRange = 0.0;
    }

    std::string base = args.outputBaseName.value_or(
        args.inputFile.stem().string());

    if (verbose) {
        std::cout << "Input:   " << args.inputFile << "\n"
                  << "Base:    " << base << "\n"
                  << "ViewBox: " << vbW << "x" << vbH << "\n"
                  << "Width:   " << width << "px\n"
                  << "PxRange: " << args.pxRange << "\n";
    }

    if (args.flatten) {
        fs::path out = fs::path(base).replace_extension("tx");
        if (verbose) std::cout << "Baking (flat): " << out << "\n";
        return bakeShape(svgRoot, {}, true, out.string(),
                         vbW, vbH, snapRange, width,
                         args.pxRange, args.angleThreshold, args.seed) ? 0 : 1;
    }

    // Layer stuff
    std::vector<SvgLayer> layers;
    findLayers(svgRoot, layers);

    if (layers.empty()) {
        if (verbose) std::cout << "No Inkscape layers found – flat bake.\n";
        fs::path out = fs::path(base).replace_extension("tx");
        return bakeShape(svgRoot, {}, true, out.string(),
                         vbW, vbH, snapRange, width,
                         args.pxRange, args.angleThreshold, args.seed) ? 0 : 1;
    }

    // Per-layer parallel bake
    std::mutex logMu;
    std::atomic<int> nextJob{0};
    int total    = (int)layers.size();
    int nThreads = std::max(1, std::min((int)std::thread::hardware_concurrency(), total));

    std::vector<bool> results(total, true);
    std::vector<std::thread> threads;

    for (int t=0; t<nThreads; ++t) {
        threads.emplace_back([&]() {
            while (true) {
                int idx = nextJob.fetch_add(1);
                if (idx >= total) break;

                const SvgLayer& L = layers[idx];
                std::string safe = L.label.empty() ? L.id : L.label;
                std::replace(safe.begin(), safe.end(), ' ', '_');

                fs::path out = fs::path(base + "_" + safe).replace_extension("tx");

                if (verbose) {
                    std::lock_guard<std::mutex> lk(logMu);
                    std::cout << "Baking [" << L.label << "]: " << out << "\n";
                }

                bool ok = bakeShape(svgRoot, L.id, false, out.string(),
                                    vbW, vbH, snapRange, width,
                                    args.pxRange, args.angleThreshold, args.seed);
                results[idx] = ok;
                if (!ok) {
                    std::lock_guard<std::mutex> lk(logMu);
                    std::cerr << "Failed: " << out << "\n";
                }
            }
        });
    }
    for (auto& t : threads) t.join();

    bool allOk = true;
    for (bool r : results) allOk = allOk && r;
    return allOk ? 0 : 1;
}