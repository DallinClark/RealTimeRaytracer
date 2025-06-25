module;

#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

export module scene.scene_info;

namespace scene {
    // INFO for a scene, bound once for frame and updated once per frame in a push constant
    export struct alignas(16) SceneInfo {
        uint32_t frame;
        uint32_t numAreaLights;
        uint32_t _pad0;
        uint32_t _pad1 = 0;
        glm::vec3 camPosition;
        float pad2_ = 0.0f;

        SceneInfo(uint32_t frame, uint32_t num, glm::vec3 cam)
                : frame(frame), numAreaLights(num), _pad0(0), _pad1(0), camPosition(cam), pad2_(0.0f) {}
    };

}
