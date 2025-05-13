module;

#include <format>
#include <glm/vec3.hpp>

export module scene.sphere;

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

//-----------------------------------------------------------------------------
//  Formatter for std::format / std::println
//-----------------------------------------------------------------------------
template<>
struct std::formatter<Sphere, char> {
    // parse is a no-op (we don't support custom format specs here)
    static constexpr auto parse(const format_parse_context& ctx) {
        return ctx.begin();
    }

    // format the Sphere into the output iterator
    static auto format(Sphere const& s, format_context& ctx) {
        return format_to(
            ctx.out(),
            "Sphere(center=({:.2f}, {:.2f}, {:.2f}), radius={:.2f}, matID={})",
            s.center.x, s.center.y, s.center.z,
            s.radius,
            s.materialID
        );
    }
};
