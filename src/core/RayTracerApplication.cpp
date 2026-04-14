#include "RayTracerApplication.h"
#include "../utils/SceneLoader.h"
#include "../objects/UtilObjects.h"
#include "FrameBuffer.h"

#include <chrono>
#include <print>
#include <fstream>

namespace rt {
    RayTracerApplication::RayTracerApplication(const std::string& scenePath, std::unique_ptr<gfx::AccelerationStruct> accelStruct)
        : screenSettings(std::make_unique<ScreenSettings>(WIDTH, HEIGHT, FOV)),
          buffer(std::make_unique<FrameBuffer>(WIDTH, HEIGHT)),
          renderer(std::make_unique<gfx::Renderer>(std::move(accelStruct))) {

        glm::vec3 modelCenter(0.0f, 0.0f, 0.0f);
        glm::vec3 cameraPos = modelCenter + glm::vec3(-3.0f, 16.0f, 40.0f);
        glm::vec3 dir = glm::normalize(modelCenter - cameraPos);
        float pitch = glm::degrees(asin(dir.y)); 
        float yaw = glm::degrees(atan2(dir.z, dir.x));

        camera = std::make_unique<Camera>(cameraPos, glm::vec3(0.0f, 1.0f, 0.0f), yaw, pitch, screenSettings->FOV);
        scene = std::make_unique<Scene>(SceneLoader::loadScene(scenePath, *camera));
    }

    void RayTracerApplication::run() {
        gfx::RendererContext context(&*scene, &*camera, &*screenSettings);

        auto start = std::chrono::high_resolution_clock::now();

        renderer->run(context, *buffer);

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
        file << screenSettings->WIDTH << " " << screenSettings->HEIGHT << "\n";
        file << "255\n";
    
        // Write RGB only, skip alpha
        for (auto it = buffer->begin(); it != buffer->end(); ++it) {
            file.put((*it).r);
            file.put((*it).g);
            file.put((*it).b);
        }
    
        if (!file) {
            throw std::runtime_error("Failed to write image data");
        }
    }
}
