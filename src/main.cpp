#include "Renderer.hpp"
#include "Framebuffer.hpp"
#include "object/Object.hpp"
#include "object/Sphere.hpp"

#include <print>
#include <vector>
#include <memory>
#include <fstream>

#include "LoaderGltf.h"

constexpr int WIDTH = 640;
constexpr int HEIGHT = 480;
constexpr int SPHERE_COUNT = 16;

void generateSpheres(std::vector<std::unique_ptr<Object> > &objects, int count) {
    int min = -5;
    int var = 10;
    for (int i = 0; i < count; i++) {
        glm::vec3 randPos(min + rand() % var,
                          min + rand() % var,
                          min + rand() % var);
        Color color(rand() % 255,
                    rand() % 255,
                    rand() % 255,
                    255);
        objects.push_back(std::make_unique<Sphere>(randPos, color, 1));
    }
}

int main() {
    LoaderGltf loader;
    loader.load("resources/simple_model/scene.gltf");

    Framebuffer fb(WIDTH, HEIGHT);
    std::vector<std::unique_ptr<Object> > objects;

    generateSpheres(objects, SPHERE_COUNT);

    const auto renderer = std::make_unique<Renderer>(WIDTH, HEIGHT, 90.0f);
    const Camera camera({0.0f, 0.0f, -5.0f});

    renderer->render(fb, objects, camera);

    std::ofstream ofs("./out.ppm");
    ofs << "P6\n" << WIDTH << " " << HEIGHT << "\n255\n";
    for (auto &row: fb.getPixels()) {
        for (auto &pixel: row) {
            unsigned char color[3] = {
                static_cast<unsigned char>(pixel.r),
                static_cast<unsigned char>(pixel.g),
                static_cast<unsigned char>(pixel.b)
            };
            ofs.write((char *) color, 3);
        }
    }
    ofs.close();
     std::println("end");
}
