module;

#include <glm/glm.hpp>
#include <functional>

export module scene.geometry.vertex;


namespace scene::geometry {

    export struct Vertex {
        glm::vec3 normal;   float pad1 = 0.0f;
        glm::vec2 uv;       glm::vec2 pad2 = glm::vec2(0.0f, 0.0f);

        // Equality operator
        bool operator==(const Vertex& other) const {
            return normal == other.normal &&
                   uv == other.uv;
        }
    };

}

// include hashing to Vertex can be in an unordered_map
namespace std {
    template <>
    struct hash<scene::geometry::Vertex> {
        size_t operator()(const scene::geometry::Vertex& vertex) const noexcept {

            size_t h4 = std::hash<float>{}(vertex.normal.x);
            size_t h5 = std::hash<float>{}(vertex.normal.y);
            size_t h6 = std::hash<float>{}(vertex.normal.z);

            size_t h7 = std::hash<float>{}(vertex.uv.x);
            size_t h8 = std::hash<float>{}(vertex.uv.y);

            size_t seed = h4;
            seed ^= h5 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= h6 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= h7 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= h8 + 0x9e3779b9 + (seed << 6) + (seed >> 2);

            return seed;
        }
    };
}