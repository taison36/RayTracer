#include <memory>
#include <functional>
#include <print>
#include <string>
#include <vector>
#include "core/RayTracerApplication.h"
#include "objects/UtilObjects.h"
#include "render/gpu/accelerationStructures/BruteForce.h"
#include "render/gpu/accelerationStructures/bvh/BVH.h"
#include "render/gpu/accelerationStructures/bvh/builder/MiddlePointBuilder.h"
#include "render/gpu/accelerationStructures/bvh/builder/SAHBuilder.h"
#include "render/gpu/accelerationStructures/kdtree/KDTree.h"
#include "render/gpu/accelerationStructures/kdtree/builder/BinnedSHABuilder.h"
#include "render/gpu/accelerationStructures/kdtree/builder/ExactSAHBuilder.h"

int main() {
    constexpr uint32_t maxBounces = 8;
    constexpr uint32_t samplesPerPixel = 40;
    constexpr uint32_t samplesPerEmissiveLight = 4;
    rt::SceneSettings scene_settings(rt::WIDTH,
                                     rt::HEIGHT,
                                     rt::FOV,
                                     maxBounces,
                                     samplesPerPixel,
                                     samplesPerEmissiveLight);

    const std::vector<std::string> scenes = {
        "dragon",
        "nuclear",
        "roomWith3Balls1",
        "old_kitchen_doorLight",
    };

    typedef std::function<std::unique_ptr<rt::gfx::AccelerationStruct>()> AccStruct;
    const std::vector<AccStruct> structs = {
        [] { return std::make_unique<rt::gfx::KDTree>(std::make_unique<rt::gfx::BinnedSAHBuilder>()); },
        [] { return std::make_unique<rt::gfx::KDTree>(std::make_unique<rt::gfx::ExactSAHBuilder>()); },
        [] { return std::make_unique<rt::gfx::BVH>(std::make_unique<rt::gfx::MiddlePointBuilder>()); },
        [] { return std::make_unique<rt::gfx::BVH>(std::make_unique<rt::gfx::SAHBuilder>()); }
    };

    for (const auto &scene: scenes) {
        const std::string pathToScene = "resources/" + scene + "/";
        std::println("-----------------------------------------------");
        std::printf("[INFO] Rendering %s scene \n", scene.c_str());
        for (const auto &as: structs) {
            const std::string outputFileName = scene + ".ppm";
            rt::RayTracerApplication app(
                pathToScene,
                as(),
                std::make_unique<rt::SceneSettings>(scene_settings),
                outputFileName
            );
            app.run();
        }
    }
}
