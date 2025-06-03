module;

#include <vulkan/vulkan.hpp>

export module vulkan.memory.descriptor_set_layout;

import core.log;

namespace vulkan::memory {

/* Wrapper for a DescriptorSetLayout,
 * Usage is creating an object, using addBinding for all bindings,
 * then using build
 * TODO make it so that the binding can be passed in a constructor,
 * TODO look into VK_push_descriptor to replace this */

export class DescriptorSetLayout {
public:
    DescriptorSetLayout(vk::Device device) : _device(device) {};

    void addBinding(uint32_t binding,                  // number that this data will be bound to, e.g 1,2,3
                    vk::DescriptorType type,           // type of data, e.g. vk::DescriptorType::eUniformBuffer
                    vk::ShaderStageFlags stageFlags,   // where it will be used within the pipe, e.g.
                    uint32_t count = 1                 // size of the data, if it's an array it will be bigger
                    );

    void build();

    vk::DescriptorSetLayout get() const noexcept { return _layout.get(); }
    const std::vector<vk::DescriptorSetLayoutBinding>& bindings() const noexcept { return _bindings; }


private:
    vk::Device _device;
    vk::UniqueDescriptorSetLayout _layout;
    std::vector<vk::DescriptorSetLayoutBinding> _bindings;
};

void DescriptorSetLayout::addBinding(uint32_t binding, vk::DescriptorType type,
                                     vk::ShaderStageFlags stageFlags, uint32_t count) {
    vk::DescriptorSetLayoutBinding layoutBinding = {};
    layoutBinding.binding = binding;
    layoutBinding.descriptorType = type;
    layoutBinding.descriptorCount = count;
    layoutBinding.stageFlags = stageFlags;
    layoutBinding.pImmutableSamplers = nullptr;

    _bindings.push_back(layoutBinding);
}

// Build the actual VkDescriptorSetLayout
void DescriptorSetLayout::build() {
    vk::DescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = vk::StructureType::eDescriptorSetLayoutCreateInfo;
    layoutInfo.bindingCount = static_cast<uint32_t>(_bindings.size());
    layoutInfo.pBindings = _bindings.data();

    _layout = _device.createDescriptorSetLayoutUnique(layoutInfo);
}
}