#pragma once
#include "CameraSample.h"
#include "glm/glm.hpp"
#include "render/Ray.h"

namespace rt {

namespace {
    constexpr float DEFAULT_YAW   = -90.0f;
    constexpr float DEFAULT_PITCH = 0.0f;
    constexpr float DEFAULT_ZOOM  = 45.0f;
};

class Camera {
    glm::vec3 position;
    glm::vec3 front{};
    glm::vec3 right{};
    glm::vec3 up;
    glm::vec3 worldUp;

    float yaw;
    float pitch;
    float zoom;

    void updateCameraVectors();
public:
    explicit Camera(const glm::vec3 &position = glm::vec3(0.0f, 0.0f, 0.0f),
                    const glm::vec3 &up = glm::vec3(0.0f, 1.0f, 0.0f),
                    float yaw = DEFAULT_YAW,
                    float pitch = DEFAULT_PITCH,
                   float zoom = DEFAULT_ZOOM);

    [[nodiscard]] glm::mat4 getView() const;
    [[nodiscard]] float getZoom() const;
    [[nodiscard]] glm::vec3 getPosition() const;
    [[nodiscard]] Ray generateRay(const CameraSample& sample) const;
};

}//rt
