RWTexture2D<float4> image;
Sampler2D cloudTex;

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
    uv = uv * 2.0 - 1.0;
    uv.x *= float(size.x) / float(size.y);

    // Clouds
    const float CLOUD_HEIGHT = 100.0;
    float3 planeNormal = float3(0.0, 1.0, 0.0);
    float3 planePoint = float3(0.0, CLOUD_HEIGHT, 0.0);

    float2 ndc = (float2(texelCoord) + 0.5) / float2(size) * 2.0 - 1.0;
    ndc.y = -ndc.y;

    float4 clip = float4(ndc, 1.0, 1.0);
    float4 viewRay = mul(pushConstants.invProj, clip);
    float3 rayView = normalize(viewRay.xyz / viewRay.w);

    float3 rd = normalize(mul((float3x3)pushConstants.view, rayView));
    float3 ro = pushConstants.cameraPos.xyz;

    image[texelCoord] = float4(0.0);

    RayMarch(ro, rd, (float3 hitPos, float3 normal, float amount) => {
                         image[texelCoord] = float4(0.5) + mul(normal, normalize(float3(1.0, 1.0, 0.5)));
    });

    // float denom = dot(rd, planeNormal);

    // float4 col = pushConstants.data1;

    // if (denom > 0.01)
    // {
    //     float t = dot(planePoint - ro, planeNormal) / denom;

    //     if (t > 0.0)
    //     {
    //         float3 hitPos = ro + rd * t;

    //         float2 skyUV = hitPos.xz * 0.001;
    //         float clouds = noise(skyUV);

    //         col = col + clouds;
    //     }
    // }

    // // Sun

    // float3 lDirection = normalize(float3(-.8, .8, -.3));
    // float sunDot = clamp(dot(rd, lDirection), 0.0, 1.0);

    // float3 sun = 0.25 * float3(1.0, 0.8, 0.4) * pow(sunDot, 5.0);
    // sun += 0.25 * float3(1.0, 0.8, 0.6) * pow(sunDot, 64.0);
    // sun += 0.2 * float3(1.0, 0.8, 0.6) * pow(sunDot, 512.0);

    // col += float4(sun, 0.0f);

    // image[texelCoord] = col;
    
}

float noise(float3 uv)
{
    uv.x += uv.y * 0.05;
    return cloudTex.SampleLevelZero(uv.xz).x;
}

void RayMarch(float3 ro, float3 rd, IFunc<void, float3, float3, float> OnSuccess)
{
    float t = 0.0;
    float amount = 0.0;
    float3 p = float3(0);

    for (int i = 0; i < 100; i++)
    {
        p = ro + rd * t;
        float d = map(p);

        if (d < 0.001)
        {
            amount += pushConstants.data3.x;
            OnSuccess(p, getNormal(p), amount);
            break;
        }

        t += d * 0.5;
        
        if (t > 250.0)
            break;
    }
}


// A Testing cloud map.
float map(float3 p)
{
    float3 cubehalfSize = float3(1000, 1, 1000);

    float cube = sdBox(p - float3(0), cubehalfSize);

    //Scales the noise
    float noiseScale = pushConstants.data2.x;

    float3 uvw = p * noiseScale;

    float cloudCutout = noise(uvw) - pushConstants.data2.y;

    cloudCutout = cloudCutout / noiseScale;

    //Boolean subtract the cloud
    float distance = max(cube, -cloudCutout);
    return distance;
}

//SDF variant of getNormal
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

float sdBox(float3 p, float3 b)
{
    float3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}