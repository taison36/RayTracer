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
                 const std::vector<Material>& materials, const std::vector<Texture>& textures,
                 const std::vector<uint32_t>& emissiveLight, const std::vector<DirectionalLight>& directionalLight,
                 const std::vector<PointLight>& pointLight, const std::vector<SpotLight>& spotLight,
                 const Camera& camera)
    : vertices(vertices),
      triangles(triangles),
      materials(materials),
      textures(textures),
      emissiveLight(emissiveLight),
      directionalLight(directionalLight),
      pointLight(pointLight),
      spotLight(spotLight),
      camera(camera) {
    }

    Scene SceneBuilder::build() {
        return Scene(vertices, triangles, materials, textures, emissiveLight, directionalLight, pointLight, spotLight, camera);
    }
} // rt
