#include "include/binding_helpers.hlsli"

struct PSInput
{
    float4 position     : SV_POSITION;
    float4 color        : COLOR;
    float2 texCoord     : TEXCOORD;
    nointerpolation uint texIndex       : TEXINDEX;
    nointerpolation uint objectID       : OBJECTID;
};

SamplerState samplerState : register(s0);

struct PSOutput
{
    float4 color : SV_TARGET0;
    uint objectID : SV_TARGET1;
};

float Median(float3 value)
{
    return max(min(value.r, value.g), min(max(value.r, value.g), value.b));
}

float ScreenPxRange(uint texIndex, float2 uv)
{
    const float pxRange = 2.0f;
    uint width = 1u;
    uint height = 1u;
    Texture2D tex = ResourceDescriptorHeap[texIndex];
    tex.GetDimensions(width, height);
    width = max(width, 1u);
    height = max(height, 1u);
    const float2 unitRange = float2(pxRange, pxRange) / float2(width, height);
    const float2 screenTexSize = 1.0f / max(fwidth(uv), float2(1e-6f, 1e-6f));
    return max(0.5f * dot(unitRange, screenTexSize), 1.0f);
}

PSOutput main(PSInput input)
{
    Texture2D tex = ResourceDescriptorHeap[input.texIndex];
    float3 msd = tex.Sample(samplerState, input.texCoord).rgb;
    float screenPxDistance = ScreenPxRange(input.texIndex, input.texCoord) * (Median(msd) - 0.5f);
    float opacity = saturate(screenPxDistance + 0.5f);

    clip(opacity - 0.0001f);

    PSOutput result;
    result.color = float4(input.color.rgb, input.color.a * opacity);
    result.objectID = input.objectID;

    return result;
}