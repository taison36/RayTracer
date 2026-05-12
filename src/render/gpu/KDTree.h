#pragma once
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>
#include "vulkan/vulkan.hpp"
#include <vulkan/vulkan_core.h>
#include <glm/glm.hpp>
#include "AccelerationStructure.h"

namespace rt::gfx {
#ifndef MAX_TEXTURE_NUMBER
#define MAX_TEXTURE_NUMBER 100
#endif

    // ── GPU-side KD-Tree node (32 bytes, matches kdtree.slang KDNode) ─────────
    //
    // Interior node:
    //   data0.xyz = AABB min
    //   data0.w   = splitPos          (stored as plain float)
    //   data1.xyz = AABB max
    //   data1.w   = bit_cast<float>(flags):
    //                 bit 31    = 0 (interior)
    //                 bits 30-29 = split axis  (0=X, 1=Y, 2=Z)
    //                 bits 0-28  = left child index  (right = leftChild + 1)
    //
    // Leaf node:
    //   data0.xyz = AABB min
    //   data0.w   = bit_cast<float>(firstPrim)
    //   data1.xyz = AABB max
    //   data1.w   = bit_cast<float>(flags):
    //                 bit 31    = 1 (leaf)
    //                 bits 0-28  = triCount
    struct alignas(16) KDNode {
        glm::vec4 data0;
        glm::vec4 data1;
    };

    struct alignas(16) KDCameraData {
        glm::mat4 viewInverse;
        glm::mat4 projInverse;
        glm::vec3 position;
        float     _pad0;
    };

    struct alignas(16) KDSceneSettings {
        KDCameraData cameraData;

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

    struct KDBuffer {
        vk::raii::Buffer       buffer = vk::raii::Buffer({});
        vk::raii::DeviceMemory memory = vk::raii::DeviceMemory({});
    };

    struct KDTextureImage {
        vk::raii::Image        image{nullptr};
        vk::raii::Sampler      sampler{nullptr};
        vk::raii::ImageView    view{nullptr};
        vk::raii::DeviceMemory memory{nullptr};
    };

    // ── KD-Tree AccelerationStruct implementation ─────────────────────────────
    class KDTree : public AccelerationStruct {
        vk::raii::Pipeline                   pipeline{nullptr};
        vk::raii::PipelineLayout             pipelineLayout{nullptr};

        vk::raii::DescriptorPool             descriptorPool{nullptr};
        vk::raii::DescriptorSetLayout        descriptorSetLayout{nullptr};
        std::vector<vk::raii::DescriptorSet> descriptorSets;

        KDSceneSettings sceneSettings;

        KDBuffer vertices;
        KDBuffer triangles;
        KDBuffer materials;

        KDBuffer emissiveLight;
        KDBuffer directionalLight;
        KDBuffer pointLight;
        KDBuffer spotLight;

        KDBuffer sceneSettingsUBO;
        KDBuffer accumBuffer;

        // KD-Tree specific buffers
        KDBuffer kdNodes;
        KDBuffer kdTriIndices;

        std::vector<KDTextureImage> textures;

        // CPU-side KD-Tree data (built once in build(), uploaded to GPU)
        std::vector<KDNode>   kdNodeData;
        std::vector<uint32_t> kdTriIndexData;

        KDSceneSettings extractSceneSettings(const RendererContext& context) const;
        KDTextureImage  extractTextureImage(const VkCore& vkCore, const Texture& texture) const;

        void buildKDTree(const std::vector<rt::Triangle>& tris,
                         const std::vector<rt::Vertex>&   verts);

        void createBuffers(const VkCore& vkCore, const RendererContext& context);
        void fillBuffers(const VkCore& vkCore, const RendererContext& context);
        void fillTextures(const VkCore& vkCore, const RendererContext& context);
        void createDescriptorPool(const VkCore& vkCore, const RendererContext& context);
        void createDescriptorLayouts(const VkCore& vkCore, const RendererContext& context, const OutputImage& outputImage);
        void writeStaticDescriptorSets(const VkCore& vkCore, const RendererContext& context, const OutputImage& outputImage);
        void writeBindlessDescriptorSets(const VkCore& vkCore, const RendererContext& context) const;
        void createPipeline(const VkCore& vkCore);

    public:
        KDTree() = default;
        void build(const VkCore& vkCore, const RendererContext& context, const OutputImage& outputImage) override;
        void record(const vk::CommandBuffer& cmb, uint32_t tileWidth, uint32_t tileHeight,
                    uint32_t sampleIndex, uint32_t tileOffsetX, uint32_t tileOffsetY) override;
        uint32_t getSamplesPerPixel() const override { return sceneSettings.samplesPerPixel; }
        const std::string getTypeName() const override;
    };

} // rt::gfx
