module;

#include <glm/glm.hpp>
#include <functional>

export module scene.geometry.vertex;


namespace scene::geometry {

    export struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 uv;

        // Equality operator
        bool operator==(const Vertex& other) const {
            return position == other.position &&
                   normal == other.normal &&
                   uv == other.uv;
        }
    };

}

// include hashing to Vertex can be in an unordered_map
namespace std {
    template <>
    struct hash<scene::geometry::Vertex> {
        size_t operator()(const scene::geometry::Vertex& vertex) const noexcept {
            size_t h1 = std::hash<float>{}(vertex.position.x);
            size_t h2 = std::hash<float>{}(vertex.position.y);
            size_t h3 = std::hash<float>{}(vertex.position.z);

            size_t h4 = std::hash<float>{}(vertex.normal.x);
            size_t h5 = std::hash<float>{}(vertex.normal.y);
            size_t h6 = std::hash<float>{}(vertex.normal.z);

            size_t h7 = std::hash<float>{}(vertex.uv.x);
            size_t h8 = std::hash<float>{}(vertex.uv.y);

            size_t seed = h1;
            seed ^= h2 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= h3 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= h4 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= h5 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= h6 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= h7 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= h8 + 0x9e3779b9 + (seed << 6) + (seed >> 2);

            return seed;
        }
    };
}