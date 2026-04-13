#include "Renderer.h"
#include <algorithm>

namespace rt::gfx {

    Renderer::Renderer(std::unique_ptr<AccelerationStruct> accelStruct)
        : accelStruct(std::move(accelStruct)) {
    }

    void Renderer::run(const RendererContext& context, FrameBuffer& fb) {
        auto outputImage = createOutputImage(context);

        const auto cmd = vkCore.beginSingleTimeCommands();

        accelStruct->build(vkCore, context, outputImage); 
        accelStruct->record(*cmd, context.screenSettings->WIDTH, context.screenSettings->HEIGHT);

        vk::DeviceSize imageSize = context.screenSettings->HEIGHT * context.screenSettings->WIDTH * 4;
        vk::raii::Buffer stagingBuffer({});
        vk::raii::DeviceMemory stagingBufferMemory({});

        vkCore.createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);
        vkCore.transitionImageLayout(outputImage.image, vk::ImageLayout::eGeneral , vk::ImageLayout::eTransferSrcOptimal, *cmd);
        vkCore.endSingleTimeCommands(*cmd);

        vkCore.copyImageToBuffer(outputImage.image, stagingBuffer, context.screenSettings->HEIGHT, context.screenSettings->WIDTH);
    
        void* data = stagingBufferMemory.mapMemory(0, imageSize);
        saveToFrameBuffer(data, fb);
        stagingBufferMemory.unmapMemory();
    }

    void Renderer::saveToFrameBuffer(void* data, FrameBuffer& fb) const {
        const uint8_t* pixels = reinterpret_cast<const uint8_t*>(data);

        for (uint32_t i = 0; i < fb.COLS * fb.ROWS; ++i) {
            Color c(pixels[i * 4 + 0], // R
                    pixels[i * 4 + 1], // G
                    pixels[i * 4 + 2], // B
                    pixels[i * 4 + 3]  // A
            );
            fb[i / fb.COLS][i % fb.COLS] = c;
        }
    }

    OutputImage Renderer::createOutputImage(const RendererContext& context) {
        OutputImage outputImage;
        vk::ImageCreateInfo imageInfo{
            .imageType = vk::ImageType::e2D,    
            .format = vk::Format::eR8G8B8A8Unorm,
            .extent.width = context.screenSettings->WIDTH,
            .extent.height = context.screenSettings->HEIGHT,
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
        vk::MemoryAllocateInfo memoryAllocateInfo{
            .allocationSize = memRequirements.size,
            .memoryTypeIndex = vkCore.findMemoryType(memRequirements.memoryTypeBits,
                                                     vk::MemoryPropertyFlagBits::eDeviceLocal )
        };

        outputImage.memory = vk::raii::DeviceMemory(vkCore.getDevice(), memoryAllocateInfo);
        outputImage.image.bindMemory(*outputImage.memory, 0);

        vk::ImageViewCreateInfo viewInfo{
            .image = *outputImage.image,
            .viewType = vk::ImageViewType::e2D,
            .format = vk::Format::eR8G8B8A8Unorm,
            .subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor,
            .subresourceRange.baseMipLevel = 0,
            .subresourceRange.levelCount = 1,
            .subresourceRange.baseArrayLayer = 0,
            .subresourceRange.layerCount = 1
        };

        outputImage.view = vk::raii::ImageView(vkCore.getDevice(), viewInfo);
        const auto cmd = vkCore.beginSingleTimeCommands();
    	vkCore.transitionImageLayout(outputImage.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral, *cmd);
        vkCore.endSingleTimeCommands(*cmd);
        return outputImage; 
    }

}// rt::gfx
