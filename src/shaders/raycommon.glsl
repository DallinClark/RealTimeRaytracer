#ifndef RAYCOMMON_GLSL
#define RAYCOMMON_GLSL

struct Vertex {
    vec3 position; float pad0;
    vec3 normal;   float pad1;
    vec2 uv;       vec2 pad2;
};

struct GPUCameraData {
    vec3 position;               float _pad0;
    vec3 topLeftViewportCorner; float _pad1;
    vec3 horizontalViewportDelta; float _pad2;
    vec3 verticalViewportDelta; float _pad3;
};

//struct SceneInfo {
//    uint frame;
//    uint numAreaLights;
//};

float random(uint seed) {
    uint state = uint(seed) * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    uint hash = (word >> 22u) ^ word;
    return float(hash) / 4294967296.0;
}

struct ObjectInfo {
    uint vertexOffset;
    uint indexOffset;
    vec2 pad0_;

    // 0 is false, 1 is true
    uint usesColorMap;
    uint usesSpecularMap;
    uint usesMetallicMap;
    uint usesOpacityMap;

    uint colorIndex;
    uint specularIndex;
    uint metallicIndex;
    uint opacityIndex;

    vec3 color;
    float pad1_;

    float specular;
    float metallic;
    vec2 pad2_;
};

struct LightInfo {
    vec3 color;
    float intensity;

    uint vertexOffset;
    uint indexOffset;
    uint numTriangles;
    bool isTwoSided;

    mat4 transform;
};


struct HitInfo {
    vec3 hitPoint;
    vec3 normal;
    vec3 color;
    float roughness;
    float metallic;

    bool hitLight;
    bool missed;
    bool isShadowRay;
};


#endif