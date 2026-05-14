#pragma once
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>
#include "vulkan/vulkan.hpp"
#include <glm/glm.hpp>

// Shared GPU types used by both BVH (midpoint) and SAHBVH (SAH) implementations.
// Both use the same node layout so the same shader can traverse either tree.

namespace rt::gfx {
#ifndef MAX_TEXTURE_NUMBER
#define MAX_TEXTURE_NUMBER 100
#endif

    // GPU BVH node (32 bytes). Packs two uint fields into .w via bit_cast.
    // aabbMinLeft.xyz  = AABB min
    // aabbMinLeft.w    = bit_cast<float>(leftFirst)
    //                     leaf:     first index in bvhTriIndices
    //                     internal: left child index (right = leftFirst + 1)
    // aabbMaxCount.xyz = AABB max
    // aabbMaxCount.w   = bit_cast<float>(triCount)  (0 = internal, >0 = leaf)
    struct alignas(16) BVHNode {
        glm::vec4 aabbMinLeft;
        glm::vec4 aabbMaxCount;
    };

    struct alignas(16) BvhCameraData {
        glm::mat4 viewInverse;
        glm::mat4 projInverse;
        glm::vec3 position;
        float     _pad0;
    };

    struct alignas(16) BvhSceneSettings {
        BvhCameraData cameraData;
        uint32_t vertexCount;
        uint32_t triangleCount;
        uint32_t emissiveLightCount;
        uint32_t directionalLightCount;
        uint32_t pointLightCount;
        uint32_t spotLightCount;
        uint32_t maxBounces;
        uint32_t samplesPerPixel;
        uint32_t samplesPerEmissiveLight;
        glm::uvec3 _pad0;
    };

    struct BvhBuffer {
        vk::raii::Buffer       buffer = vk::raii::Buffer({});
        vk::raii::DeviceMemory memory = vk::raii::DeviceMemory({});
    };

    struct BvhTextureImage {
        vk::raii::Image        image{nullptr};
        vk::raii::Sampler      sampler{nullptr};
        vk::raii::ImageView    view{nullptr};
        vk::raii::DeviceMemory memory{nullptr};
    };

} // rt::gfx
