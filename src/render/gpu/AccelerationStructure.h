#pragma once

#include "VkCore.h"
#include "vulkan/vulkan.hpp"
#include "../../objects/UtilObjects.h"

namespace rt::gfx {

    class AccelerationStruct {
    public:
        virtual void build(const VkCore&, const RendererContext&, const OutputImage&) = 0;
        virtual void record(const vk::CommandBuffer&, uint32_t tileWidth, uint32_t tileHeight,
                            uint32_t sampleIndex, uint32_t tileOffsetX, uint32_t tileOffsetY) = 0;
        virtual uint32_t getSamplesPerPixel() const = 0;

        virtual ~AccelerationStruct() = default;
        AccelerationStruct() = default;
        AccelerationStruct(const AccelerationStruct&) = delete;
        AccelerationStruct& operator=(const AccelerationStruct&) = delete;
    };
}
