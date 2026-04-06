#pragma once
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include"VkCore.h"
#include "../../objects/ScreenSettings.h"
#include <vulkan/vulkan_raii.hpp>

namespace rt::gfx {
    #ifdef NDEBUG
        constexpr bool enableValidationLayers = false;
    #else
        static constexpr bool enableValidationLayers = true;
    #endif

    struct OutputImage {
            vk::raii::Image image{nullptr};
            vk::raii::ImageView view{nullptr};
            vk::raii::DeviceMemory memory{nullptr};
    };

    class VkCore {
        vk::raii::Instance               instance{nullptr};
        vk::raii::Context                context{};
        vk::raii::Device                 device{nullptr};
        vk::raii::PhysicalDevice         physicalDevice{nullptr};
        vk::raii::Queue                  queue{nullptr};
        vk::raii::CommandPool            commandPool{nullptr};
        vk::raii::DebugUtilsMessengerEXT debugMessenger{nullptr};
        uint32_t                         queueFamilyIndex;

        void init();
        void createInstance();
        void setupDebugMessenger();
        void pickPhysicalDevice();
        void createLogicalDevice();
        void createCommandPool();
    public:
        VkCore();
        std::unique_ptr<vk::raii::CommandBuffer> beginSingleTimeCommands() const;
        void endSingleTimeCommands(vk::raii::CommandBuffer &commandBuffer) const;
        void transitionImageLayout(const vk::raii::Image &image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout) const;
        void copyImageToBuffer(const vk::raii::Image &image, vk::raii::Buffer &buffer, const uint32_t HEIGHT, const uint32_t WIDTH) const;
        void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties,
                          vk::raii::Buffer &buffer, vk::raii::DeviceMemory &bufferMemory) const;
        [[nodiscard]] uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const;
        [[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const;
        [[nodiscard]] const vk::raii::Device& getDevice() const;
    };

} //rt::gfx
