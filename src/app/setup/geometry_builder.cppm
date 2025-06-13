module;

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include <string>
#include <iostream>
#include <memory>

export module app.setup.geometry_builder;

import core.log;
import core.file;
import scene.geometry.vertex;
import scene.geometry.object;
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
        struct BLASMeshData {
            vk::DeviceSize vertexOffset;
            vk::DeviceSize indexOffset;

            vk::DeviceSize vertexSize;
            vk::DeviceSize indexSize;

            uint32_t       vertexCount;
            uint32_t       indexCount;
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
                                                    const std::vector<std::string>& objStrings,
                                                    const std::vector<scene::geometry::ObjectCreateInfo>& objectCreateInfos);

    private:
    };

    GeometryBuilder::GeometryReturnInfo GeometryBuilder::createTLASFromOBJsAndTransforms(
            const vulkan::context::Device& device, vulkan::context::CommandPool& commandPool,
            const std::vector<std::string>& objStrings,
            const std::vector<scene::geometry::ObjectCreateInfo>& objectCreateInfos){

        std::vector<glm::vec3> vertexPositions{};
        std::vector<uint32_t> indices{};
        std::vector<scene::geometry::Vertex> vertices{};
        std::vector<BLASMeshData> meshDatas{};

        vk::DeviceSize currVertexOffset = 0;
        vk::DeviceSize currIndexOffset  = 0;

        // Loads the obj info
        for (const auto& objPath : objStrings) {
            size_t prevVertexCount = vertexPositions.size();
            size_t prevIndexCount  = indices.size();

            core::file::loadModel(objPath, vertexPositions, indices, vertices);

            size_t newVertexCount = vertexPositions.size() - prevVertexCount;
            size_t newIndexCount  = indices.size() - prevIndexCount;

            BLASMeshData meshData;
            meshData.vertexOffset = currVertexOffset;
            meshData.indexOffset  = currIndexOffset;
            meshData.vertexCount  = static_cast<uint32_t>(newVertexCount);
            meshData.indexCount   = static_cast<uint32_t>(newIndexCount);
            meshData.vertexSize   = newVertexCount * sizeof(glm::vec3);
            meshData.indexSize    = newIndexCount * sizeof(uint32_t);

            currVertexOffset += newVertexCount * sizeof(glm::vec3);
            currIndexOffset  += newIndexCount * sizeof(uint32_t);

            meshDatas.push_back(meshData);
        }

        // creates the index and vertex buffers
        vk::DeviceSize vertexBufferSize = vertexPositions.size() * sizeof(glm::vec3);
        vk::DeviceSize indexBufferSize  = indices.size() * sizeof(uint32_t);

        std::vector<vulkan::memory::Buffer::FillRegion> vertexBufferFillRegions {};
        std::vector<vulkan::memory::Buffer::FillRegion>  indexBufferFillRegions {};

        for (const auto& mesh : meshDatas) { // TODO maybe I can do this in above loop
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
                    mesh.vertexOffset / sizeof(glm::vec3),
                    mesh.indexOffset / sizeof(uint32_t)
            );

            blass.push_back(newBLAS.get());      // collect raw pointer
            blasStorage.push_back(std::move(newBLAS)); // store for ownership
        }

        // create acceleration structures
        auto tlas = std::make_unique<vulkan::raytracing::TLAS>(device, commandPool, blass, objectCreateInfos);

        return GeometryReturnInfo {
                std::move(tlas),
                std::move(blasStorage),
                std::move(vertices),
                std::move(vertexPositions),
                std::move(indices),
                std::move(vertexBuffer),
                std::move(indexBuffer)
        };

//        TLAS::TLAS(const context::Device& device, context::CommandPool& commandPool,
//        const std::vector<const BLAS*>& blass, const std::vector<std::pair<vk::TransformMatrixKHR, int>> transformMatrices, uint32_t instanceCount)
    }

}