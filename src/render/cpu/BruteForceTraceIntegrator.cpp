#include "BruteForceTraceIntegrator.h"
#include <optional>
#include <ostream>

namespace rt::cpu {
    struct Hit {
        float t;
        float u;
        float v;
    };

    std::optional<Hit> intersectTriangle(const Ray &ray, const std::array<glm::vec3, 3> verticies) {
        const auto &v0 = verticies[0];
        const auto &v1 = verticies[1];
        const auto &v2 = verticies[2];

        const float EPSILON = 1e-8f;
        glm::vec3 edge1 = v1 - v0;
        glm::vec3 edge2 = v2 - v0;
        glm::vec3 h = glm::cross(ray.direction, edge2);
        float a = glm::dot(edge1, h);
        if (fabs(a) < EPSILON)
            return std::nullopt; // Ray parallel to triangle

        float f = 1.0f / a;
        glm::vec3 s = ray.origin - v0;
        float u = f * glm::dot(s, h);
        if (u < 0.0f || u > 1.0f)
            return std::nullopt;

        glm::vec3 q = glm::cross(s, edge1);
        float v = f * glm::dot(ray.direction, q);
        if (v < 0.0f || u + v > 1.0f)
            return std::nullopt;

        float t = f * glm::dot(edge2, q);
        if (t > EPSILON)
            return Hit{t, u, v};

        return std::nullopt;
    }

    Color BruteForceTraceIntegrator::traceFromCamera(const Ray &ray, const Scene &scene) const {
        Hit hit{};
        hit.t = FLT_MAX;
        const glm::mat4 viewMatrix = scene.getCamera().getView();

        std::vector<std::array<glm::vec3, 3> > trianglesInView;
        for (auto &primitive: scene.getTrianglePrimitives()) {
            for (auto it = primitive.begin(); it != primitive.end(); ++it) {
                trianglesInView.push_back(it.position(viewMatrix));
            }
        }

        for (auto &tri: trianglesInView) {
            if (auto curHit = intersectTriangle(ray, tri); curHit && curHit->t < hit.t)
                hit = *curHit;
        }

        if (hit.t != FLT_MAX) {
            // TODO there the shader should be called
            return {0.0f, 0.0f, 0.0f, 255.0f};
        }
        return {255.0f, 255.0f, 255.0f, 255.0f};
    }
}//rt::cpu
