#include "KDTree.h"
#include "../../utils/utils.h"
#include "vulkan/vulkan.hpp"
#include <cstring>
#include <cstdio>

namespace rt::gfx {

    void KDTree::buildKDTree(const std::vector<rt::Triangle>& tris,
                             const std::vector<rt::Vertex>&   verts) {
        builder->build(tris, verts, kdNodeData, kdTriIndexData);
        float dupRatio = static_cast<float>(kdTriIndexData.size()) / static_cast<float>(tris.size());
        printf("KD-Tree [%s]: %zu nodes, %zu tri refs, %.2fx duplication\n",
               builder->getName().c_str(), kdNodeData.size(), kdTriIndexData.size(), dupRatio);
    }

    // ── Vulkan setup (mirrors BVH.cpp) ───────────────────────────────────────────

    void KDTree::build(const VkCore &vkCore, const RendererContext &context,
                       const OutputImage &outputImage) {
        sceneSettings = extractSceneSettings(context);
        buildKDTree(context.scene->triangles, context.scene->vertices);
        createBuffers(vkCore, context);
        fillBuffers(vkCore, context);
        fillTextures(vkCore, context);
        createDescriptorPool(vkCore, context);
        createDescriptorLayouts(vkCore, context, outputImage);
        writeStaticDescriptorSets(vkCore, context, outputImage);
        writeBindlessDescriptorSets(vkCore, context);
        createPipeline(vkCore);
    }

    void KDTree::record(const vk::CommandBuffer &cmb,
                        const uint32_t tileWidth, const uint32_t tileHeight,
                        const uint32_t sampleIndex,
                        const uint32_t tileOffsetX, const uint32_t tileOffsetY) {
        struct PushConstants {
            uint32_t sampleIndex, tileOffsetX, tileOffsetY;
        };
        PushConstants pc{sampleIndex, tileOffsetX, tileOffsetY};

        cmb.bindPipeline(vk::PipelineBindPoint::eCompute, *pipeline);
        cmb.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                               pipelineLayout, 0, {*descriptorSets.front()}, nullptr);
        cmb.pushConstants(*pipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(pc), &pc);
        cmb.dispatch((tileWidth + 15) / 16,
                     (tileHeight + 15) / 16,
                     1);
    }

    const std::string KDTree::getTypeName() const {
        return "KD-Tree [" + builder->getName() + "]";
    }

    KDSceneSettings KDTree::extractSceneSettings(const RendererContext &context) const {
        KDCameraData cam = {
            .viewInverse = glm::inverse(context.scene->camera.getView()),
            .projInverse = glm::inverse(context.scene->camera.getProjection(
                context.screenSettings->IMAGE_ASPECT_RATIO)),
            .position = context.scene->camera.getPosition()
        };
        return KDSceneSettings{
            .cameraData = cam,
            .vertexCount = static_cast<uint32_t>(context.scene->vertices.size()),
            .triangleCount = static_cast<uint32_t>(context.scene->triangles.size()),
            .emissiveLightCount = static_cast<uint32_t>(context.scene->emissiveLight.size()),
            .directionalLightCount = static_cast<uint32_t>(context.scene->directionalLight.size()),
            .pointLightCount = static_cast<uint32_t>(context.scene->pointLight.size()),
            .spotLightCount = static_cast<uint32_t>(context.scene->spotLight.size()),
            .maxBounces              = context.screenSettings->maxBounces,
            .samplesPerPixel         = context.screenSettings->samplesPerPixel,
            .samplesPerEmissiveLight = context.screenSettings->samplesPerEmissiveLight
        };
    }

    KDTextureImage KDTree::extractTextureImage(const VkCore &vkCore, const Texture &texture) const {
        KDTextureImage ti;
        vk::ImageCreateInfo imageInfo{
            .imageType = vk::ImageType::e2D,
            .format = vk::Format::eR8G8B8A8Srgb,
            .extent.width = texture.width,
            .extent.height = texture.height,
            .extent.depth = 1,
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = vk::SampleCountFlagBits::e1,
            .tiling = vk::ImageTiling::eOptimal,
            .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
            .sharingMode = vk::SharingMode::eExclusive,
            .initialLayout = vk::ImageLayout::eUndefined
        };
        ti.image = vk::raii::Image(vkCore.getDevice(), imageInfo);
        auto mem = ti.image.getMemoryRequirements();
        ti.memory = vk::raii::DeviceMemory(vkCore.getDevice(), vk::MemoryAllocateInfo{
                                               .allocationSize = mem.size,
                                               .memoryTypeIndex = vkCore.findMemoryType(mem.memoryTypeBits,
                                                   vk::MemoryPropertyFlagBits::eDeviceLocal)
                                           });
        ti.image.bindMemory(*ti.memory, 0);
        ti.view = vk::raii::ImageView(vkCore.getDevice(), vk::ImageViewCreateInfo{
                                          .image = *ti.image,
                                          .viewType = vk::ImageViewType::e2D,
                                          .format = vk::Format::eR8G8B8A8Srgb,
                                          .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
                                      });

        vk::SamplerCreateInfo si{};
        auto mapFilter = [](Filter f) {
            return f == Filter::LINEAR ? vk::Filter::eLinear : vk::Filter::eNearest;
        };
        auto mapMip = [](Filter f) {
            return f == Filter::LINEAR ? vk::SamplerMipmapMode::eLinear : vk::SamplerMipmapMode::eNearest;
        };
        auto mapWrap = [](WrapMode w) {
            switch (w) {
                case WrapMode::CLAMP_TO_EDGE: return vk::SamplerAddressMode::eClampToEdge;
                case WrapMode::MIRRORED_REPEAT: return vk::SamplerAddressMode::eMirroredRepeat;
                default: return vk::SamplerAddressMode::eRepeat;
            }
        };
        si.magFilter = mapFilter(texture.magFilter);
        si.minFilter = mapFilter(texture.minFilter);
        si.mipmapMode = mapMip(texture.mipmapMode);
        si.addressModeU = mapWrap(texture.wrapU);
        si.addressModeV = mapWrap(texture.wrapV);
        si.anisotropyEnable = vk::True;
        si.maxAnisotropy = vkCore.getPhysicalDevice().getProperties().limits.maxSamplerAnisotropy;
        si.compareOp = vk::CompareOp::eAlways;
        ti.sampler = vk::raii::Sampler(vkCore.getDevice(), si);
        return ti;
    }

    void KDTree::createBuffers(const VkCore &vkCore, const RendererContext &context) {
        constexpr auto kStorage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst;
        constexpr auto kUniform = vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst;
        constexpr auto kDev = vk::MemoryPropertyFlagBits::eDeviceLocal;

        auto mk = [&](vk::DeviceSize size, vk::BufferUsageFlags usage, KDBuffer &out) {
            vkCore.createBuffer(size, usage, kDev, out.buffer, out.memory);
        };
        auto sz = [](const auto &v, size_t elem) { return std::max(v.size() * elem, elem); };

        const auto &s = *context.scene;
        mk(sz(s.vertices, sizeof(Vertex)), kStorage, vertices);
        mk(sz(s.triangles, sizeof(Triangle)), kStorage, triangles);
        mk(sz(s.materials, sizeof(Material)), kStorage, materials);
        mk(sz(s.emissiveLight, sizeof(uint32_t)), kStorage, emissiveLight);
        mk(sz(s.directionalLight, sizeof(DirectionalLight)), kStorage, directionalLight);
        mk(sz(s.pointLight, sizeof(PointLight)), kStorage, pointLight);
        mk(sz(s.spotLight, sizeof(SpotLight)), kStorage, spotLight);
        mk(sizeof(KDSceneSettings), kUniform, sceneSettingsUBO);

        vkCore.createBuffer(
            context.screenSettings->WIDTH * context.screenSettings->HEIGHT * sizeof(float) * 4,
            vk::BufferUsageFlagBits::eStorageBuffer, kDev,
            accumBuffer.buffer, accumBuffer.memory);

        mk(std::max(kdNodeData.size() * sizeof(KDNode), sizeof(KDNode)), kStorage, kdNodes);
        mk(std::max(kdTriIndexData.size() * sizeof(uint32_t), sizeof(uint32_t)), kStorage, kdTriIndices);
    }

    void KDTree::fillBuffers(const VkCore &vkCore, const RendererContext &context) {
        vk::raii::Buffer poolBuffer({});
        vk::raii::DeviceMemory poolMemory({});

        const auto &s = *context.scene;
        uint32_t verSize = align(s.vertices.size() * sizeof(Vertex), 4);
        uint32_t triSize = align(s.triangles.size() * sizeof(Triangle), 4);
        uint32_t matSize = align(s.materials.size() * sizeof(Material), 4);
        uint32_t emSize = align(s.emissiveLight.size() * sizeof(uint32_t), 4);
        uint32_t dirSize = align(s.directionalLight.size() * sizeof(DirectionalLight), 4);
        uint32_t ptSize = align(s.pointLight.size() * sizeof(PointLight), 4);
        uint32_t spSize = align(s.spotLight.size() * sizeof(SpotLight), 4);
        uint32_t sceneSize = align(sizeof(KDSceneSettings), 4);
        uint32_t nodeSize = align(kdNodeData.size() * sizeof(KDNode), 4);
        uint32_t idxSize = align(kdTriIndexData.size() * sizeof(uint32_t), 4);
        uint32_t total = verSize + triSize + matSize + emSize + dirSize +
                         ptSize + spSize + sceneSize + nodeSize + idxSize;

        vkCore.createBuffer(total,
                            vk::BufferUsageFlagBits::eTransferSrc,
                            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                            poolBuffer, poolMemory);

        void *mapped = poolMemory.mapMemory(0, total);
        auto cmd = vkCore.beginSingleTimeCommands();
        uint32_t off = 0;

        auto upload = [&](const void *data, uint32_t size, KDBuffer &dst) {
            if (size == 0) return;
            memcpy(static_cast<char *>(mapped) + off, data, size);
            vkCore.fillBuffer(poolBuffer, dst.buffer, size, off, *cmd);
            off += size;
        };

        upload(s.vertices.data(), verSize, vertices);
        upload(s.triangles.data(), triSize, triangles);
        upload(s.materials.data(), matSize, materials);
        upload(s.emissiveLight.data(), emSize, emissiveLight);
        upload(s.directionalLight.data(), dirSize, directionalLight);
        upload(s.pointLight.data(), ptSize, pointLight);
        upload(s.spotLight.data(), spSize, spotLight);
        upload(&sceneSettings, sceneSize, sceneSettingsUBO);
        upload(kdNodeData.data(), nodeSize, kdNodes);
        upload(kdTriIndexData.data(), idxSize, kdTriIndices);

        poolMemory.unmapMemory();
        vkCore.endSingleTimeCommands(*cmd);
    }

    void KDTree::fillTextures(const VkCore &vkCore, const RendererContext &context) {
        vk::raii::Buffer poolBuffer({});
        vk::raii::DeviceMemory poolMemory({});
        uint32_t totalSize = 0;
        auto cmd = vkCore.beginSingleTimeCommands();

        for (const Texture &tex: context.scene->textures) {
            textures.push_back(extractTextureImage(vkCore, tex));
            vkCore.transitionImageLayout(textures.back().image,
                                         vk::ImageLayout::eUndefined,
                                         vk::ImageLayout::eTransferDstOptimal, *cmd);
            totalSize += align(tex.width * tex.height * 4, 4);
        }
        if (totalSize == 0) return;

        vkCore.createBuffer(totalSize,
                            vk::BufferUsageFlagBits::eTransferSrc,
                            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                            poolBuffer, poolMemory);

        void *mapped = poolMemory.mapMemory(0, totalSize);
        uint32_t off = 0;
        for (size_t i = 0; i < context.scene->textures.size(); i++) {
            const auto &tex = context.scene->textures[i];
            uint32_t size = tex.width * tex.height * 4;
            memcpy(static_cast<char *>(mapped) + off, tex.data.data(), size);
            vkCore.copyBufferToImage(textures[i].image, poolBuffer, tex.height, tex.width, off, *cmd);
            off += align(size, 4);
        }
        poolMemory.unmapMemory();

        for (auto &t: textures)
            vkCore.transitionImageLayout(t.image,
                                         vk::ImageLayout::eTransferDstOptimal,
                                         vk::ImageLayout::eShaderReadOnlyOptimal, *cmd);
        vkCore.endSingleTimeCommands(*cmd);
    }

    void KDTree::createDescriptorPool(const VkCore &vkCore, const RendererContext &context) {
        (void) context;
        std::vector<vk::DescriptorPoolSize> sizes = {
            vk::DescriptorPoolSize(vk::DescriptorType::eStorageImage, 1),
            vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, 10),
            vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 1),
            vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, MAX_TEXTURE_NUMBER)
        };
        descriptorPool = vk::raii::DescriptorPool(vkCore.getDevice(), vk::DescriptorPoolCreateInfo{
                                                      .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet |
                                                               vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind,
                                                      .maxSets = 1,
                                                      .poolSizeCount = static_cast<uint32_t>(sizes.size()),
                                                      .pPoolSizes = sizes.data()
                                                  });
    }

    void KDTree::createDescriptorLayouts(const VkCore &vkCore, const RendererContext &context,
                                         const OutputImage &outputImage) {
        (void) outputImage;
        using B = vk::DescriptorSetLayoutBinding;
        using T = vk::DescriptorType;
        using S = vk::ShaderStageFlagBits;

        std::vector<B> bindings = {
            B{.binding = 0, .descriptorType = T::eStorageImage, .descriptorCount = 1, .stageFlags = S::eCompute},
            B{.binding = 1, .descriptorType = T::eStorageBuffer, .descriptorCount = 1, .stageFlags = S::eCompute},
            B{.binding = 2, .descriptorType = T::eStorageBuffer, .descriptorCount = 1, .stageFlags = S::eCompute},
            B{.binding = 3, .descriptorType = T::eStorageBuffer, .descriptorCount = 1, .stageFlags = S::eCompute},
            B{.binding = 4, .descriptorType = T::eStorageBuffer, .descriptorCount = 1, .stageFlags = S::eCompute},
            B{.binding = 5, .descriptorType = T::eStorageBuffer, .descriptorCount = 1, .stageFlags = S::eCompute},
            B{.binding = 6, .descriptorType = T::eStorageBuffer, .descriptorCount = 1, .stageFlags = S::eCompute},
            B{.binding = 7, .descriptorType = T::eStorageBuffer, .descriptorCount = 1, .stageFlags = S::eCompute},
            B{.binding = 8, .descriptorType = T::eUniformBuffer, .descriptorCount = 1, .stageFlags = S::eCompute},
            B{.binding = 9, .descriptorType = T::eStorageBuffer, .descriptorCount = 1, .stageFlags = S::eCompute},
            B{.binding = 10, .descriptorType = T::eStorageBuffer, .descriptorCount = 1, .stageFlags = S::eCompute},
            // kdNodes
            B{.binding = 11, .descriptorType = T::eStorageBuffer, .descriptorCount = 1, .stageFlags = S::eCompute},
            // kdTriIndices
            B{
                .binding = 12, .descriptorType = T::eCombinedImageSampler, .descriptorCount = MAX_TEXTURE_NUMBER,
                .stageFlags = S::eCompute
            }, // textures (last)
        };

        std::vector<vk::DescriptorBindingFlags> flags(bindings.size());
        flags[12] = vk::DescriptorBindingFlagBits::eUpdateAfterBind |
                    vk::DescriptorBindingFlagBits::ePartiallyBound |
                    vk::DescriptorBindingFlagBits::eVariableDescriptorCount;

        vk::DescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{
            .pBindingFlags = flags.data(),
            .bindingCount = static_cast<uint32_t>(flags.size())
        };
        descriptorSetLayout = vk::raii::DescriptorSetLayout(vkCore.getDevice(),
                                                            vk::DescriptorSetLayoutCreateInfo{
                                                                .pNext = &flagsInfo,
                                                                .flags =
                                                                vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool,
                                                                .bindingCount = static_cast<uint32_t>(bindings.size()),
                                                                .pBindings = bindings.data()
                                                            });

        uint32_t texCount = static_cast<uint32_t>(context.scene->textures.size());
        vk::DescriptorSetVariableDescriptorCountAllocateInfo varCount{
            .descriptorSetCount = 1, .pDescriptorCounts = &texCount
        };
        descriptorSets = vkCore.getDevice().allocateDescriptorSets(vk::DescriptorSetAllocateInfo{
            .pNext = &varCount,
            .descriptorPool = descriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &*descriptorSetLayout
        });
    }

    void KDTree::writeStaticDescriptorSets(const VkCore &vkCore, const RendererContext &context,
                                           const OutputImage &outputImage) {
        (void) context;
        vk::DescriptorImageInfo imgInfo{
            .sampler = {},
            .imageView = outputImage.view,
            .imageLayout = vk::ImageLayout::eGeneral
        };

        auto bi = [](const KDBuffer &b) {
            return vk::DescriptorBufferInfo{.buffer = b.buffer, .offset = 0, .range = VK_WHOLE_SIZE};
        };

        vk::DescriptorBufferInfo vBI = bi(vertices), tBI = bi(triangles),
                mBI = bi(materials), eBI = bi(emissiveLight),
                dBI = bi(directionalLight), pBI = bi(pointLight),
                sBI = bi(spotLight), ssBI = bi(sceneSettingsUBO),
                aBI = bi(accumBuffer), nBI = bi(kdNodes),
                iBI = bi(kdTriIndices);

        vk::DescriptorSet ds = descriptorSets.front();
        auto w = [&](uint32_t binding, vk::DescriptorType type, const vk::DescriptorBufferInfo *b) {
            return vk::WriteDescriptorSet{
                .dstSet = ds, .dstBinding = binding, .dstArrayElement = 0,
                .descriptorCount = 1, .descriptorType = type, .pBufferInfo = b
            };
        };

        std::vector<vk::WriteDescriptorSet> writes{
            vk::WriteDescriptorSet{
                .dstSet = ds, .dstBinding = 0, .dstArrayElement = 0, .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eStorageImage, .pImageInfo = &imgInfo
            },
            w(1, vk::DescriptorType::eStorageBuffer, &vBI),
            w(2, vk::DescriptorType::eStorageBuffer, &tBI),
            w(3, vk::DescriptorType::eStorageBuffer, &mBI),
            w(4, vk::DescriptorType::eStorageBuffer, &eBI),
            w(5, vk::DescriptorType::eStorageBuffer, &dBI),
            w(6, vk::DescriptorType::eStorageBuffer, &pBI),
            w(7, vk::DescriptorType::eStorageBuffer, &sBI),
            w(8, vk::DescriptorType::eUniformBuffer, &ssBI),
            w(9, vk::DescriptorType::eStorageBuffer, &aBI),
            w(10, vk::DescriptorType::eStorageBuffer, &nBI),
            w(11, vk::DescriptorType::eStorageBuffer, &iBI),
        };
        vkCore.getDevice().updateDescriptorSets(writes, {});
    }

    void KDTree::writeBindlessDescriptorSets(const VkCore &vkCore, const RendererContext &context) const {
        (void) context;
        std::vector<vk::DescriptorImageInfo> texInfos;
        texInfos.reserve(textures.size());
        for (const KDTextureImage &t: textures)
            texInfos.push_back({
                .sampler = t.sampler, .imageView = t.view,
                .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
            });
        if (texInfos.empty()) return;

        vkCore.getDevice().updateDescriptorSets(vk::WriteDescriptorSet{
                                                    .dstSet = descriptorSets.front(),
                                                    .dstBinding = 12,
                                                    .dstArrayElement = 0,
                                                    .descriptorCount = static_cast<uint32_t>(texInfos.size()),
                                                    .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                                                    .pImageInfo = texInfos.data()
                                                }, {});
    }

    void KDTree::createPipeline(const VkCore &vkCore) {
        vk::raii::ShaderModule shader = vkCore.createShaderModule(readFile("shaders/kdtree.spv"));
        vk::PushConstantRange push{
            .stageFlags = vk::ShaderStageFlagBits::eCompute,
            .offset = 0,
            .size = 3 * sizeof(uint32_t)
        };
        pipelineLayout = vk::raii::PipelineLayout(vkCore.getDevice(), vk::PipelineLayoutCreateInfo{
                                                      .setLayoutCount = 1,
                                                      .pSetLayouts = &*descriptorSetLayout,
                                                      .pushConstantRangeCount = 1,
                                                      .pPushConstantRanges = &push
                                                  });
        pipeline = vk::raii::Pipeline(vkCore.getDevice().createComputePipeline(nullptr,
                                                                               vk::ComputePipelineCreateInfo{
                                                                                   .stage = {
                                                                                       .stage =
                                                                                       vk::ShaderStageFlagBits::eCompute,
                                                                                       .module = shader,
                                                                                       .pName = "computeMain"
                                                                                   },
                                                                                   .layout = *pipelineLayout
                                                                               }));
    }
} // rt::gfx
