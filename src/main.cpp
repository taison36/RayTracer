#include <print>
#include <memory>
#include <fstream>

#include "SceneLoader.h"
#include "render/Renderer.h"
#include "FrameBuffer.h"
#include "render/BruteForceTraceIntegrator.h"
#include "render/DefaultRayCaster.h"

constexpr int WIDTH = 1920;
constexpr int HEIGHT = 1080;
constexpr int FOV = 60.0f;

int main() {
    rt::ScreenSettings screen_settings(WIDTH, HEIGHT, FOV);
    rt::FrameBuffer fb(WIDTH, HEIGHT);

    glm::vec3 modelCenter(-24.0f, 18.0f, 11.0f);
    glm::vec3 cameraPos = modelCenter + glm::vec3(-25.0f, 10.0f, 12.0f); // 10 up, 20 back
    glm::vec3 dir = glm::normalize(modelCenter - cameraPos); // direction from camera to model
    float pitch = glm::degrees(asin(dir.y)); // vertical angle
    float yaw = glm::degrees(atan2(dir.z, dir.x)); // horizontal angle
    const rt::Camera camera(cameraPos, glm::vec3(0.0f, 1.0f, 0.0f), yaw, pitch, screen_settings.FOV);
    rt::Scene scene = rt::SceneLoader::loadScene("resources/simple_model/scene.gltf", camera);
    const rt::Renderer renderer(std::make_unique<rt::DefaultRayCaster>(),
                                std::make_unique<rt::BruteForceTraceIntegrator>(),
                                screen_settings);

    auto start = std::chrono::high_resolution_clock::now();

    renderer.render(fb, scene);

    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::println("Time: {} ms", duration.count());

    std::ofstream ofs("./out.ppm");
    ofs << "P6\n" << WIDTH << " " << HEIGHT << "\n255\n";
    for (auto &pixel: fb) {
        unsigned char color[3] = {
            static_cast<unsigned char>(pixel.r),
            static_cast<unsigned char>(pixel.g),
            static_cast<unsigned char>(pixel.b)
        };
        ofs.write((char *) color, 3);
    }
    ofs.close();
}
