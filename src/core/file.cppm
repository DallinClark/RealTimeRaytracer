module;

#include <string>
#include <vector>
#include <fstream>
#include <stdexcept>
#include <tiny_obj_loader.h>
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>
#include <unordered_map>
#include <memory>
#include <functional>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

import scene.geometry.vertex;
import vulkan.memory.buffer;
import vulkan.memory.image;
import vulkan.context.command_pool;
import vulkan.context.device;

export module core.file;

export namespace core::file {

    inline std::vector<char> loadBinaryFile(std::string_view path) {
        std::ifstream file(std::string(path), std::ios::ate | std::ios::binary);
        if (!file.is_open())
            throw std::runtime_error("Failed to open shader file: " + std::string(path));

        size_t size = file.tellg();
        std::vector<char> buffer(size);

        file.seekg(0);
        file.read(buffer.data(), size);

        return buffer;
    }

    void loadModel(const std::string& modelPath, std::vector<glm::vec3>& vertexPositions, std::vector<uint32_t>& indices, std::vector<scene::geometry::Vertex>& vertices) {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        uint32_t newVertexCount = 0; // used for 0 indexed indices with respect to the vertices

        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, modelPath.c_str())) {
            throw std::runtime_error(warn + err);
        }

        std::unordered_map<scene::geometry::Vertex, uint32_t> uniqueVertices{};

        for (const auto &shape: shapes) {
            for (const auto &index: shape.mesh.indices) {
                scene::geometry::Vertex vertex{};

                // Position
                vertex.position = {
                        attrib.vertices[3 * index.vertex_index + 0],
                        attrib.vertices[3 * index.vertex_index + 1],
                        attrib.vertices[3 * index.vertex_index + 2]
                };

                // Normal (check if available)
                if (index.normal_index >= 0) {
                    vertex.normal = {
                            attrib.normals[3 * index.normal_index + 0],
                            attrib.normals[3 * index.normal_index + 1],
                            attrib.normals[3 * index.normal_index + 2]
                    };

                } else {
                    vertex.normal = glm::vec3(0.0f, 0.0f, 0.0f); // TODO maybe compute this later if not included
                }

                // Texture coordinates (check if available)
                if (index.texcoord_index >= 0) {
                    vertex.uv = {
                            attrib.texcoords[2 * index.texcoord_index + 0],
                            attrib.texcoords[2 * index.texcoord_index + 1]
                    };
                } else {
                    vertex.uv = glm::vec2(0.0f, 0.0f);
                }

                // Check if vertex is unique
                if (uniqueVertices.count(vertex) == 0) {
                    uniqueVertices[vertex] = newVertexCount;
                    vertices.push_back(vertex);
                    vertexPositions.push_back(vertex.position);
                    ++newVertexCount;
                }

                indices.push_back(uniqueVertices[vertex]);
            }
        }
    }

    std::unique_ptr<vulkan::memory::Image> createTextureImage(const vulkan::context::Device& device, const std::string& texturePath, vulkan::context::CommandPool& pool) {
        auto cmd = pool.getSingleUseBuffer();
        stbi_set_flip_vertically_on_load(true);

        int texWidth, texHeight, texChannels;
        stbi_uc *pixels = stbi_load(texturePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        vk::DeviceSize imageSize = texWidth * texHeight * 4;

        if (!pixels) {
            throw std::runtime_error("failed to load texture image!");
        }
        vulkan::memory::Buffer stagingBuffer(device, imageSize, vk::BufferUsageFlagBits::eTransferSrc,
                                             vk::MemoryPropertyFlagBits::eHostVisible |
                                             vk::MemoryPropertyFlagBits::eHostVisible);
        stagingBuffer.fill(pixels, imageSize);
        stbi_image_free(pixels);

        vk::Extent3D extent = {static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1};
        auto textureImage = std::make_unique<vulkan::memory::Image>(device.get(), device.physical(), extent, vk::Format::eR8G8B8A8Unorm,
                                           vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
                                           vk::ImageAspectFlagBits::eColor);

        vulkan::memory::Image::setImageLayout(cmd.get(), textureImage->getImage(), vk::ImageLayout::eUndefined,
                                              vk::ImageLayout::eTransferDstOptimal);

        vulkan::memory::Image::copyBufferToImage(cmd.get(), stagingBuffer.get(), textureImage->getImage(), extent);

        vulkan::memory::Image::setImageLayout(cmd.get(), textureImage->getImage(), vk::ImageLayout::eTransferDstOptimal,
                                              vk::ImageLayout::eShaderReadOnlyOptimal);

        pool.submitSingleUse(std::move(cmd), device.computeQueue());

        return std::move(textureImage);
    }

    std::unique_ptr<vulkan::memory::Image> createCombinedTextureImage(const vulkan::context::Device& device, vulkan::context::CommandPool& pool,
            const std::string& occlusionPath, const std::string& roughnessPath, const std::string& metallicPath) {
        int width = 0, height = 0, channels;

        stbi_uc* occlusionPixels = nullptr;
        stbi_uc* roughnessPixels = nullptr;
        stbi_uc* metallicPixels = nullptr;

        // Load each texture (force single channel), if given
        if (!occlusionPath.empty())
            occlusionPixels = stbi_load(occlusionPath.c_str(), &width, &height, &channels, 1);

        if (!roughnessPath.empty()) {
            int w, h;
            roughnessPixels = stbi_load(roughnessPath.c_str(), &w, &h, &channels, 1);
            if ((occlusionPixels && (w != width || h != height))) throw std::runtime_error("Texture sizes don't match.");
            if (!occlusionPixels) { width = w; height = h; }
        }

        if (!metallicPath.empty()) {
            int w, h;
            metallicPixels = stbi_load(metallicPath.c_str(), &w, &h, &channels, 1);
            if ((occlusionPixels && (w != width || h != height)) ||
                (roughnessPixels && (w != width || h != height)))
                throw std::runtime_error("Texture sizes don't match.");
            if (!occlusionPixels && !roughnessPixels) { width = w; height = h; }
        }

        if (!(width && height)) {
            width = 512;
            height = 512;
        }
        const int texSize = width * height;


        std::vector<uint8_t> combinedPixels(texSize * 4); // RGBA

        // Fill each channel
        for (int i = 0; i < texSize; ++i) {
            combinedPixels[i * 4 + 0] = occlusionPixels  ? occlusionPixels[i]  : 255;  // R
            combinedPixels[i * 4 + 1] = roughnessPixels  ? roughnessPixels[i]  : 5;  // G
            combinedPixels[i * 4 + 2] = metallicPixels   ? metallicPixels[i]   : 0;    // B
            combinedPixels[i * 4 + 3] = 255; // A
        }

        stbi_image_free(occlusionPixels);
        stbi_image_free(roughnessPixels);
        stbi_image_free(metallicPixels);

        vk::DeviceSize imageSize = width * height * 4;

        auto cmd = pool.getSingleUseBuffer();

        vulkan::memory::Buffer stagingBuffer(device, imageSize,
                                             vk::BufferUsageFlagBits::eTransferSrc,
                                             vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        stagingBuffer.fill(combinedPixels.data(), imageSize);

        vk::Extent3D extent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 };
        auto textureImage = std::make_unique<vulkan::memory::Image>(
                device.get(), device.physical(), extent, vk::Format::eR8G8B8A8Unorm,
                vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
                vk::ImageAspectFlagBits::eColor
        );

        vulkan::memory::Image::setImageLayout(cmd.get(), textureImage->getImage(),
                                              vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

        vulkan::memory::Image::copyBufferToImage(cmd.get(), stagingBuffer.get(),
                                                 textureImage->getImage(), extent);

        vulkan::memory::Image::setImageLayout(cmd.get(), textureImage->getImage(),
                                              vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

        pool.submitSingleUse(std::move(cmd), device.computeQueue());

        return textureImage;
    }


}
