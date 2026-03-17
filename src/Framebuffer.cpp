#include "Framebuffer.hpp"

std::vector<std::vector<Color>>& Framebuffer::getPixels() {
    return pixels;
}
Framebuffer::Framebuffer(int width, int height)
    : pixels(height, std::vector<Color>(width)) {};