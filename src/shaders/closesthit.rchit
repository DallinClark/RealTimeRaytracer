#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require
#include "raycommon.glsl"

layout(location = 0) rayPayloadInEXT RayPayload payloadIn;
layout(location = 1) rayPayloadEXT bool isShadowed;
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
layout(set = 0, binding = 5) uniform sampler2D texSamplers[];
layout(set = 0, binding = 6) readonly buffer BLASInfoBuffer {
    BLASInstanceInfo blasInfos[];
};
layout(set = 0, binding = 7) readonly buffer VertexPositionBuffer {
    vec3 vertexPositions[];
};


const float PI = 3.14159265359;

float chiGGX(float v)
{
    return v > 0 ? 1 : 0;
}
// n = surface normal
// h = the half vector between view and light,
// v = view direction (towards camera)

float GGX_Distribution(vec3 n, vec3 h, float alpha)  // output how much a particular microfacet orientation contributes
{
    float NoH = dot(n,h);
    float alpha2 = alpha * alpha;
    float NoH2 = NoH * NoH;
    float den = max(NoH2 * alpha2 + (1.0 - NoH2), 0.001);
    return (chiGGX(NoH) * alpha2) / ( PI * den * den );
}

float GGX_PartialGeometryTerm(vec3 v, vec3 n, vec3 h, float alpha) // output how much the light is reduced by microfacet shadows, bounces, etc.
{
    float VoH2 = clamp(dot(v, h), 0.001, 1.0);
    float chi = chiGGX( VoH2 / clamp(dot(v, n), 0.001, 1.0) );
    VoH2 = VoH2 * VoH2;
    float tan2 = ( 1 - VoH2 ) / VoH2;
    return (chi * 2) / ( 1 + sqrt( 1 + alpha * alpha * tan2 ) );
}

// cosT = the angle between the viewing direction and the half vector
// F0 = the materials response at normal incidence calculated as follows
// float3 F0 = abs ((1.0 - ior) / (1.0 + ior));
// F0 = F0 * F0;
// F0 = lerp(F0, materialColour.rgb, metallic);

vec3 Fresnel_Schlick(float cosT, vec3 F0) // output the way the light interacts with the surface at different angles
{
  return F0 + (1.0 - F0) * pow( 1 - cosT, 5.0);
}


void main() {


    BLASInstanceInfo blasInfo = blasInfos[gl_InstanceCustomIndexEXT];
    uint vertexOffset = blasInfo.vertexIndexOffset;
    uint indexOffset = blasInfo.indexIndexOffset;

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

    vec3 surfaceColor = texture(nonuniformEXT(texSamplers[blasInfo.textureIndex * 2]), uv).rgb;
    vec3 surfaceMaterial = texture(nonuniformEXT(texSamplers[blasInfo.textureIndex * 2 + 1]), uv).rgb;


    vec3 lightColor = vec3(1.0, 0.95, 0.9);
    float lightIntensity = 5.0;
    vec3 lightDir = normalize(vec3(5, 4, 3));
    vec3 viewDir = normalize(cam.position - hitPoint);
    vec3 halfVector = normalize(lightDir + viewDir);

    // Start shadow ray slightly offset to avoid self-intersection
    vec3 shadowRayDir = lightDir; //
    vec3 shadowRayOrigin = hitPoint + (hitNormal * 0.01);

    // Initialize shadow payload
    isShadowed = true;

    uint flags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;

    // Trace shadow ray towards the light
    traceRayEXT(
        topLevelAS,
        flags, // terminate on first hit
        0xFF,
        0,    // sbt record offset
        0,    // sbt record stride
        1,    // miss index
        shadowRayOrigin,
        0.001,   // min t
        shadowRayDir,
        1000.0,  // max t (light distance, or large for directional light)
        1        // location of the shadow ray payload (location=1)
    );

    float shadowFactor = isShadowed ? 0.0 : 1.0;

    if (payloadIn.depth < 2) {

            payloadIn.depth += 1;
            vec3 reflectedDir = reflect(-viewDir, hitNormal);
            vec3 reflectedOrigin = hitPoint + hitNormal * 0.01;

            traceRayEXT(
                topLevelAS,
                gl_RayFlagsOpaqueEXT,
                0xFF,
                0, 0, 0,
                reflectedOrigin,
                0.001,
                reflectedDir,
                1000.0,
                0 // payload location
            );

    }

    float roughness = surfaceMaterial.r;
    roughness = clamp(roughness, 0.01, 1.0);
    float metallic = surfaceMaterial.b;
    float occlusion = surfaceMaterial.g; // NEED TO IMPLEMENT
    vec3 ior = vec3(1.5); // NEED TO CHANGE TO JUST A VALUE

    vec3 F0 = abs((1.0 - ior) / (1.0 + ior));
    F0 = F0 * F0;
    F0 = mix(vec3(0.04), surfaceColor, metallic);
    float cosTheta = clamp(dot(viewDir, halfVector), 0.0, 1.0);

    float D = GGX_Distribution(hitNormal, halfVector, roughness);
    float G = GGX_PartialGeometryTerm(viewDir, hitNormal, halfVector, roughness) * GGX_PartialGeometryTerm(lightDir, hitNormal, halfVector, roughness);
    vec3 F = Fresnel_Schlick(cosTheta, F0);

    float NdotV = max(dot(hitNormal, viewDir), 0.0001);
    float NdotL = max(dot(hitNormal, lightDir), 0.0001);

    vec3 specular = (D * F * G)  * 1.0 / (4.0 * NdotV * NdotL);
    specular = specular * payloadIn.reflectionColor * shadowFactor;

    vec3 kD = vec3(1.0) - F;           // Energy conservation
    kD *= 1.0 - metallic;              // Lambert Diffuse

    vec3 diffuse = (surfaceColor / PI) * kD * clamp(shadowFactor, 0.1, 1.0);


    vec3 radiance = lightColor * lightIntensity * NdotL;  // incoming light

    vec3 color = (specular + diffuse) * radiance;

    vec3 ambient = vec3(0.1) * surfaceColor ;  // or indirect lighting / GI
    color += ambient ;//* occlusion;

    payloadIn.primaryColor = specular;//color;
}