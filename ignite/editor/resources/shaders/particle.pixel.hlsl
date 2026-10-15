// Copyright (c) 2026 Evangelion Manuhutu

#include "include/binding_helpers.hlsli"

struct ParticleRenderCB
{
    float4x4 invView;
    float2 viewportSize;
    uint useTexture;
    uint objectID;
};

cbuffer ParticleParams : register(b1)
{
    ParticleRenderCB renderParams;
};

Texture2D g_Texture : register(t1);
SamplerState g_Sampler : register(s0);

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
    nointerpolation uint emitterType : TYPE0;
    nointerpolation uint objectID : OBJECTID;
};

struct PSOutput
{
    float4 color : SV_TARGET0;
    uint objectID : SV_TARGET1;
};

PSOutput main(VSOutput input)
{
    float2 centered = input.uv * 2.0 - 1.0;
    float dist = length(centered);

    float4 outColor = input.color;

    if (renderParams.useTexture != 0)
    {
        float4 texColor = g_Texture.Sample(g_Sampler, input.uv);
        outColor *= texColor;
    }
    else
    {
        // High quality procedural particle shapes based on emitter type
        if (input.emitterType == 0) // Smoke: soft puff with natural density falloff
        {
            float alpha = smoothstep(1.0, 0.0, dist);
            alpha = pow(alpha, 1.8);
            outColor.a *= alpha;
        }
        else if (input.emitterType == 1) // Fire: burning hot core fading outwards
        {
            float core = smoothstep(1.0, 0.0, dist);
            float inner = smoothstep(0.5, 0.0, dist);
            outColor.rgb = lerp(outColor.rgb, float3(1.0, 1.0, 0.7), inner * 0.7);
            outColor.a *= core;
        }
        else if (input.emitterType == 2) // Spark: sharp bright needle / streak
        {
            float sparkX = saturate(1.0 - abs(centered.x));
            float sparkY = saturate(1.0 - abs(centered.y));
            float sparkShape = pow(sparkX, 1.5) * pow(sparkY, 0.7);
            float core = pow(sparkX, 4.0) * sparkY;
            outColor.rgb += float3(0.8, 0.8, 0.8) * core * 2.0;
            outColor.a *= sparkShape;
        }
        else // Custom / Generic particle: soft radial circle
        {
            float alpha = smoothstep(1.0, 0.0, dist);
            outColor.a *= alpha;
        }
    }

    if (outColor.a < 0.001)
        discard;

    PSOutput pixel;
    pixel.color = outColor;
    pixel.objectID = input.objectID;

    return pixel;
}
