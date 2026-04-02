RWTexture2D<float4> image;
Sampler3D cloudTex;

// push constants block
struct PushConstants
{
    float4 data1;
    float4 data2;
    float4 data3;
    float4 data4;
    float4 cameraPos;
    float4x4 view;
    float4x4 invProj;
};

[[vk::push_constant]] PushConstants pushConstants;

[shader("compute")]
[numthreads(16,16,1)]
void Main(uint2 dispatchThreadID: SV_DispatchThreadID)
{
    int2 texelCoord = dispatchThreadID;

    int2 size;
    image.GetDimensions(size.x, size.y);

    float2 uv = float2(texelCoord) / float2(size);
    // uv = uv * 2.0 - 1.0;
    uv.x *= float(size.x) / float(size.y);

    float3 lDirection = normalize(float3(-.8, .8, -.3));

    // Clouds
    const float CLOUD_HEIGHT = 100.0;
    float3 planeNormal = float3(0.0, 1.0, 0.0);
    float3 planePoint = float3(0.0, CLOUD_HEIGHT, 0.0);

    float2 ndc = (float2(texelCoord) + 0.5) / float2(size) * 2.0 - 1.0;
    ndc.y = -ndc.y;

    float4 clip = float4(ndc, 1.0, 1.0);
    float4 viewRay = mul(pushConstants.invProj, clip);
    float3 rayView = normalize(viewRay.xyz / viewRay.w);

    //Ray origin & direction
    float3 rd = normalize(mul((float3x3)pushConstants.view, rayView));
    float3 ro = pushConstants.cameraPos.xyz;

    image[texelCoord] = float4(0.0);

    RayMarch(ro, rd, lDirection, texelCoord);

    float4 col = pushConstants.data1;

    // Sun

    float sunDot = clamp(dot(rd, lDirection), 0.0, 1.0);

    float3 sun = 0.25 * float3(1.0, 0.8, 0.4) * pow(sunDot, 5.0);
    sun += 0.25 * float3(1.0, 0.8, 0.6) * pow(sunDot, 64.0);
    sun += 0.2 * float3(1.0, 0.8, 0.6) * pow(sunDot, 512.0);

    image[texelCoord] += col + float4(sun, 0.0f);

    // image[texelCoord] = col;
    
}

float noise(float3 uv)
{
    float3 u = uv;
    u.x += pushConstants.data4.z % 256;
    u.z -= pushConstants.data4.z % 256;
    return cloudTex.SampleLevelZero(u).x * cloudTex.SampleLevelZero(u * 0.1).x;
}

void RayMarch(float3 ro, float3 rd, float3 lDirection, int2 texelCoord)
{
    //If our ray is below the horizon, skip it.
    if (rd.y <= 0)
    {
        return;
    }

    //Calculate where the ray should be cast/marched from, this is the point where the ray from the camera would intersect our cloud "plane"
    float startT = (pushConstants.cameraPos.y + 25 - ro.y) / rd.y;
    float t = startT;

    if (startT > 1000) return; //Put a cap on max cloud distance.

    float amount = 0.0;
    float3 p = float3(0);

    //Unrolling the loop seemed to bring extra performance, (possible placebo.)
    [unroll]
    for (int i = 0; i < 32; i++)
    {
        //Ray position
        p = ro + rd * t;

        // Distance equation
        float d = map(p);

        //If our distance is small, we've hit something
        if (d < 0.001)
        {
            amount += pushConstants.data3.x;
            // Basic diffuse lighting.
            float4 col = float4(0.75);
            col += mul(getNormal(p), lDirection);
            image[texelCoord] = col;
            break;
        }

        //Step an defined amount
        t += d * pushConstants.data2.w;

        //If our ray marches too far past our start distance, cull the ray.
        if (t > startT + 100)
            break;
    }
}

// A Testing cloud map.
float map(float3 p)
{
    //Scales the noise
    float noiseScale = pushConstants.data2.x;
    float3 uvw = p * noiseScale;

    //Calculate our distance.
    float cloudCutout = noise(uvw) - pushConstants.data2.y;
    //Scale our distance back
    cloudCutout = cloudCutout / noiseScale;

    //return the negative or "cutout"
    return -cloudCutout;
}

//SDF getNormal
//TODO: Pre-compute this normals calculation when loading a cloud texture for the first time
//Pack normals in RGB, and noise value in A.
float3 getNormal(float3 p)
{
    const float eps = pushConstants.data2.z;
    const float2 h = float2(eps, 0);

    float3 n = map(p) - float3(
                                  map(p - h.xyy),
                                  map(p - h.yxy),
                                  map(p - h.yyx),
    );

    return normalize(n);
}