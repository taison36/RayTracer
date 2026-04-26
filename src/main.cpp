#include <memory>
#include "core/RayTracerApplication.h"
#include "render/gpu/BruteForce.h"


int main() {
    rt::RayTracerApplication app("resources/roomWithBalls5/", std::make_unique<rt::gfx::BruteForce>());
    app.run();
}
