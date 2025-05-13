module;

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

export module Camera;

namespace scene {

/* Holds camera info and data for the GPU
  TODO change projection for vulkan style instead of OpenGL */

export class Camera {
public:
    struct GPUCameraData{
        glm::mat4 view;
        glm::mat4 proj;
        glm::mat4 viewproj;
    };

    Camera(glm::vec3 position, glm::vec3 lookAt, float fovY, float viewportWidth, float viewportHeight, float nearClip, float farClip);

    [[nodiscard]] GPUCameraData getGPUData();
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
    bool          GPUDataNeedsUpdate_;
    GPUCameraData GPUData_{};

    glm::vec3     position_;
    glm::vec3     lookAtPoint_;

    float         fovY_;
    float         width_;
    float         height_;
    float         nearClip_;
    float         farClip_;

    void updateGPUData();
};

Camera::Camera(glm::vec3 position, glm::vec3 lookAt, float fovY, float viewportWidth, float viewportHeight, float nearClip, float farClip)
        : position_(position), lookAtPoint_(lookAt), fovY_(fovY), width_(viewportWidth), height_(viewportHeight), nearClip_(nearClip), farClip_(farClip) {

    updateGPUData();
    GPUDataNeedsUpdate_ = false;
}

Camera::GPUCameraData Camera::getGPUData() {
    if (GPUDataNeedsUpdate_) {
        updateGPUData();
    }
    return GPUData_;
}

void Camera::updateGPUData() {
    glm::mat4 view = glm::lookAt(position_, lookAtPoint_, glm::vec3(0.0,1.0,0.0));

    float aspect = width_ / height_;
    glm::mat4 proj = glm::perspective(fovY_, aspect, nearClip_, farClip_);

    glm::mat4 viewproj = proj * view;

    GPUData_ = GPUCameraData(view, proj, viewproj);
    GPUDataNeedsUpdate_ = false;
}
}