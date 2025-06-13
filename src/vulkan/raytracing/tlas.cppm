module;

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>

export module vulkan.raytracing.tlas;

import vulkan.context.device;
import vulkan.memory.buffer;
import vulkan.context.command_pool;
import vulkan.raytracing.blas;
import scene.geometry.object;


namespace vulkan::raytracing {

    export class TLAS {
    public:
        TLAS(const context::Device& device, context::CommandPool& commandPool,
             const std::vector<const BLAS*>& blass, const std::vector<scene::geometry::ObjectCreateInfo>& objectCreateInfos);

        struct BLASInstanceInfo {
            uint32_t vertexIndexOffset;
            uint32_t indexIndexOffset;
            uint32_t textureIndex;
            uint32_t _padding = 0;
        };


        const vk::AccelerationStructureKHR& get() const { return accelerationStructure_.get(); }
        vk::DeviceAddress deviceAddress() const { return deviceAddress_; }

        vk::Buffer getBuffer() { return buffer_->get(); }
        std::vector<BLASInstanceInfo> getBLASInfos() { return BLASInstanceInfos_; }

    private:
        vk::Device                            device_;
        vk::PhysicalDevice                    physicalDevice_;
        std::optional<vulkan::memory::Buffer> buffer_;
        vk::DeviceAddress                     deviceAddress_;
        vk::UniqueAccelerationStructureKHR    accelerationStructure_;

        // a vector of BLASInstanceInfos passed into the shader to give each blas information
        // TODO include texture index
        std::vector<BLASInstanceInfo> BLASInstanceInfos_;
    };

    TLAS::TLAS(const context::Device& device, context::CommandPool& commandPool,
               const std::vector<const BLAS*>& blass, const std::vector<scene::geometry::ObjectCreateInfo>& objectCreateInfos) :
            device_(device.get()), physicalDevice_(device.physical()) {

        std::vector<vk::AccelerationStructureInstanceKHR> accelInstances; // MAYBE ADD A .reserve


        for (int i = 0; i < objectCreateInfos.size(); ++i) {
            auto currCreateInfo = objectCreateInfos[i];

            uint32_t blasIndex = currCreateInfo.objectIndex;
            vk::TransformMatrixKHR transform = currCreateInfo.transform;

            const BLAS* blas = blass[blasIndex];

            vk::AccelerationStructureInstanceKHR instance;
            instance.setTransform(transform);
            instance.setAccelerationStructureReference(blas->getAddress());
            instance.setMask(0xFF);
            //instance.setInstanceShaderBindingTableRecordOffset(0); // Look into this later

            instance.setInstanceCustomIndex(i); // useful in shaders
            instance.setFlags(vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable);  // MAYBE ENABLE IN FUTURE

            accelInstances.push_back(instance);

            BLASInstanceInfo BLASInstanceInfo{blas->getVertexIndexOffset(), blas->getIndexIndexOffset(), currCreateInfo.textureIndex};
            BLASInstanceInfos_.push_back(BLASInstanceInfo);
        }

        uint32_t instanceCount = accelInstances.size();
        // Upload instances to buffer
        memory::Buffer instanceBuffer(
                device,
                sizeof(vk::AccelerationStructureInstanceKHR) * instanceCount,
                vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eStorageBuffer |
                vk::BufferUsageFlagBits::eShaderDeviceAddress,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
        );
        instanceBuffer.fill(accelInstances.data(), sizeof(vk::AccelerationStructureInstanceKHR) * instanceCount);

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
        buildGeometryInfo.setMode(vk::BuildAccelerationStructureModeKHR::eBuild);

        std::vector<uint32_t> primitiveCounts{ instanceCount };

        auto sizeInfo = device_.getAccelerationStructureBuildSizesKHR(
                vk::AccelerationStructureBuildTypeKHR::eDevice, buildGeometryInfo, primitiveCounts);

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
        buildRangeInfo.setPrimitiveCount(instanceCount);
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
