#pragma once

#include "TraceIntegrator.h"

namespace rt {

class BruteForceTraceIntegrator : public TraceIntegrator {
public:
    [[nodiscard]] Color traceFromCamera(const Ray& ray, const Scene& scene) const override;
};

}
