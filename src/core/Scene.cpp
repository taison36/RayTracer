#include "Scene.h"
#include <print>

namespace rt {
    Vertex::Vertex(const glm::vec4& position,
                   const glm::vec4& normal,
                   const glm::vec4& tangent,
                   const std::array<glm::vec4, RT_MAXSIZE_NUM_TEXCOORD>& texCoord) :
        position(position), 
        normal(normal),
        tangent(tangent),
        texCoord(texCoord) {
    }

    Triangle::Triangle(const std::array<uint32_t, 3>& indices, uint32_t material)
        : indices(indices), material(material){
    }

    Texture::Texture(uint32_t width, uint32_t height, const std::vector<uint8_t> &data,
                     const Filter& minFilter, const Filter& magFilter, const Filter& mipmapMode, const WrapMode& wrapU, const WrapMode& wrapV)
        : width(width),
          height(height),
          data(data),
          minFilter(minFilter),
          magFilter(magFilter),
          mipmapMode(mipmapMode),
          wrapU(wrapU),
          wrapV(wrapV) {
    }

    Scene::Scene(const std::vector<Vertex>& vertices, const std::vector<Triangle>& triangles,
                 const std::vector<Material>& materials, const std::vector<Texture>& texture)
    : vertices(vertices),
      triangles(triangles),
      materials(materials),
      textures(texture) {
        std::println("Material size: {}", sizeof(Material));
        std::println("PbrMetallicRoughness size: {}", sizeof(PbrMetallicRoughness));
        std::println("TextureInfo size: {}", sizeof(TextureInfo));
    }
} // rt
