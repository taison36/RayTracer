#pragma once
#include "../objects/ScreenSettings.h"
#include "../render/gpu/Renderer.h"

#include <memory>
namespace rt {
    class RayTracerApplication {
        ScreenSettings ss;
        FrameBuffer    fb;
        gfx::Renderer  renderer;
    public:
        RayTracerApplication(const ScreenSettings& ss, std::unique_ptr<gfx::AccelerationStruct> accelStruct);
        void run(const Scene& scene) const;
    };
}// rt
