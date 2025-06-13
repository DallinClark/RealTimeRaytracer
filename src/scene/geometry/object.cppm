module;

#include <vulkan/vulkan.hpp>
#include <functional>

export module scene.geometry.object;


namespace scene::geometry {

    export struct ObjectCreateInfo {
        vk::TransformMatrixKHR transform = std::array<std::array<float, 4>, 3>{{
                                         {1.0f, 0.0f, 0.0f, 0.0f},  // No Transform
                                         {0.0f, 1.0f, 0.0f, 0.0f},
                                         {0.0f, 0.0f, 1.0f, 0.0f}
                                  }};
        uint32_t objectIndex  = 0; // default to the first
        uint32_t textureIndex = 0;
    };

}

