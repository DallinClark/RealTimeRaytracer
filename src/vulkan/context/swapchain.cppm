module;

#include <vulkan/vulkan.hpp>

#include <vector>
#include <stdexcept>
#include <limits>

export module vulkan.context.swapchain;

import vulkan.context.instance;
import vulkan.context.device;
import vulkan.context.surface;

namespace vulkan::context {

export class Swapchain {
public:
    /// Create a swapchain for the given device/surface.
    /// desiredFormat & desiredPresentMode are hints; will fall back if unavailable.
    Swapchain(const Device& device,
              const Surface& surface,
              const vk::Format desiredFormat          = vk::Format::eB8G8R8A8Unorm,
              const vk::PresentModeKHR desiredPresent = vk::PresentModeKHR::eFifo)
      : device_{ device.get() }
      , physical_{ device.physical() }
      , surface_{ surface.get() }
    {
        createSwapchain(desiredFormat, desiredPresent);
        retrieveImagesAndViews();
    }

    ~Swapchain() = default; // vk::UniqueSwapchainKHR & vk::UniqueImageView clean up

    /// Accessors
    [[nodiscard]] vk::SwapchainKHR  get()         const noexcept { return *swapchain_; }
    [[nodiscard]] vk::Format        format()      const noexcept { return format_; }
    [[nodiscard]] vk::Extent2D      extent()      const noexcept { return extent_; }
    [[nodiscard]] const auto&       images()      const noexcept { return images_; }
    [[nodiscard]] const auto&       imageViews()  const noexcept { return imageViews_; }

    /// Recreate swapchain (e.g. on window resize). Destroys old swapchain & views.
    void recreate(const vk::Format desiredFormat = vk::Format::eB8G8R8A8Unorm,
                  const vk::PresentModeKHR desiredPresent = vk::PresentModeKHR::eFifo)
    {
        // Wait until device is idle before destroying swapchain
        device_.waitIdle();

        // Destroy old swapchain and image views by resetting Unique handles
        imageViews_.clear();
        swapchain_.reset();

        createSwapchain(desiredFormat, desiredPresent);
        retrieveImagesAndViews();
    }

private:
    // Vulkan handles
    vk::Device                         device_;
    vk::PhysicalDevice                 physical_;
    vk::UniqueSwapchainKHR             swapchain_;
    std::vector<vk::Image>             images_;
    std::vector<vk::UniqueImageView>   imageViews_;

    // Selected properties
    vk::Format     format_;
    vk::Extent2D   extent_;

    // Underlying surface
    vk::SurfaceKHR surface_;

    /// Create vk::SwapchainKHR with chosen format, present mode, and extent.
    void createSwapchain(const vk::Format desiredFormat, const vk::PresentModeKHR desiredPresent)
    {
        // Query support details
        const auto caps         = physical_.getSurfaceCapabilitiesKHR(surface_);
        const auto formats      = physical_.getSurfaceFormatsKHR  (surface_);
        const auto presentModes = physical_.getSurfacePresentModesKHR(surface_);

        // Pick format: prefer desired, else first available
        if (formats.size() == 1 && formats[0].format == vk::Format::eUndefined) {
            format_ = desiredFormat;
        } else {
            format_ = formats[0].format;
            for (auto& f : formats) {
                if (f.format == desiredFormat) {
                    format_ = desiredFormat;
                    break;
                }
            }
        }

        // Pick present mode: prefer desired, else FIFO
        auto pm = vk::PresentModeKHR::eFifo;
        for (auto& mode : presentModes) {
            if (mode == desiredPresent) {
                pm = desiredPresent;
                break;
            }
        }

        // Determine extent (resolution)
        if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            extent_ = caps.currentExtent;
        } else {
            // Fallback: choose within allowed min/max
            extent_.width  = std::clamp<uint32_t>(800, caps.minImageExtent.width,  caps.maxImageExtent.width);
            extent_.height = std::clamp<uint32_t>(600, caps.minImageExtent.height, caps.maxImageExtent.height);
        }

        // Number of images: prefer one more than minimum, but not exceeding maximum
        uint32_t imageCount = caps.minImageCount + 1;
        if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
            imageCount = caps.maxImageCount;
        }

        // Create info
        vk::SwapchainCreateInfoKHR info{};
        info.surface          = surface_;
        info.minImageCount    = imageCount;
        info.imageFormat      = format_;
        info.imageColorSpace  = formats[0].colorSpace;
        info.imageExtent      = extent_;
        info.imageArrayLayers = 1;
        info.imageUsage       = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc;
        info.imageSharingMode = vk::SharingMode::eExclusive;
        info.preTransform     = caps.currentTransform;
        info.compositeAlpha   = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        info.presentMode      = pm;
        info.clipped          = VK_TRUE;
        info.oldSwapchain     = nullptr;

        swapchain_ = device_.createSwapchainKHRUnique(info);
    }

    /// Retrieve swapchain images and create a view for each one.
    void retrieveImagesAndViews()
    {
        images_ = device_.getSwapchainImagesKHR(swapchain_.get());

        imageViews_.reserve(images_.size());
        for (const auto image : images_) {
            vk::ImageViewCreateInfo viewInfo{};
            viewInfo.image    = image;
            viewInfo.viewType = vk::ImageViewType::e2D;
            viewInfo.format   = format_;
            viewInfo.subresourceRange.aspectMask     = vk::ImageAspectFlagBits::eColor;
            viewInfo.subresourceRange.baseMipLevel   = 0;
            viewInfo.subresourceRange.levelCount     = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount     = 1;

            imageViews_.push_back(
                device_.createImageViewUnique(viewInfo)
            );
        }
    }
};

} // namespace vulkan::context
