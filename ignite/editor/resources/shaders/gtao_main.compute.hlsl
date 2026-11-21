// Copyright (c) 2026 Evangelion Manuhutu

#include "include/sh.hlsli"

#define GTAO_PI 3.14159265359f
#define GTAO_HALF_PI 1.57079632679f

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
    // Interleaved gradient noise + R2 quasirandom temporal offset
    float idx = float(temporalIdx % 64u);
    float2 p = float2(px) + float2(idx * 5.588238f, idx * 5.588238f);
    float n0 = frac(52.9829189f * frac(dot(p, float2(0.06711056f, 0.00583715f))));
    float n1 = frac(0.754877666f * float(px.x) + 0.569840291f * float(px.y) + 0.618033989f * idx);
    return float2(n0, n1);
}
float3 LoadViewNormal(float2 uv, float3 centerPosVS, float centerZ)
{
    if ((flags & 1u) != 0u)
    {
        int2 maxFull = int2(max(int(fullRes.x) - 1, 0), max(int(fullRes.y) - 1, 0));
        int2 px = clamp(int2(uv * fullRes), int2(0, 0), maxFull);
        float2 enc = t_Normal.Load(int3(px, 0)).rg;
        return OctDecode(enc);
    }

    int2 maxHalf = int2(max(int(halfRes.x) - 1, 0), max(int(halfRes.y) - 1, 0));
    int2 cpx = clamp(int2(uv * float2(halfRes)), int2(0, 0), maxHalf);
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

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= halfRes.x || DTid.y >= halfRes.y)
    {
        return;
    }

    float maxValidZ = min(farPlane, 65000.0f) * 0.998f;
    float centerZ = t_PrefilteredDepth.Load(int3(int2(DTid.xy), 0)).r;
    if (centerZ >= maxValidZ || centerZ <= 1e-4f)
    {
        u_OutAO[DTid.xy] = 1.0f;
        return;
    }

    float2 uv = (float2(DTid.xy) + 0.5f) * invHalfRes;
    float3 viewPos = ReconstructViewPos(uv, centerZ);
    float3 viewVec = ((flags & 2u) != 0u) ? float3(0.0f, 0.0f, 1.0f) : normalize(-viewPos);

    float3 viewNormal = LoadViewNormal(uv, viewPos, centerZ);
    if (dot(viewNormal, viewVec) < 0.0f)
    {
        viewNormal = -viewNormal;
    }

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

    // XeGTAO Low/Medium preset for RDNA2 660M iGPU: 2 slices x 3 steps per side
    const int kSliceCount = 2;
    const int kStepsPerSlice = 3;

    float visibility = 0.0f;

    [unroll]
    for (int s = 0; s < kSliceCount; ++s)
    {
        float phi = (GTAO_PI / float(kSliceCount)) * (float(s) + noiseSlice);
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
        float n = clamp(signNorm * acos(cosNorm), -GTAO_HALF_PI + 1e-4f, GTAO_HALF_PI - 1e-4f);

        float lowHorizonCos0 = cos(n - GTAO_HALF_PI);
        float lowHorizonCos1 = cos(n + GTAO_HALF_PI);
        float horizonCos0 = lowHorizonCos0;
        float horizonCos1 = lowHorizonCos1;

        float stepNoise = frac(noiseSample + float(s) * 0.618033989f);

        [unroll]
        for (int step = 0; step < kStepsPerSlice; ++step)
        {
            float t = (float(step) + stepNoise) / float(kStepsPerSlice);
            t = pow(max(t, 1e-4f), sampleDistributionPower);
            float sampleDistPx = max(t * screenspaceRadius, 1.0f);
            float2 sampleOffsetUV = omega * (sampleDistPx * invHalfRes);
            float mip = clamp(log2(sampleDistPx) - 1.5f, 0.0f, float(maxDepthMip));

            // Side 0: -omega
            float2 uv0 = uv - sampleOffsetUV;
            if (all(uv0 >= 0.0f) && all(uv0 <= 1.0f))
            {
                float z0 = t_PrefilteredDepth.SampleLevel(s_PointClamp, uv0, mip).r;
                if (z0 < maxValidZ && z0 > 1e-4f)
                {
                    float3 delta0 = ReconstructViewPos(uv0, z0) - viewPos;
                    float dist0 = length(delta0);
                    if (dist0 > 1e-4f)
                    {
                        float hCos0 = dot(delta0 / dist0, viewVec);
                        float weight0 = saturate(dist0 * falloffMul + falloffAdd);
                        if (thinOccluderCompensation > 0.0f && dist0 > worldRadius * 0.5f)
                        {
                            weight0 *= saturate(1.0f - thinOccluderCompensation * saturate((dist0 - worldRadius * 0.5f) / worldRadius));
                        }
                        horizonCos0 = max(horizonCos0, lerp(lowHorizonCos0, hCos0, weight0));
                    }
                }
            }

            // Side 1: +omega
            float2 uv1 = uv + sampleOffsetUV;
            if (all(uv1 >= 0.0f) && all(uv1 <= 1.0f))
            {
                float z1 = t_PrefilteredDepth.SampleLevel(s_PointClamp, uv1, mip).r;
                if (z1 < maxValidZ && z1 > 1e-4f)
                {
                    float3 delta1 = ReconstructViewPos(uv1, z1) - viewPos;
                    float dist1 = length(delta1);
                    if (dist1 > 1e-4f)
                    {
                        float hCos1 = dot(delta1 / dist1, viewVec);
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

        float h0 = n + max(-acos(clamp(horizonCos0, -1.0f, 1.0f)) - n, -GTAO_HALF_PI);
        float h1 = n + min(+acos(clamp(horizonCos1, -1.0f, 1.0f)) - n, +GTAO_HALF_PI);
        float sinN = sin(n);
        float iarc0 = (cosNorm + 2.0f * h0 * sinN - cos(2.0f * h0 - n)) * 0.25f;
        float iarc1 = (cosNorm + 2.0f * h1 * sinN - cos(2.0f * h1 - n)) * 0.25f;
        visibility += projectedNormalLen * (iarc0 + iarc1);
    }

    u_OutAO[DTid.xy] = saturate(visibility / float(kSliceCount));
}
