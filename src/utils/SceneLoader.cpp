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

        //root node
        if (node.scale.size() != 3 || node.translation.size() != 3 || node.rotation.size() != 4) {
            return {1.0f};
        }

        const glm::vec3 t{
            static_cast<float>(node.translation[0]), static_cast<float>(node.translation[1]),
            static_cast<float>(node.translation[2])
        };
        const glm::quat q{
            static_cast<float>(node.rotation[0]), static_cast<float>(node.rotation[1]),
            static_cast<float>(node.rotation[2]), static_cast<float>(node.rotation[3])
        };
        const glm::vec3 s{
            static_cast<float>(node.scale[0]), static_cast<float>(node.scale[1]), static_cast<float>(node.scale[2])
        };

        const glm::mat4 T = glm::translate(glm::mat4{}, t);
        const glm::mat4 R = glm::mat4_cast(q);
        const glm::mat4 S = glm::scale(glm::mat4{}, s);

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
        const auto &buffer = model.buffers[bufferView.buffer];

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

    std::vector<uint32_t> readPrimitiveIndices(const tinygltf::Primitive &tinyPrimitive,
                                               const tinygltf::Model &tinyModel) {
        const auto &tinyAccessor = tinyModel.accessors[tinyPrimitive.indices];
        std::vector<uint32_t> indices;

        switch (tinyAccessor.componentType) {
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
                auto bytes = readAccessorScalar<uint8_t>(tinyModel, tinyAccessor);
                indices.resize(bytes.size());
                std::ranges::transform(bytes, indices.begin(),
                                       [](const uint8_t b) { return static_cast<uint32_t>(b); });
                break;
            }
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
                auto shorts = readAccessorScalar<uint16_t>(tinyModel, tinyAccessor);
                indices.resize(shorts.size());
                std::ranges::transform(shorts, indices.begin(),
                                       [](const uint16_t s) { return static_cast<uint32_t>(s); });
                break;
            }
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
                indices = readAccessorScalar<uint32_t>(tinyModel, tinyAccessor);
                break;
            }
            default:
                throw std::runtime_error("[ERROR] unexpected accessor basis data type for indices");
        }

        return indices;
    }

    TrianglePrimitive convertTrianglePrimitive(const tinygltf::Primitive &tinyPrimitive,
                                               const tinygltf::Model &tinyModel, const glm::mat4 &worldMatrix) {
        if (tinyPrimitive.mode != TINYGLTF_MODE_TRIANGLES) {
            throw std::runtime_error("[ERROR] Only triangle primitives are supported");
        }

        //TODO apply worldMatrix and not just store it
        std::vector<uint32_t> indices = readPrimitiveIndices(tinyPrimitive, tinyModel);
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec4> tangents;
        std::vector<std::vector<glm::vec2> > textCoordinates;

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
            } else {
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

                const int texCoordIndex = std::stoi(indexStr);
                if (textCoordinates.size() <= texCoordIndex) textCoordinates.resize(texCoordIndex + 1);
                textCoordinates[texCoordIndex] = readAccessorVec<2, float>(
                    tinyModel, tinyModel.accessors[accessorIndex]);
            }
        }

        // Sanity check
        for (const auto idx: indices) {
            if (idx >= positions.size()) {
                throw std::runtime_error("[ERROR] index out of bounds in primitive: " + std::to_string(idx));
            }
        }

        //convert position into world spac
        for (auto& pos : positions) {
            pos = glm::vec3(worldMatrix * glm::vec4(pos, 1.0f));
        }
        return {indices, positions, normals, tangents, textCoordinates};
    }

    void dfsNode(const tinygltf::Model &tinyModel, std::vector<TrianglePrimitive> &primitives,
                 const tinygltf::Node &tinyNode, const glm::mat4 &parentWorldMatrix) {
        const auto worldMatrix = parentWorldMatrix * extractTransformMatrix(tinyNode);

        if (tinyNode.mesh != -1) {
            for (auto &tinyPrimitive: tinyModel.meshes[tinyNode.mesh].primitives) {
                primitives.push_back(convertTrianglePrimitive(tinyPrimitive, tinyModel, worldMatrix));
            }
        }

        for (const int child: tinyNode.children) {
            dfsNode(tinyModel, primitives, tinyModel.nodes[child], worldMatrix);
        }
    }


    Scene SceneLoader::loadScene(const std::string &path, const Camera &camera) {
        auto tinyModel = getGLTFModel(path);
        constexpr float SCALE = 0.01f;
        glm::mat4 worldMatrix{1.0f};
        worldMatrix = glm::scale(worldMatrix, glm::vec3(SCALE));

        std::vector<TrianglePrimitive> primitives;
        int defaultScene = tinyModel.defaultScene != -1 ? tinyModel.defaultScene : 0;
        for (const auto &tinyScene = tinyModel.scenes[defaultScene]; int rootNodeIndex: tinyScene.nodes) {
            dfsNode(tinyModel, primitives, tinyModel.nodes[rootNodeIndex], worldMatrix);
        }

        Scene scene(primitives, camera);
        return scene;
    }
} //rt
