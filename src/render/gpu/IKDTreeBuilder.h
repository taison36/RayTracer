#pragma once
#include <vector>
#include <string>
#include "../../core/Scene.h"

// Forward-declared GPU types (defined in KDTree.h)
namespace rt::gfx {
    struct KDNode;
}

namespace rt::gfx {

    // ── Interface for KD-Tree CPU construction algorithms ────────────────────────
    // All builders produce the same GPU output consumed by KDTree's Vulkan side:
    //   outNodes      – flat array of KDNode (root at index 0)
    //   outTriIndices – flat primitive index list referenced by leaf nodes
    class IKDTreeBuilder {
    public:
        virtual ~IKDTreeBuilder() = default;

        virtual void build(const std::vector<rt::Triangle>& tris,
                           const std::vector<rt::Vertex>&   verts,
                           std::vector<KDNode>&             outNodes,
                           std::vector<uint32_t>&           outTriIndices) = 0;

        virtual std::string getName() const = 0;
    };

} // rt::gfx
