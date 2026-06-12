#pragma once
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>
#include <memory>
#include "builder/IBVHBuilder.h"
#include "../AccelerationStructure.h"

namespace rt::gfx {

#ifndef MAX_TEXTURE_NUMBER
#define MAX_TEXTURE_NUMBER 100
#endif

    struct BvhBuffer {
        vk::raii::Buffer       buffer = vk::raii::Buffer({});
        vk::raii::DeviceMemory memory = vk::raii::DeviceMemory({});
    };

    struct BvhTextureImage {
        vk::raii::Image        image{nullptr};
        vk::raii::Sampler      sampler{nullptr};
        vk::raii::ImageView    view{nullptr};
        vk::raii::DeviceMemory memory{nullptr};
    };

    struct alignas(16) BvhCameraData {
        glm::mat4 viewInverse;
        glm::mat4 projInverse;
        glm::vec3 position;
        float     _pad0;
    };

    struct alignas(16) BvhSceneSettings {
        BvhCameraData cameraData;
        uint32_t vertexCount;
        uint32_t triangleCount;
        uint32_t emissiveLightCount;
        uint32_t directionalLightCount;
        uint32_t pointLightCount;
        uint32_t spotLightCount;
        uint32_t maxBounces;
        uint32_t samplesPerPixel;
        uint32_t samplesPerEmissiveLight;
        glm::uvec3 _pad0;
    };

    class BVH : public AccelerationStruct {
        std::unique_ptr<IBVHBuilder> builder;

        vk::raii::Pipeline                   pipeline{nullptr};
        vk::raii::PipelineLayout             pipelineLayout{nullptr};

        vk::raii::DescriptorPool             descriptorPool{nullptr};
        vk::raii::DescriptorSetLayout        descriptorSetLayout{nullptr};
        std::vector<vk::raii::DescriptorSet> descriptorSets;

        BvhSceneSettings sceneSettings;

        BvhBuffer vertices;
        BvhBuffer triangles;
        BvhBuffer materials;
        BvhBuffer emissiveLight;
        BvhBuffer directionalLight;
        BvhBuffer pointLight;
        BvhBuffer spotLight;
        BvhBuffer sceneSettingsUBO;
        BvhBuffer accumBuffer;
        BvhBuffer bvhNodes;
        BvhBuffer bvhTriIndices;

        std::vector<BvhTextureImage> textures;

        std::vector<BVHNode>  bvhNodeData;
        std::vector<uint32_t> bvhTriIndexData;

        BvhSceneSettings extractSceneSettings(const RendererContext& context) const;
        BvhTextureImage  extractTextureImage(const VkCore& vkCore, const Texture& texture) const;

        void createBuffers(const VkCore& vkCore, const RendererContext& context);
        void fillBuffers(const VkCore& vkCore, const RendererContext& context);
        void fillTextures(const VkCore& vkCore, const RendererContext& context);
        void createDescriptorPool(const VkCore& vkCore, const RendererContext& context);
        void createDescriptorLayouts(const VkCore& vkCore, const RendererContext& context, const OutputImage& outputImage);
        void writeStaticDescriptorSets(const VkCore& vkCore, const RendererContext& context, const OutputImage& outputImage);
        void writeBindlessDescriptorSets(const VkCore& vkCore, const RendererContext& context) const;
        void createPipeline(const VkCore& vkCore);

    public:
        explicit BVH(std::unique_ptr<IBVHBuilder> builder) : builder(std::move(builder)) {}

        void build(const VkCore& vkCore, const RendererContext& context, const OutputImage& outputImage) override;
        void record(const vk::CommandBuffer& cmb, uint32_t tileWidth, uint32_t tileHeight,
                    uint32_t sampleIndex, uint32_t tileOffsetX, uint32_t tileOffsetY) override;
        uint32_t getSamplesPerPixel() const override { return sceneSettings.samplesPerPixel; }
        const std::string getTypeName() const override { return builder->getName(); }
    };

} // rt::gfx
