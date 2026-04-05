#pragma once
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include "vulkan/vulkan.hpp"
#include "vulkan/vulkan_beta.h"
#include "AccelerationStructure.h"

namespace rt::gfx {

    class BruteForce : public AccelerationStruct {
        vk::raii::Pipeline                   pipeline;
        vk::raii::PipelineLayout             pipelineLayout;

        vk::raii::DescriptorPool             descriptorPool;
        vk::raii::DescriptorSetLayout        descriptorSetLayout;
        std::vector<vk::raii::DescriptorSet> descriptorSets;

        void createDescriptorPool(const VkCore& vkCore, const Scene& scene);
        void createDescriptorSets(const VkCore& vkCore, const Scene& scene, const OutputImage& outputImage);
        void createPipeline(const VkCore& vkCore);
    public:
        void build(const VkCore& vkCore, const Scene& scene, const OutputImage& outputImage) override;
        void record(const vk::CommandBuffer& commandBuffer) override;
    };

}// rt::gfx
