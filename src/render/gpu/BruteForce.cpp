#include "BruteForce.h"
#include "../../utils/utils.h"
#include "vulkan/vulkan.hpp"
#include <cstdint>
#include <cstring>

namespace rt::gfx {
    void BruteForce::build(const VkCore& vkCore, const RendererContext& context, const OutputImage& outputImage) {
        sceneSettings = extractSceneSettings(context);
        createBuffers(vkCore, context);
        fillBuffers(vkCore, context);
        fillTextures(vkCore, context);
        createDescriptorPool(vkCore, context);
        createDescriptorLayouts(vkCore, context, outputImage);
        writeStaticDescriptorSets(vkCore, context, outputImage);
        writeBindlessDescriptorSets(vkCore, context);
        createPipeline(vkCore);
    }

    void BruteForce::record(const vk::CommandBuffer& cmb, const uint32_t WIDTH, const uint32_t HEIGHT) {
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


    SceneSettings BruteForce::extractSceneSettings(const RendererContext& context) const {
        CameraData cameraData = {
            .viewInverse = glm::inverse(context.camera->getView()),
            .projInverse = glm::inverse(context.camera->getProjection(context.screenSettings->IMAGE_ASPECT_RATIO)),
            .position    = context.camera->getPosition()
        };

        SceneSettings sceneUBO = {
            .cameraData      = cameraData,
            .vertexCount     = static_cast<uint32_t>(context.scene->vertices.size()),
            .triangleCount   = static_cast<uint32_t>(context.scene->triangles.size()),
            .maxBounces      = 8,
            .samplesPerPixel = 4
        };

        return sceneUBO;
    }

    TextureImage BruteForce::extractTextureImage(const VkCore& vkCore, const Texture& texture) const {
        TextureImage textureImage;
        vk::ImageCreateInfo imageInfo{
            .imageType     = vk::ImageType::e2D,    
            .format        = vk::Format::eR8G8B8A8Unorm,
            .extent.width  = texture.width,
            .extent.height = texture.height,
            .extent.depth  = 1,
            .mipLevels     = 1,                              
            .arrayLayers   = 1,
            .samples       = vk::SampleCountFlagBits::e1,
            .tiling        = vk::ImageTiling::eOptimal,      
            .usage         = vk::ImageUsageFlagBits::eSampled |
                             vk::ImageUsageFlagBits::eTransferDst,
            .sharingMode   = vk::SharingMode::eExclusive,
            .initialLayout = vk::ImageLayout::eUndefined
        };
        textureImage.image = vk::raii::Image(vkCore.getDevice(), imageInfo);

        auto memRequirements = textureImage.image.getMemoryRequirements();
        vk::MemoryAllocateInfo memoryAllocateInfo{
            .allocationSize  = memRequirements.size,
            .memoryTypeIndex = vkCore.findMemoryType(memRequirements.memoryTypeBits,
                                                     vk::MemoryPropertyFlagBits::eDeviceLocal )
        };

        textureImage.memory = vk::raii::DeviceMemory(vkCore.getDevice(), memoryAllocateInfo);
        textureImage.image.bindMemory(*textureImage.memory, 0);

        vk::ImageViewCreateInfo viewInfo{
            .image = *textureImage.image,
            .viewType = vk::ImageViewType::e2D,
            .format = vk::Format::eR8G8B8A8Unorm,
            .subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor,
            .subresourceRange.baseMipLevel = 0,
            .subresourceRange.levelCount = 1,
            .subresourceRange.baseArrayLayer = 0,
            .subresourceRange.layerCount = 1
        };

        textureImage.view = vk::raii::ImageView(vkCore.getDevice(), viewInfo);

        vk::SamplerCreateInfo samplerInfo{};

        switch (texture.magFilter) {
            case Filter::NEAREST: samplerInfo.magFilter = vk::Filter::eNearest; break;
            case Filter::LINEAR:  samplerInfo.magFilter = vk::Filter::eLinear; break;
        }
        switch (texture.minFilter) {
            case Filter::NEAREST: samplerInfo.minFilter = vk::Filter::eNearest; break;
            case Filter::LINEAR:  samplerInfo.minFilter = vk::Filter::eLinear; break;
        }
        switch (texture.mipmapMode) {
            case Filter::NEAREST: samplerInfo.mipmapMode = vk::SamplerMipmapMode::eNearest; break;
            case Filter::LINEAR:  samplerInfo.mipmapMode  = vk::SamplerMipmapMode::eLinear; break;
        }
        switch (texture.wrapU) {
            case WrapMode::CLAMP_TO_EDGE:   samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge; break;
            case WrapMode::REPEAT:          samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat; break;
            case WrapMode::MIRRORED_REPEAT: samplerInfo.addressModeU = vk::SamplerAddressMode::eMirroredRepeat; break;
        }
        switch (texture.wrapV) {
            case WrapMode::CLAMP_TO_EDGE:   samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge; break;
            case WrapMode::REPEAT:          samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat; break;
            case WrapMode::MIRRORED_REPEAT: samplerInfo.addressModeV = vk::SamplerAddressMode::eMirroredRepeat; break;
        }

        vk::PhysicalDeviceProperties properties = vkCore.getPhysicalDevice().getProperties();
        samplerInfo.anisotropyEnable = vk::True;
        samplerInfo.maxAnisotropy    = properties.limits.maxSamplerAnisotropy;
        samplerInfo.compareEnable    = vk::False;
        samplerInfo.compareOp        = vk::CompareOp::eAlways;

        textureImage.sampler = vk::raii::Sampler(vkCore.getDevice(), samplerInfo);
        return textureImage;
    }

    void BruteForce::createBuffers(const VkCore& vkCore, const RendererContext& context) {
        vkCore.createBuffer(
            context.scene->vertices.size() * sizeof(Vertex),
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
            vk::MemoryPropertyFlagBits::eDeviceLocal,
            vertices.buffer,
            vertices.memory
        );
        vkCore.createBuffer(
            context.scene->triangles.size() * sizeof(Triangle),
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
            vk::MemoryPropertyFlagBits::eDeviceLocal,
            triangles.buffer,
            triangles.memory
        );
        vkCore.createBuffer(
            context.scene->materials.size() * sizeof(Material),
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
            vk::MemoryPropertyFlagBits::eDeviceLocal,
            materials.buffer,
            materials.memory
        );
        vkCore.createBuffer(
            sizeof(SceneSettings),
            vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst,
            vk::MemoryPropertyFlagBits::eDeviceLocal,
            sceneSettingsUBO.buffer,
            sceneSettingsUBO.memory
        );
    }


    void BruteForce::fillBuffers(const VkCore& vkCore, const RendererContext& context) {
        vk::raii::Buffer       poolBuffer({});
		vk::raii::DeviceMemory poolMemory({});
        uint32_t verSize           = align(context.scene->vertices.size()  * sizeof(Vertex)  , 4); 
        uint32_t triSize           = align(context.scene->triangles.size() * sizeof(Triangle), 4);
        uint32_t matSize           = align(context.scene->materials.size() * sizeof(Material), 4);
        uint32_t sceneSettingsSize = align(sizeof(SceneSettings), 4);
        uint32_t totalPoolSize     = verSize + triSize + matSize + sceneSettingsSize;
        uint32_t offset            = 0;
        vkCore.createBuffer(totalPoolSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                            poolBuffer, poolMemory);
        void* mappedPoolMemory = poolMemory.mapMemory(0, totalPoolSize);
        auto commandBuffer = vkCore.beginSingleTimeCommands();

        memcpy((char*)mappedPoolMemory + offset, context.scene->vertices.data(), verSize);
        vkCore.fillBuffer(poolBuffer, vertices.buffer, verSize, offset, *commandBuffer);
        offset += verSize;

        memcpy((char*)mappedPoolMemory + offset, context.scene->triangles.data(), triSize);
        vkCore.fillBuffer(poolBuffer, triangles.buffer, triSize, offset, *commandBuffer);
        offset += triSize;

        memcpy((char*)mappedPoolMemory + offset, context.scene->materials.data(), matSize);
        vkCore.fillBuffer(poolBuffer, materials.buffer, matSize, offset, *commandBuffer);
        offset += matSize;

        memcpy((char*)mappedPoolMemory + offset, &sceneSettings, sceneSettingsSize);
        vkCore.fillBuffer(poolBuffer, sceneSettingsUBO.buffer, sceneSettingsSize, offset, *commandBuffer);

		poolMemory.unmapMemory();
        vkCore.endSingleTimeCommands(*commandBuffer);
    }


    void BruteForce::fillTextures(const VkCore& vkCore, const RendererContext& context) {
        vk::raii::Buffer       poolBuffer({});
		vk::raii::DeviceMemory poolMemory({});
        uint32_t               totalPoolSize = 0;
        auto commandBuffer = vkCore.beginSingleTimeCommands();

        for (const Texture& texture : context.scene->textures) {
            textures.push_back(extractTextureImage(vkCore, texture));
            vkCore.transitionImageLayout(textures.back().image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, *commandBuffer);
            uint32_t imageSize = texture.width * texture.height * 4;
            totalPoolSize     += align(imageSize, 4);
        }

        assert(totalPoolSize != 0);
		vkCore.createBuffer(totalPoolSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, poolBuffer, poolMemory);
        void* mappedPoolMemory = poolMemory.mapMemory(0, totalPoolSize);

        uint32_t offset = 0;
        for (size_t i = 0; i < context.scene->textures.size(); ++i) {
            const auto& texture      = context.scene->textures[i];
            const auto& textureImage = textures[i];
            uint32_t imageSize       = texture.width * texture.height * 4;
            memcpy((char*)mappedPoolMemory + offset, texture.data.data(), imageSize);
		    vkCore.copyBufferToImage(textureImage.image, poolBuffer, texture.height, texture.width, offset, *commandBuffer);
            offset += align(imageSize, 4);
        }
		poolMemory.unmapMemory();

        for (auto& tex : textures) {
            vkCore.transitionImageLayout(
                tex.image,
                vk::ImageLayout::eTransferDstOptimal,
                vk::ImageLayout::eShaderReadOnlyOptimal,
                *commandBuffer
            );
        }
        
        vkCore.endSingleTimeCommands(*commandBuffer);
    }

    void BruteForce::createDescriptorPool(const VkCore& vkCore, const RendererContext& context) {
        std::vector<vk::DescriptorPoolSize> poolSizes = {
           vk::DescriptorPoolSize(vk::DescriptorType::eStorageImage,  1),       //outputImage
           vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, 3),       //scene buffers
           vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 1),       //sceneSettings buffers
           vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, MAX_TEXTURE_NUMBER) //bindless textures
        };

        vk::DescriptorPoolCreateInfo poolInfo = {
            .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet | vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind,
            .maxSets = 1,
            .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
            .pPoolSizes = poolSizes.data()
        };
    
        descriptorPool = vk::raii::DescriptorPool(vkCore.getDevice(), poolInfo);
    }

    void BruteForce::createDescriptorLayouts(const VkCore& vkCore, const RendererContext& context, const OutputImage& outputImage) {
        std::vector<vk::DescriptorSetLayoutBinding> layoutBindings = {
            vk::DescriptorSetLayoutBinding{.binding = 0, .descriptorType = vk::DescriptorType::eStorageImage        , .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eCompute},
            vk::DescriptorSetLayoutBinding{.binding = 1, .descriptorType = vk::DescriptorType::eStorageBuffer       , .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eCompute},
            vk::DescriptorSetLayoutBinding{.binding = 2, .descriptorType = vk::DescriptorType::eStorageBuffer       , .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eCompute},
            vk::DescriptorSetLayoutBinding{.binding = 3, .descriptorType = vk::DescriptorType::eStorageBuffer       , .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eCompute},
            vk::DescriptorSetLayoutBinding{.binding = 4, .descriptorType = vk::DescriptorType::eUniformBuffer       , .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eCompute},
            vk::DescriptorSetLayoutBinding{.binding = 5, .descriptorType = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = MAX_TEXTURE_NUMBER, .stageFlags = vk::ShaderStageFlagBits::eCompute}
        };
        std::vector<vk::DescriptorBindingFlags> bindingFlags(static_cast<uint32_t>(layoutBindings.size()));
        bindingFlags[5] = vk::DescriptorBindingFlagBits::eUpdateAfterBind |
                          vk::DescriptorBindingFlagBits::ePartiallyBound  |
                          vk::DescriptorBindingFlagBits::eVariableDescriptorCount;

        vk::DescriptorSetLayoutBindingFlagsCreateInfo flagsCreateInfo {
            .pBindingFlags = bindingFlags.data(),
            .bindingCount = static_cast<uint32_t>(bindingFlags.size())
        };

        vk::DescriptorSetLayoutCreateInfo layoutInfo = {
            .pNext        = &flagsCreateInfo,
            .flags        = vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool,
            .bindingCount = static_cast<uint32_t>(layoutBindings.size()),
            .pBindings    = layoutBindings.data()
        };

        descriptorSetLayout = vk::raii::DescriptorSetLayout(vkCore.getDevice(), layoutInfo);

        uint32_t currentTextureCount = static_cast<uint32_t>(context.scene->textures.size());
        vk::DescriptorSetVariableDescriptorCountAllocateInfo variableDescriptorCount {
            .descriptorSetCount = 1,
            .pDescriptorCounts = &currentTextureCount
        };

        vk::DescriptorSetAllocateInfo allocInfo{
            .pNext = &variableDescriptorCount,
            .descriptorPool = descriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &*descriptorSetLayout
        };

        descriptorSets = vkCore.getDevice().allocateDescriptorSets(allocInfo);
    
    }

    void BruteForce::writeStaticDescriptorSets(const VkCore& vkCore, const RendererContext& context, const OutputImage& outputImage) {
        vk::DescriptorImageInfo imageInfo{
            .sampler = {},
            .imageView = outputImage.view,
            .imageLayout = vk::ImageLayout::eGeneral
        };
        vk::DescriptorBufferInfo vertBufferInfo{
            .buffer = vertices.buffer,
            .offset = 0,
            .range  = VK_WHOLE_SIZE
        };
        vk::DescriptorBufferInfo triBufferInfo{
            .buffer = triangles.buffer,
            .offset = 0,
            .range  = VK_WHOLE_SIZE
        };
        vk::DescriptorBufferInfo matBufferInfo{
            .buffer = materials.buffer,
            .offset = 0,
            .range  = VK_WHOLE_SIZE
        };
        vk::DescriptorBufferInfo sceneSettsBufferInfo{
            .buffer = sceneSettingsUBO.buffer,
            .offset = 0,
            .range  = VK_WHOLE_SIZE
        };

        std::vector<vk::WriteDescriptorSet> writes{
            vk::WriteDescriptorSet {
                .dstSet = descriptorSets.front(),
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eStorageImage,
                .pImageInfo = &imageInfo 
            },
            vk::WriteDescriptorSet {
                .dstSet = descriptorSets.front(),
                .dstBinding = 1,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eStorageBuffer,
                .pBufferInfo = &vertBufferInfo
            },
            vk::WriteDescriptorSet {
                .dstSet = descriptorSets.front(),
                .dstBinding = 2,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eStorageBuffer,
                .pBufferInfo = &triBufferInfo
            },
            vk::WriteDescriptorSet {
                .dstSet = descriptorSets.front(),
                .dstBinding = 3,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eStorageBuffer,
                .pBufferInfo = &matBufferInfo
            },
            vk::WriteDescriptorSet {
                .dstSet = descriptorSets.front(),
                .dstBinding = 4,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eUniformBuffer,
                .pBufferInfo = &sceneSettsBufferInfo
            }
        };

        vkCore.getDevice().updateDescriptorSets(writes, {});
    }

    void BruteForce::writeBindlessDescriptorSets(const VkCore& vkCore, const RendererContext& context) {
        std::vector<vk::DescriptorImageInfo> texturesInfos;
        texturesInfos.reserve(textures.size());
        for (const TextureImage& tex : textures) {
            texturesInfos.emplace_back(vk::DescriptorImageInfo{
                    .sampler = tex.sampler,
                    .imageView = tex.view,
                    .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
            });
        }

        vk::WriteDescriptorSet write {
            .dstSet = descriptorSets.front(),
            .dstBinding = 5,
            .dstArrayElement = 0,
            .descriptorCount = static_cast<uint32_t>(texturesInfos.size()),
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .pImageInfo = texturesInfos.data()
        };
        vkCore.getDevice().updateDescriptorSets(write, {});

        for (int i = 0; i < 1000*1000*1000; ++i) {
        }
    }

    void BruteForce::createPipeline(const VkCore& vkCore){
       vk::raii::ShaderModule shaderModule = vkCore.createShaderModule(readFile("shaders/raytrace.spv"));
        vk::PipelineShaderStageCreateInfo shaderStageInfo{
            .stage = vk::ShaderStageFlagBits::eCompute,
            .module = shaderModule,
            .pName = "computeMain"
        };
        vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
            .flags = {},
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
