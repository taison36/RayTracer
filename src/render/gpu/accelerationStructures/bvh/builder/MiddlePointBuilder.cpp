#include "MiddlePointBuilder.h"
#include <algorithm>
#include <bit>
#include <cstdio>
#include <limits>

namespace rt::gfx {

namespace {
    struct AABB {
        glm::vec3 min{ std::numeric_limits<float>::max()};
        glm::vec3 max{-std::numeric_limits<float>::max()};

        void expand(glm::vec3 p)   { min = glm::min(min, p); max = glm::max(max, p); }
        void expand(const AABB& o) { min = glm::min(min, o.min); max = glm::max(max, o.max); }
        glm::vec3 centroid() const { return 0.5f * (min + max); }
    };

    struct TriInfo { AABB bounds; glm::vec3 centroid; };

    constexpr uint32_t MAX_LEAF_TRIS = 4;

    AABB triAABB(uint32_t idx,
                 const std::vector<rt::Triangle>& tris,
                 const std::vector<rt::Vertex>&   verts) {
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
                   std::vector<BVHNode>&       nodes,
                   std::vector<uint32_t>&      indices,
                   const std::vector<TriInfo>& infos) {
        AABB bounds;
        for (uint32_t i = first; i < first + count; i++)
            bounds.expand(infos[indices[i]].bounds);

        if (count <= MAX_LEAF_TRIS) {
            makeLeaf(nodes[nodeIdx], bounds, first, count);
            return;
        }

        AABB centBounds;
        for (uint32_t i = first; i < first + count; i++)
            centBounds.expand(infos[indices[i]].centroid);

        glm::vec3 extent = centBounds.max - centBounds.min;
        int axis = 0;
        if (extent.y > extent.x) axis = 1;
        if (extent.z > extent[axis]) axis = 2;

        uint32_t mid = first + count / 2;
        std::nth_element(indices.begin() + first,
                         indices.begin() + mid,
                         indices.begin() + first + count,
                         [&](uint32_t a, uint32_t b) {
                             return infos[a].centroid[axis] < infos[b].centroid[axis];
                         });

        uint32_t leftIdx = static_cast<uint32_t>(nodes.size());
        nodes.emplace_back();
        nodes.emplace_back();

        makeInternal(nodes[nodeIdx], bounds, leftIdx);
        buildNode(leftIdx,     first, mid - first,         nodes, indices, infos);
        buildNode(leftIdx + 1, mid,   first + count - mid, nodes, indices, infos);
    }
} // anonymous namespace

void MiddlePointBuilder::build(const std::vector<rt::Triangle>& tris,
                               const std::vector<rt::Vertex>&   verts,
                               std::vector<BVHNode>&            outNodes,
                               std::vector<uint32_t>&           outTriIndices) {
    uint32_t n = static_cast<uint32_t>(tris.size());

    if (n == 0) {
        BVHNode dummy{};
        float big = std::numeric_limits<float>::max();
        dummy.aabbMinLeft  = glm::vec4( big,  big,  big, std::bit_cast<float>(0u));
        dummy.aabbMaxCount = glm::vec4(-big, -big, -big, std::bit_cast<float>(1u));
        outNodes.push_back(dummy);
        outTriIndices.clear();
        return;
    }

    std::vector<TriInfo> infos(n);
    outTriIndices.resize(n);
    for (uint32_t i = 0; i < n; i++) {
        outTriIndices[i]  = i;
        infos[i].bounds   = triAABB(i, tris, verts);
        infos[i].centroid = infos[i].bounds.centroid();
    }

    outNodes.reserve(2 * n);
    outNodes.emplace_back();
    buildNode(0, 0, n, outNodes, outTriIndices, infos);
    printf("BVH [Midpoint]: %zu nodes, %zu tri refs\n", outNodes.size(), outTriIndices.size());
}

} // rt::gfx
