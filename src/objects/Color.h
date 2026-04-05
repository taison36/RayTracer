#pragma once
struct Color {
    float r = 0; 
    float g = 0; 
    float b = 0; 
    float a = 0; 

    Color() = default;
    Color(float r, float g, float b, float a) : r(r), g(g), b(b), a(a) {};

    Color operator*(float x) const {
        return {r * x, g * x, b * x, a};
    }
    Color operator*(const Color& other) const {
        return {r * other.r, g * other.g, b * other.b, a};
    }
    Color operator+(float x) const {
        return {r + x, g + x, b + x, a};
    }
    Color operator+(const Color& other) const {
        return {r + other.r, g + other.g, b + other.b, a};
    }

};
