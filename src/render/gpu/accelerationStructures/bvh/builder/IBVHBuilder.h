#pragma once
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "../../../../../core/Scene.h"

namespace rt::gfx {

    struct alignas(16) BVHNode {
        glm::vec4 aabbMinLeft;
        glm::vec4 aabbMaxCount;
    };

    class IBVHBuilder {
    public:
        virtual ~IBVHBuilder() = default;
        virtual void build(const std::vector<rt::Triangle>& tris,
                           const std::vector<rt::Vertex>&   verts,
                           std::vector<BVHNode>&            outNodes,
                           std::vector<uint32_t>&           outTriIndices) = 0;
        virtual std::string getName() const = 0;
    };

} // rt::gfx
