module;

#include <utility>
#include <glm/vec3.hpp>

export module Sphere;

export struct Sphere {
    glm::vec3 center {0.0, 0.0, 0.0};
    double    radius {1.0};
    int       materialID {0};

    /// Build the axis-aligned bounding box [min, max] for the GPU BLAS.
    [[nodiscard]] constexpr
    std::pair<glm::vec3, glm::vec3> getAABB() const noexcept {
        const glm::vec3 offset {radius, radius, radius};
        return { center - offset ,
                 center + offset  };
    }
};