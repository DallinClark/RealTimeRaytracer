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
layout(set = 0, binding = 6) uniform sampler2D hdri;

const uint MAX_DEPTH = 2;
const float PI = 3.14159265359;
const float EPSILON = 0.001;

// Material Properties - now using PBR parameters
float roughness = 0.8;
float metallic = 0.0;

// PBR Utility Functions
float saturate(float x) {
    return clamp(x, 0.0, 1.0);
}

vec3 saturate(vec3 x) {
    return clamp(x, 0.0, 1.0);
}

// Random function is available from raycommon.glsl

// Fresnel-Schlick approximation - MOVED UP to be available for other functions
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}

// Fresnel with roughness for IBL
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}

// GGX/Trowbridge-Reitz Normal Distribution Function
float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

// Smith's method for geometry obstruction
float geometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = geometrySchlickGGX(NdotV, roughness);
    float ggx1 = geometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// Convert 3D direction to equirectangular UV coordinates
vec2 directionToEquirectangularUV(vec3 dir) {
    float u = atan(dir.z, dir.x) / (2.0 * PI) + 0.5;
    float v = acos(clamp(dir.y, -1.0, 1.0)) / PI;
    v = 1.0 - v;
    return vec2(u, v);
}

// Sample HDRI environment map
vec3 sampleHDRI(vec3 direction) {
    vec2 uv = directionToEquirectangularUV(direction);
    return texture(hdri, uv).rgb;
}

// Generate random direction in hemisphere (cosine weighted)
vec3 sampleHemisphereCosine(vec3 normal, vec2 Xi) {
    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt(Xi.y);
    float sinTheta = sqrt(1.0 - Xi.y);

    vec3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    // Transform to world space
    vec3 up = abs(normal.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, normal));
    vec3 bitangent = cross(normal, tangent);

    return normalize(tangent * H.x + bitangent * H.y + normal * H.z);
}

// Importance sampling for GGX distribution
vec3 importanceSampleGGX(vec2 Xi, vec3 N, float roughness) {
    float a = roughness * roughness;

    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a*a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta*cosTheta);

    // From spherical coordinates to cartesian coordinates
    vec3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    // From tangent-space vector to world-space sample vector
    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);

    vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}

// Cook-Torrance BRDF evaluation
vec3 evaluateBRDF(vec3 V, vec3 L, vec3 N, vec3 baseColor, float roughness, float metallic, vec3 F0) {
    vec3 H = normalize(V + L);

    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float HdotV = max(dot(H, V), 0.0);

    // Early exit if light is below surface
    if (NdotL <= 0.0) return vec3(0.0);

    // Calculate the Cook-Torrance BRDF components
    float D = distributionGGX(N, H, roughness);
    float G = geometrySmith(N, V, L, roughness);
    vec3 F = fresnelSchlick(HdotV, F0);

    // Specular component
    vec3 numerator = D * G * F;
    float denominator = 4.0 * NdotV * NdotL + EPSILON;
    vec3 specular = numerator / denominator;

    // Energy conservation
    vec3 kS = F; // Specular contribution
    vec3 kD = vec3(1.0) - kS; // Diffuse contribution
    kD *= 1.0 - metallic; // Metals have no diffuse lighting

    // Lambertian diffuse
    vec3 diffuse = kD * baseColor / PI;

    // Combine diffuse and specular, multiply by NdotL for Lambert's law
    return (diffuse + specular) * NdotL;
}

// Sample environment lighting using multiple samples
vec3 sampleEnvironmentLighting(vec3 worldPos, vec3 normal, vec3 viewDir, vec3 baseColor, float roughness, float metallic, vec3 F0, int baseSeed) {
    const int NUM_SAMPLES = 16; // Adjust for quality vs performance
    vec3 accumulatedColor = vec3(0.0);

    for (int i = 0; i < NUM_SAMPLES; i++) {
        // Generate random numbers for sampling
        vec2 Xi = vec2(random(baseSeed + i * 2), random(baseSeed + i * 2 + 1));

        // Sample hemisphere around normal (cosine weighted for diffuse)
        vec3 lightDir = sampleHemisphereCosine(normal, Xi);

        // Sample HDRI in this direction
        vec3 lightColor = sampleHDRI(lightDir);

        // Evaluate BRDF for this light direction
        vec3 brdf = evaluateBRDF(viewDir, lightDir, normal, baseColor, roughness, metallic, F0);

        // Add contribution (divided by PDF and number of samples)
        // For cosine-weighted hemisphere sampling, PDF = NdotL / PI
        float NdotL = max(dot(normal, lightDir), 0.0);
        if (NdotL > 0.0) {
            accumulatedColor += brdf * lightColor * PI; // PI cancels with PDF
        }
    }

    return accumulatedColor / float(NUM_SAMPLES);
}

// Generate reflection direction using importance sampling
vec3 generateReflectionDirection(vec3 viewDir, vec3 normal, float roughness, int seed) {
    vec2 Xi = vec2(random(seed), random(seed + 1));
    vec3 H = importanceSampleGGX(Xi, normal, roughness);
    vec3 L = normalize(reflect(-viewDir, H));
    return L;
}

void main() {
    // Initialize seed
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

    // Interpolate position, normals, and UVs
    vec3 localPos = v0.position * bary.x + v1.position * bary.y + v2.position * bary.z;
    vec3 worldPos = vec3(gl_ObjectToWorldEXT * vec4(localPos, 1.0));
    vec3 normalLocal = normalize(v0.normal * bary.x + v1.normal * bary.y + v2.normal * bary.z);
    vec3 normalWorld = normalize(mat3(gl_ObjectToWorldEXT) * normalLocal);
    vec2 uv = v0.uv * bary.x + v1.uv * bary.y + v2.uv * bary.z;

    // Material properties
    vec3 baseColor = texture(texSampler, uv).rgb;
    vec3 viewDir = normalize(cam.position - worldPos);

    // Base reflectivity (F0)
    vec3 F0 = mix(vec3(0.04), baseColor, metallic);

    // Sample environment lighting from HDRI
    vec3 environmentLighting = sampleEnvironmentLighting(worldPos, normalWorld, viewDir, baseColor, roughness, metallic, F0, seed);

    // Optional: Add a small amount of ambient to prevent pure black areas
    vec3 ambient = vec3(0.02) * baseColor;

    // Calculate reflection for recursive tracing
    if (inPayload.depth < MAX_DEPTH) {
        // Use importance sampling for reflection direction
        vec3 reflectionDir = generateReflectionDirection(viewDir, normalWorld, roughness, seed);

        float rayOffset = max(0.001, 0.001 * length(worldPos));
        vec3 reflectedOrigin = worldPos + rayOffset * normalWorld;

        // Calculate Fresnel for reflection strength
        vec3 H = normalize(viewDir + reflectionDir);
        float HdotV = max(dot(H, viewDir), 0.0);
        vec3 F_reflection = fresnelSchlick(HdotV, F0);
        float reflectionStrength = F_reflection.r;

        // Trace reflection ray
        reflectionPayload.color = vec3(0.0);
        reflectionPayload.depth = inPayload.depth + 1;
        traceRayEXT(
        topLevelAS,
        gl_RayFlagsOpaqueEXT,
        0xFF,
        0,
        1,
        0,
        reflectedOrigin,
        0.001,
        reflectionDir,
        10000.0,
        1
        );

        // Combine environment lighting with reflection
        vec3 finalColor = environmentLighting + ambient;
        // Reduce reflection contribution to preserve base color visibility
        inPayload.color = mix(finalColor, reflectionPayload.color, reflectionStrength * (1.0 - roughness * roughness) * 0.3);
    } else {
        // No more bounces, just use environment lighting
        inPayload.color = environmentLighting + ambient;
    }
}