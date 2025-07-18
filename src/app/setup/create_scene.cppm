module;

#include <vulkan/vulkan.hpp>
#include <../../external/LUT/ltc_matrix.h>
#include <string>

export module app.setup.create_scene;

import scene.object;
import scene.area_light;

import vulkan.context.command_pool;
import vulkan.context.device;
import vulkan.memory.image_sampler;
import vulkan.memory.image;
import vulkan.memory.buffer;

import app.setup.geometry_builder;

import core.file;
import core.log;

export namespace app::setup {
    /* Usage: takes in a vector of Objects and a Vector of lights,
     * and returns a TLAS, textures, and vertex and index buffers*/

    class CreateScene {
    public:
        struct SceneReturnInfo {
            std::vector<scene::Object::GPUObjectInfo>                GPUObjects;
            std::vector<scene::AreaLight::GPUAreaLightInfo>          GPUAreaLights;
            GeometryBuilder::GeometryReturnInfo                      geoReturnInfo;
            std::vector<std::unique_ptr<vulkan::memory::Image>>      textures;
            std::vector<vk::DescriptorImageInfo>                     descriptorTextures;
            vulkan::memory::ImageSampler textureSampler;
        };
        static SceneReturnInfo createSceneFromObjectsAndLights(const vulkan::context::Device& device, vulkan::context::CommandPool &commandPool,
                                                               std::vector<std::shared_ptr<scene::Object>>& objects, std::vector<std::shared_ptr<scene::AreaLight>>& areaLights);

        static std::vector<std::unique_ptr<vulkan::memory::Image>> createLTCImages(const vulkan::context::Device& device, vulkan::context::CommandPool& pool, int LTC_WIDTH, int LTC_HEIGHT);

    private:
    };

    CreateScene::SceneReturnInfo CreateScene::createSceneFromObjectsAndLights(const vulkan::context::Device& device, vulkan::context::CommandPool &commandPool,
                                                      std::vector<std::shared_ptr<scene::Object>>& objects, std::vector<std::shared_ptr<scene::AreaLight>>& areaLights){
        // TODO let textures be reused, use unordered map
        // make the image sampler
        vulkan::memory::ImageSampler texSampler(device);

        // vector that holds all the images used for the scene
        std::vector<std::unique_ptr<vulkan::memory::Image>> textures;

        int LTC_WIDTH = 64;
        int LTC_HEIGHT = 64; // 64 because it's small but still gives good results

        auto LTCimages = createLTCImages(device, commandPool, LTC_WIDTH, LTC_HEIGHT);

        // The LTC images, used for lighting, are always at indices 0 and 1 in the buffer
        textures.push_back(std::move(LTCimages[0]));
        textures.push_back(std::move(LTCimages[1]));

        // If needed, create the textures
        for (auto& object : objects) {
            if (object->usesSpecularMap()) {
                auto spec = core::file::createTextureImage(device, object->getSpecularPath(), commandPool, true);
                object->setSpecularMapIndex(static_cast<uint32_t>(textures.size()));
                textures.push_back(std::move(spec));
            }
            if (object->usesMetallicMap()) {
                auto metal = core::file::createTextureImage(device, object->getMetallicPath(), commandPool, true);
                object->setMetallicMapIndex(static_cast<uint32_t>(textures.size()));
                textures.push_back(std::move(metal));
            }
            if (object->usesColorMap()) {
                auto color = core::file::createTextureImage(device, object->getColorPath(), commandPool, false);
                object->setColorMapIndex(static_cast<uint32_t>(textures.size()));
                textures.push_back(std::move(color));
            }
        }

        std::vector<vk::DescriptorImageInfo> descriptorTextures = {};
        for (const auto& image : textures) {
            descriptorTextures.push_back(image->getImageInfoWithSampler(texSampler.get()));
        };

        auto geoReturnInfo = app::setup::GeometryBuilder::createTLASFromOBJsAndTransforms(device, commandPool, objects, areaLights);

        std::vector<scene::Object::GPUObjectInfo>    GPUObjects;
        std::vector<scene::AreaLight::GPUAreaLightInfo> GPUAreaLights;

        for (auto& light : areaLights) {
            GPUAreaLights.push_back(light->getGPUInfo());
        }
        for (auto& object : objects) {
            GPUObjects.push_back(object->getGPUInfo());
        }
        return CreateScene::SceneReturnInfo{
            std::move(GPUObjects),
            std::move(GPUAreaLights),
            std::move(geoReturnInfo),
            std::move(textures),
            std::move(descriptorTextures),
            std::move(texSampler)
        };
    };

    std::vector<std::unique_ptr<vulkan::memory::Image>> CreateScene::createLTCImages(const vulkan::context::Device& device, vulkan::context::CommandPool& pool, int LTC_WIDTH, int LTC_HEIGHT) {
        auto cmd = pool.getSingleUseBuffer();
        int texWidth = LTC_WIDTH;
        int texHeight = LTC_HEIGHT;
        int texChannels = 4; // rgba

        vk::DeviceSize imageSize = texWidth * texHeight * texChannels * sizeof(float);



        vulkan::memory::Buffer stagingBuffer(device, imageSize, vk::BufferUsageFlagBits::eTransferSrc,
                                             vk::MemoryPropertyFlagBits::eHostVisible |vk::MemoryPropertyFlagBits::eHostCoherent);

        stagingBuffer.fill(LTC1, imageSize);

        vk::Extent3D extent = {static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1};
        auto LTC1image = std::make_unique<vulkan::memory::Image>(device.get(), device.physical(), extent, vk::Format::eR32G32B32A32Sfloat,
                                                                 vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
                                                                 vk::ImageAspectFlagBits::eColor);

        vulkan::memory::Image::setImageLayout(cmd.get(), LTC1image->getImage(), vk::ImageLayout::eUndefined,
                                              vk::ImageLayout::eTransferDstOptimal);

        vulkan::memory::Image::copyBufferToImage(cmd.get(), stagingBuffer.get(), LTC1image->getImage(), extent);

        vulkan::memory::Image::setImageLayout(cmd.get(), LTC1image->getImage(), vk::ImageLayout::eTransferDstOptimal,
                                              vk::ImageLayout::eShaderReadOnlyOptimal);


        vulkan::memory::Buffer stagingBuffer2(device, imageSize, vk::BufferUsageFlagBits::eTransferSrc,
                                              vk::MemoryPropertyFlagBits::eHostVisible |
                                              vk::MemoryPropertyFlagBits::eHostCoherent);
        stagingBuffer2.fill(LTC2, imageSize);

        auto LTC2image = std::make_unique<vulkan::memory::Image>(device.get(), device.physical(), extent, vk::Format::eR32G32B32A32Sfloat,
                                                                 vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
                                                                 vk::ImageAspectFlagBits::eColor);

        vulkan::memory::Image::setImageLayout(cmd.get(), LTC2image->getImage(), vk::ImageLayout::eUndefined,
                                              vk::ImageLayout::eTransferDstOptimal);

        vulkan::memory::Image::copyBufferToImage(cmd.get(), stagingBuffer2.get(), LTC2image->getImage(), extent);

        vulkan::memory::Image::setImageLayout(cmd.get(), LTC2image->getImage(), vk::ImageLayout::eTransferDstOptimal,
                                              vk::ImageLayout::eShaderReadOnlyOptimal);

        pool.submitSingleUse(std::move(cmd), device.computeQueue());

        std::vector<std::unique_ptr<vulkan::memory::Image>> images;
        images.push_back(std::move(LTC1image));
        images.push_back(std::move(LTC2image));
        return images;
    }

}