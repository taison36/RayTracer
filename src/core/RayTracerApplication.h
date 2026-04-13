#pragma once
#include "../objects/UtilObjects.h"
#include "../render/gpu/Renderer.h"
#include <cstdint>
#include <memory>

namespace rt {
    static constexpr uint32_t WIDTH  = 1920;
    static constexpr uint32_t HEIGHT = 1080;
    static constexpr int FOV    = 60.0f;

    class RayTracerApplication {
        std::unique_ptr<ScreenSettings> screenSettings;
        std::unique_ptr<FrameBuffer>    buffer;
        std::unique_ptr<gfx::Renderer>  renderer;
        std::unique_ptr<Scene>          scene;
        std::unique_ptr<Camera>         camera;

        void save_as_ppm(const char* filename);
    public:
        RayTracerApplication(const std::string& scenePath, std::unique_ptr<gfx::AccelerationStruct> accelStruct);
        void run();
    };
}// rt
