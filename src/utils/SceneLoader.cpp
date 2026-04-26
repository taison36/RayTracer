#include <cstdint>
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "tiny_gltf.h"
#include "../core/Scene.h"
#include "SceneLoader.h"
#include "glm/gtc/quaternion.hpp"

#include <print>
#include <cstdlib>
#include <complex>
#include <stdexcept>
#include <unordered_set>

namespace rt {
    enum class AccessorBasisDataType {
        BYTE,
        UNSIGNED_BYTE,
        SHORT,
        UNSIGNED_SHORT,
        INT,
        UNSIGNED_INT,
        FLOAT,
        DOUBLE
    };

    enum class AccessorClassDataType {
        SCALAR,
        VEC2,
        VEC3,
        VEC4,
        MAT2,
        MAT3,
        MAT4
    };

    bool checkForExpectedTypes(const tinygltf::Accessor &tinyAccessor, const AccessorClassDataType &classDataType,
                               const AccessorBasisDataType &basisDataType) {
        AccessorBasisDataType tinyAccessorBasisDataType;
        AccessorClassDataType tinyAccessorClassDataType;
        switch (tinyAccessor.componentType) {
            case TINYGLTF_COMPONENT_TYPE_BYTE: tinyAccessorBasisDataType = AccessorBasisDataType::BYTE;
                break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                tinyAccessorBasisDataType = AccessorBasisDataType::UNSIGNED_BYTE;
                break;
            case TINYGLTF_COMPONENT_TYPE_SHORT: tinyAccessorBasisDataType = AccessorBasisDataType::SHORT;
                break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                tinyAccessorBasisDataType = AccessorBasisDataType::UNSIGNED_SHORT;
                break;
            case TINYGLTF_COMPONENT_TYPE_INT: tinyAccessorBasisDataType = AccessorBasisDataType::INT;
                break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: tinyAccessorBasisDataType = AccessorBasisDataType::UNSIGNED_INT;
                break;
            case TINYGLTF_COMPONENT_TYPE_FLOAT: tinyAccessorBasisDataType = AccessorBasisDataType::FLOAT;
                break;
            case TINYGLTF_COMPONENT_TYPE_DOUBLE: tinyAccessorBasisDataType = AccessorBasisDataType::DOUBLE;
                break;
            default: throw std::runtime_error("[ERROR] unexpected accessor basis data type");
        }

        switch (tinyAccessor.type) {
            case TINYGLTF_TYPE_VEC2: tinyAccessorClassDataType = AccessorClassDataType::VEC2;
                break;
            case TINYGLTF_TYPE_VEC3: tinyAccessorClassDataType = AccessorClassDataType::VEC3;
                break;
            case TINYGLTF_TYPE_VEC4: tinyAccessorClassDataType = AccessorClassDataType::VEC4;
                break;
            case TINYGLTF_TYPE_MAT2: tinyAccessorClassDataType = AccessorClassDataType::MAT2;
                break;
            case TINYGLTF_TYPE_MAT3: tinyAccessorClassDataType = AccessorClassDataType::MAT3;
                break;
            case TINYGLTF_TYPE_MAT4: tinyAccessorClassDataType = AccessorClassDataType::MAT4;
                break;
            case TINYGLTF_TYPE_SCALAR: tinyAccessorClassDataType = AccessorClassDataType::SCALAR;
                break;
            default: throw std::runtime_error("[ERROR] unexpected accessor class data type");
        }

        return tinyAccessorClassDataType == classDataType && tinyAccessorBasisDataType == basisDataType;
    }

    static std::string GetFilePathExtension(const std::string &filePath) {
        if (filePath.find_last_of('.') != std::string::npos)
            return filePath.substr(filePath.find_last_of('.') + 1);
        return "";
    }

    tinygltf::Model getGLTFModel(const std::string &path) {
        tinygltf::Model model;
        tinygltf::TinyGLTF loader;
        std::string err;
        std::string warn;
        const std::string ext = GetFilePathExtension(path);

        bool ret = false;
        if (ext == "glb") {
            // assume binary glTF.
            ret = loader.LoadBinaryFromFile(&model, &err, &warn, path);
        } else {
            // ascii/json glTF.
            ret = loader.LoadASCIIFromFile(&model, &err, &warn, path);
        }

        if (!warn.empty()) {
            std::println("[WARN] glTF parse warning: {}", warn);
        }

        if (!err.empty()) {
            std::println("[ERROR] glTF parse error: {}", err);
        }
        if (!ret) {
            throw std::runtime_error("[ERROR] Failed to load glTF: " + path);
        }

        return model;
    }

    glm::mat4 extractTransformMatrix(const tinygltf::Node &node) {
        if (node.matrix.size() == 16) {
            return {
                node.matrix[0], node.matrix[1], node.matrix[2], node.matrix[3],
                node.matrix[4], node.matrix[5], node.matrix[6], node.matrix[7],
                node.matrix[8], node.matrix[9], node.matrix[10], node.matrix[11],
                node.matrix[12], node.matrix[13], node.matrix[14], node.matrix[15]
            };
        }

        glm::vec3 t(0.0f);
        if (node.translation.size() == 3) {
            t = {
                static_cast<float>(node.translation[0]), static_cast<float>(node.translation[1]),
                static_cast<float>(node.translation[2])
            };
        }

        glm::quat q(1.0f, 0.0f, 0.0f, 0.0f);
        if (node.rotation.size() == 4) {
            // glTF is x,y,z,w. GLM quat ctor is w,x,y,z
            q = glm::quat(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
        }
        glm::vec3 s(1.0f);
        if (node.scale.size() == 3) {
            s = {
                static_cast<float>(node.scale[0]), static_cast<float>(node.scale[1]), static_cast<float>(node.scale[2])
            };
        }

        const glm::mat4 T = glm::translate(glm::mat4(1.0f), t);
        const glm::mat4 R = glm::mat4_cast(q);
        const glm::mat4 S = glm::scale(glm::mat4(1.0f), s);

        return T * R * S;
    }

    template<int L, typename T>
    std::vector<glm::vec<L, T> > readAccessorVec(const tinygltf::Model &model,
                                                 const tinygltf::Accessor &accessor) {
        const auto &bufferView = model.bufferViews[accessor.bufferView];
        const auto &buffer = model.buffers[bufferView.buffer];

        const unsigned char *basePtr =
                buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;

        size_t stride = bufferView.byteStride;
        if (stride == 0) {
            stride = sizeof(T) * L;
        }

        std::vector<glm::vec<L, T> > result(accessor.count);

        for (size_t i = 0; i < accessor.count; ++i) {
            const T *elem = reinterpret_cast<const T *>(basePtr + i * stride);

            for (int j = 0; j < L; ++j) {
                result[i][j] = elem[j];
            }
        }

        return result;
    }

    template<typename T>
    std::vector<T> readAccessorScalar(const tinygltf::Model &model,
                                      const tinygltf::Accessor &accessor) {
        const auto &bufferView = model.bufferViews[accessor.bufferView];
        const auto &buffer     = model.buffers[bufferView.buffer];

        const unsigned char *basePtr =
                buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;

        size_t stride = bufferView.byteStride;
        if (stride == 0) {
            stride = sizeof(T);
        }

        std::vector<T> result(accessor.count);

        for (size_t i = 0; i < accessor.count; ++i) {
            const T *elem = reinterpret_cast<const T *>(basePtr + i * stride);
            result[i] = *elem;
        }

        return result;
    }

    std::vector<Triangle> readPrimitiveIndices(const tinygltf::Primitive &tinyPrimitive, const tinygltf::Model &tinyModel, const uint32_t offsetIndexBegin) {
        std::vector<Triangle> triangles;
        const auto &tinyAccessor = tinyModel.accessors[tinyPrimitive.indices];

        switch (tinyAccessor.componentType) {
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
                const auto bytes = readAccessorScalar<uint8_t>(tinyModel, tinyAccessor);
                for (size_t i = 0; i < bytes.size(); i += 3) {
                    triangles.emplace_back(
                        std::array{
                            offsetIndexBegin + static_cast<uint32_t>(bytes[i]),
                            offsetIndexBegin + static_cast<uint32_t>(bytes[i + 1]),
                            offsetIndexBegin + static_cast<uint32_t>(bytes[i + 2])
                        },
                        tinyPrimitive.material
                    );
                }

                break;
            }
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
                const auto shorts = readAccessorScalar<uint16_t>(tinyModel, tinyAccessor);
                for (size_t i = 0; i < shorts.size(); i += 3) {
                    triangles.emplace_back(
                        std::array{
                            offsetIndexBegin + static_cast<uint32_t>(shorts[i]),
                            offsetIndexBegin + static_cast<uint32_t>(shorts[i + 1]),
                            offsetIndexBegin + static_cast<uint32_t>(shorts[i + 2])
                        },
                        tinyPrimitive.material
                    );
                }
                break;
            }
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
                const auto indices = readAccessorScalar<uint32_t>(tinyModel, tinyAccessor);
                for (size_t i = 0; i < indices.size(); i += 3) {
                    triangles.emplace_back(
                        std::array{
                            offsetIndexBegin + indices[i],
                            offsetIndexBegin + indices[i + 1],
                            offsetIndexBegin + indices[i + 2]
                        },
                       tinyPrimitive.material
                    );
                }
                break;
            }
            default:
                throw std::runtime_error("[ERROR] unexpected accessor basis data type for indices");
        }

        return triangles;
    }

    std::vector<glm::vec3> buildNormals( const std::vector<Triangle>& triangles, const std::vector<glm::vec3>& positions) {
        std::vector normals(positions.size(), glm::vec3(0.0f));
    
        for (const Triangle& tri : triangles) {
            uint32_t i0 = tri.indices[0];
            uint32_t i1 = tri.indices[1];
            uint32_t i2 = tri.indices[2];
    
            glm::vec3 p0 = positions[i0];
            glm::vec3 p1 = positions[i1];
            glm::vec3 p2 = positions[i2];
    
            glm::vec3 e1 = p1 - p0;
            glm::vec3 e2 = p2 - p0;
    
            glm::vec3 faceNormal = glm::cross(e1, e2);
    
            if (glm::length(faceNormal) < 1e-8f)
                continue;
    
            normals[i0] += faceNormal;
            normals[i1] += faceNormal;
            normals[i2] += faceNormal;
        }
    
        for (auto& n : normals) {
            if (glm::length(n) > 1e-8f)
                n = glm::normalize(n);
            else
                n = glm::vec3(0, 1, 0); // fallback
        }
    
        return normals;
    }

    void convertTrianglePrimitive(const tinygltf::Primitive &tinyPrimitive, const tinygltf::Model &tinyModel,
                                  const glm::mat4 &worldMatrix, SceneBuilder& sceneBuilder) {
        if (tinyPrimitive.mode != TINYGLTF_MODE_TRIANGLES) {
            throw std::runtime_error("[ERROR] Only triangle primitives are supported");
        }
        uint32_t indexOffset = sceneBuilder.vertices.size();
        // need to shift indeces because they are zero-based and vertices can have already some data
        std::vector<Triangle> newTriangles = readPrimitiveIndices(tinyPrimitive, tinyModel, indexOffset);
        sceneBuilder.triangles.insert(
            sceneBuilder.triangles.end(),
            newTriangles.begin(),
            newTriangles.end()
        );
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec4> tangents;
        std::array<std::vector<glm::vec2>, RT_MAXSIZE_NUM_TEXCOORD> textCoordinates;

        for (auto &[attributeKey, accessorIndex]: tinyPrimitive.attributes) {
            if (attributeKey == "POSITION") {
                if (!checkForExpectedTypes(tinyModel.accessors[accessorIndex], AccessorClassDataType::VEC3,
                                           AccessorBasisDataType::FLOAT)) {
                    throw std::runtime_error("[ERROR] unexpected primitve attribute type");
                }
                positions = readAccessorVec<3, float>(tinyModel, tinyModel.accessors[accessorIndex]);
            } else if (attributeKey == "NORMAL") {
                if (!checkForExpectedTypes(tinyModel.accessors[accessorIndex], AccessorClassDataType::VEC3,
                                           AccessorBasisDataType::FLOAT)) {
                    throw std::runtime_error("[ERROR] unexpected primitve attribute type");
                }
                normals = readAccessorVec<3, float>(tinyModel, tinyModel.accessors[accessorIndex]);
            } else if (attributeKey == "TANGENT") {
                if (!checkForExpectedTypes(tinyModel.accessors[accessorIndex], AccessorClassDataType::VEC4,
                                           AccessorBasisDataType::FLOAT)) {
                    throw std::runtime_error("[ERROR] unexpected primitve attribute type");
                }
                tangents = readAccessorVec<4, float>(tinyModel, tinyModel.accessors[accessorIndex]);
            } else if (attributeKey.starts_with("TEXCOORD_")){
                if (!checkForExpectedTypes(tinyModel.accessors[accessorIndex], AccessorClassDataType::VEC2,
                                           AccessorBasisDataType::FLOAT)) {
                    throw std::runtime_error("[ERROR] unexpected primitve attribute type");
                }
                std::string startStr = "TEXCOORD_";
                size_t pos = attributeKey.rfind(startStr);
                if (pos == std::string::npos) {
                    throw std::runtime_error("Invalid TEXCOORD key: " + attributeKey);
                }

                std::string indexStr = attributeKey.substr(pos + startStr.size());

                const size_t texCoordIndex = std::stoi(indexStr);

                if (texCoordIndex >= RT_MAXSIZE_NUM_TEXCOORD) {
                    std::println("[ERROR] max TEXCOORD_N was exceeded, TEXCOORD_{}, although max size is: {}",
                                 texCoordIndex, RT_MAXSIZE_NUM_TEXCOORD);
                    throw std::runtime_error("[ERROR] max TEXCOORD_N was exceeded");
                }

                textCoordinates[texCoordIndex] = readAccessorVec<2, float>(
                    tinyModel, tinyModel.accessors[accessorIndex]);
            }else {
                std::println("[WARN] unexpected primitve attribute type: {}. Wasn't processed and got skipped", attributeKey);
            }
        }

        assert(!positions.empty());
        for (auto& pos : positions) {
            pos = glm::vec3(worldMatrix * glm::vec4(pos, 1.0f));
        }

        for (size_t i = 0; i < positions.size(); ++i) {
            Vertex v;
            v.position = glm::vec4(positions[i], 0.0f);
            v.normal   = glm::vec4(normals[i], 0.0f);
            v.tangent  = tangents.empty() ? glm::vec4(0.0f) : tangents[i];
            for (int j = 0; j < RT_MAXSIZE_NUM_TEXCOORD; ++j) {
                if (textCoordinates[j].empty()) {
                    continue;
                }
                v.texCoord[j] = glm::vec4(textCoordinates[j][i], 0.0f, 0.0f);
            }

            sceneBuilder.vertices.push_back(v);
        }
    }

    Camera convertCamera(const tinygltf::Camera &tinyCam, const glm::mat4 &worldMatrix) {
        const glm::vec3 position = glm::vec3(worldMatrix[3]);
        const glm::vec3 up       = glm::normalize(glm::vec3(worldMatrix[1]));
        const glm::vec3 forward  = -glm::normalize(glm::vec3(worldMatrix[2]));
        const float yaw          = glm::degrees(atan2(forward.z, forward.x));
        const float pitch        = glm::degrees(asin(forward.y));

        auto builder = Camera::Builder().setPosition(position)
                                        .setUp(up)
                                        .setYaw(yaw)
                                        .setPitch(pitch);

        if (tinyCam.type == "perspective") {
            const float zoom = glm::degrees(tinyCam.perspective.yfov);
            builder = builder.setZoom(zoom);
        }

        std::println("[INFO] Using gltf provided camera");

        return builder.build();
    }

    void convertLight(const tinygltf::Light &tinyLight, const glm::mat4 &worldMatrix,
                      SceneBuilder& sceneBuilder) {
        glm::vec4 position  = glm::vec4(glm::vec3(worldMatrix[3]), 1.0f);
        glm::vec4 direction = glm::vec4(-glm::normalize(glm::vec3(worldMatrix[2])), 0.0f);
        glm::vec4 color     = glm::vec4(
            tinyLight.color[0],
            tinyLight.color[1],
            tinyLight.color[2],
            1.0f
        );
        if (tinyLight.type == "point") {
            PointLight point = {
                .position  = position,
                .color     = color,
                .intensity = static_cast<float>(tinyLight.intensity),
                .range     = static_cast<float>(tinyLight.range)
            };
            sceneBuilder.pointLight.push_back(point);
        } else if (tinyLight.type == "spot") {
            SpotLight spot = {
                .position       = position,
                .color          = color,
                .direction      = direction,
                .intensity = static_cast<float>(tinyLight.intensity),
                .range     = static_cast<float>(tinyLight.range),
                .innerConeAngle = static_cast<float>(tinyLight.spot.innerConeAngle),
                .outerConeAngle = static_cast<float>(tinyLight.spot.outerConeAngle)
            };
            sceneBuilder.spotLight.push_back(spot);
        } else if (tinyLight.type == "directional") {
                DirectionalLight directional = {
                    .color     = color,
                    .direction = direction,
                    .intensity = static_cast<float>(tinyLight.intensity),
                    .range     = static_cast<float>(tinyLight.range)
                };
                sceneBuilder.directionalLight.push_back(directional);
        }
    }

    void dfsNode(const tinygltf::Model &tinyModel, const tinygltf::Node &tinyNode, const glm::mat4 &parentWorldMatrix,
                 SceneBuilder& sceneBuilder) {
        const auto worldMatrix = parentWorldMatrix * extractTransformMatrix(tinyNode);

        if (tinyNode.mesh != -1) {
            for (auto &tinyPrimitive: tinyModel.meshes[tinyNode.mesh].primitives) {
                convertTrianglePrimitive(tinyPrimitive, tinyModel, worldMatrix, sceneBuilder);
            }
        }

        if (tinyNode.camera != -1) {
            const auto tinyCam = tinyModel.cameras[tinyNode.camera];
            sceneBuilder.camera = convertCamera(tinyCam, worldMatrix);
        }

        if (tinyNode.light != -1) {
            const auto tinyLight = tinyModel.lights[tinyNode.light];
            convertLight(tinyLight, worldMatrix, sceneBuilder);
        }

        for (const int child: tinyNode.children) {
            dfsNode(tinyModel, tinyModel.nodes[child], worldMatrix, sceneBuilder);
        }
    }

    Material convertMaterial(const tinygltf::Material &tm) {
        Material material;

        material.emissiveFactor = glm::vec4(
            static_cast<float>(tm.emissiveFactor[0]),
            static_cast<float>(tm.emissiveFactor[1]),
            static_cast<float>(tm.emissiveFactor[2]),
            0.0f
        );

        const auto &pbr = tm.pbrMetallicRoughness;

        const PbrMetallicRoughness pbrMetallicRoughness = {
            .baseColorFactor{
                static_cast<float>(pbr.baseColorFactor[0]),
                static_cast<float>(pbr.baseColorFactor[1]),
                static_cast<float>(pbr.baseColorFactor[2]),
                static_cast<float>(pbr.baseColorFactor[3])
            },
            .baseColorTexture = {
                .index    = pbr.baseColorTexture.index,
                .texCoord = pbr.baseColorTexture.texCoord
            },
            .metallicFactor = static_cast<float>(pbr.metallicFactor),
            .roughnessFactor = static_cast<float>(pbr.roughnessFactor),
            .metallicRoughnessTexture = {
                .index = pbr.metallicRoughnessTexture.index,
                .texCoord = pbr.metallicRoughnessTexture.texCoord
            }
        };
        material.pbrMetallicRoughness = pbrMetallicRoughness;
        material.emissiveTexture = {
            .index    = tm.emissiveTexture.index,
            .texCoord = tm.emissiveTexture.texCoord
        };

        return material;
    }

    Texture convertTexture(const std::string& path, const tinygltf::Model &tinyModel, const tinygltf::Texture &tinyTexture) {
        int width, height, channels;
        const auto &tinyImage = tinyModel.images[tinyTexture.source];
        assert(!tinyImage.uri.empty());
        unsigned char *rawData = stbi_load((path + tinyImage.uri).c_str(), &width, &height,
                                           &channels, 4);

        assert(width == tinyImage.width);
        assert(height == tinyImage.height);

        if (!rawData) {
            std::println("[ERROR] Could not load the file {}", tinyImage.uri);
            throw std::runtime_error("[ERROR] Could not load the file");
        }

        size_t dataSize = static_cast<size_t>(width) * height * 4;

        std::vector<uint8_t> data(rawData, rawData + dataSize);

        stbi_image_free(rawData);

        const tinygltf::Sampler &tinySampler = tinyModel.samplers[tinyTexture.sampler];

        Filter minFilter = Filter::NEAREST;
        Filter mipmapMode = Filter::NEAREST;
        Filter magFilter = Filter::NEAREST;

        switch (tinySampler.minFilter) {
            case TINYGLTF_TEXTURE_FILTER_NEAREST:
            case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
                minFilter = Filter::NEAREST;
                mipmapMode = Filter::NEAREST;
                break;
            case TINYGLTF_TEXTURE_FILTER_LINEAR:
            case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
                minFilter = Filter::LINEAR;
                mipmapMode = Filter::LINEAR;
                break;
            case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
                minFilter = Filter::LINEAR;
                mipmapMode = Filter::NEAREST;
                break;
            case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
                minFilter = Filter::NEAREST;
                mipmapMode = Filter::LINEAR;
                break;
            default:
                std::println("[WARN] tinySampler.minFilter is not set. Using default");
                break;
        }

        switch (tinySampler.magFilter) {
            case TINYGLTF_TEXTURE_FILTER_NEAREST:
                magFilter = Filter::NEAREST;
                break;
            case TINYGLTF_TEXTURE_FILTER_LINEAR:
                magFilter = Filter::LINEAR;
                break;
            default:
                std::println("[WARN] tinySampler.magFilter is not set. Using default");
                break;
        }

        WrapMode wrapU{WrapMode::REPEAT};
        WrapMode wrapV{WrapMode::REPEAT};

        switch (tinySampler.wrapS) {
            case TINYGLTF_TEXTURE_WRAP_REPEAT:
                wrapU = WrapMode::REPEAT;
                break;
            case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
                wrapU = WrapMode::CLAMP_TO_EDGE;
                break;
            case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
                wrapU = WrapMode::MIRRORED_REPEAT;
            default:
                std::println("[WARN] tinySampler.wrapS is not set. Using default");
                break;
        }

        switch (tinySampler.wrapT) {
            case TINYGLTF_TEXTURE_WRAP_REPEAT:
                wrapV = WrapMode::REPEAT;
                break;
            case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
                wrapV = WrapMode::CLAMP_TO_EDGE;
                break;
            case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
                wrapV = WrapMode::MIRRORED_REPEAT;
            default:
                std::println("[WARN] tinySampler.wrapT is not set. Using default");
                break;
        }

        return {
            static_cast<uint32_t>(width), static_cast<uint32_t>(height), data, minFilter, magFilter, mipmapMode, wrapU,
            wrapV
        };
    }

    bool hasEmissiveLight(const Triangle& triangle, const std::vector<Material>& materials) {
        const auto& mat = materials[triangle.material];
        return mat.emissiveFactor.x > 0 ||
               mat.emissiveFactor.y > 0 ||
               mat.emissiveFactor.z > 0 ||
               mat.emissiveTexture.index != -1;
    }

    Scene SceneLoader::loadScene(const std::string &path) {
        auto tinyModel = getGLTFModel(path + "/scene.gltf");
        constexpr float SCALE = 1.0015f;
        glm::mat4 worldMatrix{1.0f};
        worldMatrix = glm::scale(worldMatrix, glm::vec3(SCALE));

        SceneBuilder sceneBuilder; 

        int defaultScene = tinyModel.defaultScene != -1 ? tinyModel.defaultScene : 0;
        for (const auto &tinyScene = tinyModel.scenes[defaultScene]; int rootNodeIndex: tinyScene.nodes) {
            dfsNode(tinyModel, tinyModel.nodes[rootNodeIndex], worldMatrix, sceneBuilder);
        }

        for (const auto &tinyMaterial: tinyModel.materials) {
            sceneBuilder.materials.push_back(convertMaterial(tinyMaterial));
        }

        for (const auto &tinyTexture: tinyModel.textures) {
            sceneBuilder.textures.push_back(convertTexture(path, tinyModel, tinyTexture));
        }

        for (size_t i = 0; i < sceneBuilder.triangles.size(); ++i) {
            if (hasEmissiveLight(sceneBuilder.triangles[i], sceneBuilder.materials)) {
                sceneBuilder.emissiveLight.push_back(i);
            }
        }
        
        return sceneBuilder.build();
    }
} //rt
