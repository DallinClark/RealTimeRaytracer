module;

#include <vulkan/vulkan.hpp>

export module vulkan.memory.image;

namespace vulkan::memory {

    export class Image {
    public:
        Image(vk::Device device,
              vk::PhysicalDevice physical,
              vk::Extent3D extent,
              vk::Format format,
              vk::ImageUsageFlags usage,
              vk::ImageAspectFlags aspect,
              vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal);

        vk::Image getImage() const noexcept { return image_.get(); }
        vk::ImageView getView() const noexcept { return view_.get(); }
        vk::Extent3D getExtent() const noexcept { return extent_; }
        vk::Format getFormat() const noexcept { return format_; }
        vk::DescriptorImageInfo getImageInfo();

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


} // namespace vulkan::memory
