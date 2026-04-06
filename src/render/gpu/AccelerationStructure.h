#pragma once

#include "VkCore.h"
#include "vulkan/vulkan.hpp"
#include "../../core/Scene.h"
#include "../../objects/ScreenSettings.h"

namespace rt::gfx {
    class AccelerationStruct {
    public:
        virtual void build(const VkCore&, const Scene&, const OutputImage&) = 0;
        virtual void record(const vk::CommandBuffer& , const uint32_t WIDTH, const uint32_t HEIGHT) = 0;
    
        virtual ~AccelerationStruct() = default;
        AccelerationStruct() = default;
        AccelerationStruct(const AccelerationStruct&) = delete;
        AccelerationStruct& operator=(const AccelerationStruct&) = delete;
    };
}
