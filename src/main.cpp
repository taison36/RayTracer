#include <memory>
#include "core/RayTracerApplication.h"
#include "render/gpu/BruteForce.h"
#include "render/gpu/Checker.h"


int main() {
    rt::RayTracerApplication app("resources/simple_model/scene.gltf", std::make_unique<rt::gfx::BruteForce>());
    app.run();
}
