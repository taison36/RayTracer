#include "ExactSAHBuilder.h"
#include "KDTree.h"
#include <algorithm>
#include <bit>
#include <limits>

// Implementation follows the algorithm described in:
//   Havran, "Heuristic Ray Shooting Algorithms", PhD Thesis, 2000
//   Section 4.4 (empty-space cutting) and 4.2.3 (OSAH event sweep)
//
// Key design: OSAH without triangle clipping.
// Raw triangle AABBs are used throughout — no Sutherland-Hodgman polygon clipping.
// "Exact" means split candidates are the actual triangle AABB boundaries (2N per axis),
// not a fixed bin grid, so the true SAH minimum is found.

namespace rt::gfx {
    namespace {

        static constexpr uint32_t MAX_LEAF_TRIS = 16;
        static constexpr uint32_t MAX_DEPTH     = 20;
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

        // ── Recursive builder ─────────────────────────────────────────────────────────
        void buildNode(
            uint32_t               nodeIdx,
            std::vector<uint32_t>  triIndices,
            AABB                   nodeBounds,
            std::vector<KDNode>&   nodes,
            std::vector<uint32_t>& outTriIndices,
            const std::vector<AABB>& triAABBs,
            uint32_t depth)
        {
            uint32_t N        = static_cast<uint32_t>(triIndices.size());
            float    parentSA = nodeBounds.surfaceArea();
            float    leafCost = static_cast<float>(N) * C_ISECT;

            auto emitLeaf = [&]() {
                uint32_t first = static_cast<uint32_t>(outTriIndices.size());
                outTriIndices.insert(outTriIndices.end(), triIndices.begin(), triIndices.end());
                makeLeafNode(nodes[nodeIdx], nodeBounds, first, N);
            };

            if (N <= MAX_LEAF_TRIS || depth >= MAX_DEPTH) { emitLeaf(); return; }

            // Cost-based: if leaf cost is within epsilon of best possible split, stop early
            if (leafCost < C_TRAV * 2.0f) { emitLeaf(); return; }

            // ── Phase 1: Empty Space Cutting (Havran §4.4) ───────────────────────────
            // Compute the tight union AABB of all triangles currently in this node.
            // Any gap between it and nodeBounds is guaranteed-empty space worth cutting.
            AABB tightBounds;
            for (uint32_t idx : triIndices) tightBounds.expand(triAABBs[idx]);

            int   bestEmptyAxis   = -1;
            float bestEmptyCost   = std::numeric_limits<float>::max();
            float bestEmptySplit  = 0.0f;
            bool  emptyIsLeft     = false; // true → left child is the empty leaf

            for (int axis = 0; axis < 3; axis++) {
                float lo = nodeBounds.min[axis];
                float hi = nodeBounds.max[axis];

                float nodeExtent = hi - lo;
                if (nodeExtent < 1e-7f) continue;

                // Left empty space: all triangles start to the right of lo
                if (tightBounds.min[axis] > lo) {
                    float sp            = tightBounds.min[axis];
                    float emptyFraction = (sp - lo) / nodeExtent;
                    if (emptyFraction > 0.2f) {
                        AABB  rB   = nodeBounds; rB.min[axis] = sp;
                        float cost = C_TRAV + (rB.surfaceArea() / parentSA) * static_cast<float>(N) * C_ISECT;
                        if (cost < bestEmptyCost) {
                            bestEmptyCost  = cost;
                            bestEmptyAxis  = axis;
                            bestEmptySplit = sp;
                            emptyIsLeft    = true;
                        }
                    }
                }

                // Right empty space: all triangles end before hi
                if (tightBounds.max[axis] < hi) {
                    float sp            = tightBounds.max[axis];
                    float emptyFraction = (hi - sp) / nodeExtent;
                    if (emptyFraction > 0.2f) {
                        AABB  lB   = nodeBounds; lB.max[axis] = sp;
                        float cost = C_TRAV + (lB.surfaceArea() / parentSA) * static_cast<float>(N) * C_ISECT;
                        if (cost < bestEmptyCost) {
                            bestEmptyCost  = cost;
                            bestEmptyAxis  = axis;
                            bestEmptySplit = sp;
                            emptyIsLeft    = false;
                        }
                    }
                }
            }

            // ── Phase 2: OSAH event sweep (Havran §4.2.3) ───────────────────────────
            // Split candidates = actual triangle AABB boundaries (2N per axis).
            // Events are clamped to [lo, hi] so splits are always inside the node.
            // Sort order: END before START at the same position (Havran tie-breaking).
            int   bestSAHAxis  = -1;
            float bestSAHCost  = std::numeric_limits<float>::max();
            float bestSAHSplit = 0.0f;

            enum class Ev : uint8_t { END = 0, START = 1 }; // END < START for sort
            struct Event { float pos; Ev type; };
            std::vector<Event> events;
            events.reserve(N * 2);

            for (int axis = 0; axis < 3; axis++) {
                float lo = nodeBounds.min[axis];
                float hi = nodeBounds.max[axis];
                if (hi - lo < 1e-7f) continue;

                events.clear();
                for (uint32_t idx : triIndices) {
                    // Clamp to [lo, hi]: triangle AABBs can extend outside the node
                    // (they were included because they overlap the node, not because
                    // they are contained in it). Clamping keeps split positions valid.
                    float eMin = std::clamp(triAABBs[idx].min[axis], lo, hi);
                    float eMax = std::clamp(triAABBs[idx].max[axis], lo, hi);
                    events.push_back({eMin, Ev::START});
                    events.push_back({eMax, Ev::END});
                }

                std::sort(events.begin(), events.end(), [](const Event& a, const Event& b) {
                    if (a.pos != b.pos) return a.pos < b.pos;
                    return static_cast<uint8_t>(a.type) < static_cast<uint8_t>(b.type);
                });

                // Sweep with NL = fully-left count, NR = overlapping-right count.
                // Process in batches of equal position so the evaluation sees the
                // correct counters for the split sitting exactly at that position.
                //
                // Per batch at position p:
                //   Step 1 — apply ENDs: triangle exits right side at p.
                //   Step 2 — evaluate SAH for split at p.
                //   Step 3 — apply STARTs: triangle joins left side after p.
                //
                // This correctly handles the two sides of each discontinuity:
                // after step 1 the split is evaluated on the "right of p" side
                // (triangles ending at p are left-only); after step 3 the state
                // represents triangles that have fully passed p.
                uint32_t NL = 0, NR = N;
                size_t   i  = 0;

                while (i < events.size()) {
                    float  pos    = events[i].pos;
                    uint32_t pEnd = 0, pStart = 0;
                    size_t   j    = i;

                    while (j < events.size() && events[j].pos == pos) {
                        if (events[j].type == Ev::END) pEnd++;
                        else                           pStart++;
                        j++;
                    }

                    // Evaluate SAH *before* removing ENDs.
                    // The partition rule sends triangles with eMax >= splitPos to the right
                    // child, so triangles with eMax == pos belong in NR at evaluation time.
                    // Removing them first (old step 1 before step 2) causes NR to
                    // undercount those triangles, making splits look cheaper than they are.
                    if (pos > lo && pos < hi) {
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

                    NR -= pEnd;   // remove triangles that end here from right count
                    NL += pStart; // add triangles that start here to left count
                    i   = j;
                }
            }

            // ── Phase 3: Choose best strategy ────────────────────────────────────────
            float emptyCost = (bestEmptyAxis != -1) ? bestEmptyCost : std::numeric_limits<float>::max();
            float sahCost   = (bestSAHAxis   != -1) ? bestSAHCost   : std::numeric_limits<float>::max();

            // Leaf wins if both subdivision strategies are worse
            if (leafCost <= std::min(emptyCost, sahCost)) { emitLeaf(); return; }

            if (emptyCost < sahCost) {
                // ── Empty space cut (Havran §4.4, two-plane variant Fig 4.7) ─────────
                // We check whether the opposite side of the same axis also has empty
                // space. If so, both are cut in one step (two interior nodes, no extra
                // recursion level needed). tightBounds is still valid here.
                int   ax = bestEmptyAxis;
                float sp = bestEmptySplit;

                bool secondCut = emptyIsLeft
                    ? (tightBounds.max[ax] < nodeBounds.max[ax])   // right side also empty
                    : (tightBounds.min[ax] > nodeBounds.min[ax]);  // left side also empty
                float sp2 = emptyIsLeft ? tightBounds.max[ax] : tightBounds.min[ax];

                if (secondCut) {
                    // Two-plane cut on the same axis (Havran Fig 4.7):
                    // Produces: [empty leaf | inner interior | empty leaf]
                    //   emptyIsLeft=true  → outer splits at sp  (tightBounds.min), inner at sp2 (tightBounds.max)
                    //   emptyIsLeft=false → outer splits at sp  (tightBounds.max), inner at sp2 (tightBounds.min)
                    //
                    // IMPORTANT: allocate ALL four child nodes before any recursive call.
                    // buildNode() calls nodes.emplace_back() internally which can reallocate
                    // the vector, invalidating any pointer/reference into it obtained before.
                    float outerSplit = sp;
                    float innerSplit = sp2;

                    uint32_t outerLeft = static_cast<uint32_t>(nodes.size());
                    nodes.emplace_back(); // outerLeft     (one side: empty leaf)
                    nodes.emplace_back(); // outerLeft + 1 (other side: inner interior node)
                    uint32_t innerLeft = static_cast<uint32_t>(nodes.size());
                    nodes.emplace_back(); // innerLeft     (geometry child or empty leaf)
                    nodes.emplace_back(); // innerLeft + 1 (empty leaf or geometry child)

                    AABB outerLeftB  = nodeBounds; outerLeftB.max[ax]  = outerSplit;
                    AABB outerRightB = nodeBounds; outerRightB.min[ax] = outerSplit;

                    if (emptyIsLeft) {
                        // Layout: [empty | geometry | empty]
                        //   outerLeft     = empty leaf        [nodeBounds.min .. outerSplit]
                        //   outerLeft + 1 = inner interior    [outerSplit     .. nodeBounds.max]
                        //     innerLeft     = geometry child  [outerSplit     .. innerSplit]
                        //     innerLeft + 1 = empty leaf      [innerSplit     .. nodeBounds.max]
                        AABB innerLeftB  = outerRightB; innerLeftB.max[ax]  = innerSplit;
                        AABB innerRightB = outerRightB; innerRightB.min[ax] = innerSplit;

                        makeInteriorNode(nodes[nodeIdx],       nodeBounds,  static_cast<uint32_t>(ax), outerSplit, outerLeft);
                        makeLeafNode    (nodes[outerLeft],     outerLeftB,  0, 0);
                        makeInteriorNode(nodes[outerLeft + 1], outerRightB, static_cast<uint32_t>(ax), innerSplit, innerLeft);
                        makeLeafNode    (nodes[innerLeft + 1], innerRightB, 0, 0);
                        // recurse last — vector may reallocate, all indices already captured above
                        buildNode(innerLeft, std::move(triIndices), innerLeftB,
                                  nodes, outTriIndices, triAABBs, depth + 2);
                    } else {
                        // Layout: [empty | geometry | empty]
                        //   outerLeft     = inner interior    [nodeBounds.min .. outerSplit]
                        //     innerLeft     = empty leaf      [nodeBounds.min .. innerSplit]
                        //     innerLeft + 1 = geometry child  [innerSplit     .. outerSplit]
                        //   outerLeft + 1 = empty leaf        [outerSplit     .. nodeBounds.max]
                        AABB innerLeftB  = outerLeftB; innerLeftB.max[ax]  = innerSplit;
                        AABB innerRightB = outerLeftB; innerRightB.min[ax] = innerSplit;

                        makeInteriorNode(nodes[nodeIdx],       nodeBounds,  static_cast<uint32_t>(ax), outerSplit, outerLeft);
                        makeInteriorNode(nodes[outerLeft],     outerLeftB,  static_cast<uint32_t>(ax), innerSplit, innerLeft);
                        makeLeafNode    (nodes[outerLeft + 1], outerRightB, 0, 0);
                        makeLeafNode    (nodes[innerLeft],     innerLeftB,  0, 0);
                        // recurse last
                        buildNode(innerLeft + 1, std::move(triIndices), innerRightB,
                                  nodes, outTriIndices, triAABBs, depth + 2);
                    }
                } else {
                    // Single empty cut — allocate both children before recursing
                    uint32_t leftIdx = static_cast<uint32_t>(nodes.size());
                    nodes.emplace_back(); // leftIdx
                    nodes.emplace_back(); // leftIdx + 1
                    AABB leftBounds  = nodeBounds; leftBounds.max[ax]  = sp;
                    AABB rightBounds = nodeBounds; rightBounds.min[ax] = sp;

                    makeInteriorNode(nodes[nodeIdx], nodeBounds, static_cast<uint32_t>(ax), sp, leftIdx);
                    if (emptyIsLeft) {
                        makeLeafNode(nodes[leftIdx], leftBounds, 0, 0);
                        // recurse last
                        buildNode(leftIdx + 1, std::move(triIndices), rightBounds,
                                  nodes, outTriIndices, triAABBs, depth + 1);
                    } else {
                        makeLeafNode(nodes[leftIdx + 1], rightBounds, 0, 0);
                        // recurse last
                        buildNode(leftIdx, std::move(triIndices), leftBounds,
                                  nodes, outTriIndices, triAABBs, depth + 1);
                    }
                }
                return;
            }

            // ── SAH split: partition triangles ────────────────────────────────────────
            // Left:  min <  splitPos  (triangle starts before the split)
            // Right: max >= splitPos  (triangle ends at or after the split)
            // A triangle with min < splitPos AND max >= splitPos goes to both (duplication).
            // A triangle with max == splitPos and min == splitPos (planar) goes to right only.
            // Using >= on the right ensures no triangle is ever silently dropped.
            std::vector<uint32_t> leftTris, rightTris;
            leftTris.reserve(N); rightTris.reserve(N);
            for (uint32_t idx : triIndices) {
                if (triAABBs[idx].min[bestSAHAxis] <  bestSAHSplit) leftTris.push_back(idx);
                if (triAABBs[idx].max[bestSAHAxis] >= bestSAHSplit) rightTris.push_back(idx);
            }

            // Guard: degenerate split that doesn't separate anything → leaf
            if (leftTris.empty() || rightTris.empty()) { emitLeaf(); return; }


            uint32_t leftIdx = static_cast<uint32_t>(nodes.size());
            nodes.emplace_back();
            nodes.emplace_back();
            makeInteriorNode(nodes[nodeIdx], nodeBounds,
                             static_cast<uint32_t>(bestSAHAxis), bestSAHSplit, leftIdx);

            AABB leftBounds  = nodeBounds; leftBounds.max[bestSAHAxis]  = bestSAHSplit;
            AABB rightBounds = nodeBounds; rightBounds.min[bestSAHAxis] = bestSAHSplit;

            buildNode(leftIdx,     std::move(leftTris),  leftBounds,
                      nodes, outTriIndices, triAABBs, depth + 1);
            buildNode(leftIdx + 1, std::move(rightTris), rightBounds,
                      nodes, outTriIndices, triAABBs, depth + 1);
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

        // Build per-triangle AABBs — no vertex storage needed (no clipping)
        std::vector<AABB>     triAABBs(n);
        std::vector<uint32_t> allIndices(n);
        AABB sceneBounds;

        for (uint32_t i = 0; i < n; i++) {
            allIndices[i] = i;
            triAABBs[i].expand(glm::vec3(verts[tris[i].indices[0]].position));
            triAABBs[i].expand(glm::vec3(verts[tris[i].indices[1]].position));
            triAABBs[i].expand(glm::vec3(verts[tris[i].indices[2]].position));
            sceneBounds.expand(triAABBs[i]);
        }

        outNodes.reserve(2 * n);
        outNodes.emplace_back(); // root = index 0

        buildNode(0, std::move(allIndices), sceneBounds, outNodes, outTriIndices, triAABBs, 0);
    }

} // rt::gfx
