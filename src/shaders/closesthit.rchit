#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require

#include "raycommon.glsl"

hitAttributeEXT vec2 attribs;

layout(location = 0) rayPayloadInEXT HitInfo payload;

layout(set = 0, binding = 6) readonly buffer ObjectInfoBuffer {
    ObjectInfo objectInfos[];
};

layout(set = 0, binding = 3) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout(set = 0, binding = 4) readonly buffer IndexBuffer {
    uint indices[];
};

layout( push_constant ) uniform constants
{
    uint frame;
    uint numLights;
    uint _pad0;
    uint _pad1;
    vec3 camPosition;
    float padding_;
} sceneData;

void main() {
    ObjectInfo objectInfo = objectInfos[gl_InstanceCustomIndexEXT - sceneData.numLights];

    uint vertexOffset = objectInfo.vertexOffset;
    uint indexOffset = objectInfo.indexOffset;

    uint index0 = indices[3 * gl_PrimitiveID + 0 + indexOffset];
    uint index1 = indices[3 * gl_PrimitiveID + 1 + indexOffset];
    uint index2 = indices[3 * gl_PrimitiveID + 2 + indexOffset];

    Vertex v0 = vertices[index0 + vertexOffset];
    Vertex v1 = vertices[index1 + vertexOffset];
    Vertex v2 = vertices[index2 + vertexOffset];

    vec3 v0position = v0.position;
    vec3 v1position = v1.position;
    vec3 v2position = v2.position;

    vec3 bary = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    vec3 localPos = v0position * bary.x + v1position * bary.y + v2position * bary.z;
    vec3 hitPoint = vec3(gl_ObjectToWorldEXT * vec4(localPos, 1.0));
    vec3 interpolatedLocalNormal = normalize(v0.normal * bary.x + v1.normal * bary.y + v2.normal * bary.z);
    mat3 normalMatrix = transpose(inverse(mat3(gl_ObjectToWorldEXT)));
    vec3 hitNormal = normalize(normalMatrix * interpolatedLocalNormal);
    vec2 uv = v0.uv * bary.x + v1.uv * bary.y + v2.uv * bary.z;

    // Fill payload with interpolated values
    payload.renderNodeIndex = gl_PrimitiveID;
    payload.renderPrimIndex = gl_InstanceCustomIndexEXT;
    payload.hitT = gl_HitTEXT;
    payload.tangent = vec3(0);
    payload.normal_envmapRadiance = hitNormal;
    payload.uv = uv;
    payload.bitangentSign = 1.0; // You might want to store this per-vertex if needed
    payload.objInfo = objectInfo;
}