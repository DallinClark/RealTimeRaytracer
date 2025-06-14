#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require

layout(location = 0) rayPayloadInEXT vec3 payload;
layout(location = 1) rayPayloadEXT bool isShadowed;

hitAttributeEXT vec2 attribs;

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

struct Vertex {
    vec3 normal;   float pad1;
    vec2 uv;       vec2  pad2;
};

struct BLASInstanceInfo {
    uint vertexIndexOffset;
    uint indexIndexOffset;
    uint textureIndex;
    uint _padding; // std430 requires 16-byte alignment
};

struct GPUCameraData {
    vec3 position;               float _pad0;
    vec3 topLeftViewportCorner; float _pad1;
    vec3 horizontalViewportDelta; float _pad2;
    vec3 verticalViewportDelta; float _pad3;
};

layout(set = 0, binding = 2) uniform CameraUBO {
    GPUCameraData cam;
};

layout(set = 0, binding = 3) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout(set = 0, binding = 6) readonly buffer BLASInfoBuffer {
    BLASInstanceInfo blasInfos[];
};

layout(set = 0, binding = 4) readonly buffer IndexBuffer {
    uint indices[];
};

layout(set = 0, binding = 7) readonly buffer VertexPositionBuffer {
    vec3 vertexPositions[];
};


layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;

// Array of textures
layout(set = 0, binding = 5) uniform sampler2D texSamplers[];

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

    vec3 v0position = vertexPositions[index0 + vertexOffset];
    vec3 v1position = vertexPositions[index1 + vertexOffset];
    vec3 v2position = vertexPositions[index2 + vertexOffset];


    vec3 bary = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    vec3 localPos = v0position * bary.x + v1position * bary.y + v2position * bary.z;
    vec3 hitPoint = vec3(gl_ObjectToWorldEXT * vec4(localPos, 1.0));
    vec3 interpolatedLocalNormal = normalize(v0.normal * bary.x + v1.normal * bary.y + v2.normal * bary.z);
    vec3 hitNormal = normalize(mat3(gl_ObjectToWorldEXT) * interpolatedLocalNormal);
    vec2 uv = v0.uv * bary.x + v1.uv * bary.y + v2.uv * bary.z;

    vec3 surfaceColor = texture(nonuniformEXT(texSamplers[blasInfo.textureIndex]), uv).rgb;
    vec3 lightColor = vec3(1.0, 0.95, 0.9);
    float lightIntensity = 2.0;
    vec3 lightDir = normalize(vec3(0, 1, 0));
    vec3 viewDir = normalize(cam.position - hitPoint);
    vec3 halfVector = normalize(lightDir + viewDir);

    float roughness = 0.2;  // GET FROM ROUGHNESS MAP
    vec3 ior = vec3(1.5); // GET FROM MAP, OR DONT
    float metallic = 0.0; // GET FROM MAP

    vec3 F0 = abs((1.0 - ior) / (1.0 + ior));
    F0 = F0 * F0;
    F0 = mix(F0, surfaceColor, metallic);
    float cosTheta = clamp(dot(viewDir, halfVector), 0.0, 1.0);

    float D = GGX_Distribution(hitNormal, halfVector, roughness);
    float G = GGX_PartialGeometryTerm(viewDir, hitNormal, halfVector, roughness) * GGX_PartialGeometryTerm(lightDir, hitNormal, halfVector, roughness);
    vec3 F = Fresnel_Schlick(cosTheta, F0);

    float NdotV = max(dot(hitNormal, viewDir), 0.01);
    float NdotL = max(dot(hitNormal, lightDir), 0.01);

    vec3 specular = (D * F * G) / (4.0 * NdotV * NdotL);

    vec3 kD = vec3(1.0) - F;           // Energy conservation
    kD *= 1.0 - metallic;              // Lambert Diffuse

    vec3 diffuse = (surfaceColor / PI) * kD;


    vec3 radiance = lightColor * lightIntensity * NdotL;  // incoming light

    vec3 color = (specular + diffuse) * radiance;

    vec3 ambient = vec3(0.03) * surfaceColor;  // or indirect lighting / GI
    color += ambient;

    payload = color;
}