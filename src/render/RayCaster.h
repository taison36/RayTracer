#pragma once
#include "../FrameBuffer.h"
#include "../ScreenSettings.h"
#include "../Scene.h"
#include "TraceIntegrator.h"

namespace rt {
    class RayCaster {
public:
    virtual void castRays(FrameBuffer& fb, const Scene &scene, const ScreenSettings& screenSettings, const TraceIntegrator* traceIntegrator) = 0;

    virtual ~RayCaster() = default;
    RayCaster() = default;
    RayCaster(const RayCaster&) = delete;
    RayCaster& operator=(const RayCaster&) = delete;
};

} // rt
