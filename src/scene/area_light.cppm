module;

#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <vulkan/vulkan.hpp>

export module scene.area_light;

import core.utils;

namespace scene {
    // Holds info for an area light
    // Light starts at the origin facing down the z axis, with side lengths of 1

    export class AreaLight {
    public:
        AreaLight(float intensity, glm::vec3 color, bool isTwoSided = true)
                : intensity_(intensity), color_(color), isTwoSided_(isTwoSided) {};

        struct alignas(16) GPUAreaLightInfo {
            glm::vec3 color;
            float intensity;

            uint32_t vertexOffset; // No index offset needed becuase all area lights have 4 points
            uint32_t isTwoSided;
            glm::vec2 pad = glm::vec2(0.0);

            glm::mat4 transform;
        };


        void move(const glm::vec3& move);
        void scale(glm::vec3 scale);
        void rotate(const glm::vec3& degress);

        const std::vector<glm::vec3>& getPoints() { return points_; };

        void setVertexOffset(uint32_t newOffset) { vertexOffset_ = newOffset; };

        vk::TransformMatrixKHR getTransform() { return transform_; };

        GPUAreaLightInfo getGPUInfo();

    private:
        float intensity_;
        glm::vec3 color_;
        bool isTwoSided_;

        uint32_t vertexOffset_ = 0;

        vk::TransformMatrixKHR transform_ = std::array<std::array<float, 4>, 3>{{
               {1.0f, 0.0f, 0.0f, 0.0f},  // No Transform
               {0.0f, 1.0f, 0.0f, 0.0f},
               {0.0f, 0.0f, 1.0f, 0.0f}
        }};
        const std::vector<glm::vec3> points_ = {glm::vec3{-0.5, -0.5, 0.0},
                                               glm::vec3{-0.5, 0.5, 0.0},
                                               glm::vec3{0.5, 0.5, 0.0},
                                               glm::vec3{0.5, -0.5, 0.0}}; // DO NOT TOUCH, JUST CHANGE TRANSFORM
    };

    AreaLight::GPUAreaLightInfo AreaLight::getGPUInfo() {

        return GPUAreaLightInfo {
            color_,
            intensity_,
            vertexOffset_,
            isTwoSided_,
            glm::vec2(0.0),
            core::utils::PackTransformMatrix(transform_)
        };
    }

    void AreaLight::move(const glm::vec3& move) {
        transform_.matrix[0][3] += move.x;
        transform_.matrix[1][3] += move.y;
        transform_.matrix[2][3] += move.z;
    }

    void AreaLight::scale(glm::vec3 scale) {
        transform_.matrix[0][0] *= scale.x;
        transform_.matrix[1][1] *= scale.y;
        transform_.matrix[2][2] *= scale.z;
    }

    void AreaLight::rotate(const glm::vec3& degress) {
        glm::vec3 radians = glm::radians(degress);  // Convert to radians

        // Individual axis rotations
        glm::mat3 rotX = glm::mat3(glm::rotate(glm::mat4(1.0f), radians.x, glm::vec3(1, 0, 0)));
        glm::mat3 rotY = glm::mat3(glm::rotate(glm::mat4(1.0f), radians.y, glm::vec3(0, 1, 0)));
        glm::mat3 rotZ = glm::mat3(glm::rotate(glm::mat4(1.0f), radians.z, glm::vec3(0, 0, 1)));

        glm::mat3 rotation = rotZ * rotY * rotX;

        // Apply rotation to the top-left 3x3 portion of the transform matrix
        float result[3][3];
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 3; ++col) {
                result[row][col] =
                        rotation[row][0] * transform_.matrix[0][col] +
                        rotation[row][1] * transform_.matrix[1][col] +
                        rotation[row][2] * transform_.matrix[2][col];
            }
        }

        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 3; ++col)
                transform_.matrix[row][col] = result[row][col];
    }
}


