#pragma once
#include "Framebuffer.hpp"
#include "object/Object.hpp"
#include "Camera.hpp"

class Renderer {
    struct HitObject {
        Object* obj;
        float parameter;
    };
    const int   WIDTH;
    const int   HEIGHT;
    const float FOV;

    [[nodiscard]] Color castRay(Ray ray, const std::vector<std::unique_ptr<Object>>& objects) const;
    [[nodiscard]] std::optional<HitObject> trace(const Ray& ray, const std::vector<std::unique_ptr<Object>> &objects) const;
public:
    Renderer(int WIDTH, int HEIGHT, float FOV);

    void render(Framebuffer &framebuffer,
                const std::vector<std::unique_ptr<Object>> &objects,
                const Camera &camera) const;
};
