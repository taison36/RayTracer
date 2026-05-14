#pragma once
#include "../core/Scene.h"

namespace rt {
    struct SceneSettings {
        const uint32_t WIDTH;
        const uint32_t HEIGHT;
        const float FOV;
        const float IMAGE_ASPECT_RATIO;
        const uint32_t maxBounces;
        const uint32_t samplesPerPixel;
        const uint32_t samplesPerEmissiveLight;

        SceneSettings(const uint32_t width,
                      const uint32_t height,
                      const float fov,
                      const uint32_t maxBounces,
                      const uint32_t samplesPerPixel,
                      const uint32_t samplesPerEmissiveLight)
            : WIDTH(width),
              HEIGHT(height),
              FOV(fov),
              IMAGE_ASPECT_RATIO(static_cast<float>(WIDTH) / static_cast<float>(HEIGHT)),
              maxBounces(maxBounces),
              samplesPerPixel(samplesPerPixel),
              samplesPerEmissiveLight(samplesPerEmissiveLight) {
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
            const SceneSettings *screenSettings;

            RendererContext(const Scene *scene, const SceneSettings *screenSettings)
                : scene(scene),
                  screenSettings(screenSettings) {
            }
        };
    } // gfx
} // rt
