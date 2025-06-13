module;

#include <vulkan/vulkan.hpp>

export module vulkan.raytracing.blas;

import vulkan.context.device;
import vulkan.memory.buffer;
import vulkan.context.command_pool;

namespace vulkan::raytracing {

    export class BLAS {
    public:
        BLAS(
                const context::Device& device,
                context::CommandPool& commandPool,
                const vk::DeviceAddress& vertexAddress,
                const vk::DeviceAddress& indexAddress,
                uint32_t vertexCount,
                vk::DeviceSize vertexStride,
                uint32_t indexCount,
                vk::IndexType indexType,
                uint32_t vertexIndexOffset,
                uint32_t indexIndexOffset
        );

        const vk::AccelerationStructureKHR& get() const { return accelerationStructure_.get(); }
        const vk::DeviceAddress getAddress() const { return deviceAddress_; }

        const uint32_t getVertexIndexOffset() const { return vertexIndexOffset_; }
        const uint32_t getIndexIndexOffset() const  { return indexIndexOffset_; }

    private:
        vk::Device device_;
        vk::PhysicalDevice physicalDevice_;
        std::optional<vulkan::memory::Buffer> buffer_;

        vk::DeviceAddress deviceAddress_;

        uint32_t vertexIndexOffset_; // number of vertices before this blas
        uint32_t indexIndexOffset_;  // number of indices before this blas

        vk::UniqueAccelerationStructureKHR accelerationStructure_;
    };

    // TODO USE THE MEMORY ADDRESS STUFF IN BUFFER CLASS
    BLAS::BLAS(const context::Device& device, context::CommandPool& commandPool, const vk::DeviceAddress& vertexAddress, const vk::DeviceAddress& indexAddress, uint32_t vertexCount,
                vk::DeviceSize vertexStride, uint32_t indexCount, vk::IndexType indexType, uint32_t vertexIndexOffset, uint32_t indexIndexOffset)
                    : device_(device.get()),physicalDevice_(device.physical()), vertexIndexOffset_(vertexIndexOffset), indexIndexOffset_(indexIndexOffset) {

        vk::DeviceOrHostAddressConstKHR vertexAddr{};
        vertexAddr.deviceAddress = vertexAddress;

        vk::DeviceOrHostAddressConstKHR indexAddr{};
        indexAddr.deviceAddress = indexAddress;

        //  device pointer to the buffers holding triangle vertex/index data, along with information for interpreting it as an array
        vk::AccelerationStructureGeometryTrianglesDataKHR triangles{};
        triangles.setVertexFormat(vk::Format::eR32G32B32Sfloat);
        triangles.setVertexStride(vertexStride);
        triangles.setVertexData(vertexAddr);
        triangles.setMaxVertex(vertexCount);
        triangles.setIndexType(indexType);
        triangles.setIndexData(indexAddr);

        //wrapper around the above with the geometry type enum (triangles in this case) plus flags for the AS builder
        vk::AccelerationStructureGeometryKHR geometry{};
        geometry.setGeometryType(vk::GeometryTypeKHR::eTriangles);
        geometry.setFlags(vk::GeometryFlagBitsKHR::eOpaque);
        geometry.geometry.setTriangles(triangles);

        uint32_t primitiveCount = indexCount / 3;

        // the indices within the vertex arrays to source input geometry for the BLAS.
        vk::AccelerationStructureBuildRangeInfoKHR range;
        range.setPrimitiveCount(primitiveCount);
        range.setFirstVertex(0);
        range.setPrimitiveOffset(0);
        range.setTransformOffset(0);

        vk::AccelerationStructureBuildGeometryInfoKHR buildGeometryInfo;
        buildGeometryInfo.setType(vk::AccelerationStructureTypeKHR::eBottomLevel);
        buildGeometryInfo.setFlags(vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace);
        buildGeometryInfo.setMode(vk::BuildAccelerationStructureModeKHR::eBuild);
        buildGeometryInfo.setGeometryCount(1);
        buildGeometryInfo.setGeometries({geometry});


        vk::AccelerationStructureBuildSizesInfoKHR sizeInfo = device_.getAccelerationStructureBuildSizesKHR(
                        vk::AccelerationStructureBuildTypeKHR::eDevice,
                        buildGeometryInfo,
                        { primitiveCount });

        buffer_ = memory::Buffer(
                device,
                sizeInfo.accelerationStructureSize,
                vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR |
                vk::BufferUsageFlagBits::eShaderDeviceAddress,
                vk::MemoryPropertyFlagBits::eDeviceLocal
        );

        vk::AccelerationStructureCreateInfoKHR createInfo = {};
        createInfo.buffer = buffer_->get();
        createInfo.size = sizeInfo.accelerationStructureSize;
        createInfo.type = vk::AccelerationStructureTypeKHR::eBottomLevel;

        accelerationStructure_ = device_.createAccelerationStructureKHRUnique(createInfo);

        buildGeometryInfo.dstAccelerationStructure = accelerationStructure_.get();

        memory::Buffer scratch(
                device,
                sizeInfo.buildScratchSize,
                vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
                vk::MemoryPropertyFlagBits::eDeviceLocal
        );

        vk::BufferDeviceAddressInfo scratchInfo;
        scratchInfo.buffer = scratch.get();
        vk::DeviceAddress scratchAddr = device_.getBufferAddress(scratchInfo);

        buildGeometryInfo.scratchData.deviceAddress = scratchAddr;

        const vk::AccelerationStructureBuildRangeInfoKHR* pRange = &range;   // TODO make this use a single use command buffer for ALL blas
        auto cmd = commandPool.getSingleUseBuffer();
        cmd->buildAccelerationStructuresKHR(1, &buildGeometryInfo, &pRange);
        commandPool.submitSingleUse(std::move(cmd), device.computeQueue());
        device_.waitIdle();

        vk::AccelerationStructureDeviceAddressInfoKHR addressInfo{
                accelerationStructure_.get()
        };

        deviceAddress_ = device_.getAccelerationStructureAddressKHR(addressInfo);
    }

} // namespace vulkan::raytracing
