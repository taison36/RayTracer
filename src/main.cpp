#include <memory>
#include "core/RayTracerApplication.h"
// #include "render/gpu/BruteForce.h"
#include "render/gpu/BVH.h"

int main() {
    //   BruteForce — O(n) per ray, no preprocessing
    //   BVH        — O(log n) per ray, SAH BVH built on CPU at startup
    rt::RayTracerApplication app("resources/roomWithBalls8/", std::make_unique<rt::gfx::BVH>());
    app.run();
}
