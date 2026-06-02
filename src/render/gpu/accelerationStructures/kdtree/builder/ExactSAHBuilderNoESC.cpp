#include "ExactSAHBuilderNoESC.h"
#include "../KDTree.h"
#include <algorithm>
#include <bit>
#include <limits>
#include <utility>

// Exact SAH KD-Tree builder without empty-space cutting.
// Implements OSAH event sweep (Havran §4.2.3) and spatial triangle splitting
// (Danilewski et al. 2010 §3.3), but skips Phase 1 (Havran §4.4).
// Use alongside ExactSAHBuilder to isolate the rendering cost of empty-space cuts.

namespace rt::gfx {
    namespace {

        static constexpr uint32_t MAX_LEAF_TRIS = 16;
        static constexpr uint32_t MAX_DEPTH     = 20;
        static constexpr float    C_TRAV        = 1.5f;
        static constexpr float    C_ISECT       = 1.0f;

        struct AABB {
            glm::vec3 min{ std::numeric_limits<float>::max()};
            glm::vec3 max{-std::numeric_limits<float>::max()};

            void  expand(glm::vec3 p)   { min = glm::min(min, p); max = glm::max(max, p); }
            void  expand(const AABB& o) { min = glm::min(min, o.min); max = glm::max(max, o.max); }
            float surfaceArea() const {
                glm::vec3 d = glm::max(max - min, glm::vec3(0.0f));
                return 2.0f * (d.x*d.y + d.y*d.z + d.z*d.x);
            }
        };

        struct TriInstance {
            uint32_t idx;
            AABB     bounds;
        };

        void makeLeafNode(KDNode& node, const AABB& bounds, uint32_t firstPrim, uint32_t count) {
            node.data0 = glm::vec4(bounds.min, std::bit_cast<float>(firstPrim));
            uint32_t flags = (1u << 31) | (count & 0x1FFFFFFFu);
            node.data1 = glm::vec4(bounds.max, std::bit_cast<float>(flags));
        }

        void makeInteriorNode(KDNode& node, const AABB& bounds,
                              uint32_t axis, float splitPos, uint32_t leftChild) {
            node.data0 = glm::vec4(bounds.min, splitPos);
            uint32_t flags = (axis << 29) | (leftChild & 0x1FFFFFFFu);
            node.data1 = glm::vec4(bounds.max, std::bit_cast<float>(flags));
        }

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
                if (coord <= splitPos) leftAABB.expand(v[i]);
                if (coord >= splitPos) rightAABB.expand(v[i]);

                const int j = (i + 1) % 3;
                const float ci = v[i][axis], cj = v[j][axis];
                if ((ci < splitPos) != (cj < splitPos)) {
                    const float t = (splitPos - ci) / (cj - ci);
                    const glm::vec3 p = v[i] + t * (v[j] - v[i]);
                    leftAABB.expand(p);
                    rightAABB.expand(p);
                }
            }

            leftAABB.min  = glm::max(leftAABB.min,  leftChildBounds.min);
            leftAABB.max  = glm::min(leftAABB.max,  leftChildBounds.max);
            rightAABB.min = glm::max(rightAABB.min, rightChildBounds.min);
            rightAABB.max = glm::min(rightAABB.max, rightChildBounds.max);

            return {leftAABB, rightAABB};
        }

        void buildNode(
            uint32_t                          nodeIdx,
            std::vector<TriInstance>          instances,
            AABB                              nodeBounds,
            std::vector<KDNode>&              nodes,
            std::vector<uint32_t>&            outTriIndices,
            const std::vector<rt::Triangle>&  tris,
            const std::vector<rt::Vertex>&    verts,
            uint32_t depth)
        {
            uint32_t N        = static_cast<uint32_t>(instances.size());
            float    parentSA = nodeBounds.surfaceArea();
            float    leafCost = static_cast<float>(N) * C_ISECT;

            auto makeLeaf = [&]() {
                uint32_t first = static_cast<uint32_t>(outTriIndices.size());
                for (const auto& inst : instances)
                    outTriIndices.push_back(inst.idx);
                makeLeafNode(nodes[nodeIdx], nodeBounds, first, N);
            };

            if (N <= MAX_LEAF_TRIS || depth >= MAX_DEPTH) { makeLeaf(); return; }

            // ── OSAH event sweep (Havran §4.2.3) ────────────────────────────────────
            int   bestAxis  = -1;
            float bestCost  = std::numeric_limits<float>::max();
            float bestSplit = 0.0f;

            enum class Ev : uint8_t { END = 0, START = 1 };
            struct Event { float pos; Ev type; };
            std::vector<Event> events;
            events.reserve(N * 2);

            for (int axis = 0; axis < 3; axis++) {
                float lo = nodeBounds.min[axis];
                float hi = nodeBounds.max[axis];
                if (hi - lo < 1e-7f) continue;

                events.clear();
                for (const auto& inst : instances) {
                    float eMin = std::clamp(inst.bounds.min[axis], lo, hi);
                    float eMax = std::clamp(inst.bounds.max[axis], lo, hi);
                    events.push_back({eMin, Ev::START});
                    events.push_back({eMax, Ev::END});
                }

                std::sort(events.begin(), events.end(), [](const Event& a, const Event& b) {
                    if (a.pos != b.pos) return a.pos < b.pos;
                    return static_cast<uint8_t>(a.type) < static_cast<uint8_t>(b.type);
                });

                uint32_t NL = 0, NR = N;
                size_t   i  = 0;

                while (i < events.size()) {
                    float    pos   = events[i].pos;
                    uint32_t pEnd  = 0, pStart = 0;
                    size_t   j     = i;

                    while (j < events.size() && events[j].pos == pos) {
                        if (events[j].type == Ev::END) pEnd++;
                        else                           pStart++;
                        j++;
                    }

                    NR -= pEnd;

                    if (pos > lo && pos < hi && NL > 0 && NR > 0) {
                        AABB  lB   = nodeBounds; lB.max[axis] = pos;
                        AABB  rB   = nodeBounds; rB.min[axis] = pos;
                        float cost = C_TRAV + C_ISECT * (lB.surfaceArea() * static_cast<float>(NL) +
                                                         rB.surfaceArea() * static_cast<float>(NR)) / parentSA;
                        if (cost < bestCost) {
                            bestCost  = cost;
                            bestAxis  = axis;
                            bestSplit = pos;
                        }
                    }

                    NL += pStart;
                    i   = j;
                }
            }

            // ── Choose: leaf or SAH split ────────────────────────────────────────────
            if (bestAxis == -1 || leafCost <= bestCost) { makeLeaf(); return; }

            AABB leftBounds  = nodeBounds; leftBounds.max[bestAxis]  = bestSplit;
            AABB rightBounds = nodeBounds; rightBounds.min[bestAxis] = bestSplit;

            std::vector<TriInstance> leftInsts, rightInsts;
            leftInsts.reserve(N); rightInsts.reserve(N);

            for (const auto& inst : instances) {
                const bool leftOnly  = inst.bounds.max[bestAxis] <= bestSplit;
                const bool rightOnly = inst.bounds.min[bestAxis] >= bestSplit;

                if (leftOnly) {
                    leftInsts.push_back(inst);
                } else if (rightOnly) {
                    rightInsts.push_back(inst);
                } else {
                    auto [lb, rb] = clipTriangle(inst.idx, tris, verts,
                                                 bestAxis, bestSplit,
                                                 leftBounds, rightBounds);
                    leftInsts.push_back({inst.idx, lb});
                    rightInsts.push_back({inst.idx, rb});
                }
            }

            if (leftInsts.empty() || rightInsts.empty()) { makeLeaf(); return; }

            uint32_t leftIdx = static_cast<uint32_t>(nodes.size());
            nodes.emplace_back();
            nodes.emplace_back();
            makeInteriorNode(nodes[nodeIdx], nodeBounds,
                             static_cast<uint32_t>(bestAxis), bestSplit, leftIdx);

            buildNode(leftIdx,     std::move(leftInsts),  leftBounds,
                      nodes, outTriIndices, tris, verts, depth + 1);
            buildNode(leftIdx + 1, std::move(rightInsts), rightBounds,
                      nodes, outTriIndices, tris, verts, depth + 1);
        }

    } // anonymous namespace

    void ExactSAHBuilderNoESC::build(const std::vector<rt::Triangle>& tris,
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
            instances[i].idx = i;
            instances[i].bounds.expand(glm::vec3(verts[tris[i].indices[0]].position));
            instances[i].bounds.expand(glm::vec3(verts[tris[i].indices[1]].position));
            instances[i].bounds.expand(glm::vec3(verts[tris[i].indices[2]].position));
            sceneBounds.expand(instances[i].bounds);
        }

        outNodes.reserve(2 * n);
        outNodes.emplace_back();

        buildNode(0, std::move(instances), sceneBounds, outNodes, outTriIndices, tris, verts, 0);
    }

} // rt::gfx
