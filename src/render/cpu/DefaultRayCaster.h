#pragma once
#include "RayCaster.h"
#include "../../core/FrameBuffer.h"
#include "../../core/Scene.h"

namespace rt::cpu {

    class DefaultRayCaster : public RayCaster {
    public:
        void castRays(FrameBuffer& fb, const Scene &scene, const ScreenSettings& screenSettings, const TraceIntegrator* traceIntegrator) override;
    };

} //rt::cpu
