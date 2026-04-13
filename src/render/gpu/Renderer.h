#pragma once
#include "AccelerationStructure.h"
#include "../../objects/UtilObjects.h"
#include "../../core/FrameBuffer.h"
#include "VkCore.h"
#include <memory>

namespace rt::gfx {
    class Renderer {
        const VkCore vkCore{};
        std::unique_ptr<AccelerationStruct> accelStruct{nullptr};

        OutputImage createOutputImage(const RendererContext& context);
        void saveToFrameBuffer(void* data, FrameBuffer& fb) const;
    public: 
        Renderer(std::unique_ptr<AccelerationStruct> accelStruct);
        void run(const RendererContext& context, FrameBuffer& fb);
    };
}// rt::gxf
