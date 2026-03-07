
#include <cassert>
#include <iostream>

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/timeCode.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdGeom/xformOp.h>
#include <pxr/usd/usdGeom/cube.h>
#include <pxr/usd/usdGeom/cylinder.h>

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/quatf.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/gf/quaternion.h>
#include <pxr/base/gf/rotation.h>

#include "ChronoUsdChef.h"

using namespace pxr;

std::optional<ChronoUsdChef> ChronoUsdChef::create(const std::filesystem::path& outputPath, double fps) {
    UsdStageRefPtr stage = UsdStage::CreateNew(outputPath);
    if (!stage) {
        std::cerr << ("ChronoUsdChef: failed to create USD stage at \"" + outputPath.string() + "\"") << std::endl;
        return std::nullopt;
    }

    // Z-up, meters
    UsdGeomSetStageUpAxis(stage, UsdGeomTokens->z);
    UsdGeomSetStageMetersPerUnit(stage, 1.0); 

    // timeCodes per second so viewers play at wall-clock sim time
    stage->SetTimeCodesPerSecond(fps);

    std::cout << "Stage created: " << outputPath << std::endl;

    return ChronoUsdChef(stage, fps);
}

void ChronoUsdChef::RegisterBody(
    chrono::ChBody* body,
    const std::string& primPath,
    Shape shape,
    double dim0,
    double dim1
) {
    assert(body && "RegisterBody: body pointer is null");

    SdfPath path(primPath);
    UsdGeomXform xformPrim = UsdGeomXform::Define(stage, path);

    SdfPath shapePath = path.AppendChild(TfToken("shape"));
    switch (shape) {
        case Shape::BOX: {
            UsdGeomCube cube = UsdGeomCube::Define(stage, shapePath);
            cube.GetSizeAttr().Set(dim0 * 2.0);   // USD Cube.size = full edge length
            break;
        }
        case Shape::CYLINDER: {
            UsdGeomCylinder cyl = UsdGeomCylinder::Define(stage, shapePath);
            cyl.GetRadiusAttr().Set(dim0);
            cyl.GetHeightAttr().Set(dim1 * 2.0);
            cyl.GetAxisAttr().Set(UsdGeomTokens->y);
            break;
        }
        default: break;
    }

    // Write world-space translate + orient ops.
    xformPrim.SetResetXformStack(true);
    
    UsdGeomXformOp transOp = xformPrim.AddTranslateOp();
    UsdGeomXformOp orientOp = xformPrim.AddOrientOp();

    VtArray<TfToken> opOrder = { TfToken("!resetXformStack!"), TfToken("xformOp:translate"), TfToken("xformOp:orient") };
    xformPrim.GetXformOpOrderAttr().Set(opOrder);

    entries.push_back({ body, transOp, orientOp });

    std::cout << " Registered \"" << body->GetName() << "\" -> " << primPath << "\n";
}

// called once per frame
void ChronoUsdChef::ExportData(double simTime) {
    UsdTimeCode tc(simTime * fps);

    if (firstTime < 0.0)
        firstTime = simTime;
    lastTime = simTime;

    for (auto& e : entries) {
        const auto& pos = e.body->GetPos();
        const auto& q   = e.body->GetRot();

        GfVec3d translation = GfVec3d(pos.x(), pos.y(), pos.z());

        GfQuatf orientation = GfQuatf(
            static_cast<float>(q.e0()),
            static_cast<float>(q.e1()),
            static_cast<float>(q.e2()),
            static_cast<float>(q.e3())
        );
        
        e.translateOp.Set(translation, tc);
        e.orientOp.Set(orientation, tc);
    }
}

void ChronoUsdChef::Finalize() {
    double startTC = firstTime * fps;
    double endTC = lastTime  * fps;
    stage->SetStartTimeCode(startTC);
    stage->SetEndTimeCode(endTC);

    stage->GetRootLayer()->Save();
}
