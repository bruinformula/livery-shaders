#include <iostream>

#include <OSL/oslexec.h>
#include <OSL/rendererservices.h>

using namespace OSL;

class DummyRenderer : public RendererServices {};

int main() {
    auto renderer = std::make_unique<DummyRenderer>();

    std::cout << "Dude" << "\n";
    return 0;
}
