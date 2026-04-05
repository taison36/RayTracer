#pragma once

#include "TraceIntegrator.h"

namespace rt::cpu {

    class BruteForceTraceIntegrator : public TraceIntegrator {
    public:
        [[nodiscard]] Color traceFromCamera(const Ray& ray, const Scene& scene) const override;
    };

} //rt::cpu
