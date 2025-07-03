#ifndef RAYCOMMON_GLSL
#define RAYCOMMON_GLSL

struct Vertex {
    vec3 position; float pad0;
    vec3 normal;   float pad1;
    vec2 uv;       vec2 pad2;
    vec3 tangent; float pad3;
};

struct GPUCameraData {
    vec3 position;               float _pad0;
    vec3 topLeftViewportCorner; float _pad1;
    vec3 horizontalViewportDelta; float _pad2;
    vec3 verticalViewportDelta; float _pad3;
};

struct CombinedPayload {
    vec3 primaryColor;
    float shadow;
    vec3 reflectionColor;
    int depth;
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

    // 0 is false, 1 is true
    uint usesColorMap;
    uint usesSpecularMap;
    uint usesMetallicMap;

    uint colorIndex;
    uint specularIndex;
    uint metallicIndex;

    vec3 color;
    float pad1_;

    float specular;
    float metallic;
    vec2 pad2_;
};

struct HitInfo {
    uint  renderNodeIndex;
    uint  renderPrimIndex;  // what mesh we hit
    float hitT;             // where we hit the mesh along the ray
    vec3  tangent;
    vec3  normal_envmapRadiance;  // when hitT == NRD_INF we hit the environment map and return its radiance here
    vec2  uv;
    float bitangentSign;
    ObjectInfo objInfo;
};

struct LightInfo {
    vec3 color;
    float intensity;

    uint vertexOffset;
    uint isTwoSided;
    vec2 pad;

    mat4 transform;
};


struct RayPayload {
    vec3 primaryColor;
    vec3 reflectionColor;
    int depth;
};


struct ShadowRayPayload {
    bool isShadowed;
};

struct HitState {
    vec3 pos;
    vec3 nrm;
    vec2 uv;
};

#endif