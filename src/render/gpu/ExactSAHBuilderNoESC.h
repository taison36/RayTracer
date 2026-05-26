#pragma once
#include "IKDTreeBuilder.h"

namespace rt::gfx {

    // ── Exact SAH KD-Tree builder — no empty-space cutting ────────────────────────
    // Identical to ExactSAHBuilder but with Phase 1 (Havran §4.4 empty-space
    // cutting) removed. Use this to isolate the effect of empty-space cutting
    // on rendering performance: swap ExactSAHBuilder <-> ExactSAHBuilderNoESC
    // at the call site in main.cpp.
    class ExactSAHBuilderNoESC : public IKDTreeBuilder {
    public:
        void build(const std::vector<rt::Triangle>& tris,
                   const std::vector<rt::Vertex>&   verts,
                   std::vector<KDNode>&             outNodes,
                   std::vector<uint32_t>&           outTriIndices) override;

        std::string getName() const override { return "Exact SAH (no ESC)"; }
    };

} // rt::gfx
