module;

#include <array>

import scene.geometry.vertex;
import vulkan.memory.buffer;
import vulkan.context.device;
import vulkan.types;

export module scene.geometry.triangle;

namespace scene::geometry {

export class Triangle {
public:
    Triangle(const vulkan::context::Device& device, const Vertex& v0, const Vertex& v1, const Vertex& v2);

    [[nodiscard]] const Vertex& v0() const noexcept { return vertices_[0]; }
    [[nodiscard]] const Vertex& v1() const noexcept { return vertices_[1]; }
    [[nodiscard]] const Vertex& v2() const noexcept { return vertices_[2]; }

    [[nodiscard]] vk::Buffer getBuffer() { return buffer_.get(); }
    [[nodiscard]] size_t getTriSize() { return sizeof(Vertex) * vertices_.size(); }

private:
    std::array<Vertex, 3> vertices_;

    vulkan::memory::Buffer         buffer_;
    const vulkan::context::Device& device_;


};

Triangle::Triangle(const vulkan::context::Device& device, const Vertex& v0, const Vertex& v1, const Vertex& v2)
            : vertices_{v0, v1, v2}, device_(device),
              buffer_(
                      device,
                      sizeof(Vertex) * vertices_.size(),
                      vk::BufferUsageFlagBits::eStorageBuffer |
                      vk::BufferUsageFlagBits::eVertexBuffer |
                      vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
                      vk::MemoryPropertyFlagBits::eHostVisible
                      | vk::MemoryPropertyFlagBits::eHostCoherent ){

    buffer_.fill(vertices_.data(), sizeof(Vertex) * vertices_.size(), 0);
}

} // namespace scene
