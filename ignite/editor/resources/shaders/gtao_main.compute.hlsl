// Copyright (c) 2026 Evangelion Manuhutu

#include "include/sh.hlsli"

#define GTAO_PI 3.14159265359f
#define GTAO_HALF_PI 1.57079632679f

float FastAcos(float x)
{
    float ax = abs(x);
    float res = -0.156583f * ax + GTAO_HALF_PI;
    res *= sqrt(max(1.0f - ax, 0.0f));
    return (x >= 0.0f) ? res : (GTAO_PI - res);
}

cbuffer GTAOMainParams : register(b0)
{
    float2 ndcToViewMul;
    float2 ndcToViewAdd;
    float2 invHalfRes;
    float2 fullRes;
    uint2 halfRes;
    uint maxDepthMip;
    uint noiseIndex;
    float effectRadius;
    float falloffRange;
    float radiusMultiplier;
    float sampleDistributionPower;
    float thinOccluderCompensation;
    float projScaleY;
    float farPlane;
    uint flags; // bit 0 = hasNormals, bit 1 = isOrtho
    uint sliceCount;
    uint stepsPerSlice;
    uint2 _pad;
};

Texture2D<float> t_PrefilteredDepth : register(t0);
Texture2D<float2> t_Normal : register(t1);

#ifdef TARGET_VULKAN
    [[vk::image_format("r8")]]
#endif
RWTexture2D<unorm float> u_OutAO : register(u0);

SamplerState s_PointClamp : register(s0);

float3 ReconstructViewPos(float2 uv, float viewZ)
{
    float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    if ((flags & 2u) != 0u)
    {
        return float3(ndc * ndcToViewMul + ndcToViewAdd, -viewZ);
    }
    return float3((ndc * ndcToViewMul + ndcToViewAdd) * viewZ, -viewZ);
}

float2 SpatioTemporalNoise(uint2 px, uint temporalIdx)
{
    // Stratified 4x4 Bayer + low-discrepancy sequence so every 4x4 half-res block
    // covers all 16 angle strata and radial step offsets uniformly without clumping.
    static const uint kBayer4x4[16] = {
         0u,  8u,  2u, 10u,
        12u,  4u, 14u,  6u,
         3u, 11u,  1u,  9u,
        15u,  7u, 13u,  5u
    };
    uint2 tilePx = px & 3u;
    uint bayerIdx = kBayer4x4[tilePx.y * 4u + tilePx.x];
    uint rotIdx = ((px.x + 2u * px.y) + (px.x >> 2u) * 3u + (px.y >> 2u) * 5u) & 15u;
    float t = float(temporalIdx % 64u);
    float n0 = frac((float(bayerIdx) + 0.5f) / 16.0f + t * 0.618033989f);
    float n1 = frac((float((bayerIdx * 5u + rotIdx) & 15u) + 0.5f) / 16.0f + t * 0.754877666f);
    return float2(n0, n1);
}

float3 ReconstructHalfDepthNormal(int2 cpx, float3 centerPosVS, float centerZ)
{
    int2 maxHalf = int2(max(int(halfRes.x) - 1, 0), max(int(halfRes.y) - 1, 0));
    int2 pxL = clamp(cpx + int2(-1,  0), int2(0, 0), maxHalf);
    int2 pxR = clamp(cpx + int2( 1,  0), int2(0, 0), maxHalf);
    int2 pxU = clamp(cpx + int2( 0, -1), int2(0, 0), maxHalf);
    int2 pxD = clamp(cpx + int2( 0,  1), int2(0, 0), maxHalf);

    float zL = t_PrefilteredDepth.Load(int3(pxL, 0)).r;
    float zR = t_PrefilteredDepth.Load(int3(pxR, 0)).r;
    float zU = t_PrefilteredDepth.Load(int3(pxU, 0)).r;
    float zD = t_PrefilteredDepth.Load(int3(pxD, 0)).r;

    float3 pL = ReconstructViewPos((float2(pxL) + 0.5f) * invHalfRes, zL);
    float3 pR = ReconstructViewPos((float2(pxR) + 0.5f) * invHalfRes, zR);
    float3 pU = ReconstructViewPos((float2(pxU) + 0.5f) * invHalfRes, zU);
    float3 pD = ReconstructViewPos((float2(pxD) + 0.5f) * invHalfRes, zD);

    float3 dx = (abs(zL - centerZ) < abs(zR - centerZ)) ? (centerPosVS - pL) : (pR - centerPosVS);
    float3 dy = (abs(zD - centerZ) < abs(zU - centerZ)) ? (centerPosVS - pD) : (pU - centerPosVS);
    float3 n = cross(dx, dy);
    float lenSq = dot(n, n);
    return (lenSq > 1e-8f) ? (n * rsqrt(lenSq)) : float3(0.0f, 0.0f, 1.0f);
}

float3 LoadViewNormal(int2 centerPx, float3 centerPosVS, float centerZ)
{
    if ((flags & 1u) != 0u)
    {
        int2 maxFull = int2(max(int(fullRes.x) - 1, 0), max(int(fullRes.y) - 1, 0));
        float2 uv = (float2(centerPx) + 0.5f) * invHalfRes;
        int2 fpx = clamp(int2(uv * fullRes), int2(0, 0), maxFull);
        return OctDecode(t_Normal.Load(int3(fpx, 0)).rg);
    }

    return ReconstructHalfDepthNormal(centerPx, centerPosVS, centerZ);
}

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= halfRes.x || DTid.y >= halfRes.y)
    {
        return;
    }

    int2 centerPx = int2(DTid.xy);
    int2 maxHalf = int2(max(int(halfRes.x) - 1, 0), max(int(halfRes.y) - 1, 0));
    float maxValidZ = min(farPlane, 65000.0f) * 0.998f;
    float centerZ = t_PrefilteredDepth.Load(int3(centerPx, 0)).r;
    if (centerZ >= maxValidZ || centerZ <= 1e-4f)
    {
        u_OutAO[DTid.xy] = 1.0f;
        return;
    }

    float2 uv = (float2(DTid.xy) + 0.5f) * invHalfRes;
    float3 viewPos = ReconstructViewPos(uv, centerZ);
    float3 viewVec = ((flags & 2u) != 0u) ? float3(0.0f, 0.0f, 1.0f) : normalize(-viewPos);

    float3 viewNormal = LoadViewNormal(centerPx, viewPos, centerZ);
    if (dot(viewNormal, viewVec) < 0.0f)
    {
        viewNormal = -viewNormal;
    }

    // Small view/normal bias prevents depth quantization & facet self-occlusion on flat surfaces
    float3 biasedViewPos = viewPos
        + viewVec * max(0.003f * centerZ, 0.004f)
        + viewNormal * max(0.0015f * centerZ, 0.002f);

    float worldRadius = max(effectRadius * radiusMultiplier, 0.05f);
    float screenspaceRadius = ((flags & 2u) != 0u)
        ? (worldRadius * projScaleY)
        : (worldRadius * projScaleY / max(centerZ, 0.05f));

    if (screenspaceRadius < 1.25f)
    {
        u_OutAO[DTid.xy] = 1.0f;
        return;
    }
    screenspaceRadius = min(screenspaceRadius, 64.0f);

    float falloffFrom = worldRadius * (1.0f - saturate(falloffRange));
    float falloffDenom = max(worldRadius * saturate(falloffRange), 1e-4f);
    float falloffMul = -1.0f / falloffDenom;
    float falloffAdd = falloffFrom / falloffDenom + 1.0f;

    float2 noise = SpatioTemporalNoise(DTid.xy, noiseIndex);
    float noiseSlice = noise.x;
    float noiseSample = noise.y;

    int sliceCnt = clamp(int(sliceCount), 1, 4);
    int stepCnt = clamp(int(stepsPerSlice), 1, 6);
    const float kHorizonBias = 0.03f;

    float visibility = 0.0f;

    [loop]
    for (int s = 0; s < sliceCnt; ++s)
    {
        float phi = (GTAO_PI / float(sliceCnt)) * (float(s) + noiseSlice);
        float2 omega = float2(cos(phi), sin(phi));
        // +v in UV maps to -Y in right-handed view space
        float3 directionVec = float3(omega.x, -omega.y, 0.0f);

        float3 orthoDirectionVec = directionVec - dot(directionVec, viewVec) * viewVec;
        float orthoLenSq = dot(orthoDirectionVec, orthoDirectionVec);
        orthoDirectionVec = (orthoLenSq > 1e-6f) ? (orthoDirectionVec * rsqrt(orthoLenSq)) : float3(1.0f, 0.0f, 0.0f);

        float3 axisVec = cross(orthoDirectionVec, viewVec);
        float axisLenSq = dot(axisVec, axisVec);
        axisVec = (axisLenSq > 1e-6f) ? (axisVec * rsqrt(axisLenSq)) : float3(0.0f, 1.0f, 0.0f);

        float3 projectedNormalVec = viewNormal - axisVec * dot(viewNormal, axisVec);
        float projectedNormalLen = length(projectedNormalVec);
        float signNorm = (dot(orthoDirectionVec, projectedNormalVec) >= 0.0f) ? 1.0f : -1.0f;
        float cosNorm = saturate(dot(projectedNormalVec, viewVec) / max(projectedNormalLen, 1e-5f));
        float n = clamp(signNorm * FastAcos(cosNorm), -GTAO_HALF_PI + 1e-4f, GTAO_HALF_PI - 1e-4f);

        float lowHorizonCos0 = cos(n - GTAO_HALF_PI);
        float lowHorizonCos1 = cos(n + GTAO_HALF_PI);
        float horizonCos0 = lowHorizonCos0;
        float horizonCos1 = lowHorizonCos1;

        float stepNoise = frac(noiseSample + float(s) * 0.618033989f);

        [loop]
        for (int step = 0; step < stepCnt; ++step)
        {
            float t = (float(step) + stepNoise) / float(stepCnt);
            t = (abs(sampleDistributionPower - 2.0f) < 1e-3f) ? (t * t) : pow(max(t, 1e-4f), sampleDistributionPower);
            float sampleDistPx = max(t * screenspaceRadius, 1.0f);
            float2 sampleOffsetUV = omega * (sampleDistPx * invHalfRes);

            // Side 0: -omega (sample mip 0 at exact texel centers to eliminate coarse-mip stair-stepping)
            float2 uv0 = uv - sampleOffsetUV;
            if (all(uv0 >= 0.0f) && all(uv0 <= 1.0f))
            {
                int2 spx0 = clamp(int2(uv0 * float2(halfRes)), int2(0, 0), maxHalf);
                if (!all(spx0 == centerPx))
                {
                    float z0 = t_PrefilteredDepth.Load(int3(spx0, 0)).r;
                    if (z0 < maxValidZ && z0 > 1e-4f)
                    {
                        float2 exactUV0 = (float2(spx0) + 0.5f) * invHalfRes;
                        float3 delta0 = ReconstructViewPos(exactUV0, z0) - biasedViewPos;
                        float dist0 = length(delta0);
                        if (dist0 > 1e-4f)
                        {
                            float hCos0 = dot(delta0 / dist0, viewVec) - kHorizonBias;
                            float weight0 = saturate(dist0 * falloffMul + falloffAdd);
                            if (thinOccluderCompensation > 0.0f && dist0 > worldRadius * 0.5f)
                            {
                                weight0 *= saturate(1.0f - thinOccluderCompensation * saturate((dist0 - worldRadius * 0.5f) / worldRadius));
                            }
                            horizonCos0 = max(horizonCos0, lerp(lowHorizonCos0, hCos0, weight0));
                        }
                    }
                }
            }

            // Side 1: +omega
            float2 uv1 = uv + sampleOffsetUV;
            if (all(uv1 >= 0.0f) && all(uv1 <= 1.0f))
            {
                int2 spx1 = clamp(int2(uv1 * float2(halfRes)), int2(0, 0), maxHalf);
                if (!all(spx1 == centerPx))
                {
                    float z1 = t_PrefilteredDepth.Load(int3(spx1, 0)).r;
                    if (z1 < maxValidZ && z1 > 1e-4f)
                    {
                        float2 exactUV1 = (float2(spx1) + 0.5f) * invHalfRes;
                        float3 delta1 = ReconstructViewPos(exactUV1, z1) - biasedViewPos;
                        float dist1 = length(delta1);
                        if (dist1 > 1e-4f)
                        {
                            float hCos1 = dot(delta1 / dist1, viewVec) - kHorizonBias;
                            float weight1 = saturate(dist1 * falloffMul + falloffAdd);
                            if (thinOccluderCompensation > 0.0f && dist1 > worldRadius * 0.5f)
                            {
                                weight1 *= saturate(1.0f - thinOccluderCompensation * saturate((dist1 - worldRadius * 0.5f) / worldRadius));
                            }
                            horizonCos1 = max(horizonCos1, lerp(lowHorizonCos1, hCos1, weight1));
                        }
                    }
                }
            }
        }

        float h0 = n + max(-FastAcos(clamp(horizonCos0, -1.0f, 1.0f)) - n, -GTAO_HALF_PI);
        float h1 = n + min(+FastAcos(clamp(horizonCos1, -1.0f, 1.0f)) - n, +GTAO_HALF_PI);
        float sinN = sin(n);
        float iarc0 = (cosNorm + 2.0f * h0 * sinN - cos(2.0f * h0 - n)) * 0.25f;
        float iarc1 = (cosNorm + 2.0f * h1 * sinN - cos(2.0f * h1 - n)) * 0.25f;
        visibility += projectedNormalLen * (iarc0 + iarc1);
    }

    u_OutAO[DTid.xy] = saturate(visibility / float(sliceCnt));
}
