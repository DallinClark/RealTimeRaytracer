module;

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.hpp>

#include <memory>

import vulkan.memory.buffer;
import vulkan.context.device;

export module scene.camera;

namespace scene {

/* Holds camera info and data for the GPU */

export class Camera {
public:
    struct GPUCameraData{
        glm::vec3 position;
        glm::vec3 topLeftViewportCorner;
        glm::vec3 horizontalViewportDelta;
        glm::vec3 verticalViewportDelta;
    };

    Camera(const vulkan::context::Device& device, float fovY, glm::vec3 position, glm::vec3 lookAt,
           glm::vec3 upVector, int pixelWidth, int pixelHeight);

    [[nodiscard]] GPUCameraData getGPUData();
    [[nodiscard]] vk::Buffer    getBuffer()   { return buffer_.get(); }
    [[nodiscard]] glm::vec3     getPosition() { return position_; }
    [[nodiscard]] glm::vec3     getLookAt()   { return lookAtPoint_; }

    void setPosition(glm::vec3 newPosition) {
        position_ = newPosition;
        GPUDataNeedsUpdate_ = true;
    }
    void setLookAt(glm::vec3 newLookAt) {
        lookAtPoint_ = newLookAt;
        GPUDataNeedsUpdate_ = true;
    }

private:
    void updateGPUData();

    bool          GPUDataNeedsUpdate_;
    GPUCameraData GPUData_{};

    glm::vec3 position_;
    glm::vec3 lookAtPoint_;
    glm::vec3 upVector_;
    float     fovY_;
    int       pixelWidth_;
    int       pixelHeight_;

    const vulkan::context::Device& device_;
    vulkan::memory::Buffer         buffer_;
};

Camera::Camera(const vulkan::context::Device& device, float fovY, glm::vec3 position, glm::vec3 lookAt, glm::vec3 upVector, int pixelWidth, int pixelHeight)
           : device_(device), fovY_(fovY), position_(position), lookAtPoint_(lookAt),
           pixelWidth_(pixelWidth), pixelHeight_(pixelHeight), upVector_(upVector),
           buffer_(
                   device,
                   sizeof(GPUCameraData),
                   vk::BufferUsageFlagBits::eUniformBuffer,
                   vk::MemoryPropertyFlagBits::eHostVisible
                   | vk::MemoryPropertyFlagBits::eHostCoherent ),
           GPUDataNeedsUpdate_(true) {
    updateGPUData();
}

Camera::GPUCameraData Camera::getGPUData() {
    if (GPUDataNeedsUpdate_) {
        updateGPUData();
    }
    return GPUData_;
}

void Camera::updateGPUData() {
    if (GPUDataNeedsUpdate_) {
        float aspect      = float(pixelWidth_) / float(pixelHeight_);
        float theta       = glm::radians(fovY_);
        float halfHeight  = std::tan(theta * 0.5f);
        float halfWidth   = aspect * halfHeight;

        glm::vec3 w = glm::normalize(position_ - lookAtPoint_);  // forward vector
        glm::vec3 u = glm::normalize(glm::cross(upVector_, w));  // right in world space
        glm::vec3 v = glm::cross(w, u);                          // camera's true up

        GPUData_.position                = position_;
        GPUData_.horizontalViewportDelta = (2.0f * halfWidth * u) / float(pixelWidth_);
        GPUData_.verticalViewportDelta   = -(2.0f * halfHeight * v) / float(pixelHeight_);
        GPUData_.topLeftViewportCorner   = position_ - (halfWidth * u) - (halfHeight * v) - w;

        // Upload straight to buffer
        buffer_.fill(&GPUData_, sizeof(GPUData_), 0);
        GPUDataNeedsUpdate_ = false;
    }
}
}