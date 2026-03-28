#pragma once
#include "RayCaster.h"
#include "../FrameBuffer.h"
#include "../Scene.h"

namespace rt {

class DefaultRayCaster : public RayCaster {
public:
    void castRays(FrameBuffer& fb, const Scene &scene, const ScreenSettings& screenSettings, const TraceIntegrator* traceIntegrator) override;
};

} //rt
