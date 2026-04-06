#include "RayTracerApplication.h"

#include <chrono>
#include <print>
#include <fstream>

namespace rt {
    RayTracerApplication::RayTracerApplication(const ScreenSettings& ss, std::unique_ptr<gfx::AccelerationStruct> accelStruct)
        : ss(ss), fb(ss.WIDTH, ss.HEIGHT), renderer(ss, std::move(accelStruct)) {
    }
        
    void RayTracerApplication::run(const Scene& scene) {
        auto start = std::chrono::high_resolution_clock::now();

        renderer.run(scene, fb); 

        std::println("[INFO] RendererGPU has finished Time: {} ms", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count());

        save_as_ppm("checker.ppm");
    }

    void RayTracerApplication::save_as_ppm(const char* filename) {
        std::ofstream file(filename, std::ios::binary);
        if (!file) {
            throw std::runtime_error("Failed to open file");
        }
    
        // PPM header (P6 = binary RGB)
        file << "P6\n";
        file << ss.WIDTH << " " << ss.HEIGHT << "\n";
        file << "255\n";
    
        // Write RGB only, skip alpha
        for (auto it = fb.begin(); it != fb.end(); ++it) {
            file.put((*it).r);
            file.put((*it).g);
            file.put((*it).b);
        }
    
        if (!file) {
            throw std::runtime_error("Failed to write image data");
        }
    }
}
