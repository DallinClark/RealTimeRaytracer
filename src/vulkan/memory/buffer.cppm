module;

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

#include <cstring>
#include <stdexcept>

import vulkan.context.device;

export module vulkan.memory.buffer;

namespace vulkan::memory {

export class Buffer {
public:

    // — Disable copy semantics —
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    // — Enable move semantics —
    Buffer(Buffer&&) noexcept = default;
    Buffer& operator=(Buffer&&) noexcept = default;

    /// Create a buffer of `size` bytes with given usage and memory properties.
    Buffer(const context::Device& device,
           vk::DeviceSize size,
           vk::BufferUsageFlags usage,
           vk::MemoryPropertyFlags properties);
    ~Buffer() = default;

    /// Raw vk::Buffer handle
    [[nodiscard]] vk::Buffer get() const noexcept { return buffer_.get(); }

    /// Raw vk::DeviceMemory handle
    [[nodiscard]] vk::DeviceMemory memory() const noexcept { return memory_.get(); }

    /// Return the Vulkan “device address” for this buffer,
    [[nodiscard]] vk::DeviceAddress address() const  noexcept;

    /// Map the entire buffer memory for host access
    [[nodiscard]] void* map();

    /// Unmap after writing
    void unmap();

    /// Copy `size` bytes from `src` into the buffer at `offset`
    void fill(const void* src, vk::DeviceSize size, vk::DeviceSize offset = 0);

private:
    vk::Device             device_;
    vk::PhysicalDevice     physical_;
    vk::UniqueBuffer       buffer_;
    vk::UniqueDeviceMemory memory_;

    static uint32_t findMemoryType(vk::PhysicalDevice physical,
                                   uint32_t typeFilter,
                                   vk::MemoryPropertyFlags properties);
};

inline Buffer::Buffer(const context::Device& device,
                      const vk::DeviceSize size,
                      const vk::BufferUsageFlags usage,
                      const vk::MemoryPropertyFlags properties)
    : device_{device.get()},
      physical_{device.physical()}
{
    // Create buffer handle
    const vk::BufferCreateInfo bufferInfo{
        {},            // flags
        size,
        usage,
        vk::SharingMode::eExclusive
    };
    buffer_ = device_.createBufferUnique(bufferInfo);

    // Allocate memory
    const auto memoryRequirements = device_.getBufferMemoryRequirements(buffer_.get());
    const vk::MemoryAllocateInfo allocInfo{
        memoryRequirements.size,
        findMemoryType(physical_, memoryRequirements.memoryTypeBits, properties)
    };
    memory_ = device_.allocateMemoryUnique(allocInfo);

    // Bind memory to buffer
    device_.bindBufferMemory(buffer_.get(), memory_.get(), 0);
}

inline uint32_t Buffer::findMemoryType(const vk::PhysicalDevice physical,
                                       const uint32_t typeFilter,
                                       const vk::MemoryPropertyFlags properties)
{
    const auto memProps = physical.getMemoryProperties();
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) &&
            (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find suitable memory type");
}

inline void* Buffer::map() {
    return device_.mapMemory(memory_.get(), 0, VK_WHOLE_SIZE);
}

inline void Buffer::unmap() {
    device_.unmapMemory(memory_.get());
}

inline void Buffer::fill(const void* src, const vk::DeviceSize size, const vk::DeviceSize offset) {
    auto* dest = static_cast<char*>(map()) + static_cast<size_t>(offset);
    std::memcpy(dest, src, size);
    unmap();
}

inline vk::DeviceAddress Buffer::address() const noexcept {
    // Wrap the buffer handle in the DeviceAddressInfo struct…
    vk::BufferDeviceAddressInfo addressInfo{};
    addressInfo.buffer = buffer_.get();
    // …and query the device for its GPU address.
    return device_.getBufferAddress(addressInfo);
}

} // namespace vulkan::memory
