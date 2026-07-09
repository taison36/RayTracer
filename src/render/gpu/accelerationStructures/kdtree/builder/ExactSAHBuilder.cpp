#include "ExactSAHBuilder.h"
#include "../KDTree.h"
#include <algorithm>
#include <bit>
#include <limits>
#include <utility>

// Implementation follows the algorithm described in:
//   Havran, "Heuristic Ray Shooting Algorithms", PhD Thesis, 2000
//   Section 4.4 (empty-space cutting) and 4.2.3 (OSAH event sweep)
//
// Triangle splitting (Danilewski et al. 2010, Section 3.3):
// Straddling triangles are clipped against the split plane so each child gets
// a tight AABB enclosing only the fragment on its side, rather than the full
// triangle AABB. Per-instance bounds (TriInstance.bounds) propagate these
// tighter AABBs through the recursion, which also improves the event positions
// used in the OSAH sweep at deeper levels.

namespace rt::gfx {
    namespace {

        static constexpr uint32_t MAX_LEAF_TRIS = 16;
        static constexpr uint32_t MAX_DEPTH     = 50; // ESC cuts consume depth; 40 leaves room for both
        static constexpr float    C_TRAV        = 1.5f;
        static constexpr float    C_ISECT       = 1.0f;

        // ── AABB ─────────────────────────────────────────────────────────────────────
        struct AABB {
            glm::vec3 min{ std::numeric_limits<float>::max()};
            glm::vec3 max{-std::numeric_limits<float>::max()};

            void  expand(glm::vec3 p)    { min = glm::min(min, p); max = glm::max(max, p); }
            void  expand(const AABB& o)  { min = glm::min(min, o.min); max = glm::max(max, o.max); }
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

        // ── KD-Node encoding (must match kdtree.slang) ───────────────────────────────
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

        // Compute tight per-side AABBs for a triangle straddling the split plane.
        // Vertices are classified to sides; edges strictly crossing the plane are
        // intersected and the point added to both boxes. Results are clamped to the
        // child node bounds to enforce all previous spatial constraints.
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

        // ── Recursive builder ─────────────────────────────────────────────────────────
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

            // ── Phase 1: Empty Space Cutting (Havran §4.4) ───────────────────────────
            // Compute the tight union AABB from per-instance (clipped) bounds.
            // Any gap between it and nodeBounds is guaranteed-empty space worth cutting.
            AABB tightBounds;
            for (const auto& inst : instances) tightBounds.expand(inst.bounds);

            // A cut is only useful if it beats doing nothing (making a leaf here).
            const float noSplitCost = leafCost;

            int   bestEmptyAxis   = -1;
            float bestEmptyCost   = std::numeric_limits<float>::max();
            float bestEmptySplit  = 0.0f;
            bool  emptyIsLeft     = false;

            for (int axis = 0; axis < 3; axis++) {
                float lo = nodeBounds.min[axis];
                float hi = nodeBounds.max[axis];

                float nodeExtent = hi - lo;
                if (nodeExtent < 1e-7f) continue;

                if (tightBounds.min[axis] > lo) {
                    float sp            = tightBounds.min[axis];
                    float emptyFraction = (sp - lo) / nodeExtent;
                    if (emptyFraction > 0.2f) {
                        // emptyIsLeft=true: left child is empty, right has all N triangles.
                        AABB  lB   = nodeBounds; lB.max[axis] = sp;  // empty
                        AABB  rB   = nodeBounds; rB.min[axis] = sp;  // geometry
                        float cost = C_TRAV
                                   + (rB.surfaceArea() / parentSA) * static_cast<float>(N) * C_ISECT;
                        if (cost < noSplitCost && cost < bestEmptyCost) {
                            bestEmptyCost  = cost;
                            bestEmptyAxis  = axis;
                            bestEmptySplit = sp;
                            emptyIsLeft    = true;
                        }
                    }
                }

                if (tightBounds.max[axis] < hi) {
                    float sp            = tightBounds.max[axis];
                    float emptyFraction = (hi - sp) / nodeExtent;
                    if (emptyFraction > 0.2f) {
                        // emptyIsLeft=false: right child is empty, left has all N triangles.
                        AABB  lB   = nodeBounds; lB.max[axis] = sp;  // geometry
                        AABB  rB   = nodeBounds; rB.min[axis] = sp;  // empty
                        float cost = C_TRAV
                                   + (lB.surfaceArea() / parentSA) * static_cast<float>(N) * C_ISECT;
                        if (cost < noSplitCost && cost < bestEmptyCost) {
                            bestEmptyCost  = cost;
                            bestEmptyAxis  = axis;
                            bestEmptySplit = sp;
                            emptyIsLeft    = false;
                        }
                    }
                }
            }

            // ── Phase 2: OSAH event sweep (Havran §4.2.3) ───────────────────────────
            // Split candidates are the per-instance (clipped) AABB boundaries.
            // Using clipped bounds means events are at the actual fragment extents,
            // giving more accurate SAH cost estimates than raw triangle AABBs.
            int   bestSAHAxis  = -1;
            float bestSAHCost  = std::numeric_limits<float>::max();
            float bestSAHSplit = 0.0f;

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
                    float    pos  = events[i].pos;
                    uint32_t pEnd = 0, pStart = 0;
                    size_t   j    = i;

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
                        if (cost < bestSAHCost) {
                            bestSAHCost  = cost;
                            bestSAHAxis  = axis;
                            bestSAHSplit = pos;
                        }
                    }

                    NL += pStart;
                    i   = j;
                }
            }

            // ── Phase 3: Choose best strategy ────────────────────────────────────────
            float emptyCost = (bestEmptyAxis != -1) ? bestEmptyCost : std::numeric_limits<float>::max();
            float sahCost   = (bestSAHAxis   != -1) ? bestSAHCost   : std::numeric_limits<float>::max();

            if (leafCost <= std::min(emptyCost, sahCost)) { makeLeaf(); return; }

            if (emptyCost < sahCost) {
                // ── Empty space cut ───────────────────────────────────────────────────
                // No triangle straddles an empty-space cut: tightBounds.min[ax] is the
                // minimum of all inst.bounds.min[ax], so every instance is entirely on
                // the non-empty side. Pass instances through unchanged.
                int   ax = bestEmptyAxis;
                float sp = bestEmptySplit;

                float nodeExtent = nodeBounds.max[ax] - nodeBounds.min[ax];
                float frac2 = emptyIsLeft
                    ? (nodeBounds.max[ax] - tightBounds.max[ax]) / nodeExtent
                    : (tightBounds.min[ax] - nodeBounds.min[ax]) / nodeExtent;
                bool  secondCut = frac2 > 0.2f && (depth + 2 < MAX_DEPTH);
                float sp2 = emptyIsLeft ? tightBounds.max[ax] : tightBounds.min[ax];

                if (secondCut) {
                    float outerSplit = sp;
                    float innerSplit = sp2;

                    uint32_t outerLeft = static_cast<uint32_t>(nodes.size());
                    nodes.emplace_back();
                    nodes.emplace_back();
                    uint32_t innerLeft = static_cast<uint32_t>(nodes.size());
                    nodes.emplace_back();
                    nodes.emplace_back();

                    AABB outerLeftB  = nodeBounds; outerLeftB.max[ax]  = outerSplit;
                    AABB outerRightB = nodeBounds; outerRightB.min[ax] = outerSplit;

                    if (emptyIsLeft) {
                        AABB innerLeftB  = outerRightB; innerLeftB.max[ax]  = innerSplit;
                        AABB innerRightB = outerRightB; innerRightB.min[ax] = innerSplit;

                        makeInteriorNode(nodes[nodeIdx],       nodeBounds,  static_cast<uint32_t>(ax), outerSplit, outerLeft);
                        makeLeafNode    (nodes[outerLeft],     outerLeftB,  0, 0);
                        makeInteriorNode(nodes[outerLeft + 1], outerRightB, static_cast<uint32_t>(ax), innerSplit, innerLeft);
                        makeLeafNode    (nodes[innerLeft + 1], innerRightB, 0, 0);
                        buildNode(innerLeft, std::move(instances), innerLeftB,
                                  nodes, outTriIndices, tris, verts, depth + 2);
                    } else {
                        AABB innerLeftB  = outerLeftB; innerLeftB.max[ax]  = innerSplit;
                        AABB innerRightB = outerLeftB; innerRightB.min[ax] = innerSplit;

                        makeInteriorNode(nodes[nodeIdx],       nodeBounds,  static_cast<uint32_t>(ax), outerSplit, outerLeft);
                        makeInteriorNode(nodes[outerLeft],     outerLeftB,  static_cast<uint32_t>(ax), innerSplit, innerLeft);
                        makeLeafNode    (nodes[outerLeft + 1], outerRightB, 0, 0);
                        makeLeafNode    (nodes[innerLeft],     innerLeftB,  0, 0);
                        buildNode(innerLeft + 1, std::move(instances), innerRightB,
                                  nodes, outTriIndices, tris, verts, depth + 2);
                    }
                } else {
                    uint32_t leftIdx = static_cast<uint32_t>(nodes.size());
                    nodes.emplace_back();
                    nodes.emplace_back();
                    AABB leftBounds  = nodeBounds; leftBounds.max[ax]  = sp;
                    AABB rightBounds = nodeBounds; rightBounds.min[ax] = sp;

                    makeInteriorNode(nodes[nodeIdx], nodeBounds, static_cast<uint32_t>(ax), sp, leftIdx);
                    if (emptyIsLeft) {
                        makeLeafNode(nodes[leftIdx], leftBounds, 0, 0);
                        buildNode(leftIdx + 1, std::move(instances), rightBounds,
                                  nodes, outTriIndices, tris, verts, depth + 1);
                    } else {
                        makeLeafNode(nodes[leftIdx + 1], rightBounds, 0, 0);
                        buildNode(leftIdx, std::move(instances), leftBounds,
                                  nodes, outTriIndices, tris, verts, depth + 1);
                    }
                }
                return;
            }

            // ── SAH split: partition with triangle splitting ──────────────────────────
            AABB leftBounds  = nodeBounds; leftBounds.max[bestSAHAxis]  = bestSAHSplit;
            AABB rightBounds = nodeBounds; rightBounds.min[bestSAHAxis] = bestSAHSplit;

            std::vector<TriInstance> leftInsts, rightInsts;
            leftInsts.reserve(N); rightInsts.reserve(N);

            for (const auto& inst : instances) {
                const bool leftOnly  = inst.bounds.max[bestSAHAxis] <= bestSAHSplit;
                const bool rightOnly = inst.bounds.min[bestSAHAxis] >= bestSAHSplit;

                if (leftOnly) {
                    leftInsts.push_back(inst);
                } else if (rightOnly) {
                    rightInsts.push_back(inst);
                } else {
                    auto [lb, rb] = clipTriangle(inst.idx, tris, verts,
                                                 bestSAHAxis, bestSAHSplit,
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
                             static_cast<uint32_t>(bestSAHAxis), bestSAHSplit, leftIdx);

            buildNode(leftIdx,     std::move(leftInsts),  leftBounds,
                      nodes, outTriIndices, tris, verts, depth + 1);
            buildNode(leftIdx + 1, std::move(rightInsts), rightBounds,
                      nodes, outTriIndices, tris, verts, depth + 1);
        }

    } // anonymous namespace

    void ExactSAHBuilder::build(const std::vector<rt::Triangle>& tris,
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
        outNodes.emplace_back(); // root = index 0

        buildNode(0, std::move(instances), sceneBounds, outNodes, outTriIndices, tris, verts, 0);
    }

} // rt::gfx
