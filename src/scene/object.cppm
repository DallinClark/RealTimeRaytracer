module;

#include <utility>
#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include <functional>
#include <string>
#include <glm/ext/matrix_transform.hpp>

export module scene.object;


namespace scene {
    // holds info for object, including material info and obj filepath
    // Usage: Create an object class, then set the specular, metallic and color if applicable
    // Use the transform functions to adjust the transform matrix of the object

    export class Object {
    public:
        // Aligned to std140 for vulkan
        struct alignas(16) GPUObjectInfo {
            uint32_t vertexOffset; // at which index does this object start in the vertex buffer
            uint32_t indexOffset;  // at which index does this object start in the index buffer
            glm::vec2 pad0_ = glm::vec2(0.0);

            // 0 IS FALSE, 1 IS TRUE
            uint32_t usesColorMap;    // does it use colorMap, or vector
            uint32_t usesSpecularMap;
            uint32_t usesMetallicMap;
            uint32_t usesOpacityMap;

            uint32_t colorIndex = 0;
            uint32_t specularIndex = 0;
            uint32_t metallicIndex = 0;
            uint32_t opacityIndex = 0;


            glm::vec3 color = glm::vec3(0.0);
            float pad1_ = 0.0;

            float specular = 0.0;
            float metallic = 0.0;
            glm::vec2 pad3_ = glm::vec2(0.0);
        };

        Object(const std::string& objPath) : objPath_(objPath) {};

        void setColor(const std::string& colorPath) { usesColorMap_ = 1; colorMap_ = colorPath;};
        void setColor(const glm::vec3 colorVec)     { usesColorMap_ = 0; colorVec_ = colorVec;};

        void setSpecular(const std::string& specularPath) { usesSpecularMap_ = 1; specularMap_ = specularPath;};
        void setSpecular(const float specularValue)       { usesSpecularMap_ = 0; specularFloat_ = specularValue;};

        void setMetallic(const std::string& metallicPath) { usesMetallicMap_ = 1; metallicMap_ = metallicPath;};
        void setMetallic(const float metallicValue)       { usesMetallicMap_ = 0; metallicFloat_ = metallicValue;};

        void setOpacity(const std::string& opacityPath) { usesOpacityMap_ = 1; opacityMap_ = opacityPath;};

        void setColorMapIndex(uint32_t index)    { colorMapIndex_    = index; };
        void setSpecularMapIndex(uint32_t index) { specularMapIndex_ = index; };
        void setMetallicMapIndex(uint32_t index) { metallicMapIndex_ = index; };
        void setOpacityMapIndex(uint32_t index)  { opacityMapIndex_  = index; };

        void setVertexOffset(uint32_t offset) { vertexOffset_ = offset; };
        void  setIndexOffset(uint32_t offset) { indexOffset_  = offset; };
        void setNumTriangles(uint32_t offset) { numTriangles_ = offset; };



        // move the object in x, y, and z direction
        void move(const glm::vec3& move);
        void scale(float scale);
        void rotate(const glm::vec3& degress);

        const bool usesColorMap() { return usesColorMap_; };
        const bool usesSpecularMap() { return usesSpecularMap_; };
        const bool usesMetallicMap() { return usesMetallicMap_; };
        const bool usesOpacityMap() { return usesOpacityMap_; }

        std::string getOBJPath() { return objPath_; };
        std::string getColorPath() { return colorMap_; };
        std::string getSpecularPath() { return specularMap_; };
        std::string getMetallicPath() { return metallicMap_; };
        std::string getOpacityPath()  { return opacityMap_; }

        void setBLASIndex(uint32_t index) { BLASIndex_ = index; };
        uint32_t getBLASIndex() { return BLASIndex_; };

        void setInstanceIndex(uint32_t index) { instanceIndex_ = index; };
        uint32_t getInstanceIndex() { return instanceIndex_; };

        vk::TransformMatrixKHR getTransform() { return transform_; };
        GPUObjectInfo getGPUInfo();

        // Needed for fucntion in TLAS, need to update to make this and light class to be parented to fix
        std::vector<glm::vec3> getPoints() { return {}; }

    private:
        const std::string objPath_;
        vk::TransformMatrixKHR transform_{
                std::array<std::array<float, 4>, 3>{{
                    {1.0f, 0.0f, 0.0f, 0.0f},  // no transform
                    {0.0f, 1.0f, 0.0f, 0.0f},
                    {0.0f, 0.0f, 1.0f, 0.0f}
                }}
        };

        bool usesSpecularMap_ = false;
        bool usesMetallicMap_ = false;
        bool usesColorMap_    = false;
        bool usesOpacityMap_  = false;

        float specularFloat_ = 1.0;
        float metallicFloat_ = 0.0;
        glm::vec3 colorVec_  = glm::vec3(0.5);

        std::string specularMap_;
        std::string metallicMap_;
        std::string colorMap_;
        std::string opacityMap_;

        // optional indexes to store if used
        uint32_t colorMapIndex_    = 0;
        uint32_t specularMapIndex_    = 0;
        uint32_t metallicMapIndex_ = 0;
        uint32_t opacityMapIndex_ = 0;

        uint32_t BLASIndex_ = 0; // used in the TLAS creation to see which BLAS this object corresponds to
        uint32_t vertexOffset_ = 0; // offset in the vertex buffer
        uint32_t indexOffset_ = 0; //offset in the index buffer
        uint32_t instanceIndex_ = 0; // BLAS instance index in the TLAS

        uint32_t numTriangles_ = 0;
    };

    Object::GPUObjectInfo Object::getGPUInfo() {
        return Object::GPUObjectInfo{
            vertexOffset_,
            indexOffset_,
            glm::vec2(0.0f),
            usesColorMap_,
            usesSpecularMap_,
            usesMetallicMap_,
            usesOpacityMap_,
            colorMapIndex_,
            specularMapIndex_,
            metallicMapIndex_,
            opacityMapIndex_,
            colorVec_,
            0.0,
            specularFloat_,
            metallicFloat_,
            glm::vec2(0.0f)
        };
    }


    void Object::move(const glm::vec3& move) {
        transform_.matrix[0][3] += move.x;
        transform_.matrix[1][3] += move.y;
        transform_.matrix[2][3] += move.z;
    }
    void Object::scale(float scale) {
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 3; ++col) {
                transform_.matrix[row][col] *= scale;
            }
        }
    }

    void Object::rotate(const glm::vec3& degress) {
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

