#pragma once
#include "../objects/Camera.h"
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>

namespace rt {

#define RT_MAXSIZE_NUM_TEXCOORD 16

    struct alignas(16) Vertex {
        glm::vec3 position{};
        glm::vec3 normal{}; 
        glm::vec4 tangent{};
        std::array<glm::vec2, RT_MAXSIZE_NUM_TEXCOORD> texCoord{};

        Vertex() = default;
        Vertex(const glm::vec3& position,
               const glm::vec3& normal,
               const glm::vec4& tangent,
               const std::array<glm::vec2, RT_MAXSIZE_NUM_TEXCOORD>& texCoord);
    };

    struct alignas(16) Triangle {
        std::array<uint32_t, 3> indices;
        uint32_t material;

        Triangle(const std::array<uint32_t, 3>& indices, uint32_t material);
    };

    struct alignas(8) TextureInfo {
      int index{-1};
      int texCoord{0}; // The set index of texture's TEXCOORD attribute used for
    };

    struct alignas(16) PbrMetallicRoughness
    {
        glm::vec4   baseColorFactor;
        TextureInfo baseColorTexture;
        int32_t     _pad0[2]; // pad to 16 bytes
        float      metallicFactor{1.0};
        float      roughnessFactor{1.0};
        int32_t    _pad1[2]; // pad to 16 bytes
        TextureInfo metallicRoughnessTexture;
        int32_t _pad2[2]; // pad final 16-byte boundary
    };

    struct alignas(16) Material {
        glm::vec3 emissiveFactor;
        float _pad0;
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
