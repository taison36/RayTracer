#include "SAHBVH.h"
#include "../../utils/utils.h"
#include "vulkan/vulkan.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <limits>

namespace rt::gfx {

namespace {

struct AABB {
    glm::vec3 min{ std::numeric_limits<float>::max()};
    glm::vec3 max{-std::numeric_limits<float>::max()};

    void expand(glm::vec3 p)   { min = glm::min(min, p); max = glm::max(max, p); }
    void expand(const AABB& o) { min = glm::min(min, o.min); max = glm::max(max, o.max); }

    float surfaceArea() const {
        glm::vec3 d = glm::max(max - min, glm::vec3(0.0f));
        return 2.0f * (d.x*d.y + d.y*d.z + d.z*d.x);
    }
    glm::vec3 centroid() const { return 0.5f * (min + max); }
};

struct TriInfo { AABB bounds; glm::vec3 centroid; };

constexpr uint32_t MAX_LEAF_TRIS = 4;
constexpr uint32_t NUM_BINS      = 8;
constexpr float    C_TRAV        = 1.2f;

AABB triAABB(uint32_t idx, const std::vector<rt::Triangle>& tris, const std::vector<rt::Vertex>& verts) {
    const auto& tri = tris[idx];
    AABB b;
    b.expand(glm::vec3(verts[tri.indices[0]].position));
    b.expand(glm::vec3(verts[tri.indices[1]].position));
    b.expand(glm::vec3(verts[tri.indices[2]].position));
    return b;
}

void makeLeaf(BVHNode& node, const AABB& bounds, uint32_t first, uint32_t count) {
    node.aabbMinLeft  = glm::vec4(bounds.min, std::bit_cast<float>(first));
    node.aabbMaxCount = glm::vec4(bounds.max, std::bit_cast<float>(count));
}

void makeInternal(BVHNode& node, const AABB& bounds, uint32_t leftChild) {
    node.aabbMinLeft  = glm::vec4(bounds.min, std::bit_cast<float>(leftChild));
    node.aabbMaxCount = glm::vec4(bounds.max, std::bit_cast<float>(0u));
}

void buildNode(uint32_t nodeIdx, uint32_t first, uint32_t count,
               std::vector<BVHNode>& nodes, std::vector<uint32_t>& indices,
               const std::vector<TriInfo>& infos) {
    AABB bounds;
    for (uint32_t i = first; i < first + count; i++)
        bounds.expand(infos[indices[i]].bounds);

    if (count <= MAX_LEAF_TRIS) {
        makeLeaf(nodes[nodeIdx], bounds, first, count);
        return;
    }

    // Binned SAH over all three axes
    AABB centBounds;
    for (uint32_t i = first; i < first + count; i++)
        centBounds.expand(infos[indices[i]].centroid);

    int   bestAxis  = -1;
    float bestCost  = std::numeric_limits<float>::max();
    float bestSplit = 0.0f;
    float parentSA  = bounds.surfaceArea();

    for (int axis = 0; axis < 3; axis++) {
        float lo = centBounds.min[axis];
        float hi = centBounds.max[axis];
        if (hi - lo < 1e-6f) continue;

        struct Bin { AABB b; uint32_t cnt = 0; };
        std::array<Bin, NUM_BINS> bins;

        float inv = static_cast<float>(NUM_BINS) / (hi - lo);
        for (uint32_t i = first; i < first + count; i++) {
            int b = std::min(static_cast<int>((infos[indices[i]].centroid[axis] - lo) * inv),
                             static_cast<int>(NUM_BINS) - 1);
            bins[b].b.expand(infos[indices[i]].bounds);
            bins[b].cnt++;
        }

        for (uint32_t s = 1; s < NUM_BINS; s++) {
            AABB lA, rA; uint32_t lN = 0, rN = 0;
            for (uint32_t b = 0; b < s;        b++) { lA.expand(bins[b].b); lN += bins[b].cnt; }
            for (uint32_t b = s; b < NUM_BINS; b++) { rA.expand(bins[b].b); rN += bins[b].cnt; }
            if (lN == 0 || rN == 0) continue;

            float cost = C_TRAV + (lA.surfaceArea()*lN + rA.surfaceArea()*rN) / parentSA;
            if (cost < bestCost) {
                bestCost  = cost;
                bestAxis  = axis;
                bestSplit = lo + (hi - lo) * static_cast<float>(s) / static_cast<float>(NUM_BINS);
            }
        }
    }

    if (bestAxis == -1 || bestCost >= static_cast<float>(count)) {
        makeLeaf(nodes[nodeIdx], bounds, first, count);
        return;
    }

    uint32_t mid = first, r = first + count;
    while (mid < r) {
        if (infos[indices[mid]].centroid[bestAxis] < bestSplit) ++mid;
        else std::swap(indices[mid], indices[--r]);
    }

    if (mid == first || mid == first + count) {
        makeLeaf(nodes[nodeIdx], bounds, first, count);
        return;
    }

    uint32_t leftIdx = static_cast<uint32_t>(nodes.size());
    nodes.emplace_back();
    nodes.emplace_back();
    makeInternal(nodes[nodeIdx], bounds, leftIdx);

    buildNode(leftIdx,     first, mid - first,         nodes, indices, infos);
    buildNode(leftIdx + 1, mid,   first + count - mid, nodes, indices, infos);
}

} // anonymous namespace

void SAHBVH::buildBVH(const std::vector<rt::Triangle>& tris, const std::vector<rt::Vertex>& verts) {
    uint32_t n = static_cast<uint32_t>(tris.size());

    if (n == 0) {
        BVHNode dummy{};
        float big = std::numeric_limits<float>::max();
        dummy.aabbMinLeft  = glm::vec4( big,  big,  big, std::bit_cast<float>(0u));
        dummy.aabbMaxCount = glm::vec4(-big, -big, -big, std::bit_cast<float>(1u));
        bvhNodeData.push_back(dummy);
        return;
    }

    std::vector<TriInfo> infos(n);
    bvhTriIndexData.resize(n);
    for (uint32_t i = 0; i < n; i++) {
        bvhTriIndexData[i] = i;
        infos[i].bounds    = triAABB(i, tris, verts);
        infos[i].centroid  = infos[i].bounds.centroid();
    }

    bvhNodeData.reserve(2 * n);
    bvhNodeData.emplace_back();
    buildNode(0, 0, n, bvhNodeData, bvhTriIndexData, infos);
}

// ── Vulkan setup ──────────────────────────────────────────────────────────────

void SAHBVH::build(const VkCore& vkCore, const RendererContext& context, const OutputImage& outputImage) {
    sceneSettings = extractSceneSettings(context);
    buildBVH(context.scene->triangles, context.scene->vertices);
    createBuffers(vkCore, context);
    fillBuffers(vkCore, context);
    fillTextures(vkCore, context);
    createDescriptorPool(vkCore, context);
    createDescriptorLayouts(vkCore, context, outputImage);
    writeStaticDescriptorSets(vkCore, context, outputImage);
    writeBindlessDescriptorSets(vkCore, context);
    createPipeline(vkCore);
}

void SAHBVH::record(const vk::CommandBuffer& cmb,
                    const uint32_t tileWidth, const uint32_t tileHeight,
                    const uint32_t sampleIndex,
                    const uint32_t tileOffsetX, const uint32_t tileOffsetY) {
    struct PushConstants { uint32_t sampleIndex, tileOffsetX, tileOffsetY; };
    PushConstants pc{ sampleIndex, tileOffsetX, tileOffsetY };

    cmb.bindPipeline(vk::PipelineBindPoint::eCompute, *pipeline);
    cmb.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipelineLayout, 0,
                           {*descriptorSets.front()}, nullptr);
    cmb.pushConstants(*pipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(pc), &pc);
    cmb.dispatch((tileWidth + 15) / 16, (tileHeight + 15) / 16, 1);
}

BvhSceneSettings SAHBVH::extractSceneSettings(const RendererContext& context) const {
    BvhCameraData cam = {
        .viewInverse = glm::inverse(context.scene->camera.getView()),
        .projInverse = glm::inverse(context.scene->camera.getProjection(context.screenSettings->IMAGE_ASPECT_RATIO)),
        .position    = context.scene->camera.getPosition()
    };
    return BvhSceneSettings{
        .cameraData              = cam,
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

BvhTextureImage SAHBVH::extractTextureImage(const VkCore& vkCore, const Texture& texture) const {
    BvhTextureImage ti;
    vk::ImageCreateInfo imageInfo{
        .imageType = vk::ImageType::e2D, .format = vk::Format::eR8G8B8A8Unorm,
        .extent.width = texture.width, .extent.height = texture.height, .extent.depth = 1,
        .mipLevels = 1, .arrayLayers = 1, .samples = vk::SampleCountFlagBits::e1,
        .tiling = vk::ImageTiling::eOptimal,
        .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
        .sharingMode = vk::SharingMode::eExclusive, .initialLayout = vk::ImageLayout::eUndefined
    };
    ti.image = vk::raii::Image(vkCore.getDevice(), imageInfo);
    auto mem = ti.image.getMemoryRequirements();
    ti.memory = vk::raii::DeviceMemory(vkCore.getDevice(), vk::MemoryAllocateInfo{
        .allocationSize  = mem.size,
        .memoryTypeIndex = vkCore.findMemoryType(mem.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal)
    });
    ti.image.bindMemory(*ti.memory, 0);
    ti.view = vk::raii::ImageView(vkCore.getDevice(), vk::ImageViewCreateInfo{
        .image = *ti.image, .viewType = vk::ImageViewType::e2D, .format = vk::Format::eR8G8B8A8Unorm,
        .subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
    });

    vk::SamplerCreateInfo si{};
    switch (texture.magFilter)  { case Filter::NEAREST: si.magFilter  = vk::Filter::eNearest; break; case Filter::LINEAR: si.magFilter  = vk::Filter::eLinear;  break; }
    switch (texture.minFilter)  { case Filter::NEAREST: si.minFilter  = vk::Filter::eNearest; break; case Filter::LINEAR: si.minFilter  = vk::Filter::eLinear;  break; }
    switch (texture.mipmapMode) { case Filter::NEAREST: si.mipmapMode = vk::SamplerMipmapMode::eNearest; break; case Filter::LINEAR: si.mipmapMode = vk::SamplerMipmapMode::eLinear; break; }
    switch (texture.wrapU) {
        case WrapMode::CLAMP_TO_EDGE:   si.addressModeU = vk::SamplerAddressMode::eClampToEdge;   break;
        case WrapMode::REPEAT:          si.addressModeU = vk::SamplerAddressMode::eRepeat;         break;
        case WrapMode::MIRRORED_REPEAT: si.addressModeU = vk::SamplerAddressMode::eMirroredRepeat; break;
    }
    switch (texture.wrapV) {
        case WrapMode::CLAMP_TO_EDGE:   si.addressModeV = vk::SamplerAddressMode::eClampToEdge;   break;
        case WrapMode::REPEAT:          si.addressModeV = vk::SamplerAddressMode::eRepeat;         break;
        case WrapMode::MIRRORED_REPEAT: si.addressModeV = vk::SamplerAddressMode::eMirroredRepeat; break;
    }
    si.anisotropyEnable = vk::True;
    si.maxAnisotropy    = vkCore.getPhysicalDevice().getProperties().limits.maxSamplerAnisotropy;
    si.compareOp        = vk::CompareOp::eAlways;
    ti.sampler = vk::raii::Sampler(vkCore.getDevice(), si);
    return ti;
}

void SAHBVH::createBuffers(const VkCore& vkCore, const RendererContext& context) {
    constexpr auto kS = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst;
    constexpr auto kU = vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst;
    constexpr auto kD = vk::MemoryPropertyFlagBits::eDeviceLocal;
    auto mk = [&](vk::DeviceSize sz, vk::BufferUsageFlags u, BvhBuffer& b) {
        vkCore.createBuffer(sz, u, kD, b.buffer, b.memory);
    };
    auto sz = [](const auto& v, size_t e) { return std::max(v.size() * e, e); };
    const auto& s = *context.scene;
    mk(sz(s.vertices,         sizeof(Vertex)),          kS, vertices);
    mk(sz(s.triangles,        sizeof(Triangle)),        kS, triangles);
    mk(sz(s.materials,        sizeof(Material)),        kS, materials);
    mk(sz(s.emissiveLight,    sizeof(uint32_t)),        kS, emissiveLight);
    mk(sz(s.directionalLight, sizeof(DirectionalLight)),kS, directionalLight);
    mk(sz(s.pointLight,       sizeof(PointLight)),      kS, pointLight);
    mk(sz(s.spotLight,        sizeof(SpotLight)),       kS, spotLight);
    mk(sizeof(BvhSceneSettings),                        kU, sceneSettingsUBO);
    vkCore.createBuffer(context.screenSettings->WIDTH * context.screenSettings->HEIGHT * sizeof(float) * 4,
                        vk::BufferUsageFlagBits::eStorageBuffer, kD, accumBuffer.buffer, accumBuffer.memory);
    mk(std::max(bvhNodeData.size()     * sizeof(BVHNode),   sizeof(BVHNode)),   kS, bvhNodes);
    mk(std::max(bvhTriIndexData.size() * sizeof(uint32_t),  sizeof(uint32_t)),  kS, bvhTriIndices);
}

void SAHBVH::fillBuffers(const VkCore& vkCore, const RendererContext& context) {
    vk::raii::Buffer poolBuffer({}); vk::raii::DeviceMemory poolMemory({});
    const auto& s = *context.scene;
    uint32_t verSz  = align(s.vertices.size()          * sizeof(Vertex),            4);
    uint32_t triSz  = align(s.triangles.size()         * sizeof(Triangle),          4);
    uint32_t matSz  = align(s.materials.size()         * sizeof(Material),          4);
    uint32_t emSz   = align(s.emissiveLight.size()     * sizeof(uint32_t),          4);
    uint32_t dirSz  = align(s.directionalLight.size()  * sizeof(DirectionalLight),  4);
    uint32_t ptSz   = align(s.pointLight.size()        * sizeof(PointLight),        4);
    uint32_t spSz   = align(s.spotLight.size()         * sizeof(SpotLight),         4);
    uint32_t scSz   = align(sizeof(BvhSceneSettings),                               4);
    uint32_t nodeSz = align(bvhNodeData.size()         * sizeof(BVHNode),           4);
    uint32_t idxSz  = align(bvhTriIndexData.size()     * sizeof(uint32_t),          4);
    uint32_t total  = verSz+triSz+matSz+emSz+dirSz+ptSz+spSz+scSz+nodeSz+idxSz;

    vkCore.createBuffer(total, vk::BufferUsageFlagBits::eTransferSrc,
                        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                        poolBuffer, poolMemory);
    void* mapped = poolMemory.mapMemory(0, total);
    auto  cmd    = vkCore.beginSingleTimeCommands();
    uint32_t off = 0;
    auto up = [&](const void* data, uint32_t sz, BvhBuffer& dst) {
        if (sz == 0) return;
        memcpy(static_cast<char*>(mapped) + off, data, sz);
        vkCore.fillBuffer(poolBuffer, dst.buffer, sz, off, *cmd);
        off += sz;
    };
    up(s.vertices.data(),         verSz,  vertices);
    up(s.triangles.data(),        triSz,  triangles);
    up(s.materials.data(),        matSz,  materials);
    up(s.emissiveLight.data(),    emSz,   emissiveLight);
    up(s.directionalLight.data(), dirSz,  directionalLight);
    up(s.pointLight.data(),       ptSz,   pointLight);
    up(s.spotLight.data(),        spSz,   spotLight);
    up(&sceneSettings,            scSz,   sceneSettingsUBO);
    up(bvhNodeData.data(),        nodeSz, bvhNodes);
    up(bvhTriIndexData.data(),    idxSz,  bvhTriIndices);
    poolMemory.unmapMemory();
    vkCore.endSingleTimeCommands(*cmd);
}

void SAHBVH::fillTextures(const VkCore& vkCore, const RendererContext& context) {
    vk::raii::Buffer poolBuffer({}); vk::raii::DeviceMemory poolMemory({});
    uint32_t total = 0;
    auto cmd = vkCore.beginSingleTimeCommands();
    for (const Texture& tex : context.scene->textures) {
        textures.push_back(extractTextureImage(vkCore, tex));
        vkCore.transitionImageLayout(textures.back().image, vk::ImageLayout::eUndefined,
                                     vk::ImageLayout::eTransferDstOptimal, *cmd);
        total += align(tex.width * tex.height * 4, 4);
    }
    if (total == 0) return;
    vkCore.createBuffer(total, vk::BufferUsageFlagBits::eTransferSrc,
                        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                        poolBuffer, poolMemory);
    void* mapped = poolMemory.mapMemory(0, total);
    uint32_t off = 0;
    for (size_t i = 0; i < context.scene->textures.size(); i++) {
        const auto& tex = context.scene->textures[i];
        uint32_t sz = tex.width * tex.height * 4;
        memcpy(static_cast<char*>(mapped) + off, tex.data.data(), sz);
        vkCore.copyBufferToImage(textures[i].image, poolBuffer, tex.height, tex.width, off, *cmd);
        off += align(sz, 4);
    }
    poolMemory.unmapMemory();
    for (auto& t : textures)
        vkCore.transitionImageLayout(t.image, vk::ImageLayout::eTransferDstOptimal,
                                     vk::ImageLayout::eShaderReadOnlyOptimal, *cmd);
    vkCore.endSingleTimeCommands(*cmd);
}

void SAHBVH::createDescriptorPool(const VkCore& vkCore, const RendererContext& context) {
    (void)context;
    std::vector<vk::DescriptorPoolSize> sizes = {
        { vk::DescriptorType::eStorageImage,          1 },
        { vk::DescriptorType::eStorageBuffer,        10 },
        { vk::DescriptorType::eUniformBuffer,         1 },
        { vk::DescriptorType::eCombinedImageSampler, MAX_TEXTURE_NUMBER }
    };
    descriptorPool = vk::raii::DescriptorPool(vkCore.getDevice(), vk::DescriptorPoolCreateInfo{
        .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet |
                 vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind,
        .maxSets = 1,
        .poolSizeCount = static_cast<uint32_t>(sizes.size()), .pPoolSizes = sizes.data()
    });
}

void SAHBVH::createDescriptorLayouts(const VkCore& vkCore, const RendererContext& context,
                                      const OutputImage& outputImage) {
    (void)outputImage;
    using B = vk::DescriptorSetLayoutBinding; using T = vk::DescriptorType; using S = vk::ShaderStageFlagBits;
    std::vector<B> bindings = {
        B{.binding=0,  .descriptorType=T::eStorageImage,         .descriptorCount=1,                .stageFlags=S::eCompute},
        B{.binding=1,  .descriptorType=T::eStorageBuffer,        .descriptorCount=1,                .stageFlags=S::eCompute},
        B{.binding=2,  .descriptorType=T::eStorageBuffer,        .descriptorCount=1,                .stageFlags=S::eCompute},
        B{.binding=3,  .descriptorType=T::eStorageBuffer,        .descriptorCount=1,                .stageFlags=S::eCompute},
        B{.binding=4,  .descriptorType=T::eStorageBuffer,        .descriptorCount=1,                .stageFlags=S::eCompute},
        B{.binding=5,  .descriptorType=T::eStorageBuffer,        .descriptorCount=1,                .stageFlags=S::eCompute},
        B{.binding=6,  .descriptorType=T::eStorageBuffer,        .descriptorCount=1,                .stageFlags=S::eCompute},
        B{.binding=7,  .descriptorType=T::eStorageBuffer,        .descriptorCount=1,                .stageFlags=S::eCompute},
        B{.binding=8,  .descriptorType=T::eUniformBuffer,        .descriptorCount=1,                .stageFlags=S::eCompute},
        B{.binding=9,  .descriptorType=T::eStorageBuffer,        .descriptorCount=1,                .stageFlags=S::eCompute},
        B{.binding=10, .descriptorType=T::eStorageBuffer,        .descriptorCount=1,                .stageFlags=S::eCompute},
        B{.binding=11, .descriptorType=T::eStorageBuffer,        .descriptorCount=1,                .stageFlags=S::eCompute},
        B{.binding=12, .descriptorType=T::eCombinedImageSampler, .descriptorCount=MAX_TEXTURE_NUMBER,.stageFlags=S::eCompute},
    };
    std::vector<vk::DescriptorBindingFlags> flags(bindings.size());
    flags[12] = vk::DescriptorBindingFlagBits::eUpdateAfterBind |
                vk::DescriptorBindingFlagBits::ePartiallyBound  |
                vk::DescriptorBindingFlagBits::eVariableDescriptorCount;
    vk::DescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{ .pBindingFlags=flags.data(), .bindingCount=static_cast<uint32_t>(flags.size()) };
    descriptorSetLayout = vk::raii::DescriptorSetLayout(vkCore.getDevice(), vk::DescriptorSetLayoutCreateInfo{
        .pNext=&flagsInfo, .flags=vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool,
        .bindingCount=static_cast<uint32_t>(bindings.size()), .pBindings=bindings.data()
    });
    uint32_t texCount = static_cast<uint32_t>(context.scene->textures.size());
    vk::DescriptorSetVariableDescriptorCountAllocateInfo varCount{ .descriptorSetCount=1, .pDescriptorCounts=&texCount };
    descriptorSets = vkCore.getDevice().allocateDescriptorSets(vk::DescriptorSetAllocateInfo{
        .pNext=&varCount, .descriptorPool=descriptorPool, .descriptorSetCount=1, .pSetLayouts=&*descriptorSetLayout
    });
}

void SAHBVH::writeStaticDescriptorSets(const VkCore& vkCore, const RendererContext& context,
                                        const OutputImage& outputImage) {
    (void)context;
    vk::DescriptorImageInfo imgInfo{ .sampler={}, .imageView=outputImage.view, .imageLayout=vk::ImageLayout::eGeneral };
    auto bi = [](const BvhBuffer& b) { return vk::DescriptorBufferInfo{.buffer=b.buffer,.offset=0,.range=VK_WHOLE_SIZE}; };
    vk::DescriptorBufferInfo vBI=bi(vertices), tBI=bi(triangles), mBI=bi(materials), eBI=bi(emissiveLight),
                             dBI=bi(directionalLight), pBI=bi(pointLight), sBI=bi(spotLight),
                             ssBI=bi(sceneSettingsUBO), aBI=bi(accumBuffer), nBI=bi(bvhNodes), iBI=bi(bvhTriIndices);
    vk::DescriptorSet ds = descriptorSets.front();
    auto w = [&](uint32_t b, vk::DescriptorType t, const vk::DescriptorBufferInfo* bi) {
        return vk::WriteDescriptorSet{.dstSet=ds,.dstBinding=b,.dstArrayElement=0,.descriptorCount=1,.descriptorType=t,.pBufferInfo=bi};
    };
    std::vector<vk::WriteDescriptorSet> writes{
        vk::WriteDescriptorSet{.dstSet=ds,.dstBinding=0,.dstArrayElement=0,.descriptorCount=1,
                               .descriptorType=vk::DescriptorType::eStorageImage,.pImageInfo=&imgInfo},
        w(1,vk::DescriptorType::eStorageBuffer,&vBI),  w(2,vk::DescriptorType::eStorageBuffer,&tBI),
        w(3,vk::DescriptorType::eStorageBuffer,&mBI),  w(4,vk::DescriptorType::eStorageBuffer,&eBI),
        w(5,vk::DescriptorType::eStorageBuffer,&dBI),  w(6,vk::DescriptorType::eStorageBuffer,&pBI),
        w(7,vk::DescriptorType::eStorageBuffer,&sBI),  w(8,vk::DescriptorType::eUniformBuffer,&ssBI),
        w(9,vk::DescriptorType::eStorageBuffer,&aBI),  w(10,vk::DescriptorType::eStorageBuffer,&nBI),
        w(11,vk::DescriptorType::eStorageBuffer,&iBI),
    };
    vkCore.getDevice().updateDescriptorSets(writes, {});
}

void SAHBVH::writeBindlessDescriptorSets(const VkCore& vkCore, const RendererContext& context) const {
    (void)context;
    std::vector<vk::DescriptorImageInfo> texInfos;
    texInfos.reserve(textures.size());
    for (const BvhTextureImage& t : textures)
        texInfos.push_back({.sampler=t.sampler,.imageView=t.view,.imageLayout=vk::ImageLayout::eShaderReadOnlyOptimal});
    if (texInfos.empty()) return;
    vkCore.getDevice().updateDescriptorSets(vk::WriteDescriptorSet{
        .dstSet=descriptorSets.front(),.dstBinding=12,.dstArrayElement=0,
        .descriptorCount=static_cast<uint32_t>(texInfos.size()),
        .descriptorType=vk::DescriptorType::eCombinedImageSampler,.pImageInfo=texInfos.data()
    }, {});
}

void SAHBVH::createPipeline(const VkCore& vkCore) {
    vk::raii::ShaderModule shader = vkCore.createShaderModule(readFile("shaders/sahbvh.spv"));
    vk::PushConstantRange push{ .stageFlags=vk::ShaderStageFlagBits::eCompute,.offset=0,.size=3*sizeof(uint32_t) };
    pipelineLayout = vk::raii::PipelineLayout(vkCore.getDevice(), vk::PipelineLayoutCreateInfo{
        .setLayoutCount=1,.pSetLayouts=&*descriptorSetLayout,.pushConstantRangeCount=1,.pPushConstantRanges=&push
    });
    pipeline = vk::raii::Pipeline(vkCore.getDevice().createComputePipeline(nullptr, vk::ComputePipelineCreateInfo{
        .stage={.stage=vk::ShaderStageFlagBits::eCompute,.module=shader,.pName="computeMain"},
        .layout=*pipelineLayout
    }));
}

} // rt::gfx
