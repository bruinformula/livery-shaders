#include <stdio.h>

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usdGeom/xform.h>

#include <chrono/collision/ChCollisionSystem.h>
#include <chrono/physics/ChBodyEasy.h>
#include <chrono/physics/ChContactMaterialSMC.h>
#include <chrono/physics/ChSystemSMC.h>
#include <chrono/core/ChMatrix33.h>

#include "ChronoUsdChef.h"

int main(int argc, char* argv[]) {
    using namespace chrono;
    using namespace pxr;

    ChSystemSMC sys;
    sys.SetCollisionSystemType(ChCollisionSystem::Type::BULLET);
    sys.SetContactForceModel(ChSystemSMC::ContactForceModel::Hooke);
    sys.SetGravitationalAcceleration(ChVector3d(0, 0, -9.81));

    auto mat = chrono_types::make_shared<ChContactMaterialSMC>();
    // Hooke model parameters: kn/gn must be non-zero.
    mat->SetKn(2e5f);   // N/m  — normal stiffness
    mat->SetKt(2e4f);   // N/m  — tangential stiffness
    mat->SetGn(40.0f);  // N*s/m — normal damping  (low -> bouncy)
    mat->SetGt(20.0f);  // N*s/m — tangential damping
    mat->SetRestitution(0.75f);  // used by PlainCoulomb / logged for info
    mat->SetFriction(0.4f);

    auto floor = chrono_types::make_shared<ChBodyEasyBox>( 4.0, 4.0, 0.1, 1000.0, false, true, mat );
    floor->SetFixed(true);
    floor->SetPos(ChVector3d(0, 0, -0.05));
    floor->SetName("floor");
    sys.AddBody(floor);

    auto box = chrono_types::make_shared<ChBodyEasyBox>( 0.5, 0.5, 0.5, 200.0, false, true, mat );
    box->SetPos(ChVector3d(0, 0, 5.0));
    box->SetAngVelParent(ChVector3d(0.5, 1.0, 0.3));  // small initial spin rad/s about world axes 
    box->SetName("falling_box");
    sys.AddBody(box);

    const double fps = 24.0;
    const double simEnd = 4.0; // seconds
    const double simDt = 0.002; // 2 ms physics step
    const int stepsPerFrame = static_cast<int>((1.0 / fps) / simDt + 0.5);

    std::optional<ChronoUsdChef> optionalBaker = ChronoUsdChef::create("falling_box.usda", fps);

    if (!optionalBaker) {
        std::cerr << "Failed to create USD baker" << std::endl;
        return -1;
    }

    ChronoUsdChef baker = optionalBaker.value();

    baker.RegisterBody(floor.get(), "/World/Floor", ChronoUsdChef::Shape::BOX, 2.0);
    baker.RegisterBody(box.get(),   "/World/FallingBox", ChronoUsdChef::Shape::BOX, 0.25);

    UsdGeomXform worldPrim = UsdGeomXform::Define(baker.GetStage(), SdfPath("/World"));

    baker.GetStage()->SetDefaultPrim(worldPrim.GetPrim());

    std::cout << "  Simulating " << simEnd << " s  @" << fps << " fps  " << "(dt=" << simDt << ", stepsPerFrame=" << stepsPerFrame << ")...\n";

    int step = 0;
    int frameCount = 0;

    while (sys.GetChTime() < simEnd) {
        sys.DoStepDynamics(simDt);
        ++step;

        if (step % stepsPerFrame == 0) {
            baker.ExportData(sys.GetChTime());
            ++frameCount;
        }
    }

    baker.Finalize();
}