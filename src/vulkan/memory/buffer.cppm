module;

#include <vulkan/vulkan.hpp>

export module vulkan.memory.buffer;

import core.log;
import vulkan.context.device;

namespace vulkan::memory {

/// Simple RAII wrapper for a Vulkan buffer + its device memory.
/// Supports creation, mapping, and filling from host.
/// Designed for clarity and flexibility (staging, uniform, or device-local).

// TODO can't use texel buffers yet becuase of no buffer views
export class Buffer {
public:

    // — No copies, only moves —
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&&) noexcept = default;
    Buffer& operator=(Buffer&&) noexcept = default;

    /// Construct a buffer of given size, usage, and memory properties.
    /// `device` must outlive this Buffer.
    Buffer(
        const context::Device& device,
        vk::DeviceSize size,
        vk::BufferUsageFlags usage,
        vk::MemoryPropertyFlags properties
    );

    ~Buffer() = default;

    /// Raw vk::Buffer handle
    [[nodiscard]] vk::Buffer get() const noexcept { return buffer_.get(); }

    /// Raw vk::DeviceMemory handle
    [[nodiscard]] vk::DeviceMemory memory() const noexcept { return memory_.get(); }

    /// Map (host-visible) memory at [offset, offset+size),
    /// throws if buffer was not created HOST_VISIBLE.
    [[nodiscard]] void* map(
            vk::DeviceSize offset = 0,
            vk::DeviceSize size   = VK_WHOLE_SIZE
    );

    /// Unmap memory when done
    void unmap();

    /// Convenience: map → memcpy → flush (if needed) → unmap
    void fill(
            const void* data,
            vk::DeviceSize size,
            vk::DeviceSize offset = 0
    );

    vk::DeviceAddress address() const noexcept;

private:
    vk::Device              device_;
    vk::PhysicalDevice      physical_;
    vk::UniqueBuffer        buffer_;
    vk::UniqueDeviceMemory  memory_;
    vk::DeviceSize          allocationSize_;
    vk::MemoryPropertyFlags properties_;

    /// Pick a memory type index matching `typeFilter` and `properties`
    static uint32_t findMemoryType(
            uint32_t                   typeFilter,
            vk::MemoryPropertyFlags    properties,
            vk::PhysicalDevice         physical
    );
};

// ——— Implementation ———————————————————————————————————————————————

inline Buffer::Buffer(
        const context::Device&        device,
        const vk::DeviceSize          size,
        const vk::BufferUsageFlags    usage,
        const vk::MemoryPropertyFlags properties
) : device_{device.get()}, physical_{device.physical()}, allocationSize_(size), properties_ {properties} {
    core::log::debug("Buffer: Constructing (size={}, usage={}, properties={})", size, to_string(usage), to_string(properties));

    // Create the buffer handle
    const vk::BufferCreateInfo bufferInfo{
        {}, // flags
        size,
        usage,
        vk::SharingMode::eExclusive
    };
    buffer_ = device_.createBufferUnique(bufferInfo);

    // Allocate memory for it
    const auto req = device_.getBufferMemoryRequirements(buffer_.get());
    allocationSize_ = req.size;

    core::log::debug("Buffer: Memory requirements (size={}, typeBits={:#x})", req.size, req.memoryTypeBits);

    const vk::MemoryAllocateInfo allocInfo{
            req.size, // allocationSize
            findMemoryType(req.memoryTypeBits, properties_, physical_) // memoryTypeIndex
    };
    memory_ = device_.allocateMemoryUnique(allocInfo);
    core::log::debug("Buffer: Allocated memory (size={}, memoryTypeIndex={})", req.size, allocInfo.memoryTypeIndex);

    // Bind memory to the buffer
    device_.bindBufferMemory(buffer_.get(), memory_.get(), /*offset*/ 0);
    core::log::debug("Buffer: Bound memory to buffer");
}

inline uint32_t Buffer::findMemoryType(
        const uint32_t                typeFilter,
        const vk::MemoryPropertyFlags properties,
        const vk::PhysicalDevice      physical
) {
    core::log::trace("Buffer: Searching for memory type (typeFilter={:#x}, properties={})", typeFilter, to_string(properties));

    const auto memProps = physical.getMemoryProperties();
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if (
            const auto& type = memProps.memoryTypes[i];
            typeFilter & 1u << i && (type.propertyFlags & properties) == properties
        ) {
            core::log::trace("Buffer: Selected memory type index {}", i);
            return i;
        }
    }
    core::log::error("Buffer: Failed to find suitable memory type");
    throw std::runtime_error("Buffer: Failed to find suitable memory type");
}

inline void* Buffer::map(
    const vk::DeviceSize offset,
    const vk::DeviceSize size
) {
    core::log::trace("Buffer: map(offset={}, size={})", offset, size);

    if (!(properties_ & vk::MemoryPropertyFlagBits::eHostVisible)) {
        throw std::runtime_error("Buffer: cannot map non-host-visible memory");
    }
    // If size==VK_WHOLE_SIZE, map entire allocation minus offset
    const vk::DeviceSize mapSize = size == VK_WHOLE_SIZE
        ? allocationSize_ - offset
        : size;

    return device_.mapMemory(memory_.get(), offset, mapSize);
}

inline void Buffer::unmap() {
    core::log::trace("Buffer: unmap()");
    device_.unmapMemory(memory_.get());
}

inline void Buffer::fill(
        const void*           data,
        const vk::DeviceSize  size,
        const vk::DeviceSize  offset
) {
    core::log::trace("Buffer: fill(offset={}, size={})", offset, size);

    // Simple convenience: map → memcpy → unmap
    void* dest = map(offset, size);
    std::memcpy(dest, data, size);
    core::log::trace("Buffer: Copied data into mapped memory");

    // Flush if non-coherent
    if (!(properties_ & vk::MemoryPropertyFlagBits::eHostCoherent)) {
        core::log::trace("Buffer: Flushing mapped memory ranges");
        const vk::MappedMemoryRange range{
            memory_.get(),
            offset,
            size
        };

        const auto result = device_.flushMappedMemoryRanges(1, &range);
        if (result != vk::Result::eSuccess ) {
            core::log::error("Buffer: Failed to flush mapped memory ranges (result={})", static_cast<int>(result));
            throw std::runtime_error("Buffer: failed to flush mapped memory ranges");
        }
    }

    unmap();
}

inline vk::DeviceAddress Buffer::address() const noexcept {
    core::log::debug("Buffer: address()");
    const vk::BufferDeviceAddressInfo info { buffer_.get() };
    vk::DeviceAddress address = device_.getBufferAddress(info);
    core::log::debug("Buffer: DeviceAddress = {}", address);
    return address;
}

} // namespace vulkan::memory

