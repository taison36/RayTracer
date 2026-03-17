#include <expected>
#include "Sphere.hpp"

#include <ostream>

Sphere::Sphere(const glm::vec3 &worldPosition, const Color &baseColor, float radius)
    : worldPosition(worldPosition), baseColor(baseColor), radius(radius), radiusQube(radius * radius) {
}

std::expected<float, bool> Sphere::intersect(const Ray &ray) {
    const glm::vec3 centerToOrigin = worldPosition - ray.origin;
    const float tCenter = glm::dot(centerToOrigin, ray.direction);
    //if < 0 than it is behind the ray
    if (tCenter < 0) {
        return std::unexpected(false);
    }

    const float distBetweenFirstSecondPoint = glm::dot(centerToOrigin, centerToOrigin) - tCenter * tCenter;
    if (distBetweenFirstSecondPoint > radiusQube) {
        return std::unexpected(false);
    }

    float distFromFirstPointToCenter = sqrt(radiusQube - distBetweenFirstSecondPoint);
    float intersectionT0 = tCenter - distFromFirstPointToCenter;
    float intersectionT1 = tCenter + distFromFirstPointToCenter;

    if (intersectionT0 < 0 && intersectionT1 < 0) {
        return std::unexpected(false);
    }

    return std::max(intersectionT0, intersectionT1);
};
inline Color mix(const Color &a, const Color& b, const float &mixValue) {
    return a * (1 - mixValue) + b * mixValue;
}

Color Sphere::shade(const glm::vec3 &hitPoint, const glm::vec3 &dir) {
     glm::vec3 normal = glm::normalize(hitPoint - worldPosition);
    // UV-Koordinaten für die Kugel
    float u = (1.0f + atan2(normal.z, normal.x) / M_PI) * 0.5f;
    float v = acosf(normal.y) / M_PI;

    // Checkerboard-Muster
    float scale = 4.0f;
    float pattern = (fmodf(u * scale, 1) > 0.5) ^ (fmodf(v * scale, 1) > 0.5);
    Color color = mix(baseColor, baseColor * 0.8, pattern) * std::max(0.f, glm::dot(normal, dir));
    return color;

    //Beleuchtung (Lambert)
    float lambert = std::max(0.0f, glm::dot(normal, dir));
    std::println("labert {}" ,lambert);

    return color * lambert;
//    return baseColor * lambert;
}