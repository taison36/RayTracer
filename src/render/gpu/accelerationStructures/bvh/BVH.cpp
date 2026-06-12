#include "BVH.h"
#include "../../../../utils/utils.h"
#include "vulkan/vulkan.hpp"

namespace rt::gfx {

    void BVH::build(const VkCore& vkCore, const RendererContext& context, const OutputImage& outputImage) {
        sceneSettings = extractSceneSettings(context);
        builder->build(context.scene->triangles, context.scene->vertices, bvhNodeData, bvhTriIndexData);
        createBuffers(vkCore, context);
        fillBuffers(vkCore, context);
        fillTextures(vkCore, context);
        createDescriptorPool(vkCore, context);
        createDescriptorLayouts(vkCore, context, outputImage);
        writeStaticDescriptorSets(vkCore, context, outputImage);
        writeBindlessDescriptorSets(vkCore, context);
        createPipeline(vkCore);
    }

    void BVH::record(const vk::CommandBuffer& cmb,
                     const uint32_t tileWidth, const uint32_t tileHeight,
                     const uint32_t sampleIndex,
                     const uint32_t tileOffsetX, const uint32_t tileOffsetY) {
        struct PushConstants { uint32_t sampleIndex, tileOffsetX, tileOffsetY; };
        PushConstants pc{sampleIndex, tileOffsetX, tileOffsetY};

        cmb.bindPipeline(vk::PipelineBindPoint::eCompute, *pipeline);
        cmb.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                               pipelineLayout, 0, {*descriptorSets.front()}, nullptr);
        cmb.pushConstants(*pipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(pc), &pc);
        cmb.dispatch((tileWidth + 15) / 16, (tileHeight + 15) / 16, 1);
    }

    BvhSceneSettings BVH::extractSceneSettings(const RendererContext& context) const {
        BvhCameraData cameraData = {
            .viewInverse = glm::inverse(context.scene->camera.getView()),
            .projInverse = glm::inverse(context.scene->camera.getProjection(context.screenSettings->IMAGE_ASPECT_RATIO)),
            .position    = context.scene->camera.getPosition()
        };
        return BvhSceneSettings{
            .cameraData              = cameraData,
            .vertexCount             = static_cast<uint32_t>(context.scene->vertices.size()),
            .triangleCount           = static_cast<uint32_t>(context.scene->triangles.size()),
            .emissiveLightCount      = static_cast<uint32_t>(context.scene->emissiveLight.size()),
            .directionalLightCount   = static_cast<uint32_t>(context.scene->directionalLight.size()),
            .pointLightCount         = static_cast<uint32_t>(context.scene->pointLight.size()),
            .spotLightCount          = static_cast<uint32_t>(context.scene->spotLight.size()),
            .maxBounces              = context.screenSettings->maxBounces,
            .samplesPerPixel         = context.screenSettings->samplesPerPixel,
            .samplesPerEmissiveLight = context.screenSettings->samplesPerEmissiveLight
        };
    }

    BvhTextureImage BVH::extractTextureImage(const VkCore& vkCore, const Texture& texture) const {
        BvhTextureImage textureImage;
        vk::ImageCreateInfo imageInfo{
            .imageType      = vk::ImageType::e2D,
            .format         = vk::Format::eR8G8B8A8Srgb,
            .extent.width   = texture.width,
            .extent.height  = texture.height,
            .extent.depth   = 1,
            .mipLevels      = 1,
            .arrayLayers    = 1,
            .samples        = vk::SampleCountFlagBits::e1,
            .tiling         = vk::ImageTiling::eOptimal,
            .usage          = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
            .sharingMode    = vk::SharingMode::eExclusive,
            .initialLayout  = vk::ImageLayout::eUndefined
        };
        textureImage.image = vk::raii::Image(vkCore.getDevice(), imageInfo);

        auto memReq = textureImage.image.getMemoryRequirements();
        textureImage.memory = vk::raii::DeviceMemory(vkCore.getDevice(), vk::MemoryAllocateInfo{
            .allocationSize  = memReq.size,
            .memoryTypeIndex = vkCore.findMemoryType(memReq.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal)
        });
        textureImage.image.bindMemory(*textureImage.memory, 0);

        textureImage.view = vk::raii::ImageView(vkCore.getDevice(), vk::ImageViewCreateInfo{
            .image                           = *textureImage.image,
            .viewType                        = vk::ImageViewType::e2D,
            .format                          = vk::Format::eR8G8B8A8Srgb,
            .subresourceRange.aspectMask     = vk::ImageAspectFlagBits::eColor,
            .subresourceRange.baseMipLevel   = 0,
            .subresourceRange.levelCount     = 1,
            .subresourceRange.baseArrayLayer = 0,
            .subresourceRange.layerCount     = 1
        });

        vk::SamplerCreateInfo samplerInfo{};
        switch (texture.magFilter) {
            case Filter::NEAREST: samplerInfo.magFilter = vk::Filter::eNearest; break;
            case Filter::LINEAR:  samplerInfo.magFilter = vk::Filter::eLinear;  break;
        }
        switch (texture.minFilter) {
            case Filter::NEAREST: samplerInfo.minFilter = vk::Filter::eNearest; break;
            case Filter::LINEAR:  samplerInfo.minFilter = vk::Filter::eLinear;  break;
        }
        switch (texture.mipmapMode) {
            case Filter::NEAREST: samplerInfo.mipmapMode = vk::SamplerMipmapMode::eNearest; break;
            case Filter::LINEAR:  samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;  break;
        }
        switch (texture.wrapU) {
            case WrapMode::CLAMP_TO_EDGE:   samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;    break;
            case WrapMode::REPEAT:          samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;         break;
            case WrapMode::MIRRORED_REPEAT: samplerInfo.addressModeU = vk::SamplerAddressMode::eMirroredRepeat; break;
        }
        switch (texture.wrapV) {
            case WrapMode::CLAMP_TO_EDGE:   samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;    break;
            case WrapMode::REPEAT:          samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;         break;
            case WrapMode::MIRRORED_REPEAT: samplerInfo.addressModeV = vk::SamplerAddressMode::eMirroredRepeat; break;
        }
        vk::PhysicalDeviceProperties props = vkCore.getPhysicalDevice().getProperties();
        samplerInfo.anisotropyEnable = vk::True;
        samplerInfo.maxAnisotropy    = props.limits.maxSamplerAnisotropy;
        samplerInfo.compareEnable    = vk::False;
        samplerInfo.compareOp        = vk::CompareOp::eAlways;

        textureImage.sampler = vk::raii::Sampler(vkCore.getDevice(), samplerInfo);
        return textureImage;
    }

    void BVH::createBuffers(const VkCore& vkCore, const RendererContext& context) {
        auto mkBuf = [&](vk::DeviceSize size, vk::BufferUsageFlags usage,
                         vk::MemoryPropertyFlags mem, BvhBuffer& out) {
            vkCore.createBuffer(size, usage, mem, out.buffer, out.memory);
        };

        constexpr auto kStorage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst;
        constexpr auto kUniform = vk::BufferUsageFlagBits::eUniformBuffer  | vk::BufferUsageFlagBits::eTransferDst;
        constexpr auto kDev     = vk::MemoryPropertyFlagBits::eDeviceLocal;
        auto sz = [](auto& v, size_t elem) { return std::max(v.size() * elem, elem); };

        const auto& s = *context.scene;
        mkBuf(sz(s.vertices,         sizeof(Vertex)),           kStorage, kDev, vertices);
        mkBuf(sz(s.triangles,        sizeof(Triangle)),         kStorage, kDev, triangles);
        mkBuf(sz(s.materials,        sizeof(Material)),         kStorage, kDev, materials);
        mkBuf(sz(s.emissiveLight,    sizeof(uint32_t)),         kStorage, kDev, emissiveLight);
        mkBuf(sz(s.directionalLight, sizeof(DirectionalLight)), kStorage, kDev, directionalLight);
        mkBuf(sz(s.pointLight,       sizeof(PointLight)),       kStorage, kDev, pointLight);
        mkBuf(sz(s.spotLight,        sizeof(SpotLight)),        kStorage, kDev, spotLight);
        mkBuf(sizeof(BvhSceneSettings), kUniform, kDev, sceneSettingsUBO);

        vk::DeviceSize accumSize = context.screenSettings->WIDTH * context.screenSettings->HEIGHT * sizeof(float) * 4;
        vkCore.createBuffer(accumSize, vk::BufferUsageFlagBits::eStorageBuffer, kDev,
                            accumBuffer.buffer, accumBuffer.memory);

        mkBuf(std::max(bvhNodeData.size()     * sizeof(BVHNode),  sizeof(BVHNode)),  kStorage, kDev, bvhNodes);
        mkBuf(std::max(bvhTriIndexData.size() * sizeof(uint32_t), sizeof(uint32_t)), kStorage, kDev, bvhTriIndices);
    }

    void BVH::fillBuffers(const VkCore& vkCore, const RendererContext& context) {
        vk::raii::Buffer       poolBuffer({});
        vk::raii::DeviceMemory poolMemory({});

        const auto& s = *context.scene;
        uint32_t verSize      = align(s.vertices.size()         * sizeof(Vertex),           4);
        uint32_t triSize      = align(s.triangles.size()        * sizeof(Triangle),         4);
        uint32_t matSize      = align(s.materials.size()        * sizeof(Material),         4);
        uint32_t emissiveSize = align(s.emissiveLight.size()    * sizeof(uint32_t),         4);
        uint32_t dirSize      = align(s.directionalLight.size() * sizeof(DirectionalLight), 4);
        uint32_t pointSize    = align(s.pointLight.size()       * sizeof(PointLight),       4);
        uint32_t spotSize     = align(s.spotLight.size()        * sizeof(SpotLight),        4);
        uint32_t sceneSize    = align(sizeof(BvhSceneSettings),                             4);
        uint32_t bvhNodeSize  = align(bvhNodeData.size()        * sizeof(BVHNode),          4);
        uint32_t bvhIdxSize   = align(bvhTriIndexData.size()    * sizeof(uint32_t),         4);

        uint32_t total = verSize + triSize + matSize + emissiveSize + dirSize +
                         pointSize + spotSize + sceneSize + bvhNodeSize + bvhIdxSize;

        vkCore.createBuffer(total,
                            vk::BufferUsageFlagBits::eTransferSrc,
                            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                            poolBuffer, poolMemory);

        void* mapped = poolMemory.mapMemory(0, total);
        auto  cmd    = vkCore.beginSingleTimeCommands();
        uint32_t off = 0;

        auto upload = [&](const void* data, uint32_t size, BvhBuffer& dst) {
            if (size == 0) return;
            memcpy(static_cast<char*>(mapped) + off, data, size);
            vkCore.fillBuffer(poolBuffer, dst.buffer, size, off, *cmd);
            off += size;
        };

        upload(s.vertices.data(),         verSize,      vertices);
        upload(s.triangles.data(),        triSize,      triangles);
        upload(s.materials.data(),        matSize,      materials);
        upload(s.emissiveLight.data(),    emissiveSize, emissiveLight);
        upload(s.directionalLight.data(), dirSize,      directionalLight);
        upload(s.pointLight.data(),       pointSize,    pointLight);
        upload(s.spotLight.data(),        spotSize,     spotLight);
        upload(&sceneSettings,            sceneSize,    sceneSettingsUBO);
        upload(bvhNodeData.data(),        bvhNodeSize,  bvhNodes);
        upload(bvhTriIndexData.data(),    bvhIdxSize,   bvhTriIndices);

        poolMemory.unmapMemory();
        vkCore.endSingleTimeCommands(*cmd);
    }

    void BVH::fillTextures(const VkCore& vkCore, const RendererContext& context) {
        vk::raii::Buffer       poolBuffer({});
        vk::raii::DeviceMemory poolMemory({});
        uint32_t totalPoolSize = 0;
        auto cmd = vkCore.beginSingleTimeCommands();

        for (const Texture& tex : context.scene->textures) {
            textures.push_back(extractTextureImage(vkCore, tex));
            vkCore.transitionImageLayout(textures.back().image,
                                         vk::ImageLayout::eUndefined,
                                         vk::ImageLayout::eTransferDstOptimal, *cmd);
            totalPoolSize += align(tex.width * tex.height * 4, 4);
        }

        if (totalPoolSize == 0) return;

        vkCore.createBuffer(totalPoolSize,
                            vk::BufferUsageFlagBits::eTransferSrc,
                            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                            poolBuffer, poolMemory);

        void* mapped = poolMemory.mapMemory(0, totalPoolSize);
        uint32_t off = 0;
        for (size_t i = 0; i < context.scene->textures.size(); i++) {
            const auto& tex  = context.scene->textures[i];
            uint32_t    size = tex.width * tex.height * 4;
            memcpy(static_cast<char*>(mapped) + off, tex.data.data(), size);
            vkCore.copyBufferToImage(textures[i].image, poolBuffer, tex.height, tex.width, off, *cmd);
            off += align(size, 4);
        }
        poolMemory.unmapMemory();

        for (auto& t : textures)
            vkCore.transitionImageLayout(t.image,
                                         vk::ImageLayout::eTransferDstOptimal,
                                         vk::ImageLayout::eShaderReadOnlyOptimal, *cmd);
        vkCore.endSingleTimeCommands(*cmd);
    }

    void BVH::createDescriptorPool(const VkCore& vkCore, const RendererContext& context) {
        (void)context;
        std::vector<vk::DescriptorPoolSize> poolSizes = {
            vk::DescriptorPoolSize(vk::DescriptorType::eStorageImage,         1),
            vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer,       10),
            vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer,        1),
            vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, MAX_TEXTURE_NUMBER)
        };
        descriptorPool = vk::raii::DescriptorPool(vkCore.getDevice(), vk::DescriptorPoolCreateInfo{
            .flags         = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet |
                             vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind,
            .maxSets       = 1,
            .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
            .pPoolSizes    = poolSizes.data()
        });
    }

    void BVH::createDescriptorLayouts(const VkCore& vkCore, const RendererContext& context,
                                      const OutputImage& outputImage) {
        (void)outputImage;
        using B = vk::DescriptorSetLayoutBinding;
        using T = vk::DescriptorType;
        using S = vk::ShaderStageFlagBits;

        std::vector<B> bindings = {
            B{.binding=0,  .descriptorType=T::eStorageImage,         .descriptorCount=1,                  .stageFlags=S::eCompute},
            B{.binding=1,  .descriptorType=T::eStorageBuffer,        .descriptorCount=1,                  .stageFlags=S::eCompute},
            B{.binding=2,  .descriptorType=T::eStorageBuffer,        .descriptorCount=1,                  .stageFlags=S::eCompute},
            B{.binding=3,  .descriptorType=T::eStorageBuffer,        .descriptorCount=1,                  .stageFlags=S::eCompute},
            B{.binding=4,  .descriptorType=T::eStorageBuffer,        .descriptorCount=1,                  .stageFlags=S::eCompute},
            B{.binding=5,  .descriptorType=T::eStorageBuffer,        .descriptorCount=1,                  .stageFlags=S::eCompute},
            B{.binding=6,  .descriptorType=T::eStorageBuffer,        .descriptorCount=1,                  .stageFlags=S::eCompute},
            B{.binding=7,  .descriptorType=T::eStorageBuffer,        .descriptorCount=1,                  .stageFlags=S::eCompute},
            B{.binding=8,  .descriptorType=T::eUniformBuffer,        .descriptorCount=1,                  .stageFlags=S::eCompute},
            B{.binding=9,  .descriptorType=T::eStorageBuffer,        .descriptorCount=1,                  .stageFlags=S::eCompute},
            B{.binding=10, .descriptorType=T::eStorageBuffer,        .descriptorCount=1,                  .stageFlags=S::eCompute},
            B{.binding=11, .descriptorType=T::eStorageBuffer,        .descriptorCount=1,                  .stageFlags=S::eCompute},
            B{.binding=12, .descriptorType=T::eCombinedImageSampler, .descriptorCount=MAX_TEXTURE_NUMBER, .stageFlags=S::eCompute},
        };

        std::vector<vk::DescriptorBindingFlags> flags(bindings.size());
        flags[12] = vk::DescriptorBindingFlagBits::eUpdateAfterBind |
                    vk::DescriptorBindingFlagBits::ePartiallyBound  |
                    vk::DescriptorBindingFlagBits::eVariableDescriptorCount;

        vk::DescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{
            .pBindingFlags = flags.data(),
            .bindingCount  = static_cast<uint32_t>(flags.size())
        };
        descriptorSetLayout = vk::raii::DescriptorSetLayout(vkCore.getDevice(), vk::DescriptorSetLayoutCreateInfo{
            .pNext        = &flagsInfo,
            .flags        = vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool,
            .bindingCount = static_cast<uint32_t>(bindings.size()),
            .pBindings    = bindings.data()
        });

        uint32_t texCount = static_cast<uint32_t>(context.scene->textures.size());
        vk::DescriptorSetVariableDescriptorCountAllocateInfo varCount{
            .descriptorSetCount = 1,
            .pDescriptorCounts  = &texCount
        };
        descriptorSets = vkCore.getDevice().allocateDescriptorSets(vk::DescriptorSetAllocateInfo{
            .pNext              = &varCount,
            .descriptorPool     = descriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts        = &*descriptorSetLayout
        });
    }

    void BVH::writeStaticDescriptorSets(const VkCore& vkCore, const RendererContext& context,
                                        const OutputImage& outputImage) {
        (void)context;
        vk::DescriptorImageInfo imageInfo{
            .sampler     = {},
            .imageView   = outputImage.view,
            .imageLayout = vk::ImageLayout::eGeneral
        };

        auto bufInfo = [](const BvhBuffer& b) {
            return vk::DescriptorBufferInfo{.buffer=b.buffer, .offset=0, .range=VK_WHOLE_SIZE};
        };

        vk::DescriptorBufferInfo vBI  = bufInfo(vertices);
        vk::DescriptorBufferInfo tBI  = bufInfo(triangles);
        vk::DescriptorBufferInfo mBI  = bufInfo(materials);
        vk::DescriptorBufferInfo eBI  = bufInfo(emissiveLight);
        vk::DescriptorBufferInfo dBI  = bufInfo(directionalLight);
        vk::DescriptorBufferInfo pBI  = bufInfo(pointLight);
        vk::DescriptorBufferInfo sBI  = bufInfo(spotLight);
        vk::DescriptorBufferInfo ssBI = bufInfo(sceneSettingsUBO);
        vk::DescriptorBufferInfo aBI  = bufInfo(accumBuffer);
        vk::DescriptorBufferInfo nBI  = bufInfo(bvhNodes);
        vk::DescriptorBufferInfo iBI  = bufInfo(bvhTriIndices);

        vk::DescriptorSet ds = descriptorSets.front();
        auto w = [&](uint32_t binding, vk::DescriptorType type, const vk::DescriptorBufferInfo* bi) {
            return vk::WriteDescriptorSet{
                .dstSet=ds, .dstBinding=binding, .dstArrayElement=0,
                .descriptorCount=1, .descriptorType=type, .pBufferInfo=bi
            };
        };

        vkCore.getDevice().updateDescriptorSets(std::vector<vk::WriteDescriptorSet>{
            vk::WriteDescriptorSet{
                .dstSet=ds, .dstBinding=0, .dstArrayElement=0, .descriptorCount=1,
                .descriptorType=vk::DescriptorType::eStorageImage, .pImageInfo=&imageInfo
            },
            w(1,  vk::DescriptorType::eStorageBuffer, &vBI),
            w(2,  vk::DescriptorType::eStorageBuffer, &tBI),
            w(3,  vk::DescriptorType::eStorageBuffer, &mBI),
            w(4,  vk::DescriptorType::eStorageBuffer, &eBI),
            w(5,  vk::DescriptorType::eStorageBuffer, &dBI),
            w(6,  vk::DescriptorType::eStorageBuffer, &pBI),
            w(7,  vk::DescriptorType::eStorageBuffer, &sBI),
            w(8,  vk::DescriptorType::eUniformBuffer, &ssBI),
            w(9,  vk::DescriptorType::eStorageBuffer, &aBI),
            w(10, vk::DescriptorType::eStorageBuffer, &nBI),
            w(11, vk::DescriptorType::eStorageBuffer, &iBI),
        }, {});
    }

    void BVH::writeBindlessDescriptorSets(const VkCore& vkCore, const RendererContext& context) const {
        (void)context;
        std::vector<vk::DescriptorImageInfo> texInfos;
        texInfos.reserve(textures.size());
        for (const BvhTextureImage& t : textures) {
            texInfos.push_back({
                .sampler     = t.sampler,
                .imageView   = t.view,
                .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
            });
        }
        if (texInfos.empty()) return;

        vkCore.getDevice().updateDescriptorSets(vk::WriteDescriptorSet{
            .dstSet          = descriptorSets.front(),
            .dstBinding      = 12,
            .dstArrayElement = 0,
            .descriptorCount = static_cast<uint32_t>(texInfos.size()),
            .descriptorType  = vk::DescriptorType::eCombinedImageSampler,
            .pImageInfo      = texInfos.data()
        }, {});
    }

    void BVH::createPipeline(const VkCore& vkCore) {
        const vk::raii::ShaderModule shaderModule = vkCore.createShaderModule(readFile("shaders/bvh.spv"));
        vk::PushConstantRange pushRange{
            .stageFlags = vk::ShaderStageFlagBits::eCompute,
            .offset     = 0,
            .size       = 3 * sizeof(uint32_t)
        };
        pipelineLayout = vk::raii::PipelineLayout(vkCore.getDevice(), vk::PipelineLayoutCreateInfo{
            .setLayoutCount         = 1,
            .pSetLayouts            = &*descriptorSetLayout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges    = &pushRange
        });
        pipeline = vk::raii::Pipeline(vkCore.getDevice().createComputePipeline(nullptr,
            vk::ComputePipelineCreateInfo{
                .stage  = {.stage=vk::ShaderStageFlagBits::eCompute, .module=shaderModule, .pName="computeMain"},
                .layout = *pipelineLayout
            }));
    }

} // rt::gfx
