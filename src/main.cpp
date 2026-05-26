#include <memory>
#include "core/RayTracerApplication.h"
#include "objects/UtilObjects.h"
#include "render/gpu/BruteForce.h"       // O(n)
#include "render/gpu/BVH.h"              // O(log n), midpoint-split BVH
#include "render/gpu/SAHBVH.h"           // O(log n), SAH-BVH
#include "render/gpu/KDTree.h"           // O(log n), KD-Tree (pluggable builder)
#include "render/gpu/KDTreeBuilder.h"    // BinnedSAHBuilder
#include "render/gpu/ExactSAHBuilder.h"     // ExactSAHBuilder (Wald & Havran 2006)
#include "render/gpu/ExactSAHBuilderNoESC.h" // ExactSAHBuilder without empty-space cutting

int main() {
    //const std::string pathToScene = "resources/roomWith3Balls2/";
    //const std::string pathToScene = "resources/roomWithBalls8/";
    const std::string pathToScene = "resources/bunny/";
    constexpr uint32_t maxBounces = 10;
    constexpr uint32_t samplesPerPixel = 10;
    constexpr uint32_t samplesPerEmissiveLight = 6;
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

    rt::RayTracerApplication kdtree_exact_noesc(
            pathToScene,
            std::make_unique<rt::gfx::KDTree>(std::make_unique<rt::gfx::ExactSAHBuilderNoESC>()),
            std::make_unique<rt::SceneSettings>(scene_settings)
    );
    kdtree_exact_noesc.run();

    rt::RayTracerApplication sahbvh(
            pathToScene,
            std::make_unique<rt::gfx::SAHBVH>(),
            std::make_unique<rt::SceneSettings>(scene_settings)
    );
    sahbvh.run();

    //rt::RayTracerApplication bvh(
    //        pathToScene,
    //        std::make_unique<rt::gfx::BVH>(),
    //        std::make_unique<rt::SceneSettings>(scene_settings)
    //);
    //bvh.run();
}
