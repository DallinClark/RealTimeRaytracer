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
#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

import scene.geometry.vertex;
import scene.object;
import vulkan.raytracing.blas;
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
                    vertex.normal = glm::vec3(0.0f, 0.0f, 0.0f);
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

    std::string normalizePath(std::string path) {
        std::replace(path.begin(), path.end(), '\\', '/');
        return path;
    }


    // basically an add on for app.setup.geometry_builder,
    // creates everything needed for BLAS creation given an obj file and a mtl file
    void loadOBJandMTL(const std::string& objPath, const std::string& mtlPath, std::vector<std::shared_ptr<scene::Object>>& objects,
                       std::vector<glm::vec3>& vertexPositions, std::vector<uint32_t>& indices,
                       std::vector<scene::geometry::Vertex>& vertices, std::vector<vulkan::raytracing::BLAS::BLASCreateInfo>& meshDatas) {

        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
                              objPath.c_str(), mtlPath.empty() ? nullptr : mtlPath.c_str())) {
            throw std::runtime_error(warn + err);
        }

        // Offsets to track overall buffer size for BLAS data
        vk::DeviceSize currVertexOffset = vertexPositions.size() * sizeof(glm::vec3);
        vk::DeviceSize currIndexOffset = indices.size() * sizeof(uint32_t);

        for (size_t s = 0; s < shapes.size(); ++s) {
            const auto& shape = shapes[s];
            auto obj = std::make_shared<scene::Object>(objPath);

            std::vector<scene::geometry::Vertex> shapeVertices;
            std::vector<uint32_t> shapeIndices;

            std::unordered_map<scene::geometry::Vertex, uint32_t> uniqueVertices{};
            uint32_t nextIndex = 0;

            size_t index_offset = 0;

            // Loop through the faces on a mesh
            for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
                int fv = shape.mesh.num_face_vertices[f];
                if (fv != 3) {
                    std::cerr << "Non-triangle face detected, skipping face." << std::endl;
                    index_offset += fv;
                    continue;
                }

                // Loop through vertices on a face and add them to the shapeIndices and shapeVertices vectors
                for (size_t v = 0; v < fv; ++v) {
                    tinyobj::index_t idx = shape.mesh.indices[index_offset + v];

                    scene::geometry::Vertex vertex{};

                    vertex.position = {
                            attrib.vertices[3 * idx.vertex_index + 0],
                            attrib.vertices[3 * idx.vertex_index + 1],
                            attrib.vertices[3 * idx.vertex_index + 2]
                    };

                    if (idx.normal_index >= 0) {
                        vertex.normal = {
                                attrib.normals[3 * idx.normal_index + 0],
                                attrib.normals[3 * idx.normal_index + 1],
                                attrib.normals[3 * idx.normal_index + 2]
                        };
                    } else {
                        vertex.normal = glm::vec3(0.0f);
                    }

                    if (idx.texcoord_index >= 0) {
                        vertex.uv = {
                                attrib.texcoords[2 * idx.texcoord_index + 0],
                                attrib.texcoords[2 * idx.texcoord_index + 1]
                        };
                    } else {
                        vertex.uv = glm::vec2(0.0f);
                    }

                    if (uniqueVertices.count(vertex) == 0) {
                        uniqueVertices[vertex] = nextIndex++;
                        shapeVertices.push_back(vertex);
                    }

                    shapeIndices.push_back(uniqueVertices[vertex]);
                }
                index_offset += fv;
            }

            // Append shape vertices/indices to global buffers, record offsets/counts
            size_t prevVertexCount = vertexPositions.size();
            size_t prevIndexCount = indices.size();

            for (const auto& vertex : shapeVertices) {
                vertexPositions.push_back(vertex.position);
                vertices.push_back(vertex);
            }
            for (const auto& index : shapeIndices) {
                indices.push_back(index);
            }

            size_t newVertexCount = vertexPositions.size() - prevVertexCount;
            size_t newIndexCount = indices.size() - prevIndexCount;

            // Material setup
            int matId = shape.mesh.material_ids.empty() ? -1 : shape.mesh.material_ids[0];
            if (matId >= 0 && matId < (int)materials.size()) {
                const auto& mat = materials[matId];

                if (!mat.diffuse_texname.empty()) {
                    obj->setColor(normalizePath(mtlPath + mat.diffuse_texname));
                }
                else {
                    obj->setColor(glm::vec3(mat.diffuse[0], mat.diffuse[1], mat.diffuse[2]));
                }

                if (!mat.specular_texname.empty()) {
                    obj->setSpecular(normalizePath(mtlPath + mat.specular_texname));
                }
                else {
                    obj->setSpecular(mat.specular[0]);
                }

                if (!mat.metallic_texname.empty()) {
                    obj->setMetallic(normalizePath(mtlPath + mat.metallic_texname));
                }
                else if (mat.unknown_parameter.find("metallic") != mat.unknown_parameter.end()) {
                    obj->setMetallic(std::stof(mat.unknown_parameter.at("metallic")));
                }
                else {
                    obj->setMetallic(0.0f);
                }

                if (!mat.alpha_texname.empty()) {
                    obj->setOpacity(normalizePath(mtlPath + mat.alpha_texname));
                }

            } else {
                obj->setColor(glm::vec3(0.5f));
                obj->setSpecular(1.0f);
                obj->setMetallic(0.0f);
            }

            obj->setVertexOffset(static_cast<uint32_t>(prevVertexCount));
            obj->setIndexOffset(static_cast<uint32_t>(prevIndexCount));
            obj->setNumTriangles(static_cast<uint32_t>(newIndexCount / 3));
            obj->setBLASIndex(meshDatas.size());

            objects.push_back(std::move(obj));

            // Setup BLASCreateInfo
            vulkan::raytracing::BLAS::BLASCreateInfo meshData{};
            meshData.vertexOffset = currVertexOffset;
            meshData.indexOffset = currIndexOffset;
            meshData.vertexCount = static_cast<uint32_t>(newVertexCount);
            meshData.indexCount = static_cast<uint32_t>(newIndexCount);
            meshData.vertexSize = newVertexCount * sizeof(glm::vec3);
            meshData.indexSize = newIndexCount * sizeof(uint32_t);
            meshData.vertexIndexOffset = static_cast<uint32_t>(prevVertexCount);
            meshData.indexIndexOffset = static_cast<uint32_t>(prevIndexCount);

            currVertexOffset += meshData.vertexSize;
            currIndexOffset += meshData.indexSize;

            meshDatas.push_back(meshData);
        }
    }


    std::unique_ptr<vulkan::memory::Image> createTextureImage(const vulkan::context::Device& device, const std::string& texturePath,
                                                        vulkan::context::CommandPool& pool,bool isGrayscale) {

        auto cmd = pool.getSingleUseBuffer();
        stbi_set_flip_vertically_on_load(true);

        int texWidth, texHeight, texChannels;
        int desiredChannels = isGrayscale ? STBI_grey : STBI_rgb_alpha;
        stbi_uc* pixels = stbi_load(texturePath.c_str(), &texWidth, &texHeight, &texChannels, desiredChannels);

        if (!pixels) {
            std::cerr << "Failed to load image: " << texturePath << "\n";
            std::cerr << "Reason: " << stbi_failure_reason() << "\n";
            throw std::runtime_error("Image load failed");
        }

        vk::DeviceSize imageSize = texWidth * texHeight * (isGrayscale ? 1 : 4);

        // Choose format based on grayscale vs color
        vk::Format imageFormat = isGrayscale ? vk::Format::eR8Unorm : vk::Format::eR8G8B8A8Unorm;

        vulkan::memory::Buffer stagingBuffer(device, imageSize,
                                             vk::BufferUsageFlagBits::eTransferSrc,
                                             vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        stagingBuffer.fill(pixels, imageSize);
        stbi_image_free(pixels);

        vk::Extent3D extent = {static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1};
        auto textureImage = std::make_unique<vulkan::memory::Image>(device.get(), device.physical(), extent, imageFormat,
                                                                    vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,vk::ImageAspectFlagBits::eColor);

        vulkan::memory::Image::setImageLayout(cmd.get(), textureImage->getImage(), vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
        vulkan::memory::Image::copyBufferToImage(cmd.get(), stagingBuffer.get(), textureImage->getImage(), extent);
        vulkan::memory::Image::setImageLayout(cmd.get(), textureImage->getImage(), vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

        pool.submitSingleUse(std::move(cmd), device.computeQueue());

        return textureImage;
    }


}
