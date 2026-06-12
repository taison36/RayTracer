#include <memory>
#include "core/RayTracerApplication.h"
#include "objects/UtilObjects.h"
#include "render/gpu/accelerationStructures/BruteForce.h"
#include "render/gpu/accelerationStructures/bvh/BVH.h"
#include "render/gpu/accelerationStructures/bvh/builder/MiddlePointBuilder.h"
#include "render/gpu/accelerationStructures/bvh/builder/SAHBuilder.h"
#include "render/gpu/accelerationStructures/kdtree/KDTree.h"
#include "render/gpu/accelerationStructures/kdtree/builder/BinnedSHABuilder.h"
#include "render/gpu/accelerationStructures/kdtree/builder/ExactSAHBuilder.h"
#include "render/gpu/accelerationStructures/kdtree/builder/ExactSAHBuilderNoESC.h"

int main() {
    //const std::string pathToScene = "resources/roomWith3Balls2/";
    //const std::string pathToScene = "resources/roomWithBalls8/";
    const std::string pathToScene = "resources/old_kitchen_doorLight/";
    constexpr uint32_t maxBounces = 8;
    constexpr uint32_t samplesPerPixel = 40;
    constexpr uint32_t samplesPerEmissiveLight = 2;
    rt::SceneSettings scene_settings(rt::WIDTH,
                        rt::HEIGHT,
                        rt::FOV,
                        maxBounces,
                        samplesPerPixel,
                        samplesPerEmissiveLight
    );

     rt::RayTracerApplication kdtree_binned(
             pathToScene,
             std::make_unique<rt::gfx::KDTree>(std::make_unique<rt::gfx::BinnedSAHBuilder>()),
             std::make_unique<rt::SceneSettings>(scene_settings)
     );
     kdtree_binned.run();

     rt::RayTracerApplication kdtree_exact(
             pathToScene,
             std::make_unique<rt::gfx::KDTree>(std::make_unique<rt::gfx::ExactSAHBuilder>()),
             std::make_unique<rt::SceneSettings>(scene_settings)
     );
     kdtree_exact.run();
    //
    // rt::RayTracerApplication bvh_midpoint(
    //         pathToScene,
    //         std::make_unique<rt::gfx::BVH>(std::make_unique<rt::gfx::MiddlePointBuilder>()),
    //         std::make_unique<rt::SceneSettings>(scene_settings)
    // );
    // bvh_midpoint.run();
    //
    rt::RayTracerApplication bvh_sah(
            pathToScene,
            std::make_unique<rt::gfx::BVH>(std::make_unique<rt::gfx::SAHBuilder>()),
            std::make_unique<rt::SceneSettings>(scene_settings)
    );
    bvh_sah.run();
}
