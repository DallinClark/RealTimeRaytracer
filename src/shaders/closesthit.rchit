#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_GOOGLE_include_directive : enable
#include "raycommon.glsl"

layout(location = 0) rayPayloadInEXT CombinedPayload inPayload;
layout(location = 1) rayPayloadEXT CombinedPayload reflectionPayload;

hitAttributeEXT vec2 attribs;

layout(set = 0, binding = 1) uniform accelerationStructureEXT topLevelAS;
layout(set = 0, binding = 2) uniform CameraUBO {
    GPUCameraData cam;
};
layout(set = 0, binding = 3) readonly buffer VertexBuffer {
    Vertex vertices[];
};
layout(set = 0, binding = 4) readonly buffer IndexBuffer {
    uint indices[];
};
layout(set = 0, binding = 5) uniform sampler2D texSampler;

const uint MAX_DEPTH = 2;

// Material Properties. TODO link with maps instead of hardcoded values
float roughness = .8;
float metallic = 0.0;

// Sample hemisphere around normal for rough reflections
vec3 sampleHemisphere(vec3 normal, float roughness, int baseSeed) {
    // Higher roughness = wider cone
    float maxAngle = roughness * 1.57079632679;

    float cosTheta = mix(1.0, cos(maxAngle), random(baseSeed));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    float phi = 2.0 * 3.14159265359 * random(baseSeed + 1);

    vec3 w = normal;
    vec3 u = normalize(cross(abs(w.x) > 0.1 ? vec3(0, 1, 0) : vec3(1, 0, 0), w));
    vec3 v = cross(w, u);

    return normalize(sinTheta * cos(phi) * u + sinTheta * sin(phi) * v + cosTheta * w);
}

void main() {
    // initialize seed
    int seed = int(gl_LaunchIDEXT.x) + int(gl_LaunchIDEXT.y) * 1920 + int(inPayload.depth) * 3686400;

    // Get triangle indices
    uint index0 = indices[3 * gl_PrimitiveID + 0];
    uint index1 = indices[3 * gl_PrimitiveID + 1];
    uint index2 = indices[3 * gl_PrimitiveID + 2];

    // Get vertices
    Vertex v0 = vertices[index0];
    Vertex v1 = vertices[index1];
    Vertex v2 = vertices[index2];

    // Compute barycentrics
    const vec3 bary = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

    // Interpolate position and normals
    vec3 localPos = v0.position * bary.x + v1.position * bary.y + v2.position * bary.z;
    vec3 worldPos = vec3(gl_ObjectToWorldEXT * vec4(localPos, 1.0));
    vec3 normalLocal = normalize(v0.normal * bary.x + v1.normal * bary.y + v2.normal * bary.z);
    vec3 normalWorld = normalize(mat3(gl_ObjectToWorldEXT) * normalLocal);
    vec2 uv = v0.uv * bary.x + v1.uv * bary.y + v2.uv * bary.z;

    // ------------------ Reflection Trace ------------------
    vec3 finalReflectedDir = sampleHemisphere(normalWorld, roughness, seed);
    float rayOffset = max(0.001, 0.001 * length(worldPos));
    vec3 reflectedOrigin = worldPos + rayOffset * normalWorld;

    // ----------------------------Hardcoded Light--------------------
    vec3 lightColor = vec3(1.0, 0.95, 0.9);
    float lightIntensity = 1.0;
    vec3 lightDir = normalize(vec3(-1, 3, 0));

    // --------------------------- Object Material Definitions --------------------------
    // Base object color
    vec3 baseColor = texture(texSampler, uv).rgb;

    // Light calculations
    vec3 viewDir = normalize(cam.position - worldPos);
    vec3 reflectLight = reflect(-lightDir, normalWorld);

    // Material calculations
    // Base reflectance
    vec3 F0 = mix(vec3(0.04), baseColor, metallic);
    // Fresnel approximation
    vec3 incomingDir = normalize(gl_WorldRayDirectionEXT);
    float cosTheta = max(dot(-incomingDir, normalWorld), 0.0);
    vec3 F = F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
    // Reflection strength based on fresnel and roughness
    float reflectStrength = F.r * (1.0 - roughness * roughness);

    float shininess = mix(128.0, 1.0, roughness);

    // Phong terms
    vec3 ambientColor = vec3(0.2);
    float diff = max(dot(normalWorld, lightDir), 0.0);
    float spec = pow(max(dot(viewDir, reflectLight), 0.0), shininess);
    float specStrength = mix(.8, .1, roughness);

    vec3 phong = ambientColor * baseColor +
    diff * baseColor * lightColor * lightIntensity +
    spec * specStrength * lightColor * lightIntensity;

    if (inPayload.depth < MAX_DEPTH) {
        reflectionPayload.color = vec3(0.0);
        reflectionPayload.depth = inPayload.depth + 1;
        traceRayEXT(
        topLevelAS, // accelerationStructureEXT
        gl_RayFlagsOpaqueEXT, // rayFlags
        0xFF, // cullMask
        0, // sbtRecordOffset
        1, // sbtRecordStride
        0, // missIndex
        reflectedOrigin, // rayOrigin
        0.001, // rayTMin
        finalReflectedDir, // rayDirection
        10000.0, // rayTMax
        1// rayPayload (inout)
        );

        // ------------------ Combine ------------------
        inPayload.color = mix(phong, reflectionPayload.color, reflectStrength);
    } else {
        inPayload.color = phong;
    }
}