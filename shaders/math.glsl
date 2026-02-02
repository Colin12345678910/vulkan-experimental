const float PI = 3.14159265;

const vec2 invATan = vec2(0.1591, 0.3183);
vec2 SampleSphericalMap(vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invATan;
    uv += 0.5;
    return uv;
}

vec3 GetNormal(uint face, vec2 uv)
{
    uv = uv * 2.0 - 1.0;

    switch (face)
    {
        case 0: return normalize(vec3( 1.0, -uv.y, -uv.x)); // +X
        case 1: return normalize(vec3(-1.0, -uv.y,  uv.x)); // -X
        case 2: return normalize(vec3( uv.x,  1.0,  uv.y)); // +Y
        case 3: return normalize(vec3( uv.x, -1.0, -uv.y)); // -Y
        case 4: return normalize(vec3( uv.x, -uv.y,  1.0)); // +Z
        case 5: return normalize(vec3(-uv.x, -uv.y, -1.0)); // -Z
    }
}

// RadicalInverse
// This is a bit of a mess of a function, but it is intended (Alongside hammersley and GGX) to give a very regular pseudorandom sequence
// Usually called Low-Discrepency, maybe I should look into this function more, but it's quite insane.
// Taken from learnOpenGL's implemenation.
float RadicalInverse_VdC(uint bits) 
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}
vec2 Hammersley(uint i, uint N)
{
    return vec2(float(i)/float(N), RadicalInverse_VdC(i));
}  
// ImportanceSampleGGX
// From what I understand this is effectively generates a random direction heavily biased towards
// the reflection direction/specular lobe, this helps in approximating specular reflections by reducing
// work
// Implementation comes from LearnOGL.
vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
{
    float a = roughness*roughness;
	
    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a*a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta*cosTheta);
	
    // from spherical coordinates to cartesian coordinates
    vec3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;
	
    // from tangent-space vector to world-space sample vector
    vec3 up        = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent   = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);
	
    vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}  

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float a = roughness;
    float k = (a * a) / 2.0;

    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}