#include "RayTracerApplication.h"

#include <chrono>
#include <print>

namespace rt {
    RayTracerApplication::RayTracerApplication(const ScreenSettings& ss, std::unique_ptr<gfx::AccelerationStruct> accelStruct)
        : ss(ss), fb(ss.WIDTH, ss.HEIGHT), renderer(ss, std::move(accelStruct)) {
    }
        
    void RayTracerApplication::run(const Scene& scene) const {
        auto start = std::chrono::high_resolution_clock::now();

        renderer.run(scene, fb); 

        std::println("[INFO] RendererGPU has finished Time: {} ms", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count());
        //TODO: from fb into image
    }
}
