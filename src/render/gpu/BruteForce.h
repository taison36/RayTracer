#pragma once
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>
#include "vulkan/vulkan.hpp"
#include <vulkan/vulkan_core.h>
#include <glm/glm.hpp>
#include "AccelerationStructure.h"

namespace rt::gfx {
#define MAX_TEXTURE_NUMBER 100

    struct alignas(16) CameraData {
        glm::mat4 viewInverse;
        glm::mat4 projInverse;
        glm::vec3 position;
        float     _pad0;
    };
    struct alignas(16) SceneSettings {
        CameraData cameraData;
        uint32_t vertexCount;
        uint32_t triangleCount;
        uint32_t maxBounces;
        uint32_t samplesPerPixel;
    };

    struct Buffer {
        vk::raii::Buffer       buffer = vk::raii::Buffer({});
        vk::raii::DeviceMemory memory = vk::raii::DeviceMemory({});
    };

    struct TextureImage {
            vk::raii::Image        image{nullptr};
            vk::raii::Sampler      sampler{nullptr};
            vk::raii::ImageView    view{nullptr};
            vk::raii::DeviceMemory memory{nullptr};
    };

    class BruteForce : public AccelerationStruct {
        vk::raii::Pipeline                   pipeline{nullptr};
        vk::raii::PipelineLayout             pipelineLayout{nullptr};

        vk::raii::DescriptorPool             descriptorPool{nullptr};
        vk::raii::DescriptorSetLayout        descriptorSetLayout{nullptr};
        std::vector<vk::raii::DescriptorSet> descriptorSets;

        SceneSettings sceneSettings;

        Buffer vertices;
        Buffer triangles;
        Buffer materials;
        Buffer sceneSettingsUBO;
        std::vector<TextureImage> textures;

        SceneSettings extractSceneSettings(const RendererContext& context) const;
        TextureImage  extractTextureImage(const VkCore& vkCore, const Texture& texture) const;
        void fillBuffers(const VkCore& vkCore, const RendererContext& context);
        void fillTextures(const VkCore& vkCore, const RendererContext& context);

        void createBuffers(const VkCore& vkCore, const RendererContext& context);
        void createDescriptorPool(const VkCore& vkCore, const RendererContext& context);
        void createDescriptorLayouts(const VkCore& vkCore, const RendererContext& context, const OutputImage& outputImage);
        void writeStaticDescriptorSets(const VkCore& vkCore, const RendererContext& context, const OutputImage& outputImage);
        void writeBindlessDescriptorSets(const VkCore& vkCore, const RendererContext& context, const OutputImage& outputImage);
        void createPipeline(const VkCore& vkCore);
    public:
        BruteForce() = default;
        void build(const VkCore& vkCore, const RendererContext& context, const OutputImage& outputImage) override;
        void record(const vk::CommandBuffer& cmb, const uint32_t WIDTH, const uint32_t HEIGHT) override;
    };
}// rt::gfx
