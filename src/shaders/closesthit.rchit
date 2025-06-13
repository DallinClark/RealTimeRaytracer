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

layout(set = 0, binding = 4) readonly buffer VertexPositionBuffer {
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
    vec3 worldPos = vec3(gl_ObjectToWorldEXT * vec4(localPos, 1.0));
    vec3 interpolatedLocalNormal = normalize(v0.normal * bary.x + v1.normal * bary.y + v2.normal * bary.z);
    vec3 interpolatedWorldNormal = normalize(mat3(gl_ObjectToWorldEXT) * interpolatedLocalNormal);
    vec2 uv = v0.uv * bary.x + v1.uv * bary.y + v2.uv * bary.z;

    vec3 lightColor = vec3(1.0, 0.95, 0.9);
    float lightIntensity = 1.0;
    vec3 lightDir = normalize(vec3(-1, 3, 0));
    vec3 viewDir = normalize(cam.position - worldPos);

    // Use nonuniform qualifier for dynamic indexing
    vec3 baseColor = texture(nonuniformEXT(texSamplers[blasInfo.textureIndex]), uv).rgb;

    vec3 ambientColor = vec3(0.2, 0.2, 0.2);
    float diff = max(dot(interpolatedWorldNormal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, interpolatedWorldNormal);
    float specStrength = 0.3;
    float shininess = 12.0;
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);

    vec3 color = ambientColor * baseColor +
                 diff * baseColor * lightColor * lightIntensity +
                 spec * specStrength * lightColor * lightIntensity;

    payload = color;
}
