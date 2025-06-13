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

struct CombinedPayload {
    vec3 color;
    bool isShadowed;
    int depth;
};

float random(int seed) {
    uint state = uint(seed) * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    uint hash = (word >> 22u) ^ word;
    return float(hash) / 4294967296.0;
}

#endif