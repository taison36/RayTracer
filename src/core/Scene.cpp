#include "Scene.h"

namespace rt {

    TrianglePrimitive::TrianglePrimitive(const std::vector<uint32_t> &indices,
                                         const std::vector<glm::vec3> &positions,
                                         const std::vector<glm::vec3> &normals,
                                         const std::vector<glm::vec4> &tangents,
                                         const std::vector<std::vector<glm::vec2>> &text_coordinates)
        : indices(indices),
          positions(positions),
          normals(normals),
          tangents(tangents),
          textCoordinates(text_coordinates) {
    }
    
    TrianglePrimitive::ConstIterator::ConstIterator(const TrianglePrimitive *primitives, size_t startIndex)
        : primitives(primitives), index(startIndex){}
    
    TrianglePrimitive::ConstIterator TrianglePrimitive::begin() const {
        return {this, 0};
    }
    
    TrianglePrimitive::ConstIterator TrianglePrimitive::end() const {
        return {this, indices.size()};
    }
    
    
    std::array<const glm::vec3*, 3> TrianglePrimitive::ConstIterator::operator*() const {
        return { &primitives->positions[primitives->indices[index]],
                 &primitives->positions[primitives->indices[index + 1]],
                 &primitives->positions[primitives->indices[index + 2]] };
    }
    
    std::array<glm::vec3, 3> TrianglePrimitive::ConstIterator::position(const glm::mat4& viewMatrix) const {
        assert(index + 2 < primitives->indices.size());
        const auto v1 = glm::vec3(viewMatrix * glm::vec4(primitives->positions[primitives->indices[index]], 1.0f));
        const auto v2 = glm::vec3(viewMatrix * glm::vec4(primitives->positions[primitives->indices[index + 1]], 1.0f));
        const auto v3 = glm::vec3(viewMatrix * glm::vec4(primitives->positions[primitives->indices[index + 2]], 1.0f));
        return {v1, v2, v3};
    }
    
    std::array<const glm::vec3*, 3> TrianglePrimitive::ConstIterator::normal(const glm::mat4& worldMatrix) const {
        return { &primitives->normals[primitives->indices[index]],
                 &primitives->normals[primitives->indices[index + 1]],
                 &primitives->normals[primitives->indices[index + 2]] };
    }
    
    std::array<const glm::vec4*, 4> TrianglePrimitive::ConstIterator::tangent(const glm::mat4& worldMatrix) const {
        return { &primitives->tangents[primitives->indices[index]],
                 &primitives->tangents[primitives->indices[index + 1]],
                 &primitives->tangents[primitives->indices[index + 2]],
                 &primitives->tangents[primitives->indices[index + 3]]};
    }
    
    TrianglePrimitive::ConstIterator& TrianglePrimitive::ConstIterator::operator++() {
        index += 3;
        return *this;
    }
    
    bool TrianglePrimitive::ConstIterator::operator!=(const ConstIterator& other) const {
        return index != other.index;
    }
    
    Scene::Scene(const std::vector<TrianglePrimitive> &trianglePrimitives)
        : trianglePrimitives(trianglePrimitives){
    }
    
    const std::vector<TrianglePrimitive>& Scene::getTrianglePrimitives() const {
        return trianglePrimitives;
    }
} // rt
