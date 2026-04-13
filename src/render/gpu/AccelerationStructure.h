#pragma once

#include "VkCore.h"
#include "vulkan/vulkan.hpp"
#include "../../objects/UtilObjects.h"

namespace rt::gfx {

    class AccelerationStruct {
    public:
        virtual void build(const VkCore&, const RendererContext&, const OutputImage&) = 0;
        virtual void record(const vk::CommandBuffer& , uint32_t WIDTH, uint32_t HEIGHT) = 0;
    
        virtual ~AccelerationStruct() = default;
        AccelerationStruct() = default;
        AccelerationStruct(const AccelerationStruct&) = delete;
        AccelerationStruct& operator=(const AccelerationStruct&) = delete;
    };
}
