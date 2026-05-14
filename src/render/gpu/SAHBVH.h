#pragma once
#include "BVHShared.h"
#include "AccelerationStructure.h"

namespace rt::gfx {

    // SAH-BVH: Surface Area Heuristic BVH.
    // Splits each node by evaluating NUM_BINS candidate planes per axis and
    // choosing the one that minimises expected traversal cost (SAH).
    // Better tree quality than midpoint BVH at the cost of a slower build.
    class SAHBVH : public AccelerationStruct {
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
        SAHBVH() = default;
        void build(const VkCore& vkCore, const RendererContext& context, const OutputImage& outputImage) override;
        void record(const vk::CommandBuffer& cmb, uint32_t tileWidth, uint32_t tileHeight,
                    uint32_t sampleIndex, uint32_t tileOffsetX, uint32_t tileOffsetY) override;
        uint32_t getSamplesPerPixel() const override { return sceneSettings.samplesPerPixel; }
        const std::string getTypeName() const override { return "SAH-BVH"; }
    };

} // rt::gfx
