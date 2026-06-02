#pragma once
#include "IKDTreeBuilder.h"

namespace rt::gfx {

    // ── Binned SAH builder with spatial triangle splitting ────────────────────────
    // Uses 8 fixed bins per axis to approximate the split cost.
    // Triangles straddling the split plane are spatially split: tight per-fragment
    // AABBs are computed by clipping triangle edges against the plane, reducing the
    // child bounding boxes compared to simple duplication (Danilewski et al. 2010).
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
