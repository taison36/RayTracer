#include "Renderer.h"
#include <algorithm>

namespace rt::gfx {

    Renderer::Renderer(const ScreenSettings& screenSettings, std::unique_ptr<AccelerationStruct> accelStruct) 
        : screenSettings(screenSettings),
          accelStruct(std::move(accelStruct)) {
        createOutputImage();
    };

    void Renderer::run(const Scene& scene, FrameBuffer& fb) const {
        const auto cmd = vkCore.beginSingleTimeCommands();

        accelStruct->build(vkCore, scene, outputImage); 
        accelStruct->record(*cmd); 

        vkCore.endSingleTimeCommands(*cmd);

        vk::DeviceSize imageSize = screenSettings.HEIGHT * screenSettings.WIDTH * 4;
        vk::raii::Buffer stagingBuffer({});
        vk::raii::DeviceMemory stagingBufferMemory({});
        vkCore.createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);
        vkCore.transitionImageLayout(outputImage.image, vk::ImageLayout::eGeneral , vk::ImageLayout::eTransferSrcOptimal);
        vkCore.copyImageToBuffer(outputImage.image, stagingBuffer);
    
        void* data = stagingBufferMemory.mapMemory(0, imageSize);
        saveToFramBuffer("out.ppm", data, screenSettings.WIDTH, screenSettings.HEIGHT);
        stagingBufferMemory.unmapMemory();

    }

    void Renderer::saveFoFrameBuffer(const void* data, FrameBuffer& fb) const {

        // Write RGB only, skip alpha
        for (uint32_t i = 0; i < screenSettings.WIDTH * screenSettings.HEIGHT; ++i) {
            file.put(pixels[i * 4 + 0]); // R
            file.put(pixels[i * 4 + 1]); // G
            file.put(pixels[i * 4 + 2]); // B
            file.put(pixels[i * 4 + 3]); // A
        }
    

    }

    void Renderer::createOutputImage() {
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
        outputImage.image = vk::raii::Image(vkCore.getDevice(), imageInfo);

        auto memRequirements = outputImage.image.getMemoryRequirements();
        vk::MemoryAllocateInfo memoryAllocateInfo{.allocationSize = memRequirements.size,
                                                  .memoryTypeIndex = vkCore.findMemoryType(memRequirements.memoryTypeBits,
                                                                                    vk::MemoryPropertyFlagBits::eDeviceLocal )
        };

        outputImage.memory = vk::raii::DeviceMemory(vkCore.getDevice(), memoryAllocateInfo);
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

        outputImage.view = vk::raii::ImageView(vkCore.getDevice(), viewInfo);

    	vkCore.transitionImageLayout(outputImage.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
    }

}// rt::gfx
