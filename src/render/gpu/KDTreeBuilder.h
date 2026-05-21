#pragma once
#include "IKDTreeBuilder.h"

namespace rt::gfx {

    // ── Binned SAH builder ────────────────────────────────────────────────────────
    // Uses 8 fixed bins per axis to approximate the split cost.
    // Triangles whose AABB straddles the split plane are duplicated into both children.
    // O(n * NUM_BINS) per node level. Fast build, good quality for most scenes.
    class BinnedSAHBuilder : public IKDTreeBuilder {
    public:
        void build(const std::vector<rt::Triangle>& tris,
                   const std::vector<rt::Vertex>&   verts,
                   std::vector<KDNode>&             outNodes,
                   std::vector<uint32_t>&           outTriIndices) override;

        std::string getName() const override { return "Binned SAH"; }
    };

} // rt::gfx
