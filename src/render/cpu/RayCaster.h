#pragma once
#include "../../core/FrameBuffer.h"
#include "../../objects/ScreenSettings.h"
#include "../../core/Scene.h"
#include "TraceIntegrator.h"

namespace rt::cpu {
    class RayCaster {
public:
    virtual void castRays(FrameBuffer& fb, const Scene &scene, const ScreenSettings& screenSettings, const TraceIntegrator* traceIntegrator) = 0;

    virtual ~RayCaster() = default;
    RayCaster() = default;
    RayCaster(const RayCaster&) = delete;
    RayCaster& operator=(const RayCaster&) = delete;
};

}// rt::cpu
