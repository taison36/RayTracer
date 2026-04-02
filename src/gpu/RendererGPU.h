#pragma once
#include <optional>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include "../ScreenSettings.h"
#include "../FrameBuffer.h"
#include <vulkan/vulkan_raii.hpp>
#include <memory>

namespace rt {

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
static constexpr bool enableValidationLayers = true;
#endif

struct OutputImage {
    vk::raii::Image image = nullptr;
    vk::raii::ImageView view = nullptr;
    vk::raii::DeviceMemory memory = nullptr;
};

class RendererGPU {
    const ScreenSettings                 screenSettings;
    vk::raii::Instance                   instance = nullptr;
    vk::raii::Context                    context{};
    vk::raii::Device                     device = nullptr;
    vk::raii::PhysicalDevice             physicalDevice = nullptr;
    vk::raii::DebugUtilsMessengerEXT     debugMessenger = nullptr;
    vk::raii::CommandPool                commandPool = nullptr;
    vk::raii::CommandBuffer              commandBuffer = nullptr;
    vk::raii::Queue                      computeQueue = nullptr;
    vk::raii::PipelineLayout             computePipelineLayout = nullptr;
    vk::raii::DescriptorPool             computeDescriptorPool = nullptr;
    vk::raii::DescriptorSetLayout        computeDescriptorSetLayout = nullptr;
    std::vector<vk::raii::DescriptorSet> computeDescriptorSets;
    vk::raii::Pipeline                   computePipeline = nullptr;
    uint32_t                             computeFamilyQueueIndex = -1;
    vk::raii::Fence                      executionFinishedFence = nullptr;
    OutputImage outputImage{};

    void initVulkan();
    void createInstance();
    void setupDebugMessenger();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createOutputImage();
    void createDescriptors();
    void createComputePipeline();
    void createCommandPool();
    void createCommandBuffer();
    void recordCommandBuffer();
    void createFences();
    [[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const;
    [[nodiscard]] uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const;
    void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties,
            vk::raii::Buffer &buffer, vk::raii::DeviceMemory &bufferMemory) const;
    std::unique_ptr<vk::raii::CommandBuffer> beginSingleTimeCommands();
    void endSingleTimeCommands(vk::raii::CommandBuffer &commandBuffer);
    void transitionImageLayout(const vk::raii::Image &image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout);
    void copyImageToBuffer(const vk::raii::Image &image, vk::raii::Buffer &buffer);
public:
    RendererGPU(const ScreenSettings& screenSettings);

    void run();

    void extractImage();

    void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties,
                      vk::raii::Buffer &buffer, vk::raii::DeviceMemory &bufferMemory);
};

}//rt
