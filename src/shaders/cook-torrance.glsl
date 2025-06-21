
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

vec3 sampleGGX(vec3 N, vec3 V, float roughness, vec2 rand)
{
    float a = roughness * roughness;

    float phi = 2.0 * 3.141592 * rand.x;
    float cosTheta = sqrt((1.0 - rand.y) / (1.0 + (a * a - 1.0) * rand.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    vec3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    // Orthonormal basis
    vec3 T, B;
    T = normalize(V - N * dot(N, V));
    B = cross(N, T);

    // Tangent to world space
    vec3 halfway = normalize(H.x * T + H.y * B + H.z * N);
    return reflect(-V, halfway);
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