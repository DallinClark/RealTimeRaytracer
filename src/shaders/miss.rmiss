#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable

#include "raycommon.glsl"

layout(location = 0) rayPayloadInEXT HitInfo payload;
layout(set = 1, binding = 7) uniform sampler2D hdri;
vec3 PowVec3(vec3 v, float p)
{
    return vec3(pow(v.x, p), pow(v.y, p), pow(v.z, p));
}
vec3 ToLinear(vec3 v) { return PowVec3(v, 2.2); }

void main() {
    payload.missed = true;
    if (payload.isShadowRay) {
        return;
    }

    vec3 dir = normalize(gl_WorldRayDirectionEXT);
    float u = atan(dir.z, dir.x) / (2.0 * 3.14159265) + 0.5;
    float v = acos(clamp(dir.y, -1.0, 1.0)) / 3.14159265;
    v = 1.0 - v;
    vec3 hdrColor = texture(hdri, vec2(u, v)).rgb;
    payload.color = ToLinear(hdrColor);
}