#include "TraceIntegrator.h"
#include "DefaultRayCaster.h"
#include <thread>


namespace rt::cpu {
    void DefaultRayCaster::castRays(FrameBuffer &fb, const Scene &scene, const ScreenSettings &screenSettings,
                                    const TraceIntegrator *traceIntegrator) {
        const float scale = tan(glm::radians(screenSettings.FOV * 0.5f));
        const float aspect = static_cast<float>(screenSettings.WIDTH) /
                             static_cast<float>(screenSettings.HEIGHT);

        const uint32_t numThreads = std::thread::hardware_concurrency();
        std::vector<std::thread> threads;

        const uint32_t rowsPerThread = screenSettings.HEIGHT / numThreads;

        for (int t = 0; t < numThreads; t++) {
            int start = t * rowsPerThread;
            int end = (t == numThreads - 1) ? screenSettings.HEIGHT : start + rowsPerThread;

            threads.emplace_back([&, start, end]() {
                for (int row = start; row < end; row++) {
                    for (int col = 0; col < screenSettings.WIDTH; col++) {
                        float x = (2.0f * (col + 0.5f) / screenSettings.WIDTH - 1.0f) * aspect * scale;
                        float y = (1.0f - 2.0f * (row + 0.5f) / screenSettings.HEIGHT) * scale;

                        Ray ray = scene.getCamera().generateRay({x, y});
                        fb[row][col] = traceIntegrator->traceFromCamera(ray, scene);
                    }
                }
            });
        }

        for (auto &t: threads) t.join();
    }
} // rt
