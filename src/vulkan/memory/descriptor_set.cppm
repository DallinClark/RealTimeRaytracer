module;

#include <vulkan/vulkan.hpp>

export module vulkan.memory.descriptor_set;

import core.log;

namespace vulkan::memory {

export class DescriptorSet {
public:
    DescriptorSet(
        vk::Device device,
        const std::vector<vk::DescriptorSetLayoutBinding>& bindings,
        vk::DescriptorPool pool
    ) : _device(device), _pool(pool)
    {
        // Create layout
        vk::DescriptorSetLayoutCreateInfo layoutInfo(
                {},
                static_cast<uint32_t>(bindings.size()),
                bindings.data()
        );
        _layout = _device.createDescriptorSetLayoutUnique(layoutInfo);

        // Allocate one descriptor set from the pool
        vk::DescriptorSetAllocateInfo allocInfo(
                _pool,
                1,
                &_layout.get()
        );
        auto sets = _device.allocateDescriptorSets(allocInfo);
        _set = sets[0];
    }

    ~DescriptorSet() = default;

    /// Return the layout handle
    vk::DescriptorSetLayout getLayout() const noexcept {
        return _layout.get();
    }

    /// Return the descriptor set handle
    vk::DescriptorSet getSet() const noexcept {
        return _set;
    }

    /// Update a uniform buffer binding in the set
    void writeBuffer(uint32_t binding, vk::Buffer buffer, vk::DeviceSize offset, vk::DeviceSize range) {
        vk::DescriptorBufferInfo bufInfo(buffer, offset, range);
        vk::WriteDescriptorSet write(_set, binding, 0,
                                     1, vk::DescriptorType::eUniformBuffer,
                                     nullptr, &bufInfo, nullptr);
        _device.updateDescriptorSets(1, &write, 0, nullptr);
    }

    /// Update a combined image sampler binding in the set
    void writeImage(uint32_t binding, vk::ImageView view, vk::Sampler sampler,
                    vk::ImageLayout layout = vk::ImageLayout::eShaderReadOnlyOptimal) {
        vk::DescriptorImageInfo imgInfo(sampler, view, layout);
        vk::WriteDescriptorSet write(_set, binding, 0,
                                     1, vk::DescriptorType::eCombinedImageSampler,
                                     &imgInfo, nullptr, nullptr);
        _device.updateDescriptorSets(1, &write, 0, nullptr);
    }

private:
    vk::Device                    _device;
    vk::UniqueDescriptorSetLayout _layout;
    vk::DescriptorPool            _pool;
    vk::DescriptorSet             _set;
};
}