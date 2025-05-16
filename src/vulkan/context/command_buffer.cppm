module;

#include <vulkan/vulkan.hpp>

#include <vector>
#include <string_view>
#include <set>
#include <optional>
#include <algorithm>
#include <stdexcept>
#include <cstring>

export module vulkan.context.command_buffer;

import core.log;
import vulkan.context.instance;

namespace vulkan::context {

    export class CommandBuffer {
    public:
        CommandBuffer(const vk::Device& device, uint32_t queueIndex);

        [[nodiscard]] vk::CommandBuffer get() const noexcept {
            return commandBuffer_.get();
        }

    private:
        vk::UniqueCommandPool   commandPool_;
        vk::UniqueCommandBuffer commandBuffer_;
        const vk::Device        device_;
        const uint32_t          queueIndex_;
    };

// Constructor implementation
    CommandBuffer::CommandBuffer(const vk::Device& device, uint32_t queueIndex)
            : device_(device), queueIndex_(queueIndex) {

        // Create the command pool
        vk::CommandPoolCreateInfo poolCreateInfo{
                vk::CommandPoolCreateFlagBits::eResetCommandBuffer, // Resettable buffers
                queueIndex
        };
        commandPool_ = device_.createCommandPoolUnique(poolCreateInfo);

        // Allocate a primary command buffer
        vk::CommandBufferAllocateInfo bufferAllocInfo{
                commandPool_.get(), // pool
                vk::CommandBufferLevel::ePrimary, // primary level
                1 // count
        };
        auto buffers = device_.allocateCommandBuffersUnique(bufferAllocInfo);
        commandBuffer_ = std::move(buffers[0]);
    }
}
