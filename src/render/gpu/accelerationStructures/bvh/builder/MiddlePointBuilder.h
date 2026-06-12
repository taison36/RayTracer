#pragma once
#include "IBVHBuilder.h"

namespace rt::gfx {

    class MiddlePointBuilder : public IBVHBuilder {
    public:
        void build(const std::vector<rt::Triangle>& tris,
                   const std::vector<rt::Vertex>&   verts,
                   std::vector<BVHNode>&            outNodes,
                   std::vector<uint32_t>&           outTriIndices) override;
        std::string getName() const override { return "BVH-Midpoint"; }
    };

} // rt::gfx
