module;

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include <string>
#include <iostream>
#include <memory>
#include <unordered_map>

export module app.setup.geometry_builder;

import core.log;
import core.file;
import scene.geometry.vertex;
import scene.object;
import scene.area_light;
import vulkan.raytracing.blas;
import vulkan.raytracing.tlas;
import vulkan.memory.buffer;
import vulkan.context.device;
import vulkan.context.command_pool;


export namespace app::setup  {
    /* Usage: takes in a vector of filepaths to obj files
     * and a vector of objectCreateInfos,
     * and creates all the info for them and creates a TLAS */

    class GeometryBuilder {
    public:
        // Holds Info for each BLAS
        struct BLASCreateInfo {
            vk::DeviceSize vertexOffset;
            vk::DeviceSize indexOffset;

            vk::DeviceSize vertexSize;
            vk::DeviceSize indexSize;

            uint32_t       vertexCount;
            uint32_t       indexCount;

            uint32_t       vertexIndexOffset;
            uint32_t       indexIndexOffset;

            uint32_t       vertexStride = sizeof(glm::vec3); // or your actual vertex layout
        };

        struct GeometryReturnInfo {
            std::unique_ptr<vulkan::raytracing::TLAS>                tlas;
            std::vector<std::unique_ptr<vulkan::raytracing::BLAS>>   blass;
            std::vector<scene::geometry::Vertex>                     vertices;
            std::vector<glm::vec3>                                   vertexPositions;
            std::vector<uint32_t>                                    indices;
            vulkan::memory::Buffer                                   vertexBuffer;
            vulkan::memory::Buffer                                   indexBuffer;
        };

        static GeometryReturnInfo createTLASFromOBJsAndTransforms(const vulkan::context::Device& device, vulkan::context::CommandPool& commandPool,
                                                                  std::vector<scene::Object>& objects, std::vector<scene::AreaLight>& areaLights);

    private:
    };

    GeometryBuilder::GeometryReturnInfo GeometryBuilder::createTLASFromOBJsAndTransforms(const vulkan::context::Device& device, vulkan::context::CommandPool& commandPool,
                                                                                         std::vector<scene::Object>& objects, std::vector<scene::AreaLight>& areaLights) {

        std::vector<glm::vec3> vertexPositions{};
        std::vector<uint32_t> indices{};
        std::vector<scene::geometry::Vertex> vertices{};
        std::vector<BLASCreateInfo> meshDatas{};

        vk::DeviceSize currVertexOffset = 0;
        vk::DeviceSize currIndexOffset  = 0;

        // Lights always go first before objects,
        // all area lights share vertices and indices,
        // so you can just make one BLAS for now
        size_t prevVertexCount = vertexPositions.size();
        size_t prevIndexCount  = indices.size();

        for (const glm::vec3& point : areaLights[0].getPoints()) {
            vertexPositions.push_back(point);
            vertices.push_back(scene::geometry::Vertex{point});
        }
        std::vector<uint32_t> temp = {0, 1, 2, 0, 2, 3}; // EVERY AREA LIGHT IS A RECTANGLE WITH 2 TRIANGLES
        for (const uint32_t& index : temp) {
            indices.push_back(static_cast<uint32_t>(index));
        }

        size_t newVertexCount = vertexPositions.size() - prevVertexCount;
        size_t newIndexCount  = indices.size() - prevIndexCount;

        BLASCreateInfo meshData{};
        meshData.vertexOffset  = currVertexOffset;
        meshData.indexOffset   = currIndexOffset;
        meshData.vertexCount   = static_cast<uint32_t>(newVertexCount);
        meshData.indexCount    = static_cast<uint32_t>(newIndexCount);
        meshData.vertexSize    = newVertexCount * sizeof(glm::vec3);
        meshData.indexSize     = newIndexCount * sizeof(uint32_t);
        meshData.vertexIndexOffset = prevVertexCount;
        meshData.indexIndexOffset  = prevIndexCount;

        currVertexOffset += newVertexCount * sizeof(glm::vec3);
        currIndexOffset  += newIndexCount * sizeof(uint32_t);

        meshDatas.push_back(meshData);

        for (auto& light : areaLights) {
            light.setVertexOffset(0); // Every area light uses the first 4 points
        }


        std::unordered_map<std::string, int> loadedModels;

        // Loads the models for each object
        for (auto& object : objects) {
            // check if model has already been loaded
            std::string modelPath = object.getOBJPath();
            auto it = loadedModels.find(modelPath);

            if (it != loadedModels.end()) {
                // Already mapped
                object.setBLASIndex(it->second);
                continue;
            }

            size_t prevVertexCount = vertexPositions.size();
            size_t prevIndexCount  = indices.size();

            core::file::loadModel(modelPath, vertexPositions, indices, vertices);

            size_t newVertexCount = vertexPositions.size() - prevVertexCount;
            size_t newIndexCount  = indices.size() - prevIndexCount;

            BLASCreateInfo meshData;
            meshData.vertexOffset = currVertexOffset;
            meshData.indexOffset  = currIndexOffset;
            meshData.vertexCount  = static_cast<uint32_t>(newVertexCount);
            meshData.indexCount   = static_cast<uint32_t>(newIndexCount);
            meshData.vertexSize   = newVertexCount * sizeof(glm::vec3);
            meshData.indexSize    = newIndexCount * sizeof(uint32_t);
            meshData.vertexIndexOffset = prevVertexCount;
            meshData.indexIndexOffset  = prevIndexCount;

            currVertexOffset += newVertexCount * sizeof(glm::vec3);
            currIndexOffset  += newIndexCount * sizeof(uint32_t);

            meshDatas.push_back(meshData);

            object.setBLASIndex(meshDatas.size() - 1);
            loadedModels[modelPath] = meshDatas.size() - 1;
        }

        // creates the index and vertex buffers
        vk::DeviceSize vertexBufferSize = vertexPositions.size() * sizeof(glm::vec3);
        vk::DeviceSize indexBufferSize  = indices.size() * sizeof(uint32_t);

        std::vector<vulkan::memory::Buffer::FillRegion> vertexBufferFillRegions {};
        std::vector<vulkan::memory::Buffer::FillRegion>  indexBufferFillRegions {};


        for (const auto& mesh : meshDatas) {
            vulkan::memory::Buffer::FillRegion vertexFill{};
            vertexFill.data = vertexPositions.data() + (mesh.vertexOffset / sizeof(glm::vec3));
            vertexFill.size = mesh.vertexSize;
            vertexFill.offset = mesh.vertexOffset;

            vertexBufferFillRegions.push_back(vertexFill);

            vulkan::memory::Buffer::FillRegion indexFill{};
            indexFill.data = indices.data() + (mesh.indexOffset / sizeof(uint32_t));
            indexFill.size = mesh.indexSize;
            indexFill.offset = mesh.indexOffset;

            indexBufferFillRegions.push_back(indexFill);
        }

        vk::DeviceSize totalVertexSize = vertexPositions.size() * sizeof(glm::vec3);
        vk::DeviceSize totalIndexSize  = indices.size() * sizeof(uint32_t);

        //create vertex and index buffer
        vulkan::memory::Buffer vertexBuffer = vulkan::memory::Buffer::createDeviceLocalBuffer(commandPool, device, totalVertexSize, vk::BufferUsageFlagBits::eVertexBuffer |
                                                 vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR |
                                                 vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, vertexBufferFillRegions);

        vulkan::memory::Buffer indexBuffer = vulkan::memory::Buffer::createDeviceLocalBuffer(commandPool, device, totalIndexSize,
                                                                                              vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress |
                                                                                              vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR |
                                                                                              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, indexBufferFillRegions);

        vk::DeviceAddress baseVertexAddress = vertexBuffer.getAddress();
        vk::DeviceAddress baseIndexAddress  = indexBuffer.getAddress();

        std::vector<std::unique_ptr<vulkan::raytracing::BLAS>> blasStorage; // keep ownership
        std::vector<const vulkan::raytracing::BLAS*> blass;

        // creates the BLASs
        for (const auto& mesh : meshDatas) {
            vk::DeviceAddress vertexAddress = baseVertexAddress + mesh.vertexOffset;
            vk::DeviceAddress indexAddress  = baseIndexAddress  + mesh.indexOffset;

            auto newBLAS = std::make_unique<vulkan::raytracing::BLAS>(
                    device,
                    commandPool,
                    vertexAddress,
                    indexAddress,
                    mesh.vertexCount,
                    mesh.vertexStride,
                    mesh.indexCount,
                    vk::IndexType::eUint32,
                    mesh.vertexIndexOffset,
                    mesh.indexIndexOffset
            );

            blass.push_back(newBLAS.get());      // collect raw pointer
            blasStorage.push_back(std::move(newBLAS)); // store for ownership
        }

        // create acceleration structures
        auto tlas = std::make_unique<vulkan::raytracing::TLAS>(device, commandPool, blass, objects, areaLights);

        return GeometryReturnInfo {
                std::move(tlas),
                std::move(blasStorage),
                std::move(vertices),
                std::move(vertexPositions),
                std::move(indices),
                std::move(vertexBuffer),
                std::move(indexBuffer)
        };
    }
}