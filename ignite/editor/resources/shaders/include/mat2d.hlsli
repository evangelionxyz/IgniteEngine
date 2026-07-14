// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_MAT2D_HLSLI
#define IGN_MAT2D_HLSLI

enum Material2DType
{
    MATERIAL_2D_UNLIT = 0,
    MATERIAL_2D_LIT = 1,
};

struct Material2D
{
    float4 baseColor     : COLOR0;
    float4 additiveColor : COLOR1;
    float2 tiling        : TEXCOORD0;
    uint materialType    : MATTYPE0;
};

struct PointLight2D
{
    float4 position : POSITION0;
    float4 color    : COLOR0;
    float radius    : RADIUS0;
    float intensity : INTENSITY0;
};

struct Material2DLighting
{
    uint pointLightCount;
    float3 _padding;
    PointLight2D pointLights[32];
};

static float4 ComputeMaterial2DUnlit(Material2D material, float4 textureColor)
{
    return material.baseColor * textureColor + material.additiveColor;
}

static float4 ComputeMaterial2DLit(Material2D material, Material2DLighting lighting, float3 worldPosition, float4 textureColor)
{
    float4 base = material.baseColor * textureColor + material.additiveColor;
    float3 lightingAccum = float3(0.15f, 0.15f, 0.15f);

    [loop]
    for (uint i = 0; i < lighting.pointLightCount; ++i)
    {
        PointLight2D light = lighting.pointLights[i];
        float2 toLight = light.position.xy - worldPosition.xy;
        float distanceToLight = length(toLight);

        if (distanceToLight < light.radius)
        {
            float falloff = saturate(1.0f - (distanceToLight / max(light.radius, 0.0001f)));
            float attenuation = falloff * falloff;
            lightingAccum += light.color.rgb * (light.intensity * attenuation);
        }
    }

    return float4(base.rgb * lightingAccum, base.a);
}

#endif
