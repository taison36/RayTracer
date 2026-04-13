#pragma once
#include "Camera.h"
#include "../core/Scene.h"

namespace rt {

    struct ScreenSettings {
        const uint32_t WIDTH;
        const uint32_t HEIGHT;
        const float FOV;
        const float IMAGE_ASPECT_RATIO;

        ScreenSettings(const uint32_t width, const uint32_t height, const float fov)
            : WIDTH(width),
              HEIGHT(height),
              FOV(fov),
              IMAGE_ASPECT_RATIO(static_cast<float>(WIDTH) / static_cast<float>(HEIGHT)) {
        }
    };

    struct Color {
        float r = 0;
        float g = 0;
        float b = 0;
        float a = 0;

        Color() = default;

        Color(float r, float g, float b, float a) : r(r), g(g), b(b), a(a) {
        };

        Color operator*(float x) const {
            return {r * x, g * x, b * x, a};
        }

        Color operator*(const Color &other) const {
            return {r * other.r, g * other.g, b * other.b, a};
        }

        Color operator+(float x) const {
            return {r + x, g + x, b + x, a};
        }

        Color operator+(const Color &other) const {
            return {r + other.r, g + other.g, b + other.b, a};
        }
    };

    namespace gfx {
        struct RendererContext {
            const Scene *scene;
            const Camera *camera;
            const ScreenSettings *screenSettings;

            RendererContext(const Scene *scene, const Camera *camera, const ScreenSettings *screenSettings)
                : scene(scene),
                  camera(camera),
                  screenSettings(screenSettings) {
            }
        };
    } // gfx
} // rt
