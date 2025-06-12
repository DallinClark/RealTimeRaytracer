#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable

layout(set = 0, binding = 6) uniform sampler2D hdri;

layout(location = 0) rayPayloadInEXT vec3 payload;

void main() {
    // Convert ray direction into a UV for environment map sampling
    vec3 dir = normalize(gl_WorldRayDirectionEXT);
    float u = atan(dir.z, dir.x) / (2.0 * 3.14159265) + 0.5;
    float v = acos(clamp(dir.y, -1.0, 1.0)) / 3.14159265;
    v = 1.0 - v;
    vec3 hdrColor = texture(hdri, vec2(u, v)).rgb;

    payload = hdrColor;
}