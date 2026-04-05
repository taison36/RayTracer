#pragma once
#include "RayCaster.h"
#include "../../core/FrameBuffer.h"
#include "../../objects/Camera.h"
#include "../../core/Scene.h"
#include "../../objects/ScreenSettings.h"

namespace rt::cpu {

    class Renderer {
        const std::unique_ptr<RayCaster> rayCaster;
        const std::unique_ptr<TraceIntegrator> traceIntegrator;
        const ScreenSettings screenSettings;
    
    public:
        Renderer(std::unique_ptr<RayCaster> rayCaster, std::unique_ptr<TraceIntegrator> traceIntegrator, const ScreenSettings& screenSettings);
    
        void render(FrameBuffer &fb, const Scene &scene) const;
    };

} //rt
