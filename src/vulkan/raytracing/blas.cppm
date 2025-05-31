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
                const vk::Buffer& vertexBuffer,
                uint32_t vertexCount,
                vk::DeviceSize vertexStride
        );

        const vk::AccelerationStructureKHR& get() const { return accelerationStructure_.get(); }

    private:
        vk::Device device_;
        vk::PhysicalDevice physicalDevice_;
        std::optional<vulkan::memory::Buffer> buffer_;

        vk::DeviceAddress deviceAddress_;

        vk::UniqueAccelerationStructureKHR accelerationStructure_;
        vk::Buffer vertexBuffer_;
    };

    BLAS::BLAS( const context::Device& device, context::CommandPool& commandPool, const vk::Buffer& vertexBuffer, uint32_t vertexCount, vk::DeviceSize vertexStride)
            : device_(device.get()),physicalDevice_(device.physical()), vertexBuffer_(vertexBuffer) {

        vk::DeviceOrHostAddressConstKHR vertexAddress{};

        vk::BufferDeviceAddressInfo info;
        info.buffer = vertexBuffer_;
        vk::DeviceAddress addr = device_.getBufferAddress(info);

        vertexAddress.deviceAddress = addr;

        vk::AccelerationStructureGeometryTrianglesDataKHR triangles{
                vk::Format::eR32G32B32Sfloat,
                vertexAddress,
                vertexStride,
                vertexCount,
                vk::IndexType::eNoneKHR,
                {},
                {}
        };

        vk::AccelerationStructureGeometryKHR geometry{
                vk::GeometryTypeKHR::eTriangles,
                triangles,
                vk::GeometryFlagBitsKHR::eOpaque
        };

        vk::AccelerationStructureBuildRangeInfoKHR range{
                vertexCount, 0, 0, 0
        };

        vk::AccelerationStructureBuildGeometryInfoKHR buildInfo{
                vk::AccelerationStructureTypeKHR::eBottomLevel,
                vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace,
                vk::BuildAccelerationStructureModeKHR::eBuild,
                nullptr,
                nullptr,
                1,
                &geometry,
                nullptr
        };

        vk::AccelerationStructureBuildSizesInfoKHR sizeInfo = device_.getAccelerationStructureBuildSizesKHR(
                        vk::AccelerationStructureBuildTypeKHR::eDevice,
                        buildInfo,
                        { vertexCount });

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

        buildInfo.dstAccelerationStructure = accelerationStructure_.get();

        memory::Buffer scratch(
                device,
                sizeInfo.buildScratchSize,
                vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
                vk::MemoryPropertyFlagBits::eDeviceLocal
        );

        vk::BufferDeviceAddressInfo scratchInfo;
        scratchInfo.buffer = scratch.get();
        vk::DeviceAddress scratchAddr = device_.getBufferAddress(scratchInfo);

        buildInfo.scratchData.deviceAddress = scratchAddr;

        const vk::AccelerationStructureBuildRangeInfoKHR* pRange = &range;   // TODO make this use a single use command buffer for ALL blas
        auto cmd = commandPool.getSingleUseBuffer();
        cmd->buildAccelerationStructuresKHR(1, &buildInfo, &pRange);
        commandPool.submitSingleUse(std::move(cmd), device.computeQueue());

        vk::AccelerationStructureDeviceAddressInfoKHR addressInfo{
                accelerationStructure_.get()
        };

        deviceAddress_ = device_.getAccelerationStructureAddressKHR(addressInfo);
    }

} // namespace vulkan::raytracing
