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
    float2 uv = input.texCoord;
    float mode = input.tilingFactor.x;
    float param = input.tilingFactor.y;

    float4 finalColor = input.color;

    if (mode >= 1.5f)
    {
        if (mode < 2.5f)
        {
            // Mode 2.0: Soft rounded-box SDF drop shadow / outer glow
            float2 p = abs(uv * 2.0f - 1.0f);
            float roundness = clamp(param, 0.08f, 0.85f);
            float2 q = max(p - (1.0f - roundness), 0.0f) / max(roundness, 1e-4f);
            float dist = length(q);
            float falloff = pow(saturate(1.0f - dist), 2.1f);
            finalColor = float4(input.color.rgb, input.color.a * falloff);
        }
        else if (mode < 3.5f)
        {
            // Mode 3.0: Tactical HUD panel / button surface (gradient + top bevel + scanline + hover sheen)
            float grad = lerp(1.18f, 0.84f, saturate(uv.y));
            float topBevel = smoothstep(0.14f, 0.0f, uv.y) * 0.07f;
            float bottomRim = smoothstep(0.88f, 1.0f, uv.y) * 0.04f;
            float scanline = 0.982f + 0.018f * sin(input.position.y * 1.5707963f);

            // Subtle diagonal tactical sheen on hover/active (param = hoverAnim)
            float diag = saturate(1.0f - abs((uv.x * 0.65f + uv.y * 0.35f) - 0.42f) * 3.2f);
            float hoverSheen = param * (0.08f + 0.10f * diag);

            float3 rgb = saturate(input.color.rgb * grad * scanline + topBevel + hoverSheen - bottomRim);
            finalColor = float4(rgb, input.color.a);
        }
        else
        {
            // Mode 4.0: Tactical border with intensified corner bracket accents
            float2 cornerDist = abs(uv * 2.0f - 1.0f);
            float cornerFactor = smoothstep(0.68f, 0.96f, max(cornerDist.x, cornerDist.y));
            float3 rgb = saturate(input.color.rgb * (1.0f + 0.28f * cornerFactor + 0.22f * param));
            finalColor = float4(rgb, input.color.a);
        }
    }
    else
    {
        Texture2D tex = ResourceDescriptorHeap[NonUniformResourceIndex(SafeBindlessIndex(input.texIndex))];
        float4 texColor = tex.Sample(samplerState, uv * input.tilingFactor);
        finalColor = texColor * input.color;
        finalColor.a = (input.texIndex == 0) ? input.color.a : (texColor.a * input.color.a);
    }

    // Discard pixel if alpha is zero
    clip(finalColor.a <= 0.001f ? -1.0f : 1.0f);

    PSOutput result;
    result.color = finalColor;

    return result;
}