// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_GI_HLSLI
#define IGN_GI_HLSLI

#include "binding_helpers.hlsli"
#include "scene.hlsli"
#include "sh.hlsli"

static float ComputeGIVolumeEdgeWeight(float3 worldPos, GIData gi)
{
    if (gi.biasAndFade.w < 0.5f || gi.giMode <= 0 || gi.shTextureIndices0.x < 0)
    {
        return 0.0f;
    }

    float3 gridMin = gi.gridMinAndIntensity.xyz;
    float3 gridMax = gridMin + max(gi.gridSizeAndSpacing.xyz, float3(1e-4f, 1e-4f, 1e-4f));
    float3 dMin = worldPos - gridMin;
    float3 dMax = gridMax - worldPos;
    float minEdge = min(min(dMin.x, dMin.y), dMin.z);
    minEdge = min(minEdge, min(min(dMax.x, dMax.y), dMax.z));
    if (minEdge < 0.0f)
    {
        return 0.0f;
    }
    float fadeDist = max(gi.biasAndFade.z, 1e-4f);
    return saturate(minEdge / fadeDist);
}

static float EvalSkyVisTexel(float4 raw, float3 unitN)
{
    // Legacy .igi files store (vis, vis, vis, 1.0); directional L1 packs (vis, dirX, dirY, dirZ) with |dirZ| <= 0.5.
    if (raw.a > 0.75f)
    {
        return saturate(raw.r);
    }
    return saturate(raw.r + dot(raw.gba, unitN));
}

static float3 SampleIrradianceVolumeFast(float3 worldPos, float3 N, GIData gi, SamplerState linSampler, out float outVolW, out float outSkyVis)
{
    outVolW = ComputeGIVolumeEdgeWeight(worldPos, gi);
    outSkyVis = 1.0f;
    if (outVolW <= 0.0f)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    float3 gridMin = gi.gridMinAndIntensity.xyz;
    float spacing = max(gi.gridSizeAndSpacing.w, 1e-4f);
    int3 dims = max(gi.gridDimsAndOrder.xyz, int3(1, 1, 1));
    bool isL1 = (gi.gridDimsAndOrder.w == 1);
    float3 unitN = normalize(N);

    float3 biasedPos = worldPos + unitN * (gi.biasAndFade.x * spacing);
    float3 gridCoord = clamp((biasedPos - gridMin) / spacing, float3(0.0f, 0.0f, 0.0f), float3(dims) - 1.0f);
    int3 baseCoord = min(int3(floor(gridCoord)), max(dims - 2, int3(0, 0, 0)));
    float3 alpha = saturate(gridCoord - float3(baseCoord));

    Texture3D<float4> shTex0 = ResourceDescriptorHeap[SafeBindlessIndex(gi.shTextureIndices0.x)];
    Texture3D<float4> shTex1 = ResourceDescriptorHeap[SafeBindlessIndex(gi.shTextureIndices0.y)];
    Texture3D<float4> shTex2 = ResourceDescriptorHeap[SafeBindlessIndex(gi.shTextureIndices0.z)];
    Texture3D<float4> shTex3 = ResourceDescriptorHeap[SafeBindlessIndex(gi.shTextureIndices0.w)];
    Texture3D<float4> shTex4 = ResourceDescriptorHeap[SafeBindlessIndex(gi.shTextureIndices1.x)];
    Texture3D<float4> shTex5 = ResourceDescriptorHeap[SafeBindlessIndex(gi.shTextureIndices1.y)];
    Texture3D<float4> shTex6 = ResourceDescriptorHeap[SafeBindlessIndex(gi.shTextureIndices1.z)];

    bool hasOffsets = (gi.shTextureIndices1.w >= 0);
    bool hasSkyVis = (gi.skyVisibilitySplit != 0 && gi.skyVisibilityTexIndex >= 0);
    Texture3D<float4> offsetTex = ResourceDescriptorHeap[SafeBindlessIndex(gi.shTextureIndices1.w)];
    Texture3D<float4> skyVisTex = ResourceDescriptorHeap[SafeBindlessIndex(gi.skyVisibilityTexIndex)];

    float3 accumSH[9];
    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        accumSH[i] = float3(0.0f, 0.0f, 0.0f);
    }
    float accumSkyVis = 0.0f;
    float accumW = 0.0f;

    [unroll]
    for (int corner = 0; corner < 8; ++corner)
    {
        int3 offset = int3(corner & 1, (corner >> 1) & 1, (corner >> 2) & 1);
        int3 probeCoord = min(baseCoord + offset, dims - 1);

        float3 trilinear = float3(
            offset.x != 0 ? alpha.x : (1.0f - alpha.x),
            offset.y != 0 ? alpha.y : (1.0f - alpha.y),
            offset.z != 0 ? alpha.z : (1.0f - alpha.z));
        float w = trilinear.x * trilinear.y * trilinear.z;
        if (w <= 0.01f)
        {
            continue;
        }

        int4 loadPos = int4(probeCoord, 0);
        float3 probePos = gridMin + float3(probeCoord) * spacing;
        float validity = 1.0f;
        float4 t6 = float4(0.0f, 0.0f, 0.0f, 1.0f);
        if (hasOffsets)
        {
            float4 offData = offsetTex.Load(loadPos);
            if (offData.w < 0.5f)
            {
                continue;
            }
            probePos += offData.xyz;
        }
        else
        {
            t6 = shTex6.Load(loadPos);
            validity = t6.w;
            if (validity < 0.5f)
            {
                continue;
            }
        }

        float3 diffVec = biasedPos - probePos;
        float d = length(diffVec);
        float3 toProbe = (d > 1e-5f) ? (-diffVec / d) : unitN;
        float wrap = (dot(toProbe, unitN) + 1.0f) * 0.5f;
        w *= (0.2f + 0.8f * wrap);

        if (w <= 0.01f)
        {
            continue;
        }

        float4 t0 = shTex0.Load(loadPos);
        float4 t1 = shTex1.Load(loadPos);
        float4 t2 = shTex2.Load(loadPos);

        if (isL1)
        {
            float3 probeSH1[4];
            UnpackSHL1(t0, t1, t2, probeSH1);
            accumSH[0] += probeSH1[0] * w;
            accumSH[1] += probeSH1[1] * w;
            accumSH[2] += probeSH1[2] * w;
            accumSH[3] += probeSH1[3] * w;
        }
        else
        {
            float4 t3 = shTex3.Load(loadPos);
            float4 t4 = shTex4.Load(loadPos);
            float4 t5 = shTex5.Load(loadPos);
            if (hasOffsets)
            {
                t6 = shTex6.Load(loadPos);
            }
            float3 probeSH[9];
            UnpackSHL2(t0, t1, t2, t3, t4, t5, t6, probeSH, validity);
            [unroll]
            for (int c = 0; c < 9; ++c)
            {
                accumSH[c] += probeSH[c] * w;
            }
        }

        if (hasSkyVis)
        {
            accumSkyVis += EvalSkyVisTexel(skyVisTex.Load(loadPos), unitN) * w;
        }
        accumW += w;
    }

    if (accumW > 1e-4f)
    {
        float invW = 1.0f / accumW;
        if (hasSkyVis)
        {
            outSkyVis = saturate(accumSkyVis * invW);
        }
        if (isL1)
        {
            float3 sh1[4] = { accumSH[0] * invW, accumSH[1] * invW, accumSH[2] * invW, accumSH[3] * invW };
            return EvalSHIrradianceL1(sh1, unitN);
        }
        [unroll]
        for (int c = 0; c < 9; ++c)
        {
            accumSH[c] *= invW;
        }
        return EvalSHIrradiance(accumSH, unitN);
    }

    float3 dimsF = float3(dims);
    float3 uvw = (gridCoord + 0.5f) / dimsF;

    if (hasSkyVis)
    {
        Texture3D<float4> skyVisTex = ResourceDescriptorHeap[SafeBindlessIndex(gi.skyVisibilityTexIndex)];
        outSkyVis = EvalSkyVisTexel(skyVisTex.SampleLevel(linSampler, uvw, 0.0f), unitN);
    }

    float4 t0 = shTex0.SampleLevel(linSampler, uvw, 0.0f);
    float4 t1 = shTex1.SampleLevel(linSampler, uvw, 0.0f);
    float4 t2 = shTex2.SampleLevel(linSampler, uvw, 0.0f);

    if (isL1)
    {
        float3 sh1[4];
        UnpackSHL1(t0, t1, t2, sh1);
        return EvalSHIrradianceL1(sh1, unitN);
    }

    float4 t3 = shTex3.SampleLevel(linSampler, uvw, 0.0f);
    float4 t4 = shTex4.SampleLevel(linSampler, uvw, 0.0f);
    float4 t5 = shTex5.SampleLevel(linSampler, uvw, 0.0f);
    float4 t6 = shTex6.SampleLevel(linSampler, uvw, 0.0f);

    float3 sh[9];
    float validity;
    UnpackSHL2(t0, t1, t2, t3, t4, t5, t6, sh, validity);
    return EvalSHIrradiance(sh, unitN);
}

static float3 SampleIrradianceVolumeLeakResistant(float3 worldPos, float3 N, float3 V, GIData gi, SamplerState linSampler, out float outVolW, out float outSkyVis)
{
    outVolW = ComputeGIVolumeEdgeWeight(worldPos, gi);
    outSkyVis = 1.0f;
    if (outVolW <= 0.0f)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    float3 gridMin = gi.gridMinAndIntensity.xyz;
    float spacing = max(gi.gridSizeAndSpacing.w, 1e-4f);
    int3 dims = max(gi.gridDimsAndOrder.xyz, int3(1, 1, 1));
    bool isL1 = (gi.gridDimsAndOrder.w == 1);
    float3 unitN = normalize(N);

    float3 Pb = worldPos + (unitN * gi.biasAndFade.x + V * gi.biasAndFade.y) * spacing;
    float3 gridCoord = clamp((Pb - gridMin) / spacing, float3(0.0f, 0.0f, 0.0f), float3(dims) - 1.0f);
    int3 baseCoord = min(int3(floor(gridCoord)), max(dims - 2, int3(0, 0, 0)));
    float3 alpha = saturate(gridCoord - float3(baseCoord));

    Texture3D<float4> shTex0 = ResourceDescriptorHeap[SafeBindlessIndex(gi.shTextureIndices0.x)];
    Texture3D<float4> shTex1 = ResourceDescriptorHeap[SafeBindlessIndex(gi.shTextureIndices0.y)];
    Texture3D<float4> shTex2 = ResourceDescriptorHeap[SafeBindlessIndex(gi.shTextureIndices0.z)];
    Texture3D<float4> shTex3 = ResourceDescriptorHeap[SafeBindlessIndex(gi.shTextureIndices0.w)];
    Texture3D<float4> shTex4 = ResourceDescriptorHeap[SafeBindlessIndex(gi.shTextureIndices1.x)];
    Texture3D<float4> shTex5 = ResourceDescriptorHeap[SafeBindlessIndex(gi.shTextureIndices1.y)];
    Texture3D<float4> shTex6 = ResourceDescriptorHeap[SafeBindlessIndex(gi.shTextureIndices1.z)];

    bool hasOffsets = (gi.shTextureIndices1.w >= 0);
    bool hasVisAtlas = (gi.visibilityParams.x >= 0 && gi.visibilityParams.z > 0 && gi.visibilityParams.w > 0);
    bool hasSkyVis = (gi.skyVisibilitySplit != 0 && gi.skyVisibilityTexIndex >= 0);
    Texture3D<float4> offsetTex = ResourceDescriptorHeap[SafeBindlessIndex(gi.shTextureIndices1.w)];
    Texture2D<float4> visAtlas = ResourceDescriptorHeap[SafeBindlessIndex(gi.visibilityParams.x)];
    Texture3D<float4> skyVisTex = ResourceDescriptorHeap[SafeBindlessIndex(gi.skyVisibilityTexIndex)];
    int octRes = max(gi.visibilityParams.y, 1);
    int visCols = gi.visibilityParams.z;
    int visRows = gi.visibilityParams.w;
    float tileStride = float(octRes + 2);
    float2 visAtlasSize = float2(float(visCols), float(visRows)) * tileStride;

    float3 accumSH[9];
    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        accumSH[i] = float3(0.0f, 0.0f, 0.0f);
    }
    float accumSkyVis = 0.0f;
    float accumW = 0.0f;

    [unroll]
    for (int corner = 0; corner < 8; ++corner)
    {
        int3 offset = int3(corner & 1, (corner >> 1) & 1, (corner >> 2) & 1);
        int3 probeCoord = min(baseCoord + offset, dims - 1);

        float3 trilinear = float3(
            offset.x != 0 ? alpha.x : (1.0f - alpha.x),
            offset.y != 0 ? alpha.y : (1.0f - alpha.y),
            offset.z != 0 ? alpha.z : (1.0f - alpha.z));
        float w = trilinear.x * trilinear.y * trilinear.z;
        if (w <= 0.01f)
        {
            continue;
        }

        int4 loadPos = int4(probeCoord, 0);
        float3 probePos = gridMin + float3(probeCoord) * spacing;
        float validity = 1.0f;
        float4 t6 = float4(0.0f, 0.0f, 0.0f, 1.0f);
        if (hasOffsets)
        {
            float4 offData = offsetTex.Load(loadPos);
            if (offData.w < 0.5f)
            {
                continue;
            }
            probePos += offData.xyz;
        }
        else
        {
            t6 = shTex6.Load(loadPos);
            validity = t6.w;
            if (validity <= 1e-6f)
            {
                continue;
            }
            w *= validity;
        }

        float3 diffVec = Pb - probePos;
        float d = length(diffVec);
        float3 toProbe = (d > 1e-5f) ? (-diffVec / d) : unitN;

        float wrap = (dot(toProbe, unitN) + 1.0f) * 0.5f;
        w *= (wrap * wrap) + 0.2f;

        if (hasVisAtlas && d > 1e-5f)
        {
            int probeFlatIdx = (probeCoord.z * dims.y + probeCoord.y) * dims.x + probeCoord.x;
            int tileCol = probeFlatIdx % visCols;
            int tileRow = probeFlatIdx / visCols;
            float2 octUV = OctEncode(-toProbe);
            float2 pixelCoord = float2(float(tileCol), float(tileRow)) * tileStride + 1.0f + octUV * float(octRes);
            float2 atlasUV = pixelCoord / visAtlasSize;

            float2 moments = visAtlas.SampleLevel(linSampler, atlasUV, 0.0f).rg;
            if (d > moments.x)
            {
                float variance = max(abs(moments.y - moments.x * moments.x), 1e-4f);
                float t = d - moments.x;
                float cheb = variance / (variance + t * t);
                w *= max(cheb * cheb * cheb, 0.05f);
            }
        }

        w = (w < 0.2f) ? (w * w * w) / (0.2f * 0.2f) : w;
        if (w <= 0.01f)
        {
            continue;
        }

        float4 t0 = shTex0.Load(loadPos);
        float4 t1 = shTex1.Load(loadPos);
        float4 t2 = shTex2.Load(loadPos);

        if (isL1)
        {
            float3 probeSH1[4];
            UnpackSHL1(t0, t1, t2, probeSH1);
            accumSH[0] += probeSH1[0] * w;
            accumSH[1] += probeSH1[1] * w;
            accumSH[2] += probeSH1[2] * w;
            accumSH[3] += probeSH1[3] * w;
        }
        else
        {
            float4 t3 = shTex3.Load(loadPos);
            float4 t4 = shTex4.Load(loadPos);
            float4 t5 = shTex5.Load(loadPos);
            if (hasOffsets)
            {
                t6 = shTex6.Load(loadPos);
            }
            float3 probeSH[9];
            UnpackSHL2(t0, t1, t2, t3, t4, t5, t6, probeSH, validity);
            [unroll]
            for (int c = 0; c < 9; ++c)
            {
                accumSH[c] += probeSH[c] * w;
            }
        }

        if (hasSkyVis)
        {
            accumSkyVis += EvalSkyVisTexel(skyVisTex.Load(loadPos), unitN) * w;
        }
        accumW += w;
    }

    if (accumW > 1e-4f)
    {
        float invW = 1.0f / accumW;
        if (hasSkyVis)
        {
            outSkyVis = saturate(accumSkyVis * invW);
        }
        if (isL1)
        {
            float3 sh1[4] = { accumSH[0] * invW, accumSH[1] * invW, accumSH[2] * invW, accumSH[3] * invW };
            return EvalSHIrradianceL1(sh1, unitN);
        }
        [unroll]
        for (int c = 0; c < 9; ++c)
        {
            accumSH[c] *= invW;
        }
        return EvalSHIrradiance(accumSH, unitN);
    }

    return SampleIrradianceVolumeFast(worldPos, N, gi, linSampler, outVolW, outSkyVis);
}

static float3 SampleGIIrradianceAndSkyVis(float3 worldPos, float3 N, float3 V, GIData gi, SamplerState linSampler, out float outVolW, out float outSkyVis)
{
    if (gi.giMode >= 2 && gi.visibilityParams.x >= 0)
    {
        return SampleIrradianceVolumeLeakResistant(worldPos, N, V, gi, linSampler, outVolW, outSkyVis);
    }
    return SampleIrradianceVolumeFast(worldPos, N, gi, linSampler, outVolW, outSkyVis);
}

static float SampleGISkyVisibility(float3 worldPos, float3 N, GIData gi, SamplerState linSampler)
{
    if (gi.skyVisibilitySplit == 0 || gi.skyVisibilityTexIndex < 0)
    {
        return 1.0f;
    }
    float volW = 0.0f;
    float skyVis = 1.0f;
    SampleGIIrradianceAndSkyVis(worldPos, N, N, gi, linSampler, volW, skyVis);
    return skyVis;
}

static float3 SampleGIIrradiance(float3 worldPos, float3 N, float3 V, GIData gi, SamplerState linSampler, out float outVolW)
{
    float unusedSkyVis = 1.0f;
    return SampleGIIrradianceAndSkyVis(worldPos, N, V, gi, linSampler, outVolW, unusedSkyVis);
}

#endif
