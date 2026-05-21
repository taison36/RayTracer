#include "KDTreeBuilder.h"
#include "KDTree.h"
#include <algorithm>
#include <array>
#include <bit>
#include <limits>

namespace rt::gfx {

    namespace {
        struct AABB {
            glm::vec3 min{ std::numeric_limits<float>::max()};
            glm::vec3 max{-std::numeric_limits<float>::max()};

            void expand(glm::vec3 p) { min = glm::min(min, p); max = glm::max(max, p); }
            void expand(const AABB& o) { min = glm::min(min, o.min); max = glm::max(max, o.max); }

            float surfaceArea() const {
                glm::vec3 d = glm::max(max - min, glm::vec3(0.0f));
                return 2.0f * (d.x*d.y + d.y*d.z + d.z*d.x);
            }
        };

        struct TriInfo {
            AABB      bounds;
            glm::vec3 centroid;
        };

        static constexpr uint32_t MAX_LEAF_TRIS = 16;
        static constexpr uint32_t NUM_BINS       = 8;
        static constexpr uint32_t MAX_DEPTH      = 20;
        static constexpr float    C_TRAV         = 1.5f;

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

        void makeKDLeaf(KDNode& node, const AABB& bounds, uint32_t firstPrim, uint32_t count) {
            node.data0 = glm::vec4(bounds.min, std::bit_cast<float>(firstPrim));
            uint32_t flags = (1u << 31) | (count & 0x1FFFFFFFu);
            node.data1 = glm::vec4(bounds.max, std::bit_cast<float>(flags));
        }

        void makeKDInterior(KDNode& node, const AABB& bounds,
                            uint32_t axis, float splitPos, uint32_t leftChild) {
            node.data0 = glm::vec4(bounds.min, splitPos);
            uint32_t flags = (axis << 29) | (leftChild & 0x1FFFFFFFu);
            node.data1 = glm::vec4(bounds.max, std::bit_cast<float>(flags));
        }

        void buildNode(uint32_t               nodeIdx,
                       std::vector<uint32_t>  triIndices,
                       AABB                   nodeBounds,
                       std::vector<KDNode>&   nodes,
                       std::vector<uint32_t>& outTriIndices,
                       const std::vector<TriInfo>& infos,
                       uint32_t depth)
        {
            uint32_t count = static_cast<uint32_t>(triIndices.size());

            auto makeLeaf = [&]() {
                uint32_t first = static_cast<uint32_t>(outTriIndices.size());
                outTriIndices.insert(outTriIndices.end(), triIndices.begin(), triIndices.end());
                makeKDLeaf(nodes[nodeIdx], nodeBounds, first, count);
            };

            if (count <= MAX_LEAF_TRIS || depth >= MAX_DEPTH) { makeLeaf(); return; }

            int   bestAxis  = -1;
            float bestCost  = std::numeric_limits<float>::max();
            float bestSplit = 0.0f;
            float parentSA  = nodeBounds.surfaceArea();

            for (int axis = 0; axis < 3; axis++) {
                float lo = nodeBounds.min[axis];
                float hi = nodeBounds.max[axis];
                if (hi - lo < 1e-6f) continue;

                struct BinCount { uint32_t enter = 0, exit = 0; };
                std::array<BinCount, NUM_BINS> bins{};
                float inv = static_cast<float>(NUM_BINS) / (hi - lo);

                for (uint32_t idx : triIndices) {
                    int enterBin = std::clamp(static_cast<int>((infos[idx].bounds.min[axis] - lo) * inv), 0, static_cast<int>(NUM_BINS)-1);
                    int exitBin  = std::clamp(static_cast<int>((infos[idx].bounds.max[axis] - lo) * inv), 0, static_cast<int>(NUM_BINS)-1);
                    bins[enterBin].enter++;
                    bins[exitBin ].exit++;
                }

                std::array<uint32_t, NUM_BINS> leftPrefix{}, rightSuffix{};
                leftPrefix[0] = 0;
                for (uint32_t s = 1; s < NUM_BINS; s++)
                    leftPrefix[s] = leftPrefix[s-1] + bins[s-1].enter;
                rightSuffix[NUM_BINS-1] = bins[NUM_BINS-1].exit;
                for (int s = static_cast<int>(NUM_BINS)-2; s >= 0; s--)
                    rightSuffix[s] = rightSuffix[s+1] + bins[s].exit;

                for (uint32_t s = 1; s < NUM_BINS; s++) {
                    uint32_t lCount = leftPrefix[s];
                    uint32_t rCount = rightSuffix[s];
                    if (lCount == 0 || rCount == 0) continue;

                    float splitPos = lo + (hi - lo) * static_cast<float>(s) / static_cast<float>(NUM_BINS);
                    AABB lB = nodeBounds; lB.max[axis] = splitPos;
                    AABB rB = nodeBounds; rB.min[axis] = splitPos;

                    float cost = C_TRAV + (lB.surfaceArea() * static_cast<float>(lCount) +
                                           rB.surfaceArea() * static_cast<float>(rCount)) / parentSA;
                    if (cost < bestCost) { bestCost = cost; bestAxis = axis; bestSplit = splitPos; }
                }
            }

            if (bestAxis == -1 || bestCost >= static_cast<float>(count)) { makeLeaf(); return; }

            std::vector<uint32_t> leftTris, rightTris;
            leftTris.reserve(count); rightTris.reserve(count);
            for (uint32_t idx : triIndices) {
                if (infos[idx].bounds.min[bestAxis] <  bestSplit) leftTris.push_back(idx);
                if (infos[idx].bounds.max[bestAxis] >= bestSplit) rightTris.push_back(idx);
            }

            if (leftTris.empty() || rightTris.empty()) { makeLeaf(); return; }

            uint32_t leftIdx = static_cast<uint32_t>(nodes.size());
            nodes.emplace_back();
            nodes.emplace_back();
            makeKDInterior(nodes[nodeIdx], nodeBounds, static_cast<uint32_t>(bestAxis), bestSplit, leftIdx);

            AABB leftBounds  = nodeBounds; leftBounds.max[bestAxis]  = bestSplit;
            AABB rightBounds = nodeBounds; rightBounds.min[bestAxis] = bestSplit;

            buildNode(leftIdx,     std::move(leftTris),  leftBounds,  nodes, outTriIndices, infos, depth+1);
            buildNode(leftIdx + 1, std::move(rightTris), rightBounds, nodes, outTriIndices, infos, depth+1);
        }
    } // anonymous namespace

    void BinnedSAHBuilder::build(const std::vector<rt::Triangle>& tris,
                                 const std::vector<rt::Vertex>&   verts,
                                 std::vector<KDNode>&             outNodes,
                                 std::vector<uint32_t>&           outTriIndices)
    {
        uint32_t n = static_cast<uint32_t>(tris.size());

        if (n == 0) {
            KDNode dummy{};
            float big = std::numeric_limits<float>::max();
            dummy.data0 = glm::vec4(big, big, big, std::bit_cast<float>(0u));
            dummy.data1 = glm::vec4(-big, -big, -big, std::bit_cast<float>(1u << 31));
            outNodes.push_back(dummy);
            return;
        }

        std::vector<TriInfo> infos(n);
        std::vector<uint32_t> allIndices(n);
        AABB sceneBounds;
        for (uint32_t i = 0; i < n; i++) {
            allIndices[i]    = i;
            infos[i].bounds  = triAABB(i, tris, verts);
            infos[i].centroid = 0.5f * (infos[i].bounds.min + infos[i].bounds.max);
            sceneBounds.expand(infos[i].bounds);
        }

        outNodes.reserve(2 * n);
        outNodes.emplace_back(); // root = index 0
        buildNode(0, std::move(allIndices), sceneBounds, outNodes, outTriIndices, infos, 0);
    }

} // rt::gfx
