module;

#include <vector>
#include <set>
#include <algorithm>

import core.log;
import vulkan.context.instance;
import vulkan.types;

export module vulkan.context.command_pool;

namespace vulkan::context {

    export class CommandPool {
    public:
        CommandPool(const vk::Device& device, uint32_t queueIndex);

        [[nodiscard]] vk::CommandPool get() const noexcept {
            return commandPool_.get();
        }

        vk::UniqueCommandBuffer getSingleUseBuffer();
        void submitSingleUse(vk::UniqueCommandBuffer&& buffer, vk::Queue queue) const;

    private:
        vk::UniqueCommandPool   commandPool_;
        const vk::Device        device_;
        const uint32_t          queueIndex_;
    };

    // Constructor implementation
    CommandPool::CommandPool(const vk::Device& device, uint32_t queueIndex)
            : device_(device), queueIndex_(queueIndex) {

        // Create the command pool
        vk::CommandPoolCreateInfo poolCreateInfo{
                vk::CommandPoolCreateFlagBits::eResetCommandBuffer, // Resettable buffers
                queueIndex
        };
        commandPool_ = device_.createCommandPoolUnique(poolCreateInfo);
        core::log::info("Command pool created");
    }

    vk::UniqueCommandBuffer CommandPool::getSingleUseBuffer() {
        // Allocate a primary command buffer
        vk::CommandBufferAllocateInfo bufferAllocInfo{
                commandPool_.get(), // pool
                vk::CommandBufferLevel::ePrimary, // primary level
                1 // count
        };
        auto buffers = device_.allocateCommandBuffersUnique(bufferAllocInfo);

        vk::CommandBufferBeginInfo beginInfo{
                vk::CommandBufferUsageFlagBits::eOneTimeSubmit
        };
        buffers[0]->begin(beginInfo);

        return std::move(buffers[0]);
    }

    void CommandPool::submitSingleUse(vk::UniqueCommandBuffer&& buffer, vk::Queue queue) const {
        buffer->end();

        vk::SubmitInfo submitInfo;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &buffer.get();

        queue.submit(submitInfo, vk::Fence{});
        queue.waitIdle(); // TODO use the fence
    }


}
