// JFA Outline Pass Compute Shader

cbuffer JFAOutlineParams : register(b0)
{
    float4 outlineColor;
    float outlineWidth;
    float3 _padding;
};

Texture2D<int2> jfaTexture : register(t0);

#ifdef TARGET_VULKAN
    [[vk::image_format("rgba8")]]
#endif
RWTexture2D<unorm float4> outputTexture : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 dispatchId : SV_DISPATCHTHREADID)
{
    uint2 texSize;
    outputTexture.GetDimensions(texSize.x, texSize.y);

    if (dispatchId.x >= texSize.x || dispatchId.y >= texSize.y)
        return;

    int2 pixel = int2(dispatchId.xy);
    int2 seed = jfaTexture.Load(int3(pixel, 0));

    if (seed.x == 32767)
    {
        outputTexture[pixel] = float4(0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }

    float dist = length(float2(seed - pixel));
    if (dist > 0.0f && dist <= outlineWidth)
    {
        outputTexture[pixel] = outlineColor;
    }
    else
    {
        outputTexture[pixel] = float4(0.0f, 0.0f, 0.0f, 0.0f);
    }
}
