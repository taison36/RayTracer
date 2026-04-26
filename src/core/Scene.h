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

    struct alignas(16) Triangle {
        std::array<uint32_t, 3> indices;
        uint32_t material;

        Triangle(const std::array<uint32_t, 3>& indices, uint32_t material);
    };

    struct alignas(16) TextureInfo {
        int32_t   index{-1};
        int32_t   texCoord{-1};
        glm::vec2 pad_0;
    };

    struct alignas(16) PbrMetallicRoughness {
        glm::vec4   baseColorFactor; 
        TextureInfo baseColorTexture;
        float       metallicFactor;
        float       roughnessFactor;
        glm::vec2   pad_0;
    
        TextureInfo metallicRoughnessTexture;
        glm::vec4   pad_1;
    };

    struct Material {
        glm::vec4            emissiveFactor;
        PbrMetallicRoughness pbrMetallicRoughness;
        TextureInfo          emissiveTexture;
    };

    // DirectionalLight - Needs padding
    struct alignas(16) DirectionalLight {
        glm::vec4 color;        // 16 bytes, offset 0
        glm::vec4 direction;    // 16 bytes, offset 16
        float     intensity;    // 4 bytes, offset 32
        float     range;        // 4 bytes, offset 36
        glm::vec2 _pad0;        // 8 bytes, offset 40
        // Total: 48 bytes
    };
    
    // PointLight - Needs padding
    struct alignas(16) PointLight {
        glm::vec4 position;     // 16 bytes, offset 0
        glm::vec4 color;        // 16 bytes, offset 16
        float     intensity;    // 4 bytes, offset 32
        float     range;        // 4 bytes, offset 36
        glm::vec2 _pad0;        // 8 bytes, offset 40
        // Total: 48 bytes
    };

    // SpotLight - Already correct
    struct alignas(16) SpotLight {
        glm::vec4 position;         // 16 bytes, offset 0
        glm::vec4 color;            // 16 bytes, offset 16
        glm::vec4 direction;        // 16 bytes, offset 32
        float     intensity;        // 4 bytes, offset 48
        float     range;            // 4 bytes, offset 52
        float     innerConeAngle;   // 4 bytes, offset 56
        float     outerConeAngle;   // 4 bytes, offset 60
        // Total: 64 bytes
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
        const std::vector<Texture>  textures;

        const std::vector<uint32_t>         emissiveLight;
        const std::vector<DirectionalLight> directionalLight;
        const std::vector<PointLight>       pointLight;
        const std::vector<SpotLight>        spotLight;

        const Camera camera;


        Scene(const std::vector<Vertex>& vertices, const std::vector<Triangle>& triangles,
              const std::vector<Material>& materials, const std::vector<Texture>& textures,
              const std::vector<uint32_t>& emissiveLight, const std::vector<DirectionalLight>& directionalLight,
              const std::vector<PointLight>& pointLight, const std::vector<SpotLight>& spotLight,
              const Camera& camera);
    };

    struct SceneBuilder {
        std::vector<Vertex>   vertices;
        std::vector<Triangle> triangles;
        std::vector<Material> materials;
        std::vector<Texture>  textures;

        std::vector<uint32_t>         emissiveLight;
        std::vector<DirectionalLight> directionalLight;
        std::vector<PointLight>       pointLight;
        std::vector<SpotLight>        spotLight;

        Camera camera{};

        Scene build();
    };

} // rt
