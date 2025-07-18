module;

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>

export module vulkan.raytracing.tlas;

import vulkan.context.device;
import vulkan.memory.buffer;
import vulkan.context.command_pool;
import vulkan.raytracing.blas;
import scene.object;
import scene.area_light;


namespace vulkan::raytracing {

    export class TLAS {
    public:
        // TODO make a build function that does that second half of the constructor and does refit if update = true
        TLAS(const context::Device& device, context::CommandPool& commandPool,
             const std::vector<const BLAS*>& blass, std::vector<std::shared_ptr<scene::Object>>& object,
             std::vector<std::shared_ptr<scene::AreaLight>>& areaareaLights);

        const vk::AccelerationStructureKHR& get() const { return accelerationStructure_.get(); }
        vk::DeviceAddress deviceAddress() const { return deviceAddress_; }

        vk::Buffer getBuffer() { return buffer_->get(); }

        void updateTransform(uint32_t index, const vk::TransformMatrixKHR& newTransform);
        void refit(const context::Device& device, context::CommandPool& commandPool);

    private:
        vk::Device                            device_;
        vk::PhysicalDevice                    physicalDevice_;
        std::optional<vulkan::memory::Buffer> buffer_;
        vk::DeviceAddress                     deviceAddress_;
        vk::UniqueAccelerationStructureKHR    accelerationStructure_;
        std::vector<vk::AccelerationStructureInstanceKHR> accelInstances_;
        std::optional<vulkan::memory::Buffer> instanceBuffer_;
        bool needsUpdate_;
    };

    TLAS::TLAS(const context::Device& device, context::CommandPool& commandPool,
               const std::vector<const BLAS*>& blass, std::vector<std::shared_ptr<scene::Object>>& objects, std::vector<std::shared_ptr<scene::AreaLight>>& areaLights) :
            device_(device.get()), physicalDevice_(device.physical()), needsUpdate_(false) {

        uint32_t numareaLights = areaLights.size();

        int i = 0; // custom index for the blas

        auto createBLASInstance = [&](auto& currObject) {
            uint32_t blasIndex = currObject->getBLASIndex();
            vk::TransformMatrixKHR transform = currObject->getTransform();

            const BLAS* blas = blass[blasIndex];

            currObject->setNumTriangles(blas->getIndexCount() / 3);

            vk::AccelerationStructureInstanceKHR instance;
            instance.setTransform(transform);
            instance.setAccelerationStructureReference(blas->getAddress());
            instance.setMask(0xFF);
            //instance.setInstanceShaderBindingTableRecordOffset(0); // Look into this later
            currObject->setInstanceIndex(i);
            instance.setInstanceCustomIndex(i); // used in shaders, needs to match index in object struct + number of areaLights
            instance.setFlags(vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable);  // MAYBE ENABLE IN FUTURE


            currObject->setVertexOffset(blas->getVertexIndexOffset());
            currObject->setIndexOffset(blas->getIndexIndexOffset());

            accelInstances_.push_back(instance);
            ++i;
        };
        
        for (auto& light : areaLights) {
            createBLASInstance(light);
        }
        for (auto& object : objects) {
            createBLASInstance(object);
        }

        uint32_t instanceCount = accelInstances_.size();
        // Upload instances to buffer
        instanceBuffer_ = memory::Buffer(
                device,
                sizeof(vk::AccelerationStructureInstanceKHR) * instanceCount,
                vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eStorageBuffer |
                vk::BufferUsageFlagBits::eShaderDeviceAddress,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
        );
        instanceBuffer_->fill(accelInstances_.data(), sizeof(vk::AccelerationStructureInstanceKHR) * instanceCount);

        vk::AccelerationStructureGeometryInstancesDataKHR instancesData;
        instancesData.setArrayOfPointers(false);
        instancesData.setData(instanceBuffer_->getAddress());

        vk::AccelerationStructureGeometryKHR instanceGeometry;
        instanceGeometry.setGeometryType(vk::GeometryTypeKHR::eInstances);
        instanceGeometry.setGeometry(instancesData);
        instanceGeometry.setFlags(vk::GeometryFlagBitsKHR::eOpaque);

        vk::AccelerationStructureBuildGeometryInfoKHR buildGeometryInfo;
        buildGeometryInfo.setType(vk::AccelerationStructureTypeKHR::eTopLevel);
        buildGeometryInfo.setFlags(vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace | vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate);
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

    void TLAS::updateTransform(uint32_t index, const vk::TransformMatrixKHR& newTransform) {
        accelInstances_[index].transform = newTransform;

        // Update instance buffer
        instanceBuffer_->fill(accelInstances_.data(),
                              sizeof(vk::AccelerationStructureInstanceKHR) * accelInstances_.size());

        needsUpdate_ = true;
    }

    void TLAS::refit(const context::Device& device, context::CommandPool& commandPool) {
        if (!needsUpdate_) {
            return;
        }
        vk::AccelerationStructureGeometryInstancesDataKHR instancesData;
        instancesData.setArrayOfPointers(false);
        instancesData.setData(instanceBuffer_->getAddress());

        vk::AccelerationStructureGeometryKHR instanceGeometry;
        instanceGeometry.setGeometryType(vk::GeometryTypeKHR::eInstances);
        instanceGeometry.setGeometry(instancesData);
        instanceGeometry.setFlags(vk::GeometryFlagBitsKHR::eOpaque);

        vk::AccelerationStructureBuildGeometryInfoKHR buildGeometryInfo;
        buildGeometryInfo.setType(vk::AccelerationStructureTypeKHR::eTopLevel);
        buildGeometryInfo.setFlags(vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace | vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate);
        buildGeometryInfo.setGeometries(instanceGeometry);
        buildGeometryInfo.setMode(vk::BuildAccelerationStructureModeKHR::eUpdate);
        buildGeometryInfo.setDstAccelerationStructure(accelerationStructure_.get());
        buildGeometryInfo.setSrcAccelerationStructure(accelerationStructure_.get());

        std::vector<uint32_t> primitiveCounts{ static_cast<uint32_t>(accelInstances_.size()) };

        auto sizeInfo = device_.getAccelerationStructureBuildSizesKHR(
                vk::AccelerationStructureBuildTypeKHR::eDevice,
                buildGeometryInfo, primitiveCounts);

        memory::Buffer scratch(
                device,
                sizeInfo.buildScratchSize,
                vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
                vk::MemoryPropertyFlagBits::eDeviceLocal
        );

        buildGeometryInfo.setScratchData(scratch.getAddress());

        vk::AccelerationStructureBuildRangeInfoKHR buildRangeInfo;
        buildRangeInfo.setPrimitiveCount(static_cast<uint32_t>(accelInstances_.size()));
        buildRangeInfo.setFirstVertex(0);
        buildRangeInfo.setPrimitiveOffset(0);
        buildRangeInfo.setTransformOffset(0);

        auto cmd = commandPool.getSingleUseBuffer();
        cmd->buildAccelerationStructuresKHR(buildGeometryInfo, &buildRangeInfo);
        commandPool.submitSingleUse(std::move(cmd), device.computeQueue());
        device_.waitIdle();
    }

} // namespace vulkan::raytracing
