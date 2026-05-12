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

    // ── GPU-side BVH node (32 bytes, matches bvh.slang BVHNode) ──────────────
    // Layout packs two uint fields into the .w of each float4 via bit_cast so
    // the struct stays aligned without any vec3 padding surprises.
    //
    // aabbMinLeft.xyz  = AABB min
    // aabbMinLeft.w    = leftFirst  (leaf: first idx in bvhTriIndices;
    //                                internal: left child, right = leftFirst+1)
    // aabbMaxCount.xyz = AABB max
    // aabbMaxCount.w   = triCount   (0 = internal, >0 = leaf)
    struct alignas(16) BVHNode {
        glm::vec4 aabbMinLeft;
        glm::vec4 aabbMaxCount;
    };

    // Shared Vulkan helper types (duplicated from BruteForce.h to keep headers independent)
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

    // ── BVH AccelerationStruct implementation ─────────────────────────────────
    class BVH : public AccelerationStruct {
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

        // BVH-specific buffers
        BvhBuffer bvhNodes;
        BvhBuffer bvhTriIndices;

        std::vector<BvhTextureImage> textures;

        // CPU-side BVH data (built once in build(), uploaded to GPU)
        std::vector<BVHNode>   bvhNodeData;
        std::vector<uint32_t>  bvhTriIndexData;

        BvhSceneSettings   extractSceneSettings(const RendererContext& context) const;
        BvhTextureImage    extractTextureImage(const VkCore& vkCore, const Texture& texture) const;

        void buildBVH(const std::vector<rt::Triangle>& tris,
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
        BVH() = default;
        void build(const VkCore& vkCore, const RendererContext& context, const OutputImage& outputImage) override;
        void record(const vk::CommandBuffer& cmb, uint32_t tileWidth, uint32_t tileHeight,
                    uint32_t sampleIndex, uint32_t tileOffsetX, uint32_t tileOffsetY) override;
        uint32_t getSamplesPerPixel() const override { return sceneSettings.samplesPerPixel; }
    };

} // rt::gfx
