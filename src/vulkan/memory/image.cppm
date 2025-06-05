module;

#include <vulkan/vulkan.hpp>

export module vulkan.memory.image;

namespace vulkan::memory {

    export class Image {
    public:
        Image(vk::Device device, vk::PhysicalDevice physical, vk::Extent3D extent, vk::Format format,
              vk::ImageUsageFlags usage, vk::ImageAspectFlags aspect, vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal);

        vk::Image getImage() const noexcept { return image_.get(); }
        vk::ImageView getView() const noexcept { return view_.get(); }
        vk::Extent3D getExtent() const noexcept { return extent_; }
        vk::Format getFormat() const noexcept { return format_; }
        vk::DescriptorImageInfo getImageInfo();

        static vk::AccessFlags toAccessFlags(vk::ImageLayout layout);

        static void setImageLayout(vk::CommandBuffer& commandBuffer, const vk::Image& image,const vk::ImageLayout& oldLayout,const vk::ImageLayout& newLayout);
        static void copyBufferToImage(vk::CommandBuffer& commandBuffer,const vk::Buffer& buffer,const vk::Image& image, const vk::Extent3D& extent);
        static void copyImage(vk::CommandBuffer commandBuffer, vk::Image srcImage, vk::Image dstImage, vk::Extent3D extent);

    private:
        vk::Device device_;
        vk::Extent3D extent_;
        vk::Format format_;

        vk::UniqueImage image_;
        vk::UniqueDeviceMemory memory_;
        vk::UniqueImageView view_;
    };

// -- Implementation --

    Image::Image(vk::Device device,
                 vk::PhysicalDevice physical,
                 vk::Extent3D extent,
                 vk::Format format,
                 vk::ImageUsageFlags usage,
                 vk::ImageAspectFlags aspect,
                 vk::MemoryPropertyFlags properties)
            : device_(device), extent_(extent), format_(format)
    {
        // Create image
        vk::ImageCreateInfo imageInfo{};
        imageInfo.imageType = vk::ImageType::e2D;
        imageInfo.extent = extent;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = vk::ImageTiling::eOptimal;
        imageInfo.initialLayout = vk::ImageLayout::eUndefined;
        imageInfo.usage = usage;
        imageInfo.samples = vk::SampleCountFlagBits::e1;
        imageInfo.sharingMode = vk::SharingMode::eExclusive;

        image_ = device_.createImageUnique(imageInfo);

        // Allocate memory
        vk::MemoryRequirements memRequirements = device_.getImageMemoryRequirements(image_.get());

        vk::PhysicalDeviceMemoryProperties memProps = physical.getMemoryProperties();
        uint32_t memoryTypeIndex = uint32_t(~0u);
        for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
            if ((memRequirements.memoryTypeBits & (1 << i)) &&
                (memProps.memoryTypes[i].propertyFlags & properties) == properties)
            {
                memoryTypeIndex = i;
                break;
            }
        }

        if (memoryTypeIndex == uint32_t(~0u)) {
            throw std::runtime_error("Failed to find suitable memory type for image.");
        }

        vk::MemoryAllocateInfo allocInfo{};
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = memoryTypeIndex;

        memory_ = device_.allocateMemoryUnique(allocInfo);
        device_.bindImageMemory(image_.get(), memory_.get(), 0);

        // Create view
        vk::ImageViewCreateInfo viewInfo{};
        viewInfo.image = image_.get();
        viewInfo.viewType = vk::ImageViewType::e2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = aspect;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        view_ = device_.createImageViewUnique(viewInfo);
    }

    vk::DescriptorImageInfo Image::getImageInfo() {
        vk::DescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = vk::ImageLayout::eGeneral; // required for imageStore/imageLoad
        imageInfo.imageView = view_.get();
        imageInfo.sampler = nullptr; // not needed for storage images

        return imageInfo;
    };

    void Image::setImageLayout(vk::CommandBuffer& commandBuffer, const vk::Image& image,const vk::ImageLayout& oldLayout,const vk::ImageLayout& newLayout) {
        vk::ImageMemoryBarrier barrier;
        barrier.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
        barrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
        barrier.setImage(image);
        barrier.setOldLayout(oldLayout);
        barrier.setNewLayout(newLayout);
        barrier.setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
        barrier.setSrcAccessMask(toAccessFlags(oldLayout));
        barrier.setDstAccessMask(toAccessFlags(newLayout));
        commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands,  //
                                      vk::PipelineStageFlagBits::eAllCommands,  //
                                      {}, {}, {}, barrier);
    }

    vk::AccessFlags Image::toAccessFlags(vk::ImageLayout layout) {
        switch (layout) {
            case vk::ImageLayout::eTransferSrcOptimal:
                return vk::AccessFlagBits::eTransferRead;
            case vk::ImageLayout::eTransferDstOptimal:
                return vk::AccessFlagBits::eTransferWrite;
            default:
                return {};
        }
    }

    void Image::copyImage(vk::CommandBuffer commandBuffer, vk::Image srcImage, vk::Image dstImage, vk::Extent3D extent) {
        vk::ImageCopy copyRegion;
        copyRegion.setSrcSubresource({vk::ImageAspectFlagBits::eColor, 0, 0, 1});
        copyRegion.setDstSubresource({vk::ImageAspectFlagBits::eColor, 0, 0, 1});
        copyRegion.setExtent(extent);
        commandBuffer.copyImage(srcImage, vk::ImageLayout::eTransferSrcOptimal, dstImage, vk::ImageLayout::eTransferDstOptimal, copyRegion);
    }

    void Image::copyBufferToImage(vk::CommandBuffer& commandBuffer,const vk::Buffer& buffer,const vk::Image& image, const vk::Extent3D& extent) {
        vk::BufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;

        region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;

        region.imageOffset = vk::Offset3D{0, 0, 0};
        region.imageExtent = extent;

        commandBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, region);
    }


} // namespace vulkan::memory
