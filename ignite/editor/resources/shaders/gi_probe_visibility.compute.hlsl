// Copyright (c) 2026 Evangelion Manuhutu

#include "include/cubemap_helpers.hlsli"
#include "include/sh.hlsli"

struct ProbeVisibilityStats
{
    float4 closestBackfaceDirAndDist;
    float4 closestFrontfaceDirAndDist;
    float backfaceFraction;
    uint hitCount;
    uint backfaceCount;
    float skyVisibility;
};

cbuffer VisibilityParams : register(b0)
{
    uint probeLocalIndex;
    uint cubeRes;
    uint octRes;
    uint _pad0;
    float nearPlane;
    float farPlane;
    float maxDistance;
    float sharpness;
};

Texture2DArray colorCube : register(t0);
Texture2DArray depthCube : register(t1);
RWStructuredBuffer<ProbeVisibilityStats> outStats : register(u0);
RWStructuredBuffer<float2> outOctTiles : register(u1);

groupshared float2 gs_Tile[10][10];
groupshared float4 gs_MinBack[64];
groupshared float4 gs_MinFront[64];
groupshared uint gs_HitCount[64];
groupshared uint gs_BackCount[64];
groupshared float gs_TotalOmega[64];
groupshared float gs_BackOmega[64];
groupshared float gs_SkyOmega[64];

[numthreads(8, 8, 1)]
void main(uint3 groupThreadID : SV_GroupThreadID, uint groupIndex : SV_GroupIndex)
{
    uint lx = groupThreadID.x;
    uint ly = groupThreadID.y;

    float2 octUV = (float2(lx, ly) + 0.5f) / float(max(octRes, 1u));
    float3 octDir = OctDecode(octUV);

    float sumD = 0.0f;
    float sumD2 = 0.0f;
    float sumW = 0.0f;

    float minBackDist = 1e30f;
    float3 minBackDir = float3(0.0f, 1.0f, 0.0f);
    float minFrontDist = 1e30f;
    float3 minFrontDir = float3(0.0f, 1.0f, 0.0f);
    uint localHits = 0u;
    uint localBacks = 0u;
    float localTotalOmega = 0.0f;
    float localBackOmega = 0.0f;
    float localSkyOmega = 0.0f;

    uint res = max(cubeRes, 1u);
    uint faceTexels = res * res;
    uint totalTexels = 6u * faceTexels;

    // 1. Each thread integrates its own octahedral texel across all cubemap texels
    for (uint face = 0u; face < 6u; ++face)
    {
        for (uint py = 0u; py < res; ++py)
        {
            for (uint px = 0u; px < res; ++px)
            {
                float2 uv = (float2(px, py) + 0.5f) / float(res);
                float3 sampleDir = CubemapUVToDir(face, uv);
                float ndotl = dot(octDir, sampleDir);
                if (ndotl <= 0.0f)
                {
                    continue;
                }

                float w = pow(ndotl, sharpness);
                if (w <= 1e-6f)
                {
                    continue;
                }

                float d = depthCube.Load(int4(int(px), int(py), int(face), 0)).r;
                float4 col = colorCube.Load(int4(int(px), int(py), int(face), 0));

                float rayDist = maxDistance;
                if (d < 0.99999f)
                {
                    float2 st = uv * 2.0f - 1.0f;
                    float denom = max(farPlane - d * (farPlane - nearPlane), 1e-6f);
                    float viewZ = (nearPlane * farPlane) / denom;
                    rayDist = min(viewZ * sqrt(1.0f + st.x * st.x + st.y * st.y), maxDistance);
                }

                float momentDist = (d < 0.99999f && col.a < 0.5f) ? 0.0f : rayDist;
                sumD += w * momentDist;
                sumD2 += w * (momentDist * momentDist);
                sumW += w;
            }
        }
    }

    gs_Tile[ly + 1u][lx + 1u] = float2(sumD / max(sumW, 1e-6f), sumD2 / max(sumW, 1e-6f));

    // 2. Parallel strided scan for exact closest front/back hits and solid-angle backface fraction
    for (uint idx = groupIndex; idx < totalTexels; idx += 64u)
    {
        uint face = idx / faceTexels;
        uint rem = idx - face * faceTexels;
        uint py = rem / res;
        uint px = rem - py * res;

        float2 uv = (float2(px, py) + 0.5f) / float(res);
        float3 sampleDir = CubemapUVToDir(face, uv);
        float dOmega = CubemapTexelSolidAngle(uv, res);

        float d = depthCube.Load(int4(int(px), int(py), int(face), 0)).r;
        float4 col = colorCube.Load(int4(int(px), int(py), int(face), 0));

        localTotalOmega += dOmega;
        if (col.a < 0.5f)
        {
            localBackOmega += dOmega;
        }
        if (d >= 0.99999f)
        {
            localSkyOmega += dOmega;
        }

        if (d < 0.99999f)
        {
            float2 st = uv * 2.0f - 1.0f;
            float denom = max(farPlane - d * (farPlane - nearPlane), 1e-6f);
            float viewZ = (nearPlane * farPlane) / denom;
            float rayDist = viewZ * sqrt(1.0f + st.x * st.x + st.y * st.y);

            localHits += 1u;
            if (col.a < 0.5f)
            {
                localBacks += 1u;
                if (rayDist < minBackDist)
                {
                    minBackDist = rayDist;
                    minBackDir = sampleDir;
                }
            }
            else
            {
                if (rayDist < minFrontDist)
                {
                    minFrontDist = rayDist;
                    minFrontDir = sampleDir;
                }
            }
        }
    }

    gs_MinBack[groupIndex] = float4(minBackDir, minBackDist);
    gs_MinFront[groupIndex] = float4(minFrontDir, minFrontDist);
    gs_HitCount[groupIndex] = localHits;
    gs_BackCount[groupIndex] = localBacks;
    gs_TotalOmega[groupIndex] = localTotalOmega;
    gs_BackOmega[groupIndex] = localBackOmega;
    gs_SkyOmega[groupIndex] = localSkyOmega;

    GroupMemoryBarrierWithGroupSync();

    // 3. Populate octahedral gutter borders for 8x8 interior -> 10x10 tile
    if (groupIndex == 0u)
    {
        for (uint k = 1u; k <= 8u; ++k)
        {
            gs_Tile[0u][k] = gs_Tile[1u][9u - k];
            gs_Tile[9u][k] = gs_Tile[8u][9u - k];
            gs_Tile[k][0u] = gs_Tile[9u - k][1u];
            gs_Tile[k][9u] = gs_Tile[9u - k][8u];
        }
        gs_Tile[0u][0u] = gs_Tile[8u][8u];
        gs_Tile[9u][9u] = gs_Tile[1u][1u];
        gs_Tile[0u][9u] = gs_Tile[8u][1u];
        gs_Tile[9u][0u] = gs_Tile[1u][8u];
    }

    for (uint stride = 32u; stride > 0u; stride >>= 1u)
    {
        if (groupIndex < stride)
        {
            if (gs_MinBack[groupIndex + stride].w < gs_MinBack[groupIndex].w)
            {
                gs_MinBack[groupIndex] = gs_MinBack[groupIndex + stride];
            }
            if (gs_MinFront[groupIndex + stride].w < gs_MinFront[groupIndex].w)
            {
                gs_MinFront[groupIndex] = gs_MinFront[groupIndex + stride];
            }
            gs_HitCount[groupIndex] += gs_HitCount[groupIndex + stride];
            gs_BackCount[groupIndex] += gs_BackCount[groupIndex + stride];
            gs_TotalOmega[groupIndex] += gs_TotalOmega[groupIndex + stride];
            gs_BackOmega[groupIndex] += gs_BackOmega[groupIndex + stride];
            gs_SkyOmega[groupIndex] += gs_SkyOmega[groupIndex + stride];
        }
        GroupMemoryBarrierWithGroupSync();
    }

    // 4. Write out all 100 tile texels (strided across 64 threads) + thread 0 writes stats
    uint tileBase = probeLocalIndex * 100u;
    for (uint ti = groupIndex; ti < 100u; ti += 64u)
    {
        uint ty = ti / 10u;
        uint tx = ti - ty * 10u;
        outOctTiles[tileBase + ti] = gs_Tile[ty][tx];
    }

    if (groupIndex == 0u)
    {
        ProbeVisibilityStats stats;
        float backDist = (gs_MinBack[0].w < 1e29f) ? gs_MinBack[0].w : -1.0f;
        float frontDist = (gs_MinFront[0].w < 1e29f) ? gs_MinFront[0].w : -1.0f;
        stats.closestBackfaceDirAndDist = float4(gs_MinBack[0].xyz, backDist);
        stats.closestFrontfaceDirAndDist = float4(gs_MinFront[0].xyz, frontDist);
        stats.backfaceFraction = (gs_TotalOmega[0] > 1e-8f) ? saturate(gs_BackOmega[0] / gs_TotalOmega[0]) : 0.0f;
        stats.hitCount = gs_HitCount[0];
        stats.backfaceCount = gs_BackCount[0];
        stats.skyVisibility = (gs_TotalOmega[0] > 1e-8f) ? saturate(gs_SkyOmega[0] / gs_TotalOmega[0]) : 0.0f;
        outStats[probeLocalIndex] = stats;
    }
}

