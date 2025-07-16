module;

#include <vulkan/vulkan.hpp>

export module vulkan.memory.descriptor_pool;

import core.log;
import vulkan.context.device;
import vulkan.memory.descriptor_set_layout;

namespace vulkan::memory {

export class DescriptorPool {
public:
    DescriptorPool(
            vk::Device device,
            const std::vector<vk::DescriptorPoolSize>& poolSizes,
            uint32_t maxSets
    );

    void writeBuffer(vk::DescriptorSet set, uint32_t binding, vk::Buffer buffer, vk::DeviceSize size, vk::DescriptorType type, vk::DeviceSize offset);

    void writeImage(vk::DescriptorSet set, uint32_t binding, const vk::DescriptorImageInfo& imageInfo, vk::DescriptorType type);

    void writeImages(vk::DescriptorSet set, uint32_t binding, const std::vector<vk::DescriptorImageInfo>& imageInfos, vk::DescriptorType type);

    void writeAccelerationStructure(vk::DescriptorSet set, uint32_t binding, vk::DescriptorType type, vk::AccelerationStructureKHR TLAS);

    ~DescriptorPool() = default;

    /// Allocate descriptor sets following the provided layouts.
    /// Returns a vector of handles; throws on failure.
    std::vector<vk::DescriptorSet> allocate(std::vector<DescriptorSetLayout*> layouts);
    std::vector<vk::DescriptorSet> allocate(const DescriptorSetLayout layout);

    /// Reset the pool, freeing all allocated sets. Next alloc reuses space.
    void reset(vk::DescriptorPoolResetFlags flags = {}) noexcept {
        device_.resetDescriptorPool(pool_.get(), flags);
    }

    vk::DescriptorPool get() const noexcept { return pool_.get(); }
    std::vector<vk::DescriptorSet> getSets() const noexcept { return descriptorSets_; }

private:
    vk::Device                            device_;
    vk::UniqueDescriptorPool              pool_;
    std::vector<vk::DescriptorSet>        descriptorSets_;
};


DescriptorPool::DescriptorPool(
        vk::Device device,
        const std::vector<vk::DescriptorPoolSize>& poolSizes,
        uint32_t maxSets) : device_(device) {

    vk::DescriptorPoolCreateInfo info(
            {},
            maxSets,
            static_cast<uint32_t>(poolSizes.size()),
            poolSizes.data()
    );

    pool_ = device_.createDescriptorPoolUnique(info);
    if (!pool_) {
        throw std::runtime_error("Failed to create descriptor pool");
    }
}

std::vector<vk::DescriptorSet> DescriptorPool::allocate(std::vector<DescriptorSetLayout*> layouts) {
    std::vector<vk::DescriptorSetLayout> vulkanLayouts;
    for (auto& layout : layouts) {
        vulkanLayouts.push_back(layout->get());
    }

    vk::DescriptorSetAllocateInfo allocInfo(
            pool_.get(),
            static_cast<uint32_t>(vulkanLayouts.size()),
            vulkanLayouts.data()
    );

    try {
        descriptorSets_ = device_.allocateDescriptorSets(allocInfo);
        return descriptorSets_;
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to allocate descriptor sets: " + std::string(e.what()));
    }
}

std::vector<vk::DescriptorSet> DescriptorPool::allocate(const DescriptorSetLayout layout) {
    vk::DescriptorSetLayout vulkanLayout = layout.get();

    vk::DescriptorSetAllocateInfo allocInfo(
            pool_.get(),
            1,
            &vulkanLayout
    );

    try {
        descriptorSets_ = device_.allocateDescriptorSets(allocInfo);
        return descriptorSets_;
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to allocate descriptor sets: " + std::string(e.what()));
    }
}

// TODO maybe move this out of pool, or combine into one function
void DescriptorPool::writeBuffer(vk::DescriptorSet set, uint32_t binding, vk::Buffer buffer, vk::DeviceSize size, vk::DescriptorType type, vk::DeviceSize offset) {
    vk::DescriptorBufferInfo bufferInfo{ buffer, 0, size };
    vk::WriteDescriptorSet write{
            set, binding, 0, 1, type, nullptr, &bufferInfo, nullptr
    };
    device_.updateDescriptorSets(write, {});
}

void DescriptorPool::writeImage(vk::DescriptorSet set, uint32_t binding, const vk::DescriptorImageInfo& imageInfo, vk::DescriptorType type) {
    vk::WriteDescriptorSet write{
            set, binding, 0, 1, type, &imageInfo, nullptr, nullptr
    };
    device_.updateDescriptorSets(write, {});
}

void DescriptorPool::writeAccelerationStructure(vk::DescriptorSet set, uint32_t binding, vk::DescriptorType type, vk::AccelerationStructureKHR TLAS) {
    vk::WriteDescriptorSetAccelerationStructureKHR asInfo{};
    asInfo.accelerationStructureCount = 1;
    asInfo.pAccelerationStructures = &TLAS;

    vk::WriteDescriptorSet write{};
    write.setDstSet(set);
    write.setDescriptorType(type);
    write.setDescriptorCount(1);
    write.setDstBinding(binding);
    write.setPNext(&asInfo);

    device_.updateDescriptorSets(write, {});
}

void DescriptorPool::writeImages(vk::DescriptorSet set, uint32_t binding, const std::vector<vk::DescriptorImageInfo>& imageInfos, vk::DescriptorType type) {
    if (imageInfos.empty()) {
        throw std::runtime_error("writeImages: imageInfos is empty.");
    }

    vk::WriteDescriptorSet write{};
    write.dstSet = set;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorCount = static_cast<uint32_t>(imageInfos.size());
    write.descriptorType = type;
    write.pImageInfo = imageInfos.data();

    device_.updateDescriptorSets(write, {});
}

} // namespace vulkan
