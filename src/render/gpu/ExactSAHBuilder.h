#pragma once
#include "IKDTreeBuilder.h"

namespace rt::gfx {

    // ── Exact SAH KD-Tree builder ─────────────────────────────────────────────────
    // Implements OSAH (Object SAH) with empty-space cutting.
    // Reference: Havran, "Heuristic Ray Shooting Algorithms", PhD Thesis, 2000
    //            §4.2.3 (OSAH event sweep) + §4.4 (empty-space cutting)
    //
    // Key improvements over BinnedSAHBuilder:
    //
    //  1. Exact SAH via event sweep — O(n log n)
    //     Split candidates are the actual triangle AABB boundaries (2N per axis),
    //     not 8 fixed bins. Events are sorted and swept maintaining exact NL/NR
    //     counts, evaluating the true SAH cost at every candidate position.
    //     No triangle clipping — raw AABBs are used throughout.
    //
    //  2. Empty Space Cutting (Havran §4.4)
    //     Before the SAH sweep, the tight union AABB of all triangles is compared
    //     against the node's spatial bounds. Any gap is guaranteed-empty space.
    //     An explicit split is evaluated there, creating an empty leaf child.
    //     This is a proper pre-pass, not a heuristic cost bonus.
    class ExactSAHBuilder : public IKDTreeBuilder {
    public:
        void build(const std::vector<rt::Triangle>& tris,
                   const std::vector<rt::Vertex>&   verts,
                   std::vector<KDNode>&             outNodes,
                   std::vector<uint32_t>&           outTriIndices) override;

        std::string getName() const override { return "Exact SAH"; }
    };

} // rt::gfx
