#pragma once

#include <pxr/usd/usd/common.h>
#include <string>
#include <vector>

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/xformOp.h>

#include <chrono/physics/ChBody.h>

using namespace pxr;
using namespace chrono;

// Dude it bakes
class ChronoUsdChef {
public:

    enum class Shape {
        BOX,       /// UsdGeomCube     – dim0 = half-edge length (m)
        CYLINDER,  /// UsdGeomCylinder – dim0 = radius (m), dim1 = half-height (m)
    };

    ChronoUsdChef(UsdStageRefPtr stage, double fps) :
        stage(stage),
        fps(fps)
    {}

    static std::optional<ChronoUsdChef> create(const std::filesystem::path& outputPath,  double fps = 24.0);

    void RegisterBody(
        ChBody* body,
        const std::string& primPath,
        Shape shape = Shape::BOX,
        double dim0  = 0.5,
        double dim1  = 0.15
    );

    void ExportData(double simTime);

    void Finalize();

    UsdStageRefPtr GetStage() const { return stage; }

private:

    struct BodyEntry {
        ChBody* body;
        UsdGeomXformOp translateOp;
        UsdGeomXformOp orientOp;
    };
    
    std::vector<BodyEntry> entries;

    UsdStageRefPtr stage;
    double firstTime = -1.0;
    double lastTime = 0.0;
    double fps;

};
