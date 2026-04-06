#include "VkCore.h"
#include "vulkan/vulkan_beta.h"
#include <print>
#include <iostream>
namespace rt::gfx {

    VkCore::VkCore() {
        init();
    }
    
    void VkCore::init() {
        createInstance();
        setupDebugMessenger();
        pickPhysicalDevice();
        createLogicalDevice();
        createCommandPool();
    }

    void VkCore::createInstance() {
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

    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT       severity,
                                                          vk::DebugUtilsMessageTypeFlagsEXT              type,
                                                          const vk::DebugUtilsMessengerCallbackDataEXT * pCallbackData,
                                                          void *                                         pUserData) {
      std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;
      return vk::False;
    }
    
    void VkCore::setupDebugMessenger() {
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
    
    void VkCore::pickPhysicalDevice() {
      std::vector<vk::raii::PhysicalDevice> physicalDevices = instance.enumeratePhysicalDevices();
      std::println("[INFO] PhysicalDevice count: {}", physicalDevices.size());
      auto const devIter = std::ranges::find_if( physicalDevices, [&]( auto const & physicalDevice ) { return isDeviceSuitable( physicalDevice ); } );
      if ( devIter == physicalDevices.end() )
      {
        throw std::runtime_error( "[ERROR] Failed to find a suitable GPU!" );
      }
      physicalDevice = *devIter;
    }
    
    void VkCore::createLogicalDevice() {
        std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
        auto graphicsQueueFamilyProperty = std::ranges::find_if(queueFamilyProperties, [](auto const &qfp) { return !!(qfp.queueFlags & vk::QueueFlagBits::eCompute); });
        queueFamilyIndex = static_cast<std::uint32_t>(std::distance(queueFamilyProperties.begin(), graphicsQueueFamilyProperty));
        float queuePriority = 0.5f;
        vk::DeviceQueueCreateInfo deviceQueueCreateInfo{.queueFamilyIndex = queueFamilyIndex,
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
        device = vk::raii::Device(physicalDevice, deviceCreateInfo);
        queue  = vk::raii::Queue(device, queueFamilyIndex, 0);
    }

    void VkCore::createCommandPool() {
        vk::CommandPoolCreateInfo poolInfo{.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                                           .queueFamilyIndex = queueFamilyIndex 
        };
        commandPool = vk::raii::CommandPool(device, poolInfo);
    }

    std::unique_ptr<vk::raii::CommandBuffer> VkCore::beginSingleTimeCommands() const {
    	vk::CommandBufferAllocateInfo allocInfo{.commandPool = commandPool,
                                                .level = vk::CommandBufferLevel::ePrimary,
                                                .commandBufferCount = 1
        };
    	std::unique_ptr<vk::raii::CommandBuffer> commandBuffer = std::make_unique<vk::raii::CommandBuffer>(std::move(vk::raii::CommandBuffers(device, allocInfo).front()));
    
    	vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
    	commandBuffer->begin(beginInfo);
    
    	return commandBuffer;
    }
    
    void VkCore::endSingleTimeCommands(vk::raii::CommandBuffer &commandBuffer) const {
    	commandBuffer.end();
    
    	vk::SubmitInfo submitInfo{.commandBufferCount = 1, .pCommandBuffers = &*commandBuffer};
    	queue.submit(submitInfo, nullptr);
    	queue.waitIdle();
    }
    
    void VkCore::transitionImageLayout(const vk::raii::Image &image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout) const {
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

    void VkCore::copyImageToBuffer(const vk::raii::Image &image, vk::raii::Buffer &buffer, const uint32_t HEIGHT, const uint32_t WIDTH) const {
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
                                                 .imageExtent = {WIDTH, HEIGHT, 1},
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
    
    void VkCore::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties,
                                   vk::raii::Buffer &buffer, vk::raii::DeviceMemory &bufferMemory) const {
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
    

    const vk::raii::Device& VkCore::getDevice() const {
        return device;
    }

    [[nodiscard]] uint32_t VkCore::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const {
        vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }
        
        throw std::runtime_error("[ERROR] Failed to find suitable memory type!");
    }

    [[nodiscard]] vk::raii::ShaderModule VkCore::createShaderModule(const std::vector<char>& code) const {
        vk::ShaderModuleCreateInfo createInfo{.codeSize = code.size() * sizeof(char),
                                              .pCode = reinterpret_cast<const std::uint32_t*>(code.data())
        };
        vk::raii::ShaderModule shaderModule{device, createInfo};
        return shaderModule;
    }
}// rt::gfx
