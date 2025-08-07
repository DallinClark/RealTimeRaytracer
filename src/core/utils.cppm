module;

#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

export module core.utils;

export namespace core::utils {

    // Converts a vk::TransformMatrixKHR (3x4) to a glm::mat4 (column-major)
    glm::mat4 PackTransformMatrix(const vk::TransformMatrixKHR& vkMatrix) {
        glm::mat4 matrix;

        // Column 0
        matrix[0][0] = vkMatrix.matrix[0][0];
        matrix[0][1] = vkMatrix.matrix[1][0];
        matrix[0][2] = vkMatrix.matrix[2][0];
        matrix[0][3] = 0.0f;

        // Column 1
        matrix[1][0] = vkMatrix.matrix[0][1];
        matrix[1][1] = vkMatrix.matrix[1][1];
        matrix[1][2] = vkMatrix.matrix[2][1];
        matrix[1][3] = 0.0f;

        // Column 2
        matrix[2][0] = vkMatrix.matrix[0][2];
        matrix[2][1] = vkMatrix.matrix[1][2];
        matrix[2][2] = vkMatrix.matrix[2][2];
        matrix[2][3] = 0.0f;

        // Column 3 (translation)
        matrix[3][0] = vkMatrix.matrix[0][3];
        matrix[3][1] = vkMatrix.matrix[1][3];
        matrix[3][2] = vkMatrix.matrix[2][3];
        matrix[3][3] = 1.0f;

        return matrix;
    }

    inline vk::TransformMatrixKHR PackTransformMatrix(const glm::mat4& matrix) {
        vk::TransformMatrixKHR result{};

        for (int i = 0; i < 3; ++i) {
            result.matrix[i][0] = matrix[i][0];  // X
            result.matrix[i][1] = matrix[i][1];  // Y
            result.matrix[i][2] = matrix[i][2];  // Z
            result.matrix[i][3] = matrix[i][3];  // W (translation row)
        }

        return result;
    }
}