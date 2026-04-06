#include <memory>

#include "core/RayTracerApplication.h"
#include "render/gpu/BruteForce.h"
#include "utils/SceneLoader.h"

static constexpr int WIDTH = 1920;
static constexpr int HEIGHT = 1080;
static constexpr int FOV = 60.0f;

int main() {
    rt::ScreenSettings screenSettings(WIDTH, HEIGHT, FOV);
    glm::vec3 modelCenter(-24.0f, 18.0f, 11.0f);
    glm::vec3 cameraPos = modelCenter + glm::vec3(-25.0f, 10.0f, 12.0f); // 10 up, 20 back
    glm::vec3 dir = glm::normalize(modelCenter - cameraPos); // direction from camera to model
    float pitch = glm::degrees(asin(dir.y)); // vertical angle
    float yaw = glm::degrees(atan2(dir.z, dir.x)); // horizontal angle
    const rt::Camera camera(cameraPos, glm::vec3(0.0f, 1.0f, 0.0f), yaw, pitch, screenSettings.FOV);
    rt::Scene scene = rt::SceneLoader::loadScene("resources/simple_model/scene.gltf", camera);

    rt::RayTracerApplication app(screenSettings, std::make_unique<rt::gfx::BruteForce>());

    app.run(scene);
}
