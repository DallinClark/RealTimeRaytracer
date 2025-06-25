module;

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.hpp>
#include <iostream>

#include <memory>

import vulkan.memory.buffer;
import vulkan.context.device;

export module scene.camera;

namespace scene {

    /* Holds camera info and data for the GPU */

    export class Camera {
    public:
        struct alignas(16) GPUCameraData {
            glm::vec3 position;                float _pad0 = 0.0f; // 16 bytes
            glm::vec3 topLeftViewportCorner;  float _pad1 = 0.0f; // 16 bytes
            glm::vec3 horizontalViewportDelta; float _pad2 = 0.0f; // 16 bytes
            glm::vec3 verticalViewportDelta;  float _pad3 = 0.0f; // 16 bytes
        };

        Camera(const vulkan::context::Device& device, float fovY, glm::vec3 position, glm::vec3 lookAt,
               glm::vec3 upVector, int pixelWidth, int pixelHeight);

        void processMouseMovement(float xoffset, float yoffset);
        void updateGPUData();


        [[nodiscard]] GPUCameraData getGPUData();
        [[nodiscard]] vk::Buffer    getBuffer()   { return buffer_.get(); }
        [[nodiscard]] glm::vec3     getPosition() { return position_; }
        [[nodiscard]] glm::vec3     getLookAt()   { return lookAtPoint_; }
        [[nodiscard]] glm::vec3     getForward()  { return forward_; }
        [[nodiscard]] glm::vec3     getRight()    { return right_; }

        void setPosition(glm::vec3 newPosition) {
            position_ = newPosition;
            GPUDataNeedsUpdate_ = true;
        }
        void setLookAt(glm::vec3 newLookAt) {
            lookAtPoint_ = newLookAt;
            GPUDataNeedsUpdate_ = true;
        }

    private:
        bool          GPUDataNeedsUpdate_;
        GPUCameraData GPUData_{};

        glm::vec3 position_;
        glm::vec3 lookAtPoint_;
        glm::vec3 upVector_;
        float     fovY_;
        int       pixelWidth_;
        int       pixelHeight_;

        glm::vec3 forward_{glm::vec3()};
        glm::vec3   right_{glm::vec3()};

        float yaw_ = -90.0f;   // Initialize to look down -Z
        float pitch_ = 0.0f;

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
                      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent),
              GPUDataNeedsUpdate_(true)
    {
        // Calculate initial yaw and pitch based on lookAt direction relative to position
        glm::vec3 dir = glm::normalize(lookAtPoint_ - position_);
        pitch_ = glm::degrees(asin(dir.y));
        yaw_ = glm::degrees(atan2(dir.z, dir.x));

        updateGPUData();
    }

    Camera::GPUCameraData Camera::getGPUData() { // THIS PROBABLY SHOULDN'T EXIST
        if (GPUDataNeedsUpdate_) {
            updateGPUData();
        }
        return GPUData_;
    }

    void Camera::updateGPUData() {
        if (GPUDataNeedsUpdate_) {
            float aspect = float(pixelWidth_) / float(pixelHeight_);
            float theta = glm::radians(fovY_);
            float halfHeight = std::tan(theta * 0.5f);
            float halfWidth = aspect * halfHeight;

            // Convert yaw and pitch from degrees to radians
            float yawRadians = glm::radians(yaw_);
            float pitchRadians = glm::radians(pitch_);

            // Calculate forward direction vector (right-handed coords, looking down -Z)
            glm::vec3 direction;
            direction.x = cos(pitchRadians) * cos(yawRadians);
            direction.y = sin(pitchRadians);
            direction.z = cos(pitchRadians) * sin(yawRadians);
            direction = glm::normalize(direction);

            // Update lookAt point relative to position
            lookAtPoint_ = position_ + direction;

            glm::vec3 w = glm::normalize(position_ - lookAtPoint_);  // camera forward vector (pointing backward)
            glm::vec3 u = glm::normalize(glm::cross(upVector_, w));  // right vector
            glm::vec3 v = glm::cross(w, u);                          // camera's true up

            forward_ = -w;
            right_   =  u;

            GPUData_.position = position_;
            GPUData_.horizontalViewportDelta = (2.0f * halfWidth * u) / float(pixelWidth_);
            GPUData_.verticalViewportDelta = -(2.0f * halfHeight * v) / float(pixelHeight_);
            GPUData_.topLeftViewportCorner = position_ - (halfWidth * u) + (halfHeight * v) - w;

            buffer_.fill(&GPUData_, sizeof(GPUCameraData), 0);
            GPUDataNeedsUpdate_ = false;
        }
    }

    void Camera::processMouseMovement(float xoffset, float yoffset) {
        constexpr float sensitivity = 0.1f;
        yaw_ += xoffset * sensitivity;
        pitch_ += yoffset * sensitivity;

        // Clamp pitch to avoid gimbal lock / flipping
        if (pitch_ > 89.0f)
            pitch_ = 89.0f;
        if (pitch_ < -89.0f)
            pitch_ = -89.0f;

        GPUDataNeedsUpdate_ = true;
    }

}
