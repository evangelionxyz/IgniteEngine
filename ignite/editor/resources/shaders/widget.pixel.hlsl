#include "include/binding_helpers.hlsli"

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPosition : WORLDPOS;
    float2 texCoord : TEXCOORD;
    float2 tilingFactor : TILINGFACTOR;
    float4 color : COLOR;
    nointerpolation uint texIndex : TEXINDEX;
};

SamplerState samplerState : register(s0);

struct PSOutput
{
    float4 color : SV_TARGET0;
};

PSOutput main(PSInput input)
{
    Texture2D tex = ResourceDescriptorHeap[input.texIndex];
    float4 texColor = tex.Sample(samplerState, input.texCoord * input.tilingFactor);
    float4 finalColor = texColor * input.color;
    finalColor.a = (input.texIndex == 0) ? input.color.a : (texColor.a * input.color.a);
    
    // Discard pixel if alpha is zero
    clip(finalColor.a == 0.0f ? -1.0f : 1.0f);
    
    PSOutput result;
    result.color = finalColor;

    return result;
}