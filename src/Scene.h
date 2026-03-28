#pragma once
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>

#include "Camera.h"

namespace rt {

    class TrianglePrimitive {
        std::vector<uint32_t> indices;
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec4> tangents;
        std::vector<std::vector<glm::vec2>> textCoordinates;
    public:
        class ConstIterator {
            const TrianglePrimitive* primitives;
            size_t index;

        public:
            ConstIterator(const TrianglePrimitive* primitives, size_t startIndex);
            std::array<const glm::vec3*, 3> operator*() const;
            [[nodiscard]] std::array<glm::vec3, 3> position(const glm::mat4& viewMatrix) const;
            [[nodiscard]] std::array<const glm::vec3*, 3> normal(const glm::mat4& worldMatrix) const;
            [[nodiscard]] std::array<const glm::vec4*, 4> tangent(const glm::mat4& worldMatrix) const;
            ConstIterator& operator++();
            bool operator!=(const ConstIterator& other) const;
        };

        TrianglePrimitive(const std::vector<uint32_t>  &indices,
                          const std::vector<glm::vec3> &positions,
                          const std::vector<glm::vec3> &normals,
                          const std::vector<glm::vec4> &tangents,
                          const std::vector<std::vector<glm::vec2>> &text_coordinates);
        [[nodiscard]] ConstIterator begin() const;
        [[nodiscard]] ConstIterator end() const;
    };

    struct Material {

    };

    class Scene {
        const std::vector<TrianglePrimitive> trianglePrimitives;
        const std::vector<Material> materials;
        const Camera camera;

    public:
        Scene(const std::vector<TrianglePrimitive>& trianglePrimitives,
              const Camera& camera);
        [[nodiscard]] const std::vector<TrianglePrimitive>& getTrianglePrimitives() const;
        [[nodiscard]] const Camera& getCamera() const;
    };
} // rt
