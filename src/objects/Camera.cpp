#include "Camera.h"
#include "glm/gtc/matrix_transform.hpp"

namespace rt {

    Camera::Camera(const glm::vec3 &position,
                    const glm::vec3 &up,
                    float yaw,
                    float pitch,
                    float zoom) : position(position),
                                  up(up),
                                  worldUp(up),
                                  yaw(yaw),
                                  pitch(pitch),
                                  zoom(zoom) {
        updateCameraVectors();
    };
    
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
    
    cpu::Ray Camera::generateRay(const CameraSample& sample) const {
        auto origin = glm::vec3(0.0f); // camera at origin in view space
        auto direction = glm::normalize(glm::vec3(sample.x, sample.y, -1.0f));
        return {origin, direction};
    }

}
