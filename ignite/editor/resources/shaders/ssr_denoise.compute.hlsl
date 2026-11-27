// Copyright (c) 2026 Evangelion Manuhutu

#include "include/sh.hlsli"

cbuffer SSRDenoiseParams : register(b0)
{
    float4x4 projectionInv;
    float4x4 viewInv;
    float4x4 prevViewProj;
    uint2 halfRes;
    uint2 fullRes;
    float2 invHalfRes;
    float depthSigma;
    uint historyValid;
};

Texture2D<float4> t_RawSSR : register(t0);
Texture2D<float> t_HiZDepth : register(t1);
Texture2D<float2> t_Normal : register(t2);
Texture2D<float4> t_PrevSSR : register(t3);

#ifdef TARGET_VULKAN
    [[vk::image_format("rgba16f")]]
#endif
RWTexture2D<float4> u_OutSSR : register(u0);

SamplerState s_LinearClamp : register(s0);

float DeviceDepthToLinearZ(float deviceDepth)
{
    float vz = projectionInv[2][2] * deviceDepth + projectionInv[2][3];
    float vw = projectionInv[3][2] * deviceDepth + projectionInv[3][3];
    return abs(vz / max(abs(vw), 1e-5f));
}

float3 ReconstructViewPos(float2 uv, float deviceDepth)
{
    float4 clip = float4(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, deviceDepth, 1.0f);
    float4 view = mul(projectionInv, clip);
    return view.xyz / max(abs(view.w), 1e-5f);
}

float Luma(float3 c)
{
    return dot(c, float3(0.2126f, 0.7152f, 0.0722f));
}

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
    if (centerDepth >= 0.99999f || centerDepth <= 0.0f)
    {
        u_OutSSR[DTid.xy] = float4(0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }

    float centerLinearZ = DeviceDepthToLinearZ(centerDepth);
    float linearSigma = max(centerLinearZ * max(depthSigma, 0.02f), 0.08f);
    float2 centerUV = (float2(px) + 0.5f) * invHalfRes;
    float3 centerN = OctDecode(t_Normal.Load(int3(clamp(int2(centerUV * fullRes), int2(0, 0), maxFull), 0)).rg);

    // 5x5 edge-aware bilateral spatial filter with Karis luma weighting to suppress HDR fireflies.
    static const float kGauss[5] = { 0.0625f, 0.25f, 0.375f, 0.25f, 0.0625f };

    float3 accumRGB = float3(0.0f, 0.0f, 0.0f);
    float totalRGBW = 0.0f;
    float accumAlpha = 0.0f;
    float totalAlphaW = 0.0f;

    float3 m1 = float3(0.0f, 0.0f, 0.0f);
    float3 m2 = float3(0.0f, 0.0f, 0.0f);
    float mCount = 0.0f;
    float minAlpha = 1.0f;
    float maxAlpha = 0.0f;

    [unroll]
    for (int dy = -2; dy <= 2; ++dy)
    {
        float wy = kGauss[dy + 2];
        [unroll]
        for (int dx = -2; dx <= 2; ++dx)
        {
            float wSpatial = wy * kGauss[dx + 2];
            int2 spx = clamp(px + int2(dx, dy), int2(0, 0), maxHalf);
            float4 s = t_RawSSR.Load(int3(spx, 0));
            float sd = t_HiZDepth.Load(int3(spx, 0)).r;
            float linSD = DeviceDepthToLinearZ(sd);
            float3 sn = OctDecode(t_Normal.Load(int3(clamp(int2((float2(spx) + 0.5f) * invHalfRes * fullRes), int2(0, 0), maxFull), 0)).rg);

            float wDepth = exp(-abs(linSD - centerLinearZ) / linearSigma);
            float nd = saturate(dot(sn, centerN));
            float nd2 = nd * nd;
            float nd4 = nd2 * nd2;
            float wNormal = nd4 * nd4;
            float w = wSpatial * wDepth * wNormal;

            // Karis luma weight suppresses single-pixel HDR specular spikes without darkening valid hits.
            float karisW = 1.0f / (1.0f + Luma(s.rgb));
            float rgbW = w * s.a * karisW;
            accumRGB += s.rgb * rgbW;
            totalRGBW += rgbW;
            accumAlpha += s.a * w;
            totalAlphaW += w;

            if (abs(dx) <= 1 && abs(dy) <= 1 && wDepth > 0.25f && wNormal > 0.25f)
            {
                if (s.a > 0.01f)
                {
                    m1 += s.rgb;
                    m2 += s.rgb * s.rgb;
                    mCount += 1.0f;
                }
                minAlpha = min(minAlpha, s.a);
                maxAlpha = max(maxAlpha, s.a);
            }
        }
    }

    float3 outRGB = (totalRGBW > 1e-5f) ? (accumRGB / totalRGBW) : float3(0.0f, 0.0f, 0.0f);
    float outAlpha = (totalAlphaW > 1e-5f) ? saturate(accumAlpha / totalAlphaW) : 0.0f;
    float4 spatialSSR = float4(outRGB, outAlpha);

    // Temporal accumulation stabilizes half-res SSR across frames on both FXAA and TAA.
    if (historyValid != 0u)
    {
        float3 posVS = ReconstructViewPos(centerUV, centerDepth);
        float3 posWS = mul(viewInv, float4(posVS, 1.0f)).xyz;
        float4 prevClip = mul(prevViewProj, float4(posWS, 1.0f));
        if (prevClip.w > 1e-4f)
        {
            float2 prevNDC = prevClip.xy / prevClip.w;
            float2 prevUV = float2(prevNDC.x * 0.5f + 0.5f, 0.5f - prevNDC.y * 0.5f);
            if (all(prevUV >= 0.0f) && all(prevUV <= 1.0f))
            {
                float4 prevSSR = t_PrevSSR.SampleLevel(s_LinearClamp, prevUV, 0.0f);
                prevSSR.rgb = max(prevSSR.rgb, float3(0.0f, 0.0f, 0.0f));
                prevSSR.a = saturate(prevSSR.a);

                if (mCount > 0.5f)
                {
                    float3 mu = m1 / mCount;
                    float3 sigma = sqrt(max(m2 / mCount - mu * mu, float3(0.0f, 0.0f, 0.0f)));
                    float3 clampMin = max(mu - 1.5f * sigma, float3(0.0f, 0.0f, 0.0f));
                    float3 clampMax = mu + 1.5f * sigma;
                    prevSSR.rgb = clamp(prevSSR.rgb, clampMin, clampMax);
                }
                else
                {
                    prevSSR.rgb = lerp(prevSSR.rgb, spatialSSR.rgb, 0.5f);
                }
                prevSSR.a = clamp(prevSSR.a, minAlpha, maxAlpha);

                float motionPx = length((prevUV - centerUV) * float2(halfRes));
                float blendAlpha = lerp(0.12f, 0.60f, saturate(motionPx * 0.25f));
                spatialSSR = lerp(prevSSR, spatialSSR, blendAlpha);
            }
        }
    }

    u_OutSSR[DTid.xy] = float4(max(spatialSSR.rgb, float3(0.0f, 0.0f, 0.0f)), saturate(spatialSSR.a));
}
