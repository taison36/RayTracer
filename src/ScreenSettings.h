#pragma once
#include <cstdint>

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

}// rt
