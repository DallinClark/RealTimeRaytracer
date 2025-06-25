#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require
#include "raycommon.glsl"
#include "cook-torrance.glsl"
#include "LTC.glsl"

hitAttributeEXT vec2 attribs;

layout( push_constant ) uniform constants
{
	uint frame;
	uint numLights;
    uint _pad0;
    uint _pad1;
	vec3 camPosition;
	float padding_;
} sceneData;

layout(location = 0) rayPayloadInEXT RayPayload payloadIn;
layout(location = 1) rayPayloadEXT bool isShadowed;

layout(set = 0, binding = 1) uniform accelerationStructureEXT topLevelAS;
layout(set = 0, binding = 3) readonly buffer VertexBuffer {
    Vertex vertices[];
};
layout(set = 0, binding = 4) readonly buffer IndexBuffer {
    uint indices[];
};
layout(set = 0, binding = 5) uniform sampler2D texSamplers[];
layout(set = 0, binding = 6) readonly buffer ObjectInfoBuffer {
    ObjectInfo objectInfos[];
};
layout(set = 0, binding = 7) readonly buffer LightInfoBuffer {
    LightInfo lightInfos[];
};

const float LUT_SIZE  = 64.0; // ltc_texture size
const float LUT_SCALE = (LUT_SIZE - 1.0)/LUT_SIZE;
const float LUT_BIAS  = 0.5/LUT_SIZE;

// Vector form without project to the plane (dot with the normal)
// Use for proxy sphere clipping
vec3 IntegrateEdgeVec(vec3 v1, vec3 v2)
{
  // Using built-in acos() function will result flaws
  // Using fitting result for calculating acos()
  float x = dot(v1, v2);
  float y = abs(x);

  float a = 0.8543985 + (0.4965155 + 0.0145206*y)*y;
  float b = 3.4175940 + (4.1616724 + y)*y;
  float v = a / b;

  float theta_sintheta = (x > 0.0) ? v : 0.5*inversesqrt(max(1.0 - x*x, 1e-7)) - v;

  return cross(v1, v2)*theta_sintheta;
}

float IntegrateEdge(vec3 v1, vec3 v2)
{
  return IntegrateEdgeVec(v1, v2).z;
}

// P is fragPos in world space (LTC distribution)
vec3 LTC_Evaluate(vec3 N, vec3 V, vec3 P, mat3 Minv, vec3 points[4], bool twoSided)
{
  // construct orthonormal basis around N
  vec3 T1, T2;
  T1 = normalize(V - N * dot(V, N));
  T2 = cross(N, T1);

  // rotate area light in (T1, T2, N) basis
  Minv = Minv * transpose(mat3(T1, T2, N));

  // polygon (allocate 4 vertices for clipping)
  vec3 L[4];
  // transform polygon from LTC back to origin Do (cosine weighted)
  L[0] = Minv * (points[0] - P);
  L[1] = Minv * (points[1] - P);
  L[2] = Minv * (points[2] - P);
  L[3] = Minv * (points[3] - P);

  // use tabulated horizon-clipped sphere
  // check if the shading point is behind the light
  vec3 dir = points[0] - P; // LTC space
  vec3 lightNormal = cross(points[1] - points[0], points[3] - points[0]);
  bool behind = (dot(dir, lightNormal) < 0.0);

  // cos weighted space
  L[0] = normalize(L[0]);
  L[1] = normalize(L[1]);
  L[2] = normalize(L[2]);
  L[3] = normalize(L[3]);

  // integrate
  vec3 vsum = vec3(0.0);
  vsum += IntegrateEdgeVec(L[0], L[1]);
  vsum += IntegrateEdgeVec(L[1], L[2]);
  vsum += IntegrateEdgeVec(L[2], L[3]);
  vsum += IntegrateEdgeVec(L[3], L[0]);

  // form factor of the polygon in direction vsum
  float len = length(vsum);

  float z = vsum.z/len;
  if (behind)
      z = -z;

  vec2 uv = vec2(z*0.5f + 0.5f, len); // range [0, 1]
  uv = uv*LUT_SCALE + LUT_BIAS;

  // Fetch the form factor for horizon clipping
  float scale = texture(texSamplers[1], uv).w;

  float sum = len*scale;
  if (!behind && !twoSided)
      sum = 0.0;

  // Outgoing radiance (solid angle) for the entire polygon
  vec3 Lo_i = vec3(sum, sum, sum);
  return Lo_i;
}

void main() {
    if (gl_InstanceCustomIndexEXT < sceneData.numLights) {  // If this is a light
        payloadIn.primaryColor = lightInfos[gl_InstanceCustomIndexEXT].color;
        return;
    }

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

    float roughness = 0.0;
    float metallic = 0.0;
    vec3 surfaceColor = vec3(0.0);

    if (objectInfo.usesColorMap != 0) {
          surfaceColor = texture(nonuniformEXT(texSamplers[objectInfo.colorIndex]), uv).rgb;
    } else {
        surfaceColor = objectInfo.color;
    }

    if (objectInfo.usesSpecularMap != 0) {
        roughness = texture(nonuniformEXT(texSamplers[objectInfo.specularIndex]), uv).r;
    } else {
        roughness = objectInfo.specular;
    }

    if (objectInfo.usesMetallicMap != 0) {
        metallic = texture(nonuniformEXT(texSamplers[objectInfo.metallicIndex]), uv).r;
    } else {
        metallic = objectInfo.metallic;
    }

    vec3 viewDir = normalize(sceneData.camPosition - hitPoint);

    vec3 mDiffuse = (1.0 - metallic) * surfaceColor;
    vec3 mSpecular = mix(vec3(0.04), surfaceColor, metallic);

   vec3 N = hitNormal;
   vec3 V = viewDir;
   vec3 P = hitPoint;
   float dotNV = clamp(dot(N, V), 0.0f, 1.0f);


   // use roughness and sqrt(1-cos_theta) to sample M_texture
   vec2 LUTuv = vec2(roughness, sqrt(1.0f - dotNV));
   LUTuv = LUTuv*LUT_SCALE + LUT_BIAS;


   // get 4 parameters for inverse_M
   vec4 t1 = texture(nonuniformEXT(texSamplers[0]), LUTuv);

   // Get 2 parameters for Fresnel calculation
   vec4 t2 = texture(nonuniformEXT(texSamplers[1]), LUTuv);

   mat3 Minv = mat3(
       vec3(t1.x, 0, t1.y),
       vec3(  0,  1,    0),
       vec3(t1.z, 0, t1.w)
   );

  vec3 result = vec3(0.0);
  vec3 specular = vec3(0.0);
  vec3 diffuse = vec3(0.0);


   for (uint i = 0; i < sceneData.numLights; ++i) {

       LightInfo currLight = lightInfos[i];

       uint lightVertexOffset = currLight.vertexOffset;

       Vertex lv0 = vertices[lightVertexOffset + 0];
       Vertex lv1 = vertices[lightVertexOffset + 1];
       Vertex lv2 = vertices[lightVertexOffset + 2];
       Vertex lv3 = vertices[lightVertexOffset + 3];

       mat4 lightTransform = currLight.transform;

       vec3 translatedPoints[4];
       translatedPoints[0] = (lightTransform * vec4(lv0.position, 1.0)).xyz;
       translatedPoints[1] = (lightTransform * vec4(lv1.position, 1.0)).xyz;
       translatedPoints[2] = (lightTransform * vec4(lv2.position, 1.0)).xyz;
       translatedPoints[3] = (lightTransform * vec4(lv3.position, 1.0)).xyz;

       const uint numShadowSamples = 24;
       float shadowFactor = 0.0;
       vec3 lightSamplePos = vec3(0.0);



       for (uint s = 0; s < numShadowSamples; ++s) {
           // Compute shadow ray direction towards some point on the area light
           // For now, just pick a simple center or random point on the light
           uvec2 pixel = gl_LaunchIDEXT.xy;
           uint seed = s + pixel.x * 733 + pixel.y * 1933 + sceneData.frame;

            float r1 = random(seed);
            float r2 = random(seed + 100);

            bool useSecondTriangle = (random(s + 64) > 0.5);

           if (!useSecondTriangle) {
               // Triangle 1: translatedPoints[0], translatedPoints[1], translatedPoints[2]
               if (r1 + r2 > 1.0) {
                   r1 = 1.0 - r1;
                   r2 = 1.0 - r2;
               }
               lightSamplePos = translatedPoints[0] + r1 * (translatedPoints[1] - translatedPoints[0]) + r2 * (translatedPoints[2] - translatedPoints[0]);
           } else {
                   // Triangle 2: translatedPoints[0], translatedPoints[2], translatedPoints[3]
                   if (r1 + r2 > 1.0) {
                       r1 = 1.0 - r1;
                       r2 = 1.0 - r2;
                   }
                   lightSamplePos = translatedPoints[0] + r1 * (translatedPoints[2] - translatedPoints[0]) + r2 * (translatedPoints[3] - translatedPoints[0]);
               }


           vec3 shadowRayDir = normalize(lightSamplePos - hitPoint);
           float lightDistance = length(lightSamplePos - hitPoint);

           vec3 shadowRayOrigin = hitPoint + hitNormal * 0.01;

           // Reset shadow payload before trace
           isShadowed = true;

           uint shadowFlags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;

           traceRayEXT(
               topLevelAS,
               shadowFlags,
               0xFF,
               0, 0, 0,
               shadowRayOrigin,
               0.001,
               shadowRayDir,
               lightDistance - 0.5,
               1 // location of shadow ray payload
           );

           // if any hit, isShadowed will be true, so shadowFactor only adds when not shadowed
           shadowFactor += isShadowed ? 0.0 : 1.0;
       }

       shadowFactor /= float(numShadowSamples);

       bool twoSided = true;

       diffuse = LTC_Evaluate(N, V, P, mat3(1), translatedPoints, twoSided);
       specular = LTC_Evaluate(N, V, P, Minv, translatedPoints, twoSided);


       vec3 fresnel = mSpecular * t2.x + (vec3(1.0) - mSpecular) * t2.y;
       specular *= fresnel;

       result += currLight.color * currLight.intensity * shadowFactor * (specular + mDiffuse * diffuse);
   }

   payloadIn.primaryColor = result;

}