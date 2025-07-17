#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require
#include "raycommon.glsl"
#include "cook-torrance.glsl"

hitAttributeEXT vec2 attribs;

layout(location = 0) rayPayloadInEXT HitInfo payload;

layout(set = 1, binding = 2) readonly buffer VertexBuffer {
    Vertex vertices[];
};
layout(set = 1, binding = 3) readonly buffer IndexBuffer {
    uint indices[];
};
layout(set = 1, binding = 4) uniform sampler2D texSamplers[];
layout(set = 1, binding = 5) readonly buffer ObjectInfoBuffer {
    ObjectInfo objectInfos[];
};
layout(set = 1, binding = 6) readonly buffer LightInfoBuffer {
    LightInfo lightInfos[];
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
vec3 PowVec3(vec3 v, float p)
{
    return vec3(pow(v.x, p), pow(v.y, p), pow(v.z, p));
}
vec3 ToLinear(vec3 v) { return PowVec3(v, 2.2); }
float ToLinear(float v) { return pow(v, 2.2); }

void main() {
    if (gl_InstanceCustomIndexEXT < sceneData.numLights) {  // If this is a light
        payload.hitLight =  true;
        payload.color = lightInfos[gl_InstanceCustomIndexEXT].color;
        return;
    }
    payload.hitLight = false;

    uint objIndex = gl_InstanceCustomIndexEXT - sceneData.numLights;
    ObjectInfo objectInfo = objectInfos[objIndex];

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

    if (objectInfo.usesColorMap != 0) {
        payload.color = texture(nonuniformEXT(texSamplers[objectInfo.colorIndex]), uv).rgb;
    } else {
        payload.color = objectInfo.color;
    }

    if (objectInfo.usesSpecularMap != 0) {
        payload.roughness = texture(nonuniformEXT(texSamplers[objectInfo.specularIndex]), uv).r;
    } else {
        payload.roughness = objectInfo.specular;
    }

    if (objectInfo.usesMetallicMap != 0) {
        payload.metallic = texture(nonuniformEXT(texSamplers[objectInfo.metallicIndex]), uv).r;
    } else {
        payload.metallic = objectInfo.metallic;
    }

   payload.color = ToLinear(payload.color);
   //payload.roughness = ToLinear(payload.roughness);
   payload.normal = hitNormal;
   payload.hitPoint = hitPoint;

   return;
}