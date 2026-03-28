#pragma once
#include "Object.hpp"
#include "../Color.hpp"
#include "../render/Ray.hpp"
#include "glm/glm.hpp"

class Sphere : public Object {
    glm::vec3 worldPosition;
    Color baseColor;
    float radius;
    float radiusQube;

public:
    Sphere(const glm::vec3 &worldPosition, const Color &baseColor, float radius);
    std::expected<float, bool> intersect(const Ray &) override;
    Color shade(const glm::vec3 &hitPoint, const glm::vec3 &dir) override;
};
