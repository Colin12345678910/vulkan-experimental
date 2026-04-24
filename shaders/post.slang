RWTexture2D<float4> image;

// push constants block
struct PushConstants
{
    float pixelOffset;
    float samplePosMult;
    float strength;
    float padding;
};

[[vk::push_constant]] PushConstants pushConstants;

// Basic implementation of the Kawase blur filter.
// As described: https://blog.frost.kiwi/dual-kawase/
[shader("compute")]
[numthreads(16,16,1)]
void Main(uint2 dispatchThreadID: SV_DispatchThreadID)
{
    int2 texelCoord = dispatchThreadID;

    //Offset
    float2 o = float2(pushConstants.pixelOffset + 0.5) * pushConstants.samplePosMult;

    // Sample the four corners.
    float4 color = float4(0.0);
    color += image[texelCoord + int2(float2(o.x, o.y))];
    color += image[texelCoord + int2(float2(-o.x, o.y))];
    color += image[texelCoord + int2(float2(-o.x, -o.y))];
    color += image[texelCoord + int2(float2(o.x, -o.y))];
    color /= 4.0;

    image[texelCoord] = color * pushConstants.strength;
}