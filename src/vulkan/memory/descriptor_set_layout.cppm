module;

#include <vulkan/vulkan.hpp>
#include <unordered_map>

export module vulkan.memory.descriptor_set_layout;

import core.log;

namespace vulkan::memory {

    /* Wrapper for a DescriptorSetLayout,
     * Usage is creating an object, using addBinding for all bindings,
     * then using build
     * TODO make it so that the binding can be passed in a constructor */

    export class DescriptorSetLayout {
    public:
        DescriptorSetLayout(vk::Device device) : _device(device) {}

        void addBinding(uint32_t binding,                  // number that this data will be bound to, e.g 1,2,3
                        vk::DescriptorType type,           // type of data, e.g. vk::DescriptorType::eUniformBuffer
                        vk::ShaderStageFlags stageFlags,   // where it will be used within the pipe, e.g.
                        uint32_t count = 1                 // size of the data, if it's an array it will be bigger
        );

        void build();

        vk::DescriptorSetLayout get() const noexcept { return _layout.get(); }
        const std::vector<vk::DescriptorSetLayoutBinding>& bindings() const noexcept { return _bindings; }

        std::vector<vk::DescriptorPoolSize> getPoolSizes() const;
        static std::vector<vk::DescriptorPoolSize> combinePoolSizes(std::vector<std::vector<vk::DescriptorPoolSize>>& poolSizesVector);

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

    std::vector<vk::DescriptorPoolSize> DescriptorSetLayout::getPoolSizes() const {
        std::unordered_map<vk::DescriptorType, uint32_t> descriptorTypeCounts;

        for (const auto &binding: _bindings) {
            descriptorTypeCounts[binding.descriptorType] += binding.descriptorCount;
        }

        std::vector<vk::DescriptorPoolSize> poolSizes;
        for (const auto &[type, count]: descriptorTypeCounts) {
            poolSizes.emplace_back(type, count);
        }
        return poolSizes;
    }
    std::vector<vk::DescriptorPoolSize> DescriptorSetLayout::combinePoolSizes(std::vector<std::vector<vk::DescriptorPoolSize>>& poolSizesVector) {
        std::unordered_map<vk::DescriptorType, uint32_t> poolSizeMap;

        for (const auto& poolSizes : poolSizesVector) {
            for (const auto &poolSize: poolSizes) {
                poolSizeMap[poolSize.type] += poolSize.descriptorCount;
            }
        }

        // Convert map back to vector
        std::vector<vk::DescriptorPoolSize> poolSizes;
        for (const auto& [type, count] : poolSizeMap) {
            poolSizes.emplace_back(type, count);
        }
        return poolSizes;
    }


}
