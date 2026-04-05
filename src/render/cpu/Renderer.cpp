#include "Renderer.h"
#include <ostream>

namespace rt::cpu {

Renderer::Renderer(std::unique_ptr<RayCaster> rayCaster,
                   std::unique_ptr<TraceIntegrator> traceIntegrator,
                   const ScreenSettings& screenSettings)
    : rayCaster(std::move(rayCaster)), traceIntegrator(std::move(traceIntegrator)), screenSettings(screenSettings){}

void Renderer::render(FrameBuffer &fb, const Scene& scene) const {
    rayCaster->castRays(fb, scene, screenSettings, traceIntegrator.get());
}

}//rt
