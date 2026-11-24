// Copyright (c) 2026 Evangelion Manuhutu

#include "include/sh.hlsli"

struct GizmoPointLight
{
    float4 positionAndRange;
    float4 color;
};

struct GizmoSpotLight
{
    float4 positionAndRange;
    float4 directionAndCosOuter;
    float4 color;
    float4 cosInnerAndPad;
};

struct GizmoFrameConstants
{
    float4x4 viewProjection;
    float4 cameraPosAndIsOrtho;
    float4 cameraForwardAndNear;
    float4 viewportAndDepthSize;
    float4 occlusionParams;
    float4 sunDirection;
    float4 sunColor;
    float4 lightingCounts;
    float4 skySH[9];
    GizmoPointLight pointLights[8];
    GizmoSpotLight spotLights[4];
};

cbuffer GizmoFrameBuffer : register(b1, space0) { GizmoFrameConstants u_Frame; }
Texture2D t_SceneDepth : register(t1, space0);

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPos : WORLDPOS;
    float3 worldNormal : NORMAL;
    float4 color : COLOR;
    float4 ringCenterAndFlag : TEXCOORD0;
    float hoverFactor : TEXCOORD1;
    nointerpolation uint shadingMode : TEXCOORD2;
};

static float3 LinearToSRGB(float3 c)
{
    c = max(c, 0.0f);
    float3 lo = c * 12.92f;
    float3 hi = 1.055f * pow(c, 1.0f / 2.4f) - 0.055f;
    return lerp(lo, hi, step(0.0031308f, c));
}

static float ComputeSpecularGGX(float3 N, float3 V, float3 L, float roughness)
{
    float3 H = normalize(V + L);
    float NdotH = saturate(dot(N, H));
    float NdotL = saturate(dot(N, L));
    float NdotV = max(saturate(dot(N, V)), 0.05f);
    float a = roughness * roughness;
    float a2 = a * a;
    float denom = NdotH * NdotH * (a2 - 1.0f) + 1.0f;
    float D = a2 / max(3.14159265f * denom * denom, 1e-4f);
    float F = 0.04f + 0.96f * pow(1.0f - saturate(dot(H, V)), 5.0f);
    float k = (roughness + 1.0f);
    k = (k * k) * 0.125f;
    float G = (NdotV / (NdotV * (1.0f - k) + k)) * (NdotL / (NdotL * (1.0f - k) + k));
    return (D * F * G) * NdotL;
}

float4 main(PSInput input) : SV_TARGET
{
    float3 N = normalize(input.worldNormal);
    float3 V = (u_Frame.cameraPosAndIsOrtho.w > 0.5f)
        ? normalize(-u_Frame.cameraForwardAndNear.xyz)
        : normalize(u_Frame.cameraPosAndIsOrtho.xyz - input.worldPos);

    float3 albedo = input.color.rgb;
    float alpha = saturate(input.color.a);

    // Unreal-style ring back-hemisphere horizon attenuation
    if (input.shadingMode == 2u && input.ringCenterAndFlag.w > 0.5f)
    {
        float3 ringDir = normalize(input.worldPos - input.ringCenterAndFlag.xyz);
        float horizon = dot(ringDir, V);
        float backFade = smoothstep(-0.22f, 0.12f, horizon);
        alpha *= lerp(0.14f, 1.0f, backFade);
        if (alpha < 0.02f)
            discard;
    }

    float3 litColor = albedo;

    if (input.shadingMode != 1u)
    {
        // Hybrid studio + world lighting so gizmos are always crisp yet respond to scene lighting
        float3 studioKeyDir = normalize(V * 0.65f + float3(0.32f, 0.72f, 0.28f));
        float3 studioFillDir = normalize(-V * 0.25f + float3(-0.45f, 0.35f, -0.35f));

        float studioKey = saturate(dot(N, studioKeyDir)) * 0.42f;
        float studioFill = saturate(dot(N, studioFillDir)) * 0.18f;
        float studioHemi = lerp(0.28f, 0.44f, N.y * 0.5f + 0.5f);
        float3 studioDiffuse = float3(studioHemi + studioKey + studioFill, studioHemi + studioKey + studioFill, studioHemi + studioKey + studioFill);
        float studioSpec = ComputeSpecularGGX(N, V, studioKeyDir, 0.30f) * 0.35f;

        // World directional sun
        float3 worldDiffuse = float3(0.0f, 0.0f, 0.0f);
        float3 worldSpec = float3(0.0f, 0.0f, 0.0f);
        if (u_Frame.sunColor.w > 0.0f)
        {
            float3 L = normalize(u_Frame.sunDirection.xyz);
            float NdotL = saturate(dot(N, L));
            float3 sunIrradiance = u_Frame.sunColor.rgb * min(u_Frame.sunColor.w, 6.0f);
            worldDiffuse += sunIrradiance * (NdotL * 0.48f);
            worldSpec += sunIrradiance * (ComputeSpecularGGX(N, V, L, 0.28f) * 0.45f);
        }

        // World sky spherical harmonics & scene ambient
        if (u_Frame.lightingCounts.w > 0.5f)
        {
            float3 skyIrr = max(EvalSHIrradiance4(u_Frame.skySH, N), float3(0.0f, 0.0f, 0.0f));
            worldDiffuse += min(skyIrr, float3(1.2f, 1.2f, 1.2f)) * 0.28f;
        }
        worldDiffuse += float3(1.0f, 1.0f, 1.0f) * (saturate(u_Frame.lightingCounts.x) * 0.22f);

        // World Point Lights (up to 8)
        int numPoints = (int)u_Frame.lightingCounts.y;
        for (int pi = 0; pi < numPoints && pi < 8; ++pi)
        {
            float3 toLight = u_Frame.pointLights[pi].positionAndRange.xyz - input.worldPos;
            float dist = length(toLight);
            float range = max(u_Frame.pointLights[pi].positionAndRange.w, 0.001f);
            if (dist < range)
            {
                float3 L = toLight / max(dist, 1e-4f);
                float falloff = pow(saturate(1.0f - (dist / range)), 2.0f);
                float NdotL = saturate(dot(N, L));
                float3 lightIrr = u_Frame.pointLights[pi].color.rgb * min(u_Frame.pointLights[pi].color.w, 12.0f) * falloff;
                worldDiffuse += lightIrr * (NdotL * 0.55f);
                worldSpec += lightIrr * (ComputeSpecularGGX(N, V, L, 0.26f) * 0.55f);
            }
        }

        // World Spot Lights (up to 4)
        int numSpots = (int)u_Frame.lightingCounts.z;
        for (int si = 0; si < numSpots && si < 4; ++si)
        {
            float3 toLight = u_Frame.spotLights[si].positionAndRange.xyz - input.worldPos;
            float dist = length(toLight);
            float range = max(u_Frame.spotLights[si].positionAndRange.w, 0.001f);
            if (dist < range)
            {
                float3 L = toLight / max(dist, 1e-4f);
                float cosAngle = dot(-L, normalize(u_Frame.spotLights[si].directionAndCosOuter.xyz));
                float cosOuter = u_Frame.spotLights[si].directionAndCosOuter.w;
                float cosInner = u_Frame.spotLights[si].cosInnerAndPad.x;
                float spotAtten = saturate((cosAngle - cosOuter) / max(cosInner - cosOuter, 1e-4f));
                float falloff = pow(saturate(1.0f - (dist / range)), 2.0f) * spotAtten;
                float NdotL = saturate(dot(N, L));
                float3 lightIrr = u_Frame.spotLights[si].color.rgb * min(u_Frame.spotLights[si].color.w, 12.0f) * falloff;
                worldDiffuse += lightIrr * (NdotL * 0.55f);
                worldSpec += lightIrr * (ComputeSpecularGGX(N, V, L, 0.26f) * 0.55f);
            }
        }

        // Softly compress world light contribution so axis colors remain distinct under extreme HDR lights
        float3 compressedWorld = worldDiffuse / (1.0f + 0.35f * worldDiffuse);
        float3 compressedSpec = worldSpec / (1.0f + 0.35f * worldSpec);
        float fresnelRim = pow(1.0f - saturate(dot(N, V)), 3.0f) * (0.16f + 0.14f * input.hoverFactor);

        litColor = albedo * (studioDiffuse + compressedWorld) + (studioSpec + compressedSpec) + fresnelRim * lerp(albedo, float3(1.0f, 0.94f, 0.72f), 0.5f);
    }
    else
    {
        // Translucent planar/wedge handles subtly respond to world luminance
        float worldBoost = 1.0f + saturate(u_Frame.sunColor.w * 0.12f);
        litColor = albedo * worldBoost;
    }

    // Hover / Active highlight glow
    if (input.hoverFactor > 0.01f)
    {
        litColor = lerp(litColor, litColor * 1.28f + float3(0.14f, 0.11f, 0.02f), saturate(input.hoverFactor));
    }

    // World Depth Occlusion Dimming (Unreal Engine style: dim + translucent behind world geometry)
    float2 safeViewport = max(u_Frame.viewportAndDepthSize.xy, float2(1.0f, 1.0f));
    float2 uv = saturate(input.position.xy / safeViewport);
    int2 depthSize = max(int2(u_Frame.viewportAndDepthSize.zw), int2(1, 1));
    int2 depthPx = clamp(int2(uv * float2(depthSize)), int2(0, 0), depthSize - int2(1, 1));
    float sceneDepth = t_SceneDepth.Load(int3(depthPx, 0)).r;

    float depthDelta = input.position.z - sceneDepth;
    if (depthDelta > u_Frame.occlusionParams.z)
    {
        float luma = dot(litColor, float3(0.299f, 0.587f, 0.114f));
        litColor = lerp(float3(luma, luma, luma), litColor, 0.78f) * u_Frame.occlusionParams.x;
        alpha *= u_Frame.occlusionParams.y;

        // Subtle intersection contact rim where gizmo passes behind foreground geometry
        float contactHalo = exp(-depthDelta * 900.0f) * 0.16f;
        litColor += albedo * contactHalo;
    }

    float3 srgb = LinearToSRGB(litColor);
    return float4(srgb * alpha, alpha);
}

