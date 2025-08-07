module;

#include <vulkan/vulkan.hpp>

export module vulkan.context.command_pool;

import core.log;
import vulkan.context.instance;

namespace vulkan::context {

    export class CommandPool {
    public:
        CommandPool(const vk::Device& device, uint32_t queueIndex);

        [[nodiscard]] vk::CommandPool get() const noexcept {
            return commandPool_.get();
        }

        // For single-use (transient) command buffers
        vk::UniqueCommandBuffer getSingleUseBuffer();

        // For reusable command buffers: allocate upfront, user must manage begin/end
        std::vector<vk::UniqueCommandBuffer> allocateCommandBuffers(int amount);

        // Submit single-use command buffer with fence and wait on it properly
        void submitSingleUse(vk::UniqueCommandBuffer&& buffer, vk::Queue queue) const;

    private:
        vk::UniqueCommandPool   commandPool_;
        const vk::Device        device_;
        const uint32_t          queueIndex_;
    };

    // Constructor implementation
    CommandPool::CommandPool(const vk::Device& device, uint32_t queueIndex)
            : device_(device), queueIndex_(queueIndex) {

        vk::CommandPoolCreateInfo poolCreateInfo{
                vk::CommandPoolCreateFlagBits::eResetCommandBuffer, // allows resetting buffers individually
                queueIndex_
        };
        commandPool_ = device_.createCommandPoolUnique(poolCreateInfo);
        core::log::info("Command pool created");
    }

    vk::UniqueCommandBuffer CommandPool::getSingleUseBuffer() {
        vk::CommandBufferAllocateInfo allocInfo{
                commandPool_.get(),
                vk::CommandBufferLevel::ePrimary,
                1
        };
        auto buffers = device_.allocateCommandBuffersUnique(allocInfo);

        vk::CommandBufferBeginInfo beginInfo{
                vk::CommandBufferUsageFlagBits::eOneTimeSubmit
        };
        buffers[0]->begin(beginInfo);

        return std::move(buffers[0]);
    }

    // Allocate reusable command buffer (user must call begin/end manually)
    std::vector<vk::UniqueCommandBuffer> CommandPool::allocateCommandBuffers(int amount) {
        vk::CommandBufferAllocateInfo allocInfo{
                commandPool_.get(),
                vk::CommandBufferLevel::ePrimary,
                static_cast<uint32_t>(amount)
        };
        return device_.allocateCommandBuffersUnique(allocInfo);
    }

    void CommandPool::submitSingleUse(vk::UniqueCommandBuffer&& buffer, vk::Queue queue) const {
        buffer->end();

        // Create a fence to synchronize CPU-GPU
        vk::UniqueFence fence = device_.createFenceUnique({});

        vk::SubmitInfo submitInfo{
                0, nullptr,
                nullptr,
                1, &buffer.get()
        };

        queue.submit(submitInfo, fence.get());

        // Wait on the fence (CPU blocks until GPU work completes)
        if (device_.waitForFences(fence.get(), VK_TRUE, UINT64_MAX) != vk::Result::eSuccess) {
            throw std::runtime_error("Failed to wait for fence.");
        }
    }
}
