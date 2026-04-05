#pragma once
#include "AccelerationStructure.h"
#include "../../objects/ScreenSettings.h"
#include "../../core/FrameBuffer.h"
#include "../../core/Scene.h"
#include <memory>

namespace rt::gfx {

    class Renderer {
        const VkCore vkCore{};
        const ScreenSettings screenSettings;
        std::unique_ptr<AccelerationStruct> accelStruct{nullptr};
        OutputImage outputImage;

        void createOutputImage();
        void saveFoFrameBuffer(const void* data, FrameBuffer& fb) const;
    public: 
        Renderer(const ScreenSettings& screenSettings, std::unique_ptr<AccelerationStruct> accelStruct);
        void run(const Scene& scene, FrameBuffer& fb) const;
    };

}// rt::gxf
