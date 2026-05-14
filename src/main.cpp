#include <memory>
#include "core/RayTracerApplication.h"
#include "render/gpu/BruteForce.h"  // O(n)
#include "render/gpu/BVH.h"         // O(log n), midpoint-split BVH
#include "render/gpu/SAHBVH.h"      // O(log n), SAH-BVH
#include "render/gpu/KDTree.h"       // O(log n), SAH KD-Tree, triangles may be duplicated

int main() {
    const std::string pathToScene = "resources/roomWith3Balls2/";
    constexpr uint32_t maxBounces = 8;
    constexpr uint32_t samplesPerPixel = 900;
    constexpr uint32_t samplesPerEmissiveLight = 4;
    auto scene_settings = std::make_unique<rt::SceneSettings>(rt::WIDTH,
                                                              rt::HEIGHT,
                                                              rt::FOV,
                                                              maxBounces,
                                                              samplesPerPixel,
                                                              samplesPerEmissiveLight
    );
    //rt::RayTracerApplication kdtree(pathToScene, std::make_unique<rt::gfx::KDTree>());
    //kdtree.run();

    rt::RayTracerApplication sahbvh(pathToScene, std::make_unique<rt::gfx::SAHBVH>(), std::move(scene_settings));
    sahbvh.run();

    //rt::RayTracerApplication bvh(pathToScene, std::make_unique<rt::gfx::BVH>());
    //bvh.run();
}
