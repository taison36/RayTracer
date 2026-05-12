#include <memory>
#include "core/RayTracerApplication.h"
 #include "render/gpu/BruteForce.h"   // O(n) per ray, no preprocessing
 #include "render/gpu/BVH.h"          // O(log n), SAH BVH, each triangle in one node
#include "render/gpu/KDTree.h"          // O(log n), SAH KD-Tree, triangles may be duplicated

int main() {
    rt::RayTracerApplication app("resources/roomWithBalls8/", std::make_unique<rt::gfx::KDTree>());
    //rt::RayTracerApplication app("resources/roomWithBalls8/", std::make_unique<rt::gfx::BVH>());
    app.run();
}
