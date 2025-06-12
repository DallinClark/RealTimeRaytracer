module;

#include <vulkan/vulkan.hpp>

export module vulkan.memory.image_sampler;

import vulkan.context.device; // Assuming you have a Device wrapper that gives vk::Device and vk::PhysicalDevice

namespace vulkan::memory {

    export class ImageSampler {
    public:
        ImageSampler(const vulkan::context::Device& device);
        const vk::Sampler& get() const;

    private:
        const vulkan::context::Device* device_; // to access physical and logical device
        vk::UniqueSampler texSampler_;
    };

// -- Implementation --

    ImageSampler::ImageSampler(const vulkan::context::Device& device)
            : device_(&device) {
        vk::SamplerCreateInfo samplerCreateInfo{};
        samplerCreateInfo.magFilter = vk::Filter::eLinear;
        samplerCreateInfo.minFilter = vk::Filter::eLinear;
        samplerCreateInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
        samplerCreateInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
        samplerCreateInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
        samplerCreateInfo.anisotropyEnable = VK_TRUE;

        vk::PhysicalDeviceProperties properties = device.physical().getProperties();
        samplerCreateInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
        samplerCreateInfo.borderColor = vk::BorderColor::eFloatOpaqueBlack;
        samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
        samplerCreateInfo.compareEnable = VK_FALSE;
        samplerCreateInfo.compareOp = vk::CompareOp::eAlways;
        samplerCreateInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
        samplerCreateInfo.mipLodBias = 0.0f;
        samplerCreateInfo.minLod = 0.0f;
        samplerCreateInfo.maxLod = 0.0f;

        texSampler_ = device.get().createSamplerUnique(samplerCreateInfo);
    }

    const vk::Sampler& ImageSampler::get() const {
        return texSampler_.get();
    }

} // namespace vulkan::memory
