#include "SAHBuilder.h"
#include <algorithm>
#include <array>
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
                for (uint32_t b = 0;        b < s;        b++) { lA.expand(bins[b].b); lN += bins[b].cnt; }
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

void SAHBuilder::build(const std::vector<rt::Triangle>& tris,
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
        return;
    }

    std::vector<TriInfo> infos(n);
    outTriIndices.resize(n);
    for (uint32_t i = 0; i < n; i++) {
        outTriIndices[i]   = i;
        infos[i].bounds    = triAABB(i, tris, verts);
        infos[i].centroid  = infos[i].bounds.centroid();
    }

    outNodes.reserve(2 * n);
    outNodes.emplace_back();
    buildNode(0, 0, n, outNodes, outTriIndices, infos);
    printf("BVH [SAH]: %zu nodes, %zu tri refs\n", outNodes.size(), outTriIndices.size());
}

} // rt::gfx
