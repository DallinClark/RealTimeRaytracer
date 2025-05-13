module;

#include <vulkan/vulkan.hpp>

export module vulkan.memory.descriptor_pool;

import core.log;
import vulkan.context.device;
import vulkan.memory.descriptor_set;

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
    std::vector<vk::DescriptorSet> allocate(const std::vector<DescriptorSet>& layouts) const;

    /// Reset the pool, freeing all allocated sets. Next alloc reuses space.
    void reset(vk::DescriptorPoolResetFlags flags = {}) const {
        device_.resetDescriptorPool(pool_.get(), flags);
    }

    /// Access the raw VkDescriptorPool handle
    vk::DescriptorPool get() const noexcept { return pool_.get(); }

private:
    vk::Device                            device_;
    vk::UniqueDescriptorPool              pool_;
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
}

std::vector<vk::DescriptorSet> DescriptorPool::allocate(const std::vector<DescriptorSet>& sets) const {
    std::vector<vk::DescriptorSetLayout> layouts;
    layouts.reserve(sets.size());
    for (const auto& set : sets) {
        layouts.push_back(set.getLayout());
    }
    vk::DescriptorSetAllocateInfo allocInfo(
            pool_.get(),
            static_cast<uint32_t>(layouts.size()),
            layouts.data()
    );
    return device_.allocateDescriptorSets(allocInfo);
}
} // namespace vulkan
