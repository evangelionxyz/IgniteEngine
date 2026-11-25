#include "include/helpers.hlsli"
#include "include/binding_helpers.hlsli"

struct Constants
{
    float2 invDisplaySize;
    float2 displayPos;
    float  time;
    uint   shaderFlags; // 0 = Default, 1 = Shimmer/Skeleton, 2 = Architectural Dot-Grid
    float  param0;
    float  param1;
};

DECLARE_PUSH_CONSTANTS(Constants, g_Const, 0, 0);

struct PS_INPUT
{
    float4 pos       : SV_POSITION;
    float4 col       : COLOR0;
    float2 uv        : TEXCOORD0;
    float2 screenPos : TEXCOORD1;
};

Texture2D texture0 : register(t0);
sampler sampler0 : register(s0);

float4 main(PS_INPUT input) : SV_Target
{
    float4 texColor = texture0.Sample(sampler0, input.uv);
    float4 base = input.col * texColor;

    if (g_Const.shaderFlags == 1u)
    {
        float speed = max(g_Const.param0, 0.65);
        float phase = frac((input.screenPos.x * 0.0035 + input.screenPos.y * 0.0015) - g_Const.time * speed);
        float dist = abs(phase - 0.5) * 2.0;
        float band = pow(saturate(1.0 - dist * 2.4), 2.0);
        base.rgb += band * 0.12 * base.a;
    }
    else if (g_Const.shaderFlags == 2u)
    {
        float spacing = max(g_Const.param1, 14.0);
        float2 cell = abs(frac(input.screenPos / spacing - 0.5) - 0.5) * spacing;
        float dotMask = 1.0 - smoothstep(0.65, 1.35, length(cell));
        float intensity = max(g_Const.param0, 1.0);
        base.rgb += dotMask * 0.045 * intensity * base.a;
    }

    return base;
}