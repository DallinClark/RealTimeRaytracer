module;

#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

export module scene.area_light;

namespace scene {
    export struct AreaLight {
        float intensity;
        glm::vec3 color;
        glm::vec3 points[4];  // stored clockwise
        bool isTwoSided = true;
        vk::TransformMatrixKHR transform = std::array<std::array<float, 4>, 3>{{
                                                                                       {1.0f, 0.0f, 0.0f, 0.0f},  // No Transform
                                                                                       {0.0f, 1.0f, 0.0f, 0.0f},
                                                                                       {0.0f, 0.0f, 1.0f, 0.0f}
                                                                               }};
    };
}
