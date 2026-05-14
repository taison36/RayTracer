#pragma once
#include "../objects/UtilObjects.h"
#include "../render/gpu/Renderer.h"
#include <memory>

namespace rt {
    static constexpr uint32_t WIDTH  = 800;
    static constexpr uint32_t HEIGHT = 600;
    static constexpr int FOV    = 60.0f;

    class RayTracerApplication {
        std::unique_ptr<SceneSettings> screenSettings;
        std::unique_ptr<FrameBuffer>    buffer;
        std::unique_ptr<gfx::Renderer>  renderer;
        std::unique_ptr<Scene>          scene;

        void save_as_ppm(const char* filename);
    public:
        RayTracerApplication(const std::string& scenePath, std::unique_ptr<gfx::AccelerationStruct> accelStruct, std::unique_ptr<SceneSettings> sceneSettings);
        void run();
    };
}// rt
