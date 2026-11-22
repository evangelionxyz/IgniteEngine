// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_REFLECTION_PROBES_HLSLI
#define IGN_REFLECTION_PROBES_HLSLI

#include "binding_helpers.hlsli"
#include "scene.hlsli"

// Lagarde specular occlusion from ambient occlusion, NdotV, and surface roughness
static float ComputeSpecularOcclusion(float NdotV, float ao, float roughness)
{
    float a = max(0.0f, saturate(NdotV) + saturate(ao));
    float exponent = exp2(-16.0f * saturate(roughness) - 1.0f);
    return saturate(pow(a, exponent) - 1.0f + saturate(ao));
}

static float ComputeProbeInfluenceWeight(float3 worldPos, ReflectionProbeGPU probe)
{
    float3 localP = mul(probe.worldToLocal, float4(worldPos, 1.0f)).xyz;
    float3 infExt = probe.influenceMaxAndInt.xyz;
    float3 dist = infExt - abs(localP);
    float minD = min(min(dist.x, dist.y), dist.z);
    if (minD <= 0.0f)
    {
        return 0.0f;
    }

    float blend = probe.boxMaxAndBlend.w;
    if (blend <= 1e-5f)
    {
        return 1.0f;
    }

    float t = saturate(minD / blend);
    return t * t * (3.0f - 2.0f * t);
}

static float3 BoxProjectOBB(float3 worldPos, float3 R, ReflectionProbeGPU probe)
{
    float3 unitR = normalize(R);
    float3 localP = mul(probe.worldToLocal, float4(worldPos, 1.0f)).xyz;
    float3 localR = mul((float3x3)probe.worldToLocal, unitR);

    float3 safeR = float3(
        abs(localR.x) > 1e-6f ? localR.x : (localR.x >= 0.0f ? 1e-6f : -1e-6f),
        abs(localR.y) > 1e-6f ? localR.y : (localR.y >= 0.0f ? 1e-6f : -1e-6f),
        abs(localR.z) > 1e-6f ? localR.z : (localR.z >= 0.0f ? 1e-6f : -1e-6f));

    float3 boxMin = probe.boxMinAndSlice.xyz;
    float3 boxMax = probe.boxMaxAndBlend.xyz;

    float3 firstPlane = (boxMax - localP) / safeR;
    float3 secondPlane = (boxMin - localP) / safeR;
    float3 furthestPlane = max(firstPlane, secondPlane);
    float t = min(min(furthestPlane.x, furthestPlane.y), furthestPlane.z);
    if (t <= 1e-5f)
    {
        return unitR;
    }

    float3 localHit = localP + localR * t;
    float3 worldHit = mul(probe.localToWorld, float4(localHit, 1.0f)).xyz;
    float3 dir = worldHit - probe.capturePosAndNorm.xyz;
    float lenSq = dot(dir, dir);
    return (lenSq > 1e-8f) ? (dir * rsqrt(lenSq)) : unitR;
}

static float3 EvalProbeCaptureIrradiance(ReflectionProbeGPU probe, float3 N)
{
    float3 unitN = normalize(N);
    float b0 = 0.28209479f;
    float b1 = 0.48860251f * unitN.y;
    float b2 = 0.48860251f * unitN.z;
    float b3 = 0.48860251f * unitN.x;
    return max(
        probe.captureSH[0].xyz * b0 +
        probe.captureSH[1].xyz * b1 +
        probe.captureSH[2].xyz * b2 +
        probe.captureSH[3].xyz * b3,
        float3(0.0f, 0.0f, 0.0f));
}

static float ComputeProbeNormalizationFactor(float3 pixelDiffuseGI, float3 probeCaptureGI, float strength)
{
    const float3 lumWeights = float3(0.2126f, 0.7152f, 0.0722f);
    float lumPixel = dot(max(pixelDiffuseGI, float3(0.0f, 0.0f, 0.0f)), lumWeights);
    float lumProbe = dot(max(probeCaptureGI, float3(0.0f, 0.0f, 0.0f)), lumWeights);
    float ratio = (lumProbe > 1e-4f) ? clamp(lumPixel / lumProbe, 0.0f, 2.0f) : 1.0f;
    return lerp(1.0f, ratio, saturate(strength));
}

static void EvaluateSingleReflectionProbe(
    ReflectionProbeGPU probe,
    TextureCubeArray probeCubeArray,
    SamplerState linSampler,
    float maxMip,
    float3 worldPos,
    float3 N,
    float3 R,
    float baseRoughness,
    float coatRoughness,
    float3 pixelDiffuseGI,
    out float3 outBaseSpec,
    out float3 outCoatSpec)
{
    bool doBoxProj = (probe.captureSH[0].w > 0.5f);
    float3 lookupDir = doBoxProj ? BoxProjectOBB(worldPos, R, probe) : normalize(R);
    float slice = probe.boxMinAndSlice.w;

    float baseMip = saturate(baseRoughness) * max(maxMip, 0.0f);
    float coatMip = saturate(coatRoughness) * max(maxMip, 0.0f);

    float3 probeIrradiance = EvalProbeCaptureIrradiance(probe, N);
    float normFactor = ComputeProbeNormalizationFactor(pixelDiffuseGI, probeIrradiance, probe.capturePosAndNorm.w);
    float scale = normFactor * max(probe.influenceMaxAndInt.w, 0.0f);

    outBaseSpec = probeCubeArray.SampleLevel(linSampler, float4(lookupDir, slice), baseMip).rgb * scale;
    outCoatSpec = probeCubeArray.SampleLevel(linSampler, float4(lookupDir, slice), coatMip).rgb * scale;
}

static void EvaluateReflectionProbes(
    float3 worldPos,
    float3 N,
    float3 R,
    float baseRoughness,
    float coatRoughness,
    float3 pixelDiffuseGI,
    GIData gi,
    StructuredBuffer<ReflectionProbeGPU> probes,
    SamplerState linSampler,
    out float3 outBaseSpec,
    out float3 outCoatSpec,
    out float outProbeWeight)
{
    outBaseSpec = float3(0.0f, 0.0f, 0.0f);
    outCoatSpec = float3(0.0f, 0.0f, 0.0f);
    outProbeWeight = 0.0f;

    int count = min(gi.reflectionProbeCount, MAX_REFLECTION_PROBES);
    if (count <= 0 || gi.reflectionCubeArrayIndex < 0)
    {
        return;
    }

    int bestIdx0 = -1;
    int bestPrio0 = -2147483647;
    float bestWeight0 = 0.0f;

    int bestIdx1 = -1;
    int bestPrio1 = -2147483647;
    float bestWeight1 = 0.0f;

    for (int i = 0; i < count; ++i)
    {
        float w = ComputeProbeInfluenceWeight(worldPos, probes[i]);
        if (w <= 1e-5f)
        {
            continue;
        }

        int prio = int(round(probes[i].influenceMinAndPrio.w));
        if (bestIdx0 < 0 || prio > bestPrio0 || (prio == bestPrio0 && w > bestWeight0))
        {
            bestIdx1 = bestIdx0;
            bestPrio1 = bestPrio0;
            bestWeight1 = bestWeight0;

            bestIdx0 = i;
            bestPrio0 = prio;
            bestWeight0 = w;
        }
        else if (bestIdx1 < 0 || prio > bestPrio1 || (prio == bestPrio1 && w > bestWeight1))
        {
            bestIdx1 = i;
            bestPrio1 = prio;
            bestWeight1 = w;
        }
    }

    if (bestIdx0 < 0)
    {
        return;
    }

    TextureCubeArray probeCubeArray = ResourceDescriptorHeap[SafeBindlessIndex(gi.reflectionCubeArrayIndex)];
    float maxMip = float(max(gi.reflectionMipCount - 1, 0));

    float3 b0, c0;
    EvaluateSingleReflectionProbe(probes[bestIdx0], probeCubeArray, linSampler, maxMip,
        worldPos, N, R, baseRoughness, coatRoughness, pixelDiffuseGI, b0, c0);

    if (bestIdx1 < 0)
    {
        outBaseSpec = b0;
        outCoatSpec = c0;
        outProbeWeight = saturate(bestWeight0);
        return;
    }

    float3 b1, c1;
    EvaluateSingleReflectionProbe(probes[bestIdx1], probeCubeArray, linSampler, maxMip,
        worldPos, N, R, baseRoughness, coatRoughness, pixelDiffuseGI, b1, c1);

    float w0 = bestWeight0;
    float w1 = (bestPrio0 > bestPrio1) ? ((1.0f - w0) * bestWeight1) : bestWeight1;
    float totalW = w0 + w1;
    if (totalW > 1.0f)
    {
        w0 /= totalW;
        w1 /= totalW;
        totalW = 1.0f;
    }

    float invTotal = 1.0f / max(totalW, 1e-4f);
    outBaseSpec = b0 * (w0 * invTotal) + b1 * (w1 * invTotal);
    outCoatSpec = c0 * (w0 * invTotal) + c1 * (w1 * invTotal);
    outProbeWeight = saturate(totalW);
}

#endif

