#pragma once
#include "glm/glm.hpp"

namespace rt {

    namespace {
        constexpr float     DEFAULT_YAW      = -90.0f;
        constexpr float     DEFAULT_PITCH    = 0.0f;
        constexpr float     DEFAULT_ZOOM     = 45.0f;
        constexpr glm::vec3 DEFAULT_POSITION = {0.0f, 5.0f, -5.0f};
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
        Camera();
        Camera(const glm::vec3& position, const glm::vec3& up, float yaw, float pitch, float zoom);
    
        [[nodiscard]] glm::mat4 getView() const;
        [[nodiscard]] float getZoom() const;
        [[nodiscard]] glm::vec3 getPosition() const;
        [[nodiscard]] glm::mat4 getProjection(float aspectRatio) const;

        class Builder {
            glm::vec3 position = DEFAULT_POSITION;
            glm::vec3 up{0.0f, 1.0f, 0.0f};
            float yaw{DEFAULT_YAW};
            float pitch{DEFAULT_PITCH};
            float zoom{DEFAULT_ZOOM};

        public:
            Builder& setPosition(const glm::vec3& p) {
                position = p;
                return *this;
            }

            Builder& setUp(const glm::vec3& u) {
                up = u;
                return *this;
            }

            Builder& setYaw(float y) {
                yaw = y;
                return *this;
            }

            Builder& setPitch(float p) {
                pitch = p;
                return *this;
            }

            Builder& setZoom(float z) {
                zoom = z;
                return *this;
            }

            Camera build() const {
                return Camera(position, up, yaw, pitch, zoom);
            }
        };
    };
    
    }//rt
