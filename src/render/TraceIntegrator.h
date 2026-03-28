#pragma once
#include "Ray.h"
#include "../Scene.h"
#include "../Color.h"

namespace rt {

class TraceIntegrator {
public:
    [[nodiscard]] virtual Color traceFromCamera(const Ray& ray, const Scene& scene) const = 0;

    virtual ~TraceIntegrator() = default;
    TraceIntegrator() = default;
    TraceIntegrator(const TraceIntegrator&) = delete;
    TraceIntegrator& operator=(const TraceIntegrator&) = delete;
};

}
