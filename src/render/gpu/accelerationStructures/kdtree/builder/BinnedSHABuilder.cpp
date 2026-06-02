#include "BinnedSHABuilder.h"
#include "../KDTree.h"
#include <algorithm>
#include <array>
#include <bit>
#include <limits>
#include <utility>

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

        // Per-instance triangle: carries the effective (possibly clipped) AABB
        // for this subtree rather than the original full triangle AABB.
        struct TriInstance {
            uint32_t idx;
            AABB     bounds;
        };

        static constexpr uint32_t MAX_LEAF_TRIS = 16;
        static constexpr uint32_t NUM_BINS       = 33;
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

        // Compute tight per-side AABBs for a triangle straddling the split plane.
        //
        // For each edge crossing the plane we compute the intersection point and add
        // it to both sides; vertices are assigned to the side they sit on.
        // The resulting boxes are clamped to the child node bounds to account for
        // tightening done by previous splits higher in the tree.
        //
        // Based on Section 3.3 "Triangle-Splitter" of Danilewski et al. 2010.
        static std::pair<AABB, AABB> clipTriangle(
            uint32_t triIdx,
            const std::vector<rt::Triangle>& tris,
            const std::vector<rt::Vertex>&   verts,
            int axis, float splitPos,
            const AABB& leftChildBounds,
            const AABB& rightChildBounds)
        {
            const auto& tri = tris[triIdx];
            const glm::vec3 v[3] = {
                glm::vec3(verts[tri.indices[0]].position),
                glm::vec3(verts[tri.indices[1]].position),
                glm::vec3(verts[tri.indices[2]].position)
            };

            AABB leftAABB, rightAABB;

            for (int i = 0; i < 3; i++) {
                const float coord = v[i][axis];
                // Vertices on or to the left of the plane go into the left box;
                // vertices on or to the right go into the right box.
                if (coord <= splitPos) leftAABB.expand(v[i]);
                if (coord >= splitPos) rightAABB.expand(v[i]);

                // For edges that strictly cross the plane, compute the intersection
                // point and add it to both bounding boxes.
                const int j = (i + 1) % 3;
                const float ci = v[i][axis], cj = v[j][axis];
                if ((ci < splitPos) != (cj < splitPos)) {
                    const float t = (splitPos - ci) / (cj - ci);
                    const glm::vec3 p = v[i] + t * (v[j] - v[i]);
                    leftAABB.expand(p);
                    rightAABB.expand(p);
                }
            }

            // Clamp to child node bounds — triangles may have been clipped in prior
            // splits, so the child bounds can be tighter than the split plane alone.
            leftAABB.min  = glm::max(leftAABB.min,  leftChildBounds.min);
            leftAABB.max  = glm::min(leftAABB.max,  leftChildBounds.max);
            rightAABB.min = glm::max(rightAABB.min, rightChildBounds.min);
            rightAABB.max = glm::min(rightAABB.max, rightChildBounds.max);

            return {leftAABB, rightAABB};
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

        void buildNode(uint32_t                          nodeIdx,
                       std::vector<TriInstance>          instances,
                       AABB                              nodeBounds,
                       std::vector<KDNode>&              nodes,
                       std::vector<uint32_t>&            outTriIndices,
                       const std::vector<rt::Triangle>&  tris,
                       const std::vector<rt::Vertex>&    verts,
                       uint32_t depth)
        {
            uint32_t N = static_cast<uint32_t>(instances.size());

            auto makeLeaf = [&]() {
                uint32_t first = static_cast<uint32_t>(outTriIndices.size());
                for (const auto& inst : instances)
                    outTriIndices.push_back(inst.idx);
                makeKDLeaf(nodes[nodeIdx], nodeBounds, first, N);
            };

            if (N <= MAX_LEAF_TRIS || depth >= MAX_DEPTH) { makeLeaf(); return; }

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

                for (const auto& inst : instances) {
                    int enterBin = std::clamp(static_cast<int>((inst.bounds.min[axis] - lo) * inv), 0, static_cast<int>(NUM_BINS)-1);
                    int exitBin  = std::clamp(static_cast<int>((inst.bounds.max[axis] - lo) * inv), 0, static_cast<int>(NUM_BINS)-1);
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

            if (bestAxis == -1 || bestCost >= static_cast<float>(N)) { makeLeaf(); return; }

            AABB leftChildBounds  = nodeBounds; leftChildBounds.max[bestAxis]  = bestSplit;
            AABB rightChildBounds = nodeBounds; rightChildBounds.min[bestAxis] = bestSplit;

            std::vector<TriInstance> leftInsts, rightInsts;
            leftInsts.reserve(N);
            rightInsts.reserve(N);

            for (const auto& inst : instances) {
                const bool goLeft  = inst.bounds.min[bestAxis] <  bestSplit;
                const bool goRight = inst.bounds.max[bestAxis] >= bestSplit;

                if (goLeft && goRight) {
                    // Triangle straddles the plane: compute tight AABBs for each
                    // fragment by clipping against the split plane and child bounds.
                    auto [lb, rb] = clipTriangle(inst.idx, tris, verts,
                                                 bestAxis, bestSplit,
                                                 leftChildBounds, rightChildBounds);
                    leftInsts.push_back({inst.idx, lb});
                    rightInsts.push_back({inst.idx, rb});
                } else if (goLeft) {
                    leftInsts.push_back(inst);
                } else {
                    rightInsts.push_back(inst);
                }
            }

            if (leftInsts.empty() || rightInsts.empty()) { makeLeaf(); return; }

            uint32_t leftIdx = static_cast<uint32_t>(nodes.size());
            nodes.emplace_back();
            nodes.emplace_back();
            makeKDInterior(nodes[nodeIdx], nodeBounds, static_cast<uint32_t>(bestAxis), bestSplit, leftIdx);

            buildNode(leftIdx,     std::move(leftInsts),  leftChildBounds,  nodes, outTriIndices, tris, verts, depth+1);
            buildNode(leftIdx + 1, std::move(rightInsts), rightChildBounds, nodes, outTriIndices, tris, verts, depth+1);
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

        std::vector<TriInstance> instances(n);
        AABB sceneBounds;
        for (uint32_t i = 0; i < n; i++) {
            instances[i].idx    = i;
            instances[i].bounds = triAABB(i, tris, verts);
            sceneBounds.expand(instances[i].bounds);
        }

        outNodes.reserve(2 * n);
        outNodes.emplace_back(); // root = index 0
        buildNode(0, std::move(instances), sceneBounds, outNodes, outTriIndices, tris, verts, 0);
    }

} // rt::gfx
