// Copyright (c) 2026 Evangelion Manuhutu

#include "include/sh.hlsli"

cbuffer GTAODenoiseParams : register(b0)
{
    float4x4 projectionInv;
    uint2 halfRes;
    uint2 fullRes;
    int2 direction;
    float depthSigma;
    float farPlane;
    float nearPlane;
    uint hasNormals;
    uint passMode; // 0 = H half-res denoise, 1 = V half-res denoise, 2 = full-res 3x3 bilateral upsample, 3 = full-res 2x2 bilateral upsample
    uint denoiseRadius;
};

Texture2D<float> t_SrcAO : register(t0);
Texture2D<float> t_Depth : register(t1);
Texture2D<float2> t_Normal : register(t2);
Texture2D<float> t_FullDepth : register(t3);

#ifdef TARGET_VULKAN
    [[vk::image_format("r8")]]
#endif
RWTexture2D<unorm float> u_DstAO : register(u0);

float3 LoadViewNormalAtHalf(int2 hpx, int2 maxFull)
{
    float2 uv = (float2(hpx) + 0.5f) / float2(halfRes);
    int2 fpx = clamp(int2(uv * float2(fullRes)), int2(0, 0), maxFull);
    return OctDecode(t_Normal.Load(int3(fpx, 0)).rg);
}

float DeviceDepthToViewZ(float d)
{
    float maxZ = min(farPlane, 65000.0f);
    if (d >= 0.99999f)
    {
        return maxZ;
    }
    float4 view = mul(projectionInv, float4(0.0f, 0.0f, d, 1.0f));
    float viewZ = abs(view.z / max(abs(view.w), 1e-6f));
    return clamp(viewZ, max(nearPlane, 1e-3f), maxZ);
}

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    int2 maxHalf = int2(max(int(halfRes.x) - 1, 0), max(int(halfRes.y) - 1, 0));
    int2 maxFull = int2(max(int(fullRes.x) - 1, 0), max(int(fullRes.y) - 1, 0));
    float maxValidZ = min(farPlane, 65000.0f) * 0.998f;

    if (passMode == 2u || passMode == 3u)
    {
        if (DTid.x >= fullRes.x || DTid.y >= fullRes.y)
        {
            return;
        }

        int2 fullPx = int2(DTid.xy);
        float centerZ = DeviceDepthToViewZ(t_FullDepth.Load(int3(fullPx, 0)).r);
        if (centerZ >= maxValidZ || centerZ <= 1e-4f)
        {
            u_DstAO[DTid.xy] = 1.0f;
            return;
        }

        float3 centerN = (hasNormals != 0u) ? OctDecode(t_Normal.Load(int3(fullPx, 0)).rg) : float3(0.0f, 0.0f, 1.0f);
        float2 halfPos = ((float2(fullPx) + 0.5f) / float2(fullRes)) * float2(halfRes) - 0.5f;
        float invDepthDenom = 1.0f / max(centerZ * max(depthSigma, 0.01f), 0.02f);

        float accumAO = 0.0f;
        float accumW = 0.0f;

        if (passMode == 3u)
        {
            int2 baseHalf = int2(floor(halfPos));
            [unroll]
            for (int dy = 0; dy <= 1; ++dy)
            {
                [unroll]
                for (int dx = 0; dx <= 1; ++dx)
                {
                    int2 shPx = clamp(baseHalf + int2(dx, dy), int2(0, 0), maxHalf);
                    float sampleZ = t_Depth.Load(int3(shPx, 0)).r;
                    if (sampleZ >= maxValidZ || sampleZ <= 1e-4f)
                    {
                        continue;
                    }

                    float2 delta = float2(shPx) - halfPos;
                    float w = exp(-dot(delta, delta) * 0.85f);
                    w *= exp(-abs(sampleZ - centerZ) * invDepthDenom);
                    if (hasNormals != 0u)
                    {
                        float3 sampleN = LoadViewNormalAtHalf(shPx, maxFull);
                        w *= pow(saturate(dot(centerN, sampleN)), 12.0f);
                    }

                    if (w > 1e-5f)
                    {
                        accumAO += t_SrcAO.Load(int3(shPx, 0)).r * w;
                        accumW += w;
                    }
                }
            }
        }
        else
        {
            int2 baseHalf = int2(round(halfPos));
            [unroll]
            for (int dy = -1; dy <= 1; ++dy)
            {
                [unroll]
                for (int dx = -1; dx <= 1; ++dx)
                {
                    int2 shPx = clamp(baseHalf + int2(dx, dy), int2(0, 0), maxHalf);
                    float sampleZ = t_Depth.Load(int3(shPx, 0)).r;
                    if (sampleZ >= maxValidZ || sampleZ <= 1e-4f)
                    {
                        continue;
                    }

                    float2 delta = float2(shPx) - halfPos;
                    float w = exp(-dot(delta, delta) * 0.85f);
                    w *= exp(-abs(sampleZ - centerZ) * invDepthDenom);
                    if (hasNormals != 0u)
                    {
                        float3 sampleN = LoadViewNormalAtHalf(shPx, maxFull);
                        w *= pow(saturate(dot(centerN, sampleN)), 12.0f);
                    }

                    if (w > 1e-5f)
                    {
                        accumAO += t_SrcAO.Load(int3(shPx, 0)).r * w;
                        accumW += w;
                    }
                }
            }
        }

        if (accumW > 1e-5f)
        {
            u_DstAO[DTid.xy] = saturate(accumAO / accumW);
        }
        else
        {
            int2 fallbackPx = clamp(int2(round(halfPos)), int2(0, 0), maxHalf);
            u_DstAO[DTid.xy] = t_SrcAO.Load(int3(fallbackPx, 0)).r;
        }
        return;
    }

    if (DTid.x >= halfRes.x || DTid.y >= halfRes.y)
    {
        return;
    }

    int2 centerPx = int2(DTid.xy);
    float centerZ = t_Depth.Load(int3(centerPx, 0)).r;
    if (centerZ >= maxValidZ || centerZ <= 1e-4f)
    {
        u_DstAO[DTid.xy] = 1.0f;
        return;
    }

    float centerAO = t_SrcAO.Load(int3(centerPx, 0)).r;
    float3 centerN = (hasNormals != 0u) ? LoadViewNormalAtHalf(centerPx, maxFull) : float3(0.0f, 0.0f, 1.0f);

    // Slope-compensated planar depth difference preserves smooth gradients on angled floors and walls
    int2 pMinus = clamp(centerPx - direction, int2(0, 0), maxHalf);
    int2 pPlus = clamp(centerPx + direction, int2(0, 0), maxHalf);
    float zMinus = t_Depth.Load(int3(pMinus, 0)).r;
    float zPlus = t_Depth.Load(int3(pPlus, 0)).r;
    float dzL = (zMinus < maxValidZ && zMinus > 1e-4f) ? (centerZ - zMinus) : 0.0f;
    float dzR = (zPlus < maxValidZ && zPlus > 1e-4f) ? (zPlus - centerZ) : 0.0f;
    float slope = (abs(dzL) < abs(dzR)) ? dzL : dzR;

    static const float kSpatialWeights[5] = { 1.0f, 0.8825f, 0.6065f, 0.3247f, 0.1353f };
    float invDepthDenom = 1.0f / max(centerZ * max(depthSigma, 0.01f), 0.02f);
    int radius = clamp(int(denoiseRadius), 1, 4);

    float accumAO = centerAO;
    float accumW = 1.0f;

    [loop]
    for (int offset = -radius; offset <= radius; ++offset)
    {
        if (offset == 0)
        {
            continue;
        }

        int2 samplePx = clamp(centerPx + direction * offset, int2(0, 0), maxHalf);
        float sampleZ = t_Depth.Load(int3(samplePx, 0)).r;
        if (sampleZ >= maxValidZ || sampleZ <= 1e-4f)
        {
            continue;
        }

        float expectedZ = centerZ + slope * float(offset);
        float w = kSpatialWeights[abs(offset)];
        w *= exp(-abs(sampleZ - expectedZ) * invDepthDenom);
        if (hasNormals != 0u)
        {
            float3 sampleN = LoadViewNormalAtHalf(samplePx, maxFull);
            w *= pow(saturate(dot(centerN, sampleN)), 12.0f);
        }

        if (w > 1e-5f)
        {
            float sampleAO = t_SrcAO.Load(int3(samplePx, 0)).r;
            accumAO += sampleAO * w;
            accumW += w;
        }
    }

    u_DstAO[DTid.xy] = saturate(accumAO / max(accumW, 1e-5f));
}
