#pragma once
#include "Object.hpp"

class Model : public Object {
public:
    std::expected<float, bool> intersect(const Ray &ray) override;
    Color shade(const glm::vec3 &hitPoint, const glm::vec3 &dir) override;
};
