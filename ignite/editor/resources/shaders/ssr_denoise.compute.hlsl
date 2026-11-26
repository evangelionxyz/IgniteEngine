// Copyright (c) 2026 Evangelion Manuhutu

#include "include/sh.hlsli"

cbuffer SSRDenoiseParams : register(b0)
{
    uint2 halfRes;
    uint2 fullRes;
    float2 invHalfRes;
    float depthSigma;
    float _pad;
};

Texture2D<float4> t_RawSSR : register(t0);
Texture2D<float> t_HiZDepth : register(t1);
Texture2D<float2> t_Normal : register(t2);

#ifdef TARGET_VULKAN
    [[vk::image_format("rgba16f")]]
#endif
RWTexture2D<float4> u_OutSSR : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= halfRes.x || DTid.y >= halfRes.y)
    {
        return;
    }

    int2 px = int2(DTid.xy);
    int2 maxHalf = int2(max(int(halfRes.x) - 1, 0), max(int(halfRes.y) - 1, 0));
    int2 maxFull = int2(max(int(fullRes.x) - 1, 0), max(int(fullRes.y) - 1, 0));

    float centerDepth = t_HiZDepth.Load(int3(px, 0)).r;
    if (centerDepth >= 0.99999f)
    {
        u_OutSSR[DTid.xy] = float4(0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }

    float3 centerN = OctDecode(t_Normal.Load(int3(clamp(px * 2, int2(0, 0), maxFull), 0)).rg);

    float4 accum = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float totalW = 0.0f;

    [unroll]
    for (int dy = -1; dy <= 1; ++dy)
    {
        [unroll]
        for (int dx = -1; dx <= 1; ++dx)
        {
            int2 spx = clamp(px + int2(dx, dy), int2(0, 0), maxHalf);
            float4 s = t_RawSSR.Load(int3(spx, 0));
            float sd = t_HiZDepth.Load(int3(spx, 0)).r;
            float3 sn = OctDecode(t_Normal.Load(int3(clamp(spx * 2, int2(0, 0), maxFull), 0)).rg);

            float wDepth = exp(-abs(sd - centerDepth) / max(depthSigma, 1e-4f));
            float wNormal = pow(saturate(dot(sn, centerN)), 8.0f);
            float w = wDepth * wNormal;

            accum += s * w;
            totalW += w;
        }
    }

    u_OutSSR[DTid.xy] = (totalW > 1e-5f) ? (accum / totalW) : float4(0.0f, 0.0f, 0.0f, 0.0f);
}
