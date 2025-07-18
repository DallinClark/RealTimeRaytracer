// LTC helper functions
vec3 IntegrateEdgeVec(vec3 v1, vec3 v2)
{
    float x = dot(v1, v2);
    float y = abs(x);

    float a = 0.8543985 + (0.4965155 + 0.0145206*y)*y;
    float b = 3.4175940 + (4.1616724 + y)*y;
    float v = a / b;

    float theta_sintheta = (x > 0.0) ? v : 0.5*inversesqrt(max(1.0 - x*x, 1e-7)) - v;

    return cross(v1, v2)*theta_sintheta;
}

vec3 LTC_Evaluate(vec3 N, vec3 V, vec3 P, mat3 Minv, vec3 points[3], vec3 lightNormal, bool twoSided)
{
    // construct orthonormal basis around N
    vec3 T1, T2;
    T1 = normalize(V - N * dot(V, N));
    T2 = cross(N, T1);

    // rotate area light in (T1, T2, N) basis
    Minv = Minv * transpose(mat3(T1, T2, N));

    // polygon (allocate 4 vertices for clipping)
    vec3 L[3];
    // transform polygon from LTC back to origin Do (cosine weighted)
    L[0] = Minv * (points[0] - P);
    L[1] = Minv * (points[1] - P);
    L[2] = Minv * (points[2] - P);

    // use tabulated horizon-clipped sphere
    // check if the shading point is behind the light
    vec3 dir = points[0] - P; // LTC space
    // lightNormal = cross(points[1] - points[0], points[2] - points[0]);
    bool behind = (dot(dir, lightNormal) < 0.0);

    // cos weighted space
    L[0] = normalize(L[0]);
    L[1] = normalize(L[1]);
    L[2] = normalize(L[2]);

    // integrate
    vec3 vsum = vec3(0.0);
    vsum += IntegrateEdgeVec(L[0], L[1]);
    vsum += IntegrateEdgeVec(L[1], L[2]);
    vsum += IntegrateEdgeVec(L[2], L[0]);

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