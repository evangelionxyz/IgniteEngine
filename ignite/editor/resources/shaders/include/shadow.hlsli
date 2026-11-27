// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_SHADOW_HLSLI
#define IGN_SHADOW_HLSLI

#include "binding_helpers.hlsli"
#include "helpers.hlsli"

#define NUM_CASCADES 4

struct CascadesShadows
{
    float4x4 lightViewProjection[NUM_CASCADES];
    
    float4 cascadeSplits; // view-space distances (camera space z positive forward magnitude)
    
    float shadowStrength;
    float minBias;
    float maxBias;
    float pcfRadius;

    int cascadeIndex;
    float shadowTexelSize;
    int softShadows;
    float padding;
};

// ---------------------------------------------------------------------------
// 16-tap Poisson disk in unit-disk space.
// Samples are hand-tuned for good coverage without visible patterns.
// ---------------------------------------------------------------------------
static const float2 k_PoissonDisk[16] =
{
    float2(-0.94201624f, -0.39906216f),
    float2(0.94558609f, -0.76890725f),
    float2(-0.09418410f, -0.92938870f),
    float2(0.34495938f, 0.29387760f),
    float2(-0.91588581f, 0.45771432f),
    float2(-0.81544232f, -0.87912464f),
    float2(-0.38277543f, 0.27676845f),
    float2(0.97484398f, 0.75648379f),
    float2(0.44323325f, -0.97511554f),
    float2(0.53742981f, -0.47373420f),
    float2(-0.26496911f, -0.41893023f),
    float2(0.79197514f, 0.19090188f),
    float2(-0.24188840f, 0.99706507f),
    float2(-0.81409955f, 0.91437590f),
    float2(0.19984126f, 0.78641367f),
    float2(0.14383161f, -0.14100790f)
};

// Balanced, zero-mean 4-tap antipodal Poisson disk for local shadow PCF
static const float2 k_LocalShadowDisk4[4] =
{
    float2( 0.3827f,  0.9239f),
    float2(-0.3827f, -0.9239f),
    float2(-0.9239f,  0.3827f),
    float2( 0.9239f, -0.3827f)
};

static int GetCascadeIndex(float4 cascadeSplits, float viewDepth)
{
    if (viewDepth < cascadeSplits.x)
        return 0;
    if (viewDepth < cascadeSplits.y)
        return 1;
    if (viewDepth < cascadeSplits.z)
        return 2;
    return 3;
}

// ---------------------------------------------------------------------------
// Interleaved-gradient noise hash — fast, low-correlation per-pixel rotation
// angle used to decorrelate Poisson taps across neighbouring pixels, which
// removes the regular banding you get from an unrotated disk.
// Reference: Jimenez 2014, "Next-Generation Post Processing in Call of Duty"
// ---------------------------------------------------------------------------
static float InterleavedGradientNoise(float2 screenPos)
{
    float3 magic = float3(0.06711056f, 0.00583715f, 52.9829189f);
    return frac(magic.z * frac(dot(screenPos, magic.xy)));
}

static float SampleCascadeBilinearPCF(Texture2DArray shadowMap, SamplerState shadowSampler,
    float2 uv, int cascadeIdx, float compareDepth, float texelSize)
{
    float mapSize = 1.0f / texelSize;
    float2 texelCoord = uv * mapSize - 0.5f;
    float2 f = frac(texelCoord);
    // Snap gather center to the upper-left corner of the 2x2 texel quad.
    // HLSL GatherRed returns: w=(0,0), z=(1,0), x=(0,1), y=(1,1).
    float2 gatherUV = saturate((floor(texelCoord) + 1.0f) * texelSize);
    float4 depths = shadowMap.GatherRed(shadowSampler, float3(gatherUV, float(cascadeIdx)));
    float4 vis = step(compareDepth, depths);
    float top = lerp(vis.w, vis.z, f.x);
    float bot = lerp(vis.x, vis.y, f.x);
    return lerp(top, bot, f.y);
}

static float SampleCascadePCF(CascadesShadows csm, Texture2DArray shadowMap, SamplerState shadowSampler,
    int cascadeIdx, float3 worldPos, float3 normal, float3 lightDirection, float2 screenPos, bool isBlendTap = false)
{
    float4x4 lightVP = csm.lightViewProjection[cascadeIdx];
    float invHalfExtentX = max(length(lightVP[0].xyz), 1e-6f);
    float ndcPerMeter = max(length(lightVP[2].xyz), 1e-6f);
    float ts = csm.shadowTexelSize > 0.0f ? csm.shadowTexelSize : (1.0f / 2048.0f);
    float worldTexelSize = (2.0f / invHalfExtentX) * ts;

    // Slope-scaled normal offset + physical meter depth bias (capped per cascade texel)
    // eliminates shadow acne with double-sided CSM casters without 25m+ Peter-Panning cutouts.
    float cosTheta = saturate(dot(normal, lightDirection));
    float3 shadowWorldPos = worldPos + normal * (worldTexelSize * (1.25f * (1.0f - cosTheta)));

    float4 lightSpace = mul(lightVP, float4(shadowWorldPos, 1.0f));
    float3 ndc = lightSpace.xyz / lightSpace.w;
    float2 shadowUV = ndc.xy * 0.5f + 0.5f;
    float shadowDepth = ndc.z; // orthoZO outputs z in [0,1] for both D3D12 and Vulkan
    // NRI normalizes viewport coordinates to D3D12 top-left origin on both D3D12 and Vulkan,
    // so ndc.y = +1 maps to row 0 (v = 0) and ndc.y = -1 maps to row H (v = 1).
    shadowUV.y = 1.0f - shadowUV.y;

    if (shadowUV.x < 0.0f || shadowUV.x > 1.0f ||
        shadowUV.y < 0.0f || shadowUV.y > 1.0f ||
        shadowDepth < 0.0f || shadowDepth > 1.0f)
    {
        return 1.0f;
    }

    float loBias = min(csm.minBias, csm.maxBias);
    float hiBias = max(csm.minBias, csm.maxBias);
    float baseBias = lerp(hiBias, loBias, cosTheta);
    float biasMeters = clamp(baseBias * 20.0f, worldTexelSize * 0.5f, worldTexelSize * 4.0f);
    float bias = biasMeters * ndcPerMeter;
    float compareDepth = shadowDepth - bias;

    if (csm.softShadows == 0 || isBlendTap || cascadeIdx >= 2)
    {
        float visibility = SampleCascadeBilinearPCF(shadowMap, shadowSampler, shadowUV, cascadeIdx, compareDepth, ts);
        return saturate(lerp(1.0f, visibility, csm.shadowStrength));
    }

    // Do not inflate filter radius on far cascades where world-space texels are already large.
    float cascadeScale = 1.0f / (1.0f + float(cascadeIdx) * 0.35f);
    float filterRadius = clamp(csm.pcfRadius, 0.5f, 4.0f) * cascadeScale;

    // Deterministic 2x2 bilinear PCF tent (16 continuous depth comparisons, zero IGN dithering noise)
    float2 offsetStep = float2(ts, ts) * (filterRadius * 0.65f);
    float visibility = 0.0f;
    visibility += SampleCascadeBilinearPCF(shadowMap, shadowSampler, saturate(shadowUV + float2(-offsetStep.x, -offsetStep.y)), cascadeIdx, compareDepth, ts);
    visibility += SampleCascadeBilinearPCF(shadowMap, shadowSampler, saturate(shadowUV + float2( offsetStep.x, -offsetStep.y)), cascadeIdx, compareDepth, ts);
    visibility += SampleCascadeBilinearPCF(shadowMap, shadowSampler, saturate(shadowUV + float2(-offsetStep.x,  offsetStep.y)), cascadeIdx, compareDepth, ts);
    visibility += SampleCascadeBilinearPCF(shadowMap, shadowSampler, saturate(shadowUV + float2( offsetStep.x,  offsetStep.y)), cascadeIdx, compareDepth, ts);
    visibility *= 0.25f;

    return saturate(lerp(1.0f, visibility, csm.shadowStrength));
}

static float SampleShadow(CascadesShadows csm, Texture2DArray shadowMap, SamplerState shadowSampler,
    float4x4 cameraView, float3 worldPos, float3 normal, float3 lightDirection, float2 screenPos)
{
    if (csm.shadowStrength <= 0.001f)
    {
        return 1.0f;
    }

    float3 viewPos = mul(cameraView, float4(worldPos, 1.0f)).xyz;
    float viewDepth = -viewPos.z;
    int cascadeIdx = clamp(GetCascadeIndex(csm.cascadeSplits, viewDepth), 0, NUM_CASCADES - 1);

    float shadowVis = SampleCascadePCF(csm, shadowMap, shadowSampler, cascadeIdx, worldPos, normal, lightDirection, screenPos, false);
    if (cascadeIdx < NUM_CASCADES - 1)
    {
        float splitFar = (cascadeIdx == 0) ? csm.cascadeSplits.x : ((cascadeIdx == 1) ? csm.cascadeSplits.y : csm.cascadeSplits.z);
        float splitNear = (cascadeIdx == 0) ? 0.0f : ((cascadeIdx == 1) ? csm.cascadeSplits.x : csm.cascadeSplits.y);
        float blendZone = max((splitFar - splitNear) * 0.1f, 0.5f);
        float blendStart = splitFar - blendZone;
        if (viewDepth > blendStart)
        {
            float nextVis = SampleCascadePCF(csm, shadowMap, shadowSampler, cascadeIdx + 1, worldPos, normal, lightDirection, screenPos, true);
            float t = saturate((viewDepth - blendStart) / max(blendZone, 1e-4f));
            shadowVis = lerp(shadowVis, nextVis, t);
        }
    }
    return shadowVis;
}

static float SampleLocalCubeArrayTap(int cubeArrayIdx, SamplerState samp, float3 dir, int sliceIndex, float normDist, float bias)
{
    if (cubeArrayIdx < 0)
    {
        return 1.0f;
    }

    TextureCubeArray<float4> shadowCubes = ResourceDescriptorHeap[SafeBindlessIndex(cubeArrayIdx)];
    float stored = shadowCubes.SampleLevel(samp, float4(dir, float(sliceIndex)), 0.0f).r;
    return ((normDist - bias) <= stored) ? 1.0f : 0.0f;
}

static float SampleLocalPointOrSpotShadow(
    int staticCubeArrayIdx,
    int dynamicCubeArrayIdx,
    SamplerState samp,
    float4 shadowParams,
    float3 lightPos,
    float lightRange,
    float3 worldPos,
    float3 N,
    float2 screenPos)
{
    if (shadowParams.x < -0.5f || lightRange <= 1e-4f || shadowParams.w <= 0.001f)
    {
        return 1.0f;
    }
    if (staticCubeArrayIdx < 0 && dynamicCubeArrayIdx < 0)
    {
        return 1.0f;
    }

    int sliceIndex = int(shadowParams.x + 0.5f);
    float baseBias = shadowParams.y;
    float pcfRadius = shadowParams.z;
    float strength = saturate(shadowParams.w);

    float3 toWorld = worldPos - lightPos;
    float dist = length(toWorld);
    if (dist >= lightRange || dist <= 1e-4f)
    {
        return 1.0f;
    }

    float3 dir = toWorld / dist;
    float ndl = saturate(dot(N, -dir));
    float bias = baseBias * (0.35f + 1.25f * (1.0f - ndl));
    float normDist = dist / lightRange;

    const bool hasStaticCube = (staticCubeArrayIdx >= 0);
    const bool hasDynamicCube = (dynamicCubeArrayIdx >= 0);
    TextureCubeArray<float4> staticCubes = ResourceDescriptorHeap[SafeBindlessIndex(staticCubeArrayIdx)];
    TextureCubeArray<float4> dynamicCubes = ResourceDescriptorHeap[SafeBindlessIndex(dynamicCubeArrayIdx)];
    float sliceF = float(sliceIndex);

    float vis = 1.0f;
    if (pcfRadius <= 1e-4f)
    {
        float sVis = hasStaticCube ? (((normDist - bias) <= staticCubes.SampleLevel(samp, float4(dir, sliceF), 0.0f).r) ? 1.0f : 0.0f) : 1.0f;
        float dVis = (sVis > 0.5f && hasDynamicCube) ? (((normDist - bias) <= dynamicCubes.SampleLevel(samp, float4(dir, sliceF), 0.0f).r) ? 1.0f : 0.0f) : 1.0f;
        vis = min(sVis, dVis);
    }
    else
    {
        float3 T, B;
        BuildOrthonormalBasis(dir, T, B);
        float tapScale = 0.75f * pcfRadius;

        float accum = 0.0f;
        [unroll]
        for (int i = 0; i < 4; ++i)
        {
            float2 off = k_LocalShadowDisk4[i] * tapScale;
            float3 sampleDir = normalize(dir + T * off.x + B * off.y);
            float tapNormDist = normDist;
            float tapNdl = dot(N, -sampleDir);
            if (ndl > 1e-4f && tapNdl > ndl)
            {
                // Intersect sampleDir with receiver tangent plane through worldPos:
                // t_plane = dist * (N . -dir) / (N . -sampleDir).
                // Only shrink (tapNdl > ndl) so outward taps on curved/grazing surfaces never overshoot.
                tapNormDist = normDist * clamp(ndl / tapNdl, 0.2f, 1.0f);
            }
            float compareDist = tapNormDist - bias;
            float sVis = hasStaticCube ? ((compareDist <= staticCubes.SampleLevel(samp, float4(sampleDir, sliceF), 0.0f).r) ? 1.0f : 0.0f) : 1.0f;
            float dVis = (sVis > 0.5f && hasDynamicCube) ? ((compareDist <= dynamicCubes.SampleLevel(samp, float4(sampleDir, sliceF), 0.0f).r) ? 1.0f : 0.0f) : 1.0f;
            accum += min(sVis, dVis);
        }
        vis = accum * 0.25f;
    }

    return saturate(lerp(1.0f, vis, strength));
}

#endif
