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

    ~DescriptorPool() = default;

    /// Allocate descriptor sets following the provided layouts.
    /// Returns a vector of handles; throws on failure.
    void allocate(const std::vector<DescriptorSetLayout>& layouts);

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

void DescriptorPool::allocate(const std::vector<DescriptorSetLayout>& layouts) {
    std::vector<vk::DescriptorSetLayout> vulkanLayouts;
    for (auto& layout : layouts) {
        vulkanLayouts.push_back(layout.get());
    }

    vk::DescriptorSetAllocateInfo allocInfo(
            pool_.get(),
            static_cast<uint32_t>(vulkanLayouts.size()),
            vulkanLayouts.data()
    );

    try {
        descriptorSets_ = device_.allocateDescriptorSets(allocInfo);
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to allocate descriptor sets: " + std::string(e.what()));
    }
}

} // namespace vulkan
