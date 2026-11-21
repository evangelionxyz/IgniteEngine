// Copyright (c) 2026 Evangelion Manuhutu

#include "include/sh.hlsli"

cbuffer GTAODenoiseParams : register(b0)
{
    uint2 halfRes;
    uint2 fullRes;
    int2 direction;
    float depthSigma;
    float farPlane;
    uint hasNormals;
    uint3 _pad;
};

Texture2D<float> t_SrcAO : register(t0);
Texture2D<float> t_Depth : register(t1);
Texture2D<float2> t_Normal : register(t2);

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
[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= halfRes.x || DTid.y >= halfRes.y)
    {
        return;
    }

    int2 maxHalf = int2(max(int(halfRes.x) - 1, 0), max(int(halfRes.y) - 1, 0));
    int2 maxFull = int2(max(int(fullRes.x) - 1, 0), max(int(fullRes.y) - 1, 0));
    int2 centerPx = int2(DTid.xy);

    float centerZ = t_Depth.Load(int3(centerPx, 0)).r;
    float maxValidZ = min(farPlane, 65000.0f) * 0.998f;
    if (centerZ >= maxValidZ || centerZ <= 1e-4f)
    {
        u_DstAO[DTid.xy] = 1.0f;
        return;
    }

    float centerAO = t_SrcAO.Load(int3(centerPx, 0)).r;
    float3 centerN = (hasNormals != 0u) ? LoadViewNormalAtHalf(centerPx, maxFull) : float3(0.0f, 0.0f, 1.0f);

    static const float kSpatialWeights[4] = { 1.0f, 0.7788f, 0.3679f, 0.1054f };
    float invDepthDenom = 1.0f / max(centerZ * max(depthSigma, 0.01f), 0.02f);

    float accumAO = centerAO;
    float accumW = 1.0f;

    [unroll]
    for (int offset = -3; offset <= 3; ++offset)
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

        float w = kSpatialWeights[abs(offset)];
        w *= exp(-abs(sampleZ - centerZ) * invDepthDenom);
        if (hasNormals != 0u)
        {
            float3 sampleN = LoadViewNormalAtHalf(samplePx, maxFull);
            w *= pow(saturate(dot(centerN, sampleN)), 8.0f);
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
