module;

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>

export module vulkan.raytracing.tlas;

import vulkan.context.device;
import vulkan.memory.buffer;
import vulkan.context.command_pool;
import vulkan.raytracing.blas;
namespace vulkan::raytracing {

    export class TLAS {
    public:
        TLAS(const context::Device& device, context::CommandPool& commandPool,
             const BLAS& blass);

        const vk::AccelerationStructureKHR& get() const { return accelerationStructure_.get(); }
        vk::DeviceAddress deviceAddress() const { return deviceAddress_; }

        vk::Buffer getBuffer() { return buffer_->get(); }

    private:
        vk::Device                            device_;
        vk::PhysicalDevice                    physicalDevice_;
        std::optional<vulkan::memory::Buffer> buffer_;
        vk::DeviceAddress                     deviceAddress_;
        vk::UniqueAccelerationStructureKHR    accelerationStructure_;
    };
    TLAS::TLAS(const context::Device& device, context::CommandPool& commandPool,
         const BLAS& blas) :
            device_(device.get()), physicalDevice_(device.physical()) {

        vk::TransformMatrixKHR transformMatrix = std::array{
                std::array{1.0f, 0.0f, 0.0f, 0.0f},
                std::array{0.0f, 1.0f, 0.0f, 0.0f},
                std::array{0.0f, 0.0f, 1.0f, 0.0f},
        };


        vk::AccelerationStructureInstanceKHR accelInstance;  // THIS IS HARD CODED FOR NOW, AS WELL AS TRANSFORMATION MATRIX, JUST FOR TESTING
        accelInstance.setTransform(transformMatrix);
        accelInstance.setMask(0xFF);
        accelInstance.setAccelerationStructureReference(blas.getAddress());
        accelInstance.setFlags(vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable);

        // Upload instances to buffer
        memory::Buffer instanceBuffer(
                device,
                sizeof(vk::AccelerationStructureInstanceKHR),
                vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eStorageBuffer |
                vk::BufferUsageFlagBits::eShaderDeviceAddress,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
        );
        instanceBuffer.fill(&accelInstance, sizeof(vk::AccelerationStructureInstanceKHR));

        vk::AccelerationStructureGeometryInstancesDataKHR instancesData;
        instancesData.setArrayOfPointers(false);
        instancesData.setData(instanceBuffer.getAddress());

        vk::AccelerationStructureGeometryKHR instanceGeometry;
        instanceGeometry.setGeometryType(vk::GeometryTypeKHR::eInstances);
        instanceGeometry.setGeometry(instancesData);
        instanceGeometry.setFlags(vk::GeometryFlagBitsKHR::eOpaque);

        vk::AccelerationStructureBuildGeometryInfoKHR buildGeometryInfo;
        buildGeometryInfo.setType(vk::AccelerationStructureTypeKHR::eTopLevel);
        buildGeometryInfo.setFlags(vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace);
        buildGeometryInfo.setGeometries(instanceGeometry);

        vk::AccelerationStructureBuildSizesInfoKHR sizeInfo = device_.getAccelerationStructureBuildSizesKHR(
                vk::AccelerationStructureBuildTypeKHR::eDevice, buildGeometryInfo, 1);   // HARD CODED FOR NOW

        buffer_ = memory::Buffer( device, sizeInfo.accelerationStructureSize,
                vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress,
                vk::MemoryPropertyFlagBits::eDeviceLocal );


        vk::AccelerationStructureCreateInfoKHR createInfo = {};
        createInfo.buffer = buffer_->get();
        createInfo.size = sizeInfo.accelerationStructureSize;
        createInfo.type = vk::AccelerationStructureTypeKHR::eTopLevel;
        accelerationStructure_ = device_.createAccelerationStructureKHRUnique(createInfo);

        memory::Buffer scratch(
                device,
                sizeInfo.buildScratchSize,
                vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
                vk::MemoryPropertyFlagBits::eDeviceLocal
        );

        buildGeometryInfo.setDstAccelerationStructure(*accelerationStructure_);
        buildGeometryInfo.setScratchData(scratch.getAddress());
        buildGeometryInfo.setMode(vk::BuildAccelerationStructureModeKHR::eBuild);

        vk::AccelerationStructureBuildRangeInfoKHR buildRangeInfo;
        buildRangeInfo.setPrimitiveCount(1);
        buildRangeInfo.setFirstVertex(0);
        buildRangeInfo.setPrimitiveOffset(0);
        buildRangeInfo.setTransformOffset(0);

        auto cmd = commandPool.getSingleUseBuffer();
        cmd->buildAccelerationStructuresKHR(buildGeometryInfo, &buildRangeInfo);
        commandPool.submitSingleUse(std::move(cmd), device.computeQueue());
        device_.waitIdle();

        deviceAddress_ = device_.getAccelerationStructureAddressKHR({accelerationStructure_.get()});
    }
} // namespace vulkan::raytracing
