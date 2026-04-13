#include "Checker.h"
#include "../../utils/utils.h"
#include <vector>

namespace rt::gfx {
    void Checker::build(const VkCore& vkCore, const RendererContext& rendererContext, const OutputImage& outputImage) {
        createDescriptorPool(vkCore, *rendererContext.scene);
        createDescriptorSets(vkCore, *rendererContext.scene, outputImage);
        createPipeline(vkCore);
    }

    void Checker::record(const vk::CommandBuffer& cmb, const uint32_t WIDTH, const uint32_t HEIGHT) {
        cmb.bindPipeline(vk::PipelineBindPoint::eCompute, *pipeline);
        cmb.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                               pipelineLayout,
                               0,
                               {*descriptorSets.front()}, 
                               nullptr);
        cmb.dispatch((WIDTH  + 15)  / 16,
                     (HEIGHT + 15) / 16,
                     1);
    }

    void Checker::createDescriptorPool(const VkCore& vkCore, const Scene& scene) {
        std::vector<vk::DescriptorPoolSize> poolSizes;
        poolSizes.emplace_back(vk::DescriptorType::eStorageImage, 1); //outputImage
        vk::DescriptorPoolCreateInfo poolInfo{.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
                                              .maxSets = 1,
                                              .poolSizeCount = 1,
                                              .pPoolSizes = poolSizes.data()
        };
    
        descriptorPool = vk::raii::DescriptorPool(vkCore.getDevice(), poolInfo);
    }

    void Checker::createDescriptorSets(const VkCore& vkCore, const Scene& scene, const OutputImage& outputImage) {
        std::vector<vk::DescriptorSetLayoutBinding> layoutBindings
            = {vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute, nullptr)};
    
        vk::DescriptorSetLayoutCreateInfo layoutInfo{ .flags = {},
                                                      .bindingCount= 1,
                                                      .pBindings = layoutBindings.data()
        };
        descriptorSetLayout = vk::raii::DescriptorSetLayout(vkCore.getDevice(), layoutInfo);
    
        vk::DescriptorSetAllocateInfo allocInfo{.descriptorPool = descriptorPool,
                                                .descriptorSetCount = 1,
                                                .pSetLayouts = &*descriptorSetLayout
        };
        descriptorSets = vkCore.getDevice().allocateDescriptorSets(allocInfo);
    
        vk::DescriptorImageInfo imageInfo{.sampler = {},
                                          .imageView = outputImage.view,
                                          .imageLayout = vk::ImageLayout::eGeneral
        };
        vk::WriteDescriptorSet descriptorWrite{.dstSet = descriptorSets.front(),
                                               .dstBinding = 0,
                                               .dstArrayElement = 0,
                                               .descriptorCount = 1,
                                               .descriptorType = vk::DescriptorType::eStorageImage,
                                               .pImageInfo = &imageInfo 
        };
        vkCore.getDevice().updateDescriptorSets(descriptorWrite, {});
    }

    void Checker::createPipeline(const VkCore& vkCore) {
        vk::raii::ShaderModule shaderModule = vkCore.createShaderModule(readFile("shaders/checker.spv"));
        vk::PipelineShaderStageCreateInfo shaderStageInfo{.stage = vk::ShaderStageFlagBits::eCompute,
                                                                 .module = shaderModule,
                                                                 .pName = "computeMain"
        };
        vk::PipelineLayoutCreateInfo pipelineLayoutInfo{.flags = {},
                                                        .setLayoutCount = 1,
                                                        .pSetLayouts = &*descriptorSetLayout
        };
        pipelineLayout = vk::raii::PipelineLayout(vkCore.getDevice(), pipelineLayoutInfo );
    
        vk::ComputePipelineCreateInfo pipelineInfo{.flags = {},
                                                   .stage = shaderStageInfo,
                                                   .layout= *pipelineLayout
        };
        pipeline = vk::raii::Pipeline(vkCore.getDevice().createComputePipeline(nullptr, pipelineInfo));
    }
    
}// rt::gfx
