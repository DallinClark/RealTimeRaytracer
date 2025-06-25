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
        TLAS(const context::Device& device, context::CommandPool& commandPool,
             const std::vector<const BLAS*>& blass, std::vector<scene::Object>& objects,
             std::vector<scene::AreaLight>& areaLights);

        const vk::AccelerationStructureKHR& get() const { return accelerationStructure_.get(); }
        vk::DeviceAddress deviceAddress() const { return deviceAddress_; }

        vk::Buffer getBuffer() { return buffer_->get(); }

    private:
        vk::Device                            device_;
        vk::PhysicalDevice                    physicalDevice_;
        std::optional<vulkan::memory::Buffer> buffer_;
        vk::DeviceAddress                     deviceAddress_;
        vk::UniqueAccelerationStructureKHR    accelerationStructure_;
        std::vector<scene::AreaLight> lights_;

    };

    TLAS::TLAS(const context::Device& device, context::CommandPool& commandPool,
               const std::vector<const BLAS*>& blass, std::vector<scene::Object>& objects, std::vector<scene::AreaLight>& areaLights) :
            device_(device.get()), physicalDevice_(device.physical()), lights_(areaLights) {

        std::vector<vk::AccelerationStructureInstanceKHR> accelInstances; // MAYBE ADD A .reserve

        uint32_t numLights = areaLights.size();

        for (int i = 0; i < numLights; ++i) {
            auto currLight = areaLights[i];

            uint32_t blasIndex = 0; // ALL AREA LIGHTS SHARE A BLAS AT 0
            vk::TransformMatrixKHR transform = currLight.getTransform();

            const BLAS* blas = blass[blasIndex];

            vk::AccelerationStructureInstanceKHR instance;
            instance.setTransform(transform);
            instance.setAccelerationStructureReference(blas->getAddress());
            instance.setMask(0xFF);
            //instance.setInstanceShaderBindingTableRecordOffset(0); // Look into this later

            instance.setInstanceCustomIndex(i); // index in shader, needs to match index in light struct passed to shader
            instance.setFlags(vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable);

            accelInstances.push_back(instance);
        }

        for (int i = 0; i < objects.size(); ++i) {
            scene::Object& currObject = objects[i];

            uint32_t blasIndex = currObject.getBLASIndex();
            vk::TransformMatrixKHR transform = currObject.getTransform();

            const BLAS* blas = blass[blasIndex];

            vk::AccelerationStructureInstanceKHR instance;
            instance.setTransform(transform);
            instance.setAccelerationStructureReference(blas->getAddress());
            instance.setMask(0xFF);
            //instance.setInstanceShaderBindingTableRecordOffset(0); // Look into this later

            instance.setInstanceCustomIndex(i + numLights); // used in shaders, needs to match index in object struct + number of lights
            instance.setFlags(vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable);  // MAYBE ENABLE IN FUTURE

            currObject.setVertexOffset(blas->getVertexIndexOffset());
            currObject.setIndexOffset(blas->getIndexIndexOffset());

            accelInstances.push_back(instance);
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
