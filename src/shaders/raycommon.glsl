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
    uint numLights;             uint _pad4[3];
};

struct CombinedPayload {
    vec3 color;
    bool isShadowed;
    int depth;
};

float random(uint seed) {
    uint state = uint(seed) * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    uint hash = (word >> 22u) ^ word;
    return float(hash) / 4294967296.0;
}

struct ObjectInfo {
    uint vertexIndexOffset;   // 4
    uint indexIndexOffset;    // 4
    uint textureIndex;        // 4
    uint _padding0;           // 4 -> align next to 16

    float intensity;          // 4
    float _padding1;          // 4
    float _padding2;          // 4
    float _padding3;          // 4 -> align next vec3

    vec3 emmisiveColor;       // 12
    float _padding4;          // 4 -> pad to make struct size 48 bytes (aligned to 16)
};

struct RayPayload {
    vec3 primaryColor;
    vec3 reflectionColor;
    int depth;
};


struct ShadowRayPayload {
    bool isShadowed;
};

#endif