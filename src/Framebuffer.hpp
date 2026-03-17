#pragma once
#include "Color.hpp"
#include <vector>

class Framebuffer {
    std::vector<std::vector<Color>> pixels;

public:

    Framebuffer(int width, int height);
    std::vector<std::vector<Color>>& getPixels(); 
};
