#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec3 payload;

hitAttributeEXT vec3 attribs;

struct Vertex {
    vec3 position; float pad0;
    vec3 normal;   float pad1;
    vec2 uv;       vec2  pad2;
};

layout(set = 0, binding = 3) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout(set = 0, binding = 4) readonly buffer IndexBuffer {
    uint indices[];
};

void main() {
    uint vertIndex0 = indices[3 * gl_PrimitiveID  + 0];
    uint vertIndex1 = indices[3 * gl_PrimitiveID  + 1];
    uint vertIndex2 = indices[3 * gl_PrimitiveID  + 2];

    Vertex v0 = vertices[vertIndex0];
    Vertex v1 = vertices[vertIndex1];
    Vertex v2 = vertices[vertIndex2];

    const vec3 barycentricCoords = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
    const vec3 position = v0.position * barycentricCoords.x + v1.position * barycentricCoords.y + v2.position * barycentricCoords.z;

    payload = vec3(position.x,0.0,0.0);//clamp(position,0.0,1.0);
}
