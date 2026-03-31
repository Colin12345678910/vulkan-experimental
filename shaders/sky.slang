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

    float denom = dot(rd, planeNormal);

    float4 col = pushConstants.data1;

    if (denom > 0.01)
    {
        float t = dot(planePoint - ro, planeNormal) / denom;

        if (t > 0.0)
        {
            float3 hitPos = ro + rd * t;

            float2 skyUV = hitPos.xz * 0.001;
            float clouds = noise(skyUV);

            col = col + clouds;
        }
    }

    // Sun

    float3 lDirection = normalize(float3(-.8, .8, -.3));
    float sunDot = clamp(dot(rd, lDirection), 0.0, 1.0);

    float3 sun = 0.25 * float3(1.0, 0.8, 0.4) * pow(sunDot, 5.0);
    sun += 0.25 * float3(1.0, 0.8, 0.6) * pow(sunDot, 64.0);
    sun += 0.2 * float3(1.0, 0.8, 0.6) * pow(sunDot, 512.0);

    col += float4(sun, 0.0f);

    image[texelCoord] = col;
}

float noise(float2 uv)
{
    uv.x += pushConstants.data4.z;
    uv.y += pushConstants.data4.z;
    return cloudTex.SampleLevelZero(uv).x;
}