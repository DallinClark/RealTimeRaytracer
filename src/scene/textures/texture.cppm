module;

#include <vulkan/vulkan.hpp>
#include <string>
#include <../../external/LUT/ltc_matrix.h>

import vulkan.memory.buffer;
import vulkan.context.device;
import vulkan.context.command_pool;
import vulkan.memory.image;
import core.file;

export module scene.textures.texture;

int LTC_WIDTH = 64;
int LTC_HEIGHT = 64; // 64 becuase it's small but still gives good results

namespace scene::textures {

    export class Texture {
    public:
        Texture(const vulkan::context::Device& device, vulkan::context::CommandPool& pool,
                std::string colorPath, std::string specularPath, std::string metalnessPath, std::string occlusionPath);

        Texture(const vulkan::context::Device& device, vulkan::context::CommandPool& pool,
                std::string colorPath, std::string occlusionRoughnessMetallicPath);

        Texture(std::unique_ptr<vulkan::memory::Image> colorImage, std::unique_ptr<vulkan::memory::Image> materialImage) :
                colorImage_(std::move(colorImage)), materialImage_(std::move(materialImage)) {};

        static Texture createLTCtables(const vulkan::context::Device& device, vulkan::context::CommandPool& pool);

        vulkan::memory::Image& getColorImage() const { return *colorImage_; }
        vulkan::memory::Image& getMaterialImage() const { return *materialImage_; }

    private:
        std::unique_ptr<vulkan::memory::Image> colorImage_;
        std::unique_ptr<vulkan::memory::Image> materialImage_;  // holds occlusion, roughness, and metallic in r, g, and b respectively
    };

    Texture::Texture(const vulkan::context::Device& device, vulkan::context::CommandPool& pool,
                std::string colorPath, std::string specularPath, std::string metalnessPath, std::string occlusionPath) {

        colorImage_ = core::file::createTextureImage(device, colorPath, pool);
        materialImage_ = core::file::createCombinedTextureImage(device, pool, occlusionPath, metalnessPath, specularPath);
    }

    Texture::Texture(const vulkan::context::Device& device, vulkan::context::CommandPool& pool,
            std::string colorPath, std::string occlusionRoughnessMetallicPath) {

        colorImage_ = core::file::createTextureImage(device, colorPath, pool);
        materialImage_ = core::file::createTextureImage(device, occlusionRoughnessMetallicPath, pool);
    }

    Texture Texture::createLTCtables(const vulkan::context::Device& device, vulkan::context::CommandPool& pool) {
        auto cmd = pool.getSingleUseBuffer();
        int texWidth = LTC_WIDTH;
        int texHeight = LTC_HEIGHT;
        int texChannels = 4; // rgba

        vk::DeviceSize imageSize = texWidth * texHeight * texChannels * sizeof(float);



        vulkan::memory::Buffer stagingBuffer(device, imageSize, vk::BufferUsageFlagBits::eTransferSrc,
                                             vk::MemoryPropertyFlagBits::eHostVisible |
                                             vk::MemoryPropertyFlagBits::eHostCoherent);

        stagingBuffer.fill(LTC1, imageSize);

        vk::Extent3D extent = {static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1};
        auto colorImage = std::make_unique<vulkan::memory::Image>(device.get(), device.physical(), extent, vk::Format::eR32G32B32A32Sfloat,
                                                                    vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
                                                                    vk::ImageAspectFlagBits::eColor);

        vulkan::memory::Image::setImageLayout(cmd.get(), colorImage->getImage(), vk::ImageLayout::eUndefined,
                                              vk::ImageLayout::eTransferDstOptimal);

        vulkan::memory::Image::copyBufferToImage(cmd.get(), stagingBuffer.get(), colorImage->getImage(), extent);

        vulkan::memory::Image::setImageLayout(cmd.get(), colorImage->getImage(), vk::ImageLayout::eTransferDstOptimal,
                                              vk::ImageLayout::eShaderReadOnlyOptimal);

        // LTC1 IS COLOR IMAGE

        vulkan::memory::Buffer stagingBuffer2(device, imageSize, vk::BufferUsageFlagBits::eTransferSrc,
                                             vk::MemoryPropertyFlagBits::eHostVisible |
                                             vk::MemoryPropertyFlagBits::eHostCoherent);
        stagingBuffer2.fill(LTC2, imageSize);

        auto materialImage = std::make_unique<vulkan::memory::Image>(device.get(), device.physical(), extent, vk::Format::eR32G32B32A32Sfloat,
                                                                    vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
                                                                    vk::ImageAspectFlagBits::eColor);

        vulkan::memory::Image::setImageLayout(cmd.get(), materialImage->getImage(), vk::ImageLayout::eUndefined,
                                              vk::ImageLayout::eTransferDstOptimal);

        vulkan::memory::Image::copyBufferToImage(cmd.get(), stagingBuffer2.get(), materialImage->getImage(), extent);

        vulkan::memory::Image::setImageLayout(cmd.get(), materialImage->getImage(), vk::ImageLayout::eTransferDstOptimal,
                                              vk::ImageLayout::eShaderReadOnlyOptimal);

        pool.submitSingleUse(std::move(cmd), device.computeQueue());
        // LTC2 IS MATERIAL IMAGE

        return Texture(std::move(colorImage), std::move(materialImage));
    }

}

