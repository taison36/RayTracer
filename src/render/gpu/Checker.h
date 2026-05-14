#pragma once
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include "vulkan/vulkan.hpp"
#include "AccelerationStructure.h"

namespace rt::gfx {

    class Checker : public AccelerationStruct {
        vk::raii::Pipeline                   pipeline{nullptr};
        vk::raii::PipelineLayout             pipelineLayout{nullptr};

        vk::raii::DescriptorPool             descriptorPool{nullptr};
        vk::raii::DescriptorSetLayout        descriptorSetLayout{nullptr};
        std::vector<vk::raii::DescriptorSet> descriptorSets;

        void createDescriptorPool(const VkCore& vkCore, const Scene& scene);
        void createDescriptorSets(const VkCore& vkCore, const Scene& scene, const OutputImage& outputImage);
        void createPipeline(const VkCore& vkCore);
    public:
        Checker() = default;
        void build(const VkCore& vkCore, const RendererContext& rendererContext, const OutputImage& outputImage) override;
        void record(const vk::CommandBuffer& cmb, uint32_t tileWidth, uint32_t tileHeight,
                    uint32_t sampleIndex, uint32_t tileOffsetX, uint32_t tileOffsetY) override;
        uint32_t getSamplesPerPixel() const override { return 1; }
        const std::string getTypeName() const override { return "Checker"; }
    };

}// rt::gfx
