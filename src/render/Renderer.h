#pragma once
#include "RayCaster.h"
#include "../FrameBuffer.h"
#include "../Camera.h"
#include "../Scene.h"
#include "../ScreenSettings.h"

namespace rt {

class Renderer {
    const std::unique_ptr<RayCaster> rayCaster;
    const std::unique_ptr<TraceIntegrator> traceIntegrator;
    const ScreenSettings screenSettings;

public:
    Renderer(std::unique_ptr<RayCaster> rayCaster, std::unique_ptr<TraceIntegrator> traceIntegrator, const ScreenSettings& screenSettings);

    void render(FrameBuffer &fb, const Scene &scene) const;
};

} //rt
