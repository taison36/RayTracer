#pragma once
#include "BVHShared.h"
#include "../AccelerationStructure.h"

namespace rt::gfx {

    // Midpoint-split BVH. Splits each node at the midpoint of the longest axis.
    // No cost evaluation — always splits until MAX_LEAF_TRIS is reached.
    // Faster to build than SAH-BVH but produces a lower-quality tree.
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
        BvhBuffer bvhNodes;
        BvhBuffer bvhTriIndices;

        std::vector<BvhTextureImage> textures;

        std::vector<BVHNode>  bvhNodeData;
        std::vector<uint32_t> bvhTriIndexData;

        BvhSceneSettings extractSceneSettings(const RendererContext& context) const;
        BvhTextureImage  extractTextureImage(const VkCore& vkCore, const Texture& texture) const;
        void buildBVH(const std::vector<rt::Triangle>& tris, const std::vector<rt::Vertex>& verts);

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
        const std::string getTypeName() const override { return "BVH"; }
    };

} // rt::gfx
