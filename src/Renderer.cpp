#include "Renderer.hpp"
#include <cmath>
#include <ostream>

Renderer::Renderer(const int WIDTH,
                   const int HEIGHT,
                   const float FOV) : WIDTH(WIDTH),
                                      HEIGHT(HEIGHT),
                                      FOV(FOV) {};

std::optional<Renderer::HitObject> Renderer::trace(const Ray& ray, const std::vector<std::unique_ptr<Object>> &objects) const {
    float nearestT = INT_MAX;
    Object* hitObject = nullptr;

    for (auto& obj : objects) {
        const auto intersectionT= obj->intersect(ray);
        if (intersectionT && intersectionT.value() < nearestT) {
            nearestT = intersectionT.value();
            hitObject = obj.get();
        }
    }

    if (hitObject) {
       return HitObject{hitObject, nearestT};
    }
    return std::nullopt;
}

Color Renderer::castRay (Ray ray, const std::vector<std::unique_ptr<Object>> &objects) const{
    if (auto hit = trace(ray, objects)) {
        auto hitObject = *hit;
        glm::vec3 hitPoint = ray.origin + ray.direction * hitObject.parameter;
        std::println("hit");
        //return {55.0f, 55.0f,55.0f,255.0f};
        return hitObject.obj->shade(hitPoint, ray.direction);
    }
    std::println("no hit");
    return {0.0f, 0.0f,0.0f,255.0f};
}

void Renderer::render(Framebuffer &framebuffer,
                const std::vector<std::unique_ptr<Object>> &objects,
                const Camera &camera) const {
    const float scale = static_cast<float>(tan(glm::radians(FOV * 0.5)));
    const float imageAspectRatio = static_cast<float>(WIDTH) / static_cast<float>(HEIGHT);

    const glm::mat4 view = camera.getView();
    const glm::vec3 origin= glm::vec3(view * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

    auto& pixels = framebuffer.getPixels();

    for (int j = 0; j < HEIGHT; j++) {
        for (int i = 0; i < WIDTH; i++) {
            const float x = static_cast<float>((2 * (i + 0.5) / static_cast<float>(WIDTH) - 1) * scale);
            const float y = static_cast<float>(1 - 2 * (j + 0.5) / static_cast<float>(HEIGHT) * scale * 1 / imageAspectRatio);
            glm::vec3 direction = glm::vec3(view * glm::vec4(x, y, -1.0f, 0.0f));
            direction = glm::normalize(direction);

            pixels[j][i] = castRay({origin, direction}, objects);
        }
    }   
}
