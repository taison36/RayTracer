#pragma once
#include "../objects/Camera.h"
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>

namespace rt {

#define RT_MAXSIZE_NUM_TEXCOORD 16

    struct alignas(16) Vertex {
        glm::vec4 position{};
        glm::vec4 normal{}; 
        glm::vec4 tangent{};
        std::array<glm::vec4, RT_MAXSIZE_NUM_TEXCOORD> texCoord{};

        Vertex() = default;
        Vertex(const glm::vec4& position,
               const glm::vec4& normal,
               const glm::vec4& tangent,
               const std::array<glm::vec4, RT_MAXSIZE_NUM_TEXCOORD>& texCoord);
    };

    struct Triangle {
        std::array<uint32_t, 3> indices;
        uint32_t material;

        Triangle(const std::array<uint32_t, 3>& indices, uint32_t material);
    };

    struct TextureInfo {
        int32_t index{-1};
        int32_t texCoord{-1};
    };

    struct PbrMetallicRoughness {
        glm::vec4 baseColorFactor; 
        TextureInfo baseColorTexture;
        float metallicFactor;
        float roughnessFactor;

        glm::vec2 _pad0;
    
        TextureInfo metallicRoughnessTexture;

        glm::vec4 _pad1;
    };

    struct Material {
        glm::vec4 emissiveFactor;
        PbrMetallicRoughness pbrMetallicRoughness;
    };

    enum class Filter {
        NEAREST,
        LINEAR,
    };

    enum class WrapMode {
        REPEAT,
        CLAMP_TO_EDGE,
        MIRRORED_REPEAT
    };

    struct Texture {
        const uint32_t width{0};
        const uint32_t height{0};
        const std::vector<uint8_t> data;
        const Filter   minFilter{Filter::NEAREST};
        const Filter   magFilter{Filter::NEAREST};
        const Filter   mipmapMode{Filter::NEAREST};
        const WrapMode wrapU{WrapMode::REPEAT};
        const WrapMode wrapV{WrapMode::REPEAT};

        Texture(uint32_t width, uint32_t height, const std::vector<uint8_t> &data,
                const Filter& minFilter, const Filter& magFilter, const Filter& mipmapMode, const WrapMode& wrapU, const WrapMode& wrapV);
    };

    struct Scene {
        const std::vector<Vertex>   vertices;
        const std::vector<Triangle> triangles;
        const std::vector<Material> materials;
        const std::vector<Texture> textures;

        Scene(const std::vector<Vertex>& vertices, const std::vector<Triangle>& triangles,
              const std::vector<Material>& materials, const std::vector<Texture>& texture);
    };
} // rt
