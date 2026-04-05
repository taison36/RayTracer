#include "RendererGPU.h"
#include "vulkan/vulkan.hpp"
#include "vulkan/vulkan_beta.h"
#include "../../utils/utils.h"
#include "../../objects/ScreenSettings.h"
#include <map>
#include <print>
#include <fstream>
#include <cstdint>
#include <iostream>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_to_string.hpp>

namespace rt::gpu {
    RendererGPU::RendererGPU(const ScreenSettings& screenSettings) : screenSettings(screenSettings) {
        initVulkan();
    }

    void RendererGPU::initVulkan() {
        createInstance();
        setupDebugMessenger();
        pickPhysicalDevice();
        createLogicalDevice();
    
        createFences();
    
        createCommandPool();
        createCommandBuffer();
    
        createOutputImage();
        createDescriptors();
        createComputePipeline();
    }
    
    void RendererGPU::createInstance() {
        constexpr vk::ApplicationInfo appInfo{.pApplicationName   = "Ray Tracer",
                                              .applicationVersion = VK_MAKE_VERSION( 1, 0, 0 ),
                                              .pEngineName        = "No Engine",
                                              .engineVersion      = VK_MAKE_VERSION( 1, 0, 0 ),
                                              .apiVersion         = vk::ApiVersion14
        };
        //MAC specific extensions
        std::vector<const char*> requiredExtensions = {VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME};
    
        const std::vector<const char *> validationLayers = {
            "VK_LAYER_KHRONOS_validation"
        };
    
        std::vector<char const*> requiredLayers;
        if (enableValidationLayers) {
            requiredLayers.assign(validationLayers.begin(), validationLayers.end());
            requiredExtensions.push_back(vk::EXTDebugUtilsExtensionName);
        }
    
        auto layerProperties    = context.enumerateInstanceLayerProperties();
    	auto unsupportedLayerIt = std::ranges::find_if(requiredLayers,
    		                                               [&layerProperties](auto const &requiredLayer) {
    			                                               return std::ranges::none_of(layerProperties,
    			                                                                           [requiredLayer](auto const &layerProperty) { return strcmp(layerProperty.layerName, requiredLayer) == 0; });
    		                                               });
    	if (unsupportedLayerIt != requiredLayers.end()) {
    		throw std::runtime_error("Required layer not supported: " + std::string(*unsupportedLayerIt));
    	}
    
    	auto extensionProperties = context.enumerateInstanceExtensionProperties();
    	auto unsupportedPropertyIt =
    	    std::ranges::find_if(requiredExtensions,
    	                         [&extensionProperties](auto const &requiredExtension) {
    		                         return std::ranges::none_of(extensionProperties,
    		                                                     [requiredExtension](auto const &extensionProperty) { return strcmp(extensionProperty.extensionName, requiredExtension) == 0; });
    	                         });
    	if (unsupportedPropertyIt != requiredExtensions.end()) {
    		throw std::runtime_error("Required extension not supported: " + std::string(*unsupportedPropertyIt));
    	}
    
        vk::InstanceCreateInfo createInfo{.flags                   = vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR,
                                          .pApplicationInfo        = &appInfo,
                                          .enabledLayerCount       = static_cast<std::uint32_t>(requiredLayers.size()),
                                          .ppEnabledLayerNames     = requiredLayers.data(),
                                          .enabledExtensionCount   = static_cast<std::uint32_t>(requiredExtensions.size()),
                                          .ppEnabledExtensionNames = requiredExtensions.data()
        };
    
        instance = vk::raii::Instance(context, createInfo);
    }
    
    std::unique_ptr<vk::raii::CommandBuffer> RendererGPU::beginSingleTimeCommands() {
    	vk::CommandBufferAllocateInfo allocInfo{.commandPool = commandPool,
                                                .level = vk::CommandBufferLevel::ePrimary,
                                                .commandBufferCount = 1
        };
    	std::unique_ptr<vk::raii::CommandBuffer> commandBuffer = std::make_unique<vk::raii::CommandBuffer>(std::move(vk::raii::CommandBuffers(device, allocInfo).front()));
    
    	vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
    	commandBuffer->begin(beginInfo);
    
    	return commandBuffer;
    }
    
    void RendererGPU::endSingleTimeCommands(vk::raii::CommandBuffer &commandBuffer) {
    	commandBuffer.end();
    
    	vk::SubmitInfo submitInfo{.commandBufferCount = 1, .pCommandBuffers = &*commandBuffer};
    	computeQueue.submit(submitInfo, nullptr);
    	computeQueue.waitIdle();
    }
    
    void RendererGPU::transitionImageLayout(const vk::raii::Image &image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout) {
    	auto commandBuffer = beginSingleTimeCommands();
    
    	vk::ImageMemoryBarrier2 imgMemoryBarrier{.oldLayout = oldLayout,
                                        .newLayout = newLayout,
                                        .image = image,
                                        .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
        };
    
    	if (oldLayout == vk::ImageLayout::eGeneral && newLayout == vk::ImageLayout::eTransferSrcOptimal) {
            imgMemoryBarrier.setSrcAccessMask(vk::AccessFlagBits2::eShaderWrite);
    		imgMemoryBarrier.setDstAccessMask(vk::AccessFlagBits2::eTransferWrite);
    
    		imgMemoryBarrier.setSrcStageMask(vk::PipelineStageFlagBits2::eComputeShader);
    		imgMemoryBarrier.setDstStageMask(vk::PipelineStageFlagBits2::eTransfer);
    	} else if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eGeneral) {
            imgMemoryBarrier.setSrcAccessMask({});
    		imgMemoryBarrier.setDstAccessMask(vk::AccessFlagBits2::eShaderWrite);
    
    		imgMemoryBarrier.setSrcStageMask(vk::PipelineStageFlagBits2::eTopOfPipe);
    		imgMemoryBarrier.setDstStageMask(vk::PipelineStageFlagBits2::eComputeShader);
        } else {
            std::println("[ERROR] vk::ImageLayout not found: {}", vk::to_string(newLayout));
    		throw std::invalid_argument("unsupported layout transition:");
    	}
        vk::DependencyInfo dependencyInfo{.pImageMemoryBarriers = &imgMemoryBarrier,
                                          .imageMemoryBarrierCount = 1
        };
        commandBuffer->pipelineBarrier2(dependencyInfo);
    	endSingleTimeCommands(*commandBuffer);
    }
    
    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT       severity,
                                                          vk::DebugUtilsMessageTypeFlagsEXT              type,
                                                          const vk::DebugUtilsMessengerCallbackDataEXT * pCallbackData,
                                                          void *                                         pUserData) {
      std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;
      return vk::False;
    }
    
    void RendererGPU::setupDebugMessenger() {
        if (!enableValidationLayers) return;
        vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                                            vk::DebugUtilsMessageSeverityFlagBitsEXT::eError );
        vk::DebugUtilsMessageTypeFlagsEXT     messageTypeFlags(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                                               vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                                                               vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
        vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT{.messageSeverity = severityFlags,
                                                                              .messageType     = messageTypeFlags,
                                                                              .pfnUserCallback = &debugCallback};
        debugMessenger = instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
    }
    
    bool isDeviceSuitable( vk::raii::PhysicalDevice const & physicalDevice ) {
      bool supportsVulkan1_3 = physicalDevice.getProperties().apiVersion >= vk::ApiVersion13;
    
      auto queueFamilies    = physicalDevice.getQueueFamilyProperties();
      bool supportsGraphics = std::ranges::any_of( queueFamilies, []( auto const & qfp ) {
            return !!(qfp.queueFlags & vk::QueueFlagBits::eCompute);});
    
      auto features = physicalDevice.template getFeatures2<vk::PhysicalDeviceFeatures2,
                                                           vk::PhysicalDeviceVulkan13Features,
                                                           vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
      bool supportsRequiredFeatures = features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
                                      features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;
    
      return supportsVulkan1_3 && supportsGraphics && supportsRequiredFeatures;
    }
    
    void RendererGPU::pickPhysicalDevice() {
      std::vector<vk::raii::PhysicalDevice> physicalDevices = instance.enumeratePhysicalDevices();
      std::println("[INFO] PhysicalDevice count: {}", physicalDevices.size());
      auto const devIter = std::ranges::find_if( physicalDevices, [&]( auto const & physicalDevice ) { return isDeviceSuitable( physicalDevice ); } );
      if ( devIter == physicalDevices.end() )
      {
        throw std::runtime_error( "[ERROR] Failed to find a suitable GPU!" );
      }
      physicalDevice = *devIter;
    }
    
    void RendererGPU::createLogicalDevice() {
        std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
        auto graphicsQueueFamilyProperty = std::ranges::find_if(queueFamilyProperties, [](auto const &qfp) { return !!(qfp.queueFlags & vk::QueueFlagBits::eCompute); });
        computeFamilyQueueIndex = static_cast<std::uint32_t>(std::distance(queueFamilyProperties.begin(), graphicsQueueFamilyProperty));
        float queuePriority = 0.5f;
        vk::DeviceQueueCreateInfo deviceQueueCreateInfo{.queueFamilyIndex = computeFamilyQueueIndex,
                                                        .queueCount = 1,
                                                        .pQueuePriorities = &queuePriority
        };
        vk::StructureChain<vk::PhysicalDeviceFeatures2,
                           vk::PhysicalDeviceVulkan13Features,
                           vk::PhysicalDeviceTimelineSemaphoreFeatures,
                           vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> featureChain = {{},                              
                                                                                              {.dynamicRendering     = true,
                                                                                               .synchronization2     = true },    
                                                                                              {.timelineSemaphore    = true},
                                                                                              {.extendedDynamicState = true }
        };
    
        std::vector<const char*> deviceExtensions;
        if (enableValidationLayers) 
        {
          deviceExtensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME); // needed for macOS
        }
    
        vk::DeviceCreateInfo deviceCreateInfo{.pNext                    = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
                                              .queueCreateInfoCount     = 1,
                                              .pQueueCreateInfos        = &deviceQueueCreateInfo,
                                              .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
                                              .ppEnabledExtensionNames = deviceExtensions.data()
        };
        device        = vk::raii::Device(physicalDevice, deviceCreateInfo);
        computeQueue  = vk::raii::Queue(device, computeFamilyQueueIndex, 0);
    }
    
    
    void RendererGPU::createOutputImage() {
        vk::ImageCreateInfo imageInfo{.imageType = vk::ImageType::e2D,    
                                      .format = vk::Format::eR8G8B8A8Unorm,
                                      .extent.width = screenSettings.WIDTH, 
                                      .extent.height = screenSettings.HEIGHT,
                                      .extent.depth = 1,
                                      .mipLevels = 1,                              
                                      .arrayLayers = 1,
                                      .samples = vk::SampleCountFlagBits::e1,
                                      .tiling = vk::ImageTiling::eOptimal,      
                                      .usage = vk::ImageUsageFlagBits::eStorage |
                                               vk::ImageUsageFlagBits::eTransferSrc,
                                      .sharingMode = vk::SharingMode::eExclusive,
                                      .initialLayout = vk::ImageLayout::eUndefined
        };
        outputImage.image = vk::raii::Image(device, imageInfo);

        auto memRequirements = outputImage.image.getMemoryRequirements();
        vk::MemoryAllocateInfo memoryAllocateInfo{.allocationSize = memRequirements.size,
                                                  .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits,
                                                                                    vk::MemoryPropertyFlagBits::eDeviceLocal )
        };

        outputImage.memory = vk::raii::DeviceMemory(device, memoryAllocateInfo);
        outputImage.image.bindMemory(*outputImage.memory, 0);

        vk::ImageViewCreateInfo viewInfo{.image = *outputImage.image,
                                         .viewType = vk::ImageViewType::e2D,
                                         .format = vk::Format::eR8G8B8A8Unorm,
                                         .subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor,
                                         .subresourceRange.baseMipLevel = 0,
                                         .subresourceRange.levelCount = 1,
                                         .subresourceRange.baseArrayLayer = 0,
                                         .subresourceRange.layerCount = 1
        };

        outputImage.view = vk::raii::ImageView(device, viewInfo);

    	transitionImageLayout(outputImage.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
    }
    
    
    void RendererGPU::createDescriptors() {
        std::vector<vk::DescriptorSetLayoutBinding> layoutBindings
            = {vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute, nullptr)};
    
        vk::DescriptorSetLayoutCreateInfo layoutInfo{ .flags = {},
                                                      .bindingCount= 1,
                                                      .pBindings = layoutBindings.data()
        };
        computeDescriptorSetLayout = vk::raii::DescriptorSetLayout(device, layoutInfo);
    
        vk::DescriptorPoolSize poolSize(vk::DescriptorType::eStorageImage, 1);
        vk::DescriptorPoolCreateInfo poolInfo{.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
                                              .maxSets = 1,
                                              .poolSizeCount = 1,
                                              .pPoolSizes = &poolSize 
        };
    
        computeDescriptorPool = vk::raii::DescriptorPool(device, poolInfo);
        vk::DescriptorSetAllocateInfo allocInfo{.descriptorPool = computeDescriptorPool,
                                                .descriptorSetCount = 1,
                                                .pSetLayouts = &*computeDescriptorSetLayout
        };
        computeDescriptorSets = device.allocateDescriptorSets(allocInfo);
    
        vk::DescriptorImageInfo imageInfo{.sampler = {},
                                          .imageView = outputImage.view,
                                          .imageLayout = vk::ImageLayout::eGeneral
        };
        vk::WriteDescriptorSet descriptorWrite{.dstSet = computeDescriptorSets.front(),
                                               .dstBinding = 0,
                                               .dstArrayElement = 0,
                                               .descriptorCount = 1,
                                               .descriptorType = vk::DescriptorType::eStorageImage,
                                               .pImageInfo = &imageInfo 
        };
        device.updateDescriptorSets(descriptorWrite, {});
    }
    
    void RendererGPU::createComputePipeline() {
        vk::raii::ShaderModule shaderModule = createShaderModule(readFile("shaders/checker.spv"));
        vk::PipelineShaderStageCreateInfo computeShaderStageInfo{.stage = vk::ShaderStageFlagBits::eCompute,
                                                                 .module = shaderModule,
                                                                 .pName = "computeMain"
        };
        vk::PipelineLayoutCreateInfo pipelineLayoutInfo{.flags = {},
                                                        .setLayoutCount = 1,
                                                        .pSetLayouts = &*computeDescriptorSetLayout
        };
        computePipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo );
    
        vk::ComputePipelineCreateInfo pipelineInfo{.flags = {},
                                                   .stage = computeShaderStageInfo,
                                                   .layout= *computePipelineLayout
        };
        computePipeline = vk::raii::Pipeline(device.createComputePipeline(nullptr, pipelineInfo));
    }
    
    [[nodiscard]] vk::raii::ShaderModule RendererGPU::createShaderModule(const std::vector<char>& code) const {
        vk::ShaderModuleCreateInfo createInfo{.codeSize = code.size() * sizeof(char),
                                              .pCode = reinterpret_cast<const std::uint32_t*>(code.data())
        };
        vk::raii::ShaderModule shaderModule{device, createInfo};
        return shaderModule;
    }
    
    [[nodiscard]] uint32_t RendererGPU::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const {
        vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }
    
        throw std::runtime_error("[ERROR] Failed to find suitable memory type!");
    }
    
    void RendererGPU::createCommandPool() {
        vk::CommandPoolCreateInfo poolInfo{ .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                                            .queueFamilyIndex = computeFamilyQueueIndex 
        };
        commandPool = vk::raii::CommandPool(device, poolInfo);
    }
    
    void RendererGPU::createCommandBuffer() {
        vk::CommandBufferAllocateInfo allocInfo{.commandPool = commandPool,
                                                .level = vk::CommandBufferLevel::ePrimary,
                                                .commandBufferCount = 1 };
    
        commandBuffer = std::move(vk::raii::CommandBuffers(device, allocInfo).front());
    }
    
    void RendererGPU::recordCommandBuffer() {
        commandBuffer.begin({});
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, *computePipeline);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                        computePipelineLayout,
                                        0,
                                        {*computeDescriptorSets.front()}, 
                                        nullptr);
        commandBuffer.dispatch((screenSettings.WIDTH + 15)  / 16,
                               (screenSettings.HEIGHT + 15) / 16,
                               1);
        commandBuffer.end();
    }
    
    void RendererGPU::run() {
        recordCommandBuffer();
        vk::CommandBufferSubmitInfo cmdBufferInfo{
            .commandBuffer = *commandBuffer
        };
        vk::SubmitInfo2 submitInfo2{
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &cmdBufferInfo
        };
    
        computeQueue.submit2(submitInfo2, executionFinishedFence);
        extractImage();
    }
    
    void save_as_ppm(const char* filename, const void* data, uint32_t width, uint32_t height) {
        std::ofstream file(filename, std::ios::binary);
        if (!file) {
            throw std::runtime_error("Failed to open file");
        }
    
        // PPM header (P6 = binary RGB)
        file << "P6\n";
        file << width << " " << height << "\n";
        file << "255\n";
    
        const uint8_t* pixels = reinterpret_cast<const uint8_t*>(data);
    
        // Write RGB only, skip alpha
        for (uint32_t i = 0; i < width * height; ++i) {
            file.put(pixels[i * 4 + 0]); // R
            file.put(pixels[i * 4 + 1]); // G
            file.put(pixels[i * 4 + 2]); // B
        }
    
        if (!file) {
            throw std::runtime_error("Failed to write image data");
        }
    }
    
    void RendererGPU::extractImage() {
        const auto result = device.waitForFences({executionFinishedFence}, true, UINT64_MAX);
        if (result != vk::Result::eSuccess) {
            std::println("[ERROR] waiting for fence  before extracting image wasnt successedd, instead : {}", vk::to_string((result)));
        }
    
        vk::DeviceSize imageSize = screenSettings.HEIGHT * screenSettings.WIDTH * 4;
        vk::raii::Buffer stagingBuffer({});
        vk::raii::DeviceMemory stagingBufferMemory({});
        createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);
        transitionImageLayout(outputImage.image, vk::ImageLayout::eGeneral , vk::ImageLayout::eTransferSrcOptimal);
        copyImageToBuffer(outputImage.image, stagingBuffer);
    
        void* data = stagingBufferMemory.mapMemory(0, imageSize);
        save_as_ppm("out.ppm", data, screenSettings.WIDTH, screenSettings.HEIGHT);
        stagingBufferMemory.unmapMemory();
    }
    
    void RendererGPU::copyImageToBuffer(const vk::raii::Image &image, vk::raii::Buffer &buffer) {
    	const auto singleTimeCommandBuffer = beginSingleTimeCommands();
        vk::ImageSubresourceLayers imageSubresourceLayersInfo{.aspectMask = vk::ImageAspectFlagBits::eColor,
                                                              .baseArrayLayer = 0,
                                                              .layerCount = 1,
                                                              .mipLevel = 0
        };
        vk::BufferImageCopy2 bufferImageCopyInfo{.bufferOffset = 0,
                                                 .bufferImageHeight = 0,
                                                 .bufferRowLength = 0,
                                                 .imageSubresource = imageSubresourceLayersInfo,
                                                 .imageExtent = {1, 1, 1},
                                                 .imageOffset = {0, 0, 0}
        };
        vk::CopyImageToBufferInfo2 copyImageToBufferInfo{.srcImage = image,
                                                         .srcImageLayout = vk::ImageLayout::eTransferSrcOptimal,
                                                         .dstBuffer = buffer,
                                                         .pRegions = &bufferImageCopyInfo,
                                                         .regionCount = 1
        };
        singleTimeCommandBuffer->copyImageToBuffer2(copyImageToBufferInfo);
        endSingleTimeCommands(*singleTimeCommandBuffer);
    }
    
    void RendererGPU::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties,
                                   vk::raii::Buffer &buffer, vk::raii::DeviceMemory &bufferMemory) {
    	vk::BufferCreateInfo bufferInfo{.size = size,
                                        .usage = usage,
                                        .sharingMode = vk::SharingMode::eExclusive
        };
    	buffer = vk::raii::Buffer(device, bufferInfo);
    	vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();
    	vk::MemoryAllocateInfo allocInfo{.allocationSize = memRequirements.size,
                                         .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties)
        };
    	bufferMemory = vk::raii::DeviceMemory(device, allocInfo);
    	buffer.bindMemory(bufferMemory, 0);
    }
    
    void RendererGPU::createFences() {
        vk::FenceCreateInfo fenceInfo{};
        executionFinishedFence = vk::raii::Fence(device, fenceInfo);
    }

}//rt::gpu
