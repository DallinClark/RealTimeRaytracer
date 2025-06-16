module;

#include <vulkan/vulkan.hpp>
#include <string>

import vulkan.context.device;
import vulkan.context.command_pool;
import vulkan.memory.image;
import core.file;

export module scene.textures.texture;


namespace scene::textures {

    export class Texture {
    public:
        Texture(const vulkan::context::Device& device, vulkan::context::CommandPool& pool,
                std::string colorPath, std::string specularPath, std::string metalnessPath, std::string occlusionPath);

        Texture(const vulkan::context::Device& device, vulkan::context::CommandPool& pool,
                std::string colorPath, std::string occlusionRoughnessMetallicPath);

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

}

