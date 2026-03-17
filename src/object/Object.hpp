#pragma once
#include <expected>
#include "../Ray.hpp"

class Object {
public:
    virtual ~Object() = default;
    virtual std::expected<float, bool> intersect(const Ray &ray) = 0;
    virtual Color shade(const glm::vec3 &hitPoint, const glm::vec3 &dir) = 0;
};
