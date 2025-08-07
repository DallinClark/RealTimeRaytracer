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

        struct GeometryReturnInfo {
            std::unique_ptr<vulkan::raytracing::TLAS>                tlas;
            std::vector<std::unique_ptr<vulkan::raytracing::BLAS>>   blass;
            std::vector<scene::geometry::Vertex>                     vertices;
            std::vector<glm::vec3>                                   vertexPositions;
            std::vector<uint32_t>                                    indices;
            vulkan::memory::Buffer                                   vertexBuffer;
            vulkan::memory::Buffer                                   indexBuffer;
        };

        static GeometryReturnInfo createAccelerationStructures(const vulkan::context::Device& device, vulkan::context::CommandPool& commandPool,
                                                                                    std::vector<std::shared_ptr<scene::Object>>& objects,
                                                                                    const std::vector<std::pair<std::string, std::string>>& objMtlPairs,
                                                                                    std::vector<std::shared_ptr<scene::AreaLight>>& areaLights);

    private:
    };

    GeometryBuilder::GeometryReturnInfo GeometryBuilder::createAccelerationStructures(const vulkan::context::Device& device, vulkan::context::CommandPool& commandPool,
                                                                                         std::vector<std::shared_ptr<scene::Object>>& objects,
                                                                                         const std::vector<std::pair<std::string, std::string>>& objMtlPairs,
                                                                                         std::vector<std::shared_ptr<scene::AreaLight>>& areaLights) {

        std::vector<glm::vec3> vertexPositions{};
        std::vector<uint32_t> indices{};
        std::vector<scene::geometry::Vertex> vertices{};
        std::vector<vulkan::raytracing::BLAS::BLASCreateInfo> meshDatas{};

        vk::DeviceSize currVertexOffset = vertices.size() * sizeof(glm::vec3);
        vk::DeviceSize currIndexOffset = indices.size() * sizeof(uint32_t);

        // Lights always go first before objects
        size_t prevVertexCount = vertexPositions.size();
        size_t prevIndexCount  = indices.size();

        std::unordered_map<std::string, int> loadedModels;

        // Loads the models for each object
        auto processGeometryObject = [&](auto& instance) {
            std::string modelPath = instance->getOBJPath();
            auto it = loadedModels.find(modelPath);

            if (it != loadedModels.end()) {
                instance->setBLASIndex(it->second);
                return;
            }

            size_t prevVertexCount = vertexPositions.size();
            size_t prevIndexCount  = indices.size();

            if (modelPath == "square") {
                for (const glm::vec3& point : instance->getPoints()) {
                    vertexPositions.push_back(point);
                    vertices.push_back(scene::geometry::Vertex{point});
                }
                std::vector<uint32_t> temp = {0, 1, 2, 0, 2, 3};
                for (const uint32_t& index : temp) {
                    indices.push_back(index);
                }
            } else {
                core::file::loadModel(modelPath, vertexPositions, indices, vertices);
            }

            size_t newVertexCount = vertexPositions.size() - prevVertexCount;
            size_t newIndexCount  = indices.size() - prevIndexCount;

            vulkan::raytracing::BLAS::BLASCreateInfo meshData;
            meshData.vertexOffset = currVertexOffset;
            meshData.indexOffset  = currIndexOffset;
            meshData.vertexCount  = static_cast<uint32_t>(newVertexCount);
            meshData.indexCount   = static_cast<uint32_t>(newIndexCount);
            meshData.vertexSize   = newVertexCount * sizeof(glm::vec3);
            meshData.indexSize    = newIndexCount * sizeof(uint32_t);
            meshData.vertexIndexOffset = prevVertexCount;
            meshData.indexIndexOffset  = prevIndexCount;

            if (instance->usesOpacityMap()) {
                meshData.isOpaque = false;
            } else {
                meshData.isOpaque = true;
            }

            currVertexOffset += newVertexCount * sizeof(glm::vec3);
            currIndexOffset  += newIndexCount * sizeof(uint32_t);

            meshDatas.push_back(meshData);

            instance->setBLASIndex(meshDatas.size() - 1);
            loadedModels[modelPath] = meshDatas.size() - 1;
        };

        for (auto& light : areaLights) {
            processGeometryObject(light);
        }

        for (auto& object : objects) {
            processGeometryObject(object);
        }

        // load objects and data from obj and mtl pairs
        for (const auto& [objPath, mtlPath] : objMtlPairs) {
            // Load geometry and material-assigned objects
            core::file::loadOBJandMTL(objPath, mtlPath, objects, vertexPositions, indices, vertices, meshDatas);
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

        //create vertex and index buffer
        vulkan::memory::Buffer vertexBuffer = vulkan::memory::Buffer::createDeviceLocalBuffer(commandPool, device, vertexBufferSize, vk::BufferUsageFlagBits::eVertexBuffer |
                                                 vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR |
                                                 vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, vertexBufferFillRegions);

        vulkan::memory::Buffer indexBuffer = vulkan::memory::Buffer::createDeviceLocalBuffer(commandPool, device, indexBufferSize,
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
                    mesh.indexIndexOffset,
                    mesh.isOpaque
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