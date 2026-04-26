#include "Camera.h"
#include "glm/gtc/matrix_transform.hpp"

namespace rt {

    Camera::Camera()
        : position(DEFAULT_POSITION),
          worldUp(glm::vec3(0.0f, 1.0f, 0.0f)),
          zoom(DEFAULT_ZOOM) {
        glm::vec3 target  = glm::vec3(0.0f);
        glm::vec3 forward = glm::normalize(target - position);
    
        pitch = glm::degrees(std::asin(forward.y));
        yaw   = glm::degrees(std::atan2(forward.z, forward.x));
    
        updateCameraVectors();
    }

    Camera::Camera(const glm::vec3& position,
                   const glm::vec3& up,
                   float yaw,
                   float pitch,
                   float zoom)
        : position(position),
          worldUp(up),
          yaw(yaw),
          pitch(pitch),
          zoom(zoom) {
        updateCameraVectors();
    }

    void Camera::updateCameraVectors() {
        glm::vec3 newFront;
        newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        newFront.y = sin(glm::radians(pitch));
        newFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front = glm::normalize(newFront);
    
        right = glm::normalize(glm::cross(front, worldUp));
        up = glm::normalize(glm::cross(right, front));
    }
    
    glm::mat4 Camera::getView() const {
        return glm::lookAt(position, position + front, up);
    }
    
    float Camera::getZoom() const {
        return zoom;
    }
    
    glm::vec3 Camera::getPosition() const {
        return position;
    }

    glm::mat4 Camera::getProjection(float aspectRatio) const {
        float fov  = glm::radians(zoom);
        float near = 0.1f;
        float far  = 100.0f;
        glm::mat4 proj = glm::perspective(fov, aspectRatio, near, far);
        return proj;
    }
}
