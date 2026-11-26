// Copyright (c) 2026 Evangelion Manuhutu

#include "include/sh.hlsli"

cbuffer SSRTraceParams : register(b0)
{
    float4x4 projection;
    float4x4 projectionInv;
    float4x4 viewInv;
    float4x4 prevViewProj;
    float2 invHalfRes;
    float2 fullRes;
    uint2 halfRes;
    uint maxDepthMip;
    uint frameIndex;
    float nearPlane;
    float farPlane;
    float maxRoughness;
    float _pad;
};

Texture2D<float> t_HiZDepth : register(t0);
Texture2D<float2> t_Normal : register(t1);
Texture2D<float4> t_PrevColor : register(t2);

#ifdef TARGET_VULKAN
    [[vk::image_format("rgba16f")]]
#endif
RWTexture2D<float4> u_OutSSR : register(u0);

SamplerState s_LinearClamp : register(s0);

float3 ReconstructViewPos(float2 uv, float deviceDepth)
{
    float4 clip = float4(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, deviceDepth, 1.0f);
    float4 view = mul(projectionInv, clip);
    return view.xyz / max(abs(view.w), 1e-5f);
}

float3 ProjectViewToScreen(float3 posVS)
{
    float4 clip = mul(projection, float4(posVS, 1.0f));
    float invW = 1.0f / max(clip.w, 1e-5f);
    float3 ndc = clip.xyz * invW;
    return float3(ndc.x * 0.5f + 0.5f, 0.5f - ndc.y * 0.5f, ndc.z);
}

float DeviceDepthToLinearZ(float deviceDepth)
{
    float4 view = mul(projectionInv, float4(0.0f, 0.0f, deviceDepth, 1.0f));
    return abs(view.z / max(abs(view.w), 1e-5f));
}

float Bayer4x4(uint2 px)
{
    static const uint kBayer[16] = {
         0u,  8u,  2u, 10u,
        12u,  4u, 14u,  6u,
         3u, 11u,  1u,  9u,
        15u,  7u, 13u,  5u
    };
    uint2 p = px & 3u;
    return (float(kBayer[p.y * 4u + p.x]) + 0.5f) / 16.0f;
}

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= halfRes.x || DTid.y >= halfRes.y)
    {
        return;
    }

    int2 px = int2(DTid.xy);
    float centerDepth = t_HiZDepth.Load(int3(px, 0)).r;
    if (centerDepth >= 0.99999f || centerDepth <= 0.0f)
    {
        u_OutSSR[DTid.xy] = float4(0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }

    float2 uv = (float2(px) + 0.5f) * invHalfRes;
    float3 posVS = ReconstructViewPos(uv, centerDepth);
    float startLinearZ = max(-posVS.z, nearPlane);

    int2 maxFull = int2(max(int(fullRes.x) - 1, 0), max(int(fullRes.y) - 1, 0));
    int2 fullPx = clamp(px * 2, int2(0, 0), maxFull);
    float3 normalVS = OctDecode(t_Normal.Load(int3(fullPx, 0)).rg);
    float nLenSq = dot(normalVS, normalVS);
    normalVS = (nLenSq > 1e-6f) ? (normalVS * rsqrt(nLenSq)) : float3(0.0f, 0.0f, 1.0f);

    float3 V = normalize(-posVS);
    float3 R = normalize(reflect(-V, normalVS));

    // Rays pointing strongly toward the camera rarely find valid screen-space hits.
    float backwardFade = 1.0f - smoothstep(0.35f, 0.85f, R.z);
    if (backwardFade <= 0.001f)
    {
        u_OutSSR[DTid.xy] = float4(0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }

    // Clip ray end against near plane (-nearPlane in right-handed view space).
    float maxDist = min(farPlane * 0.5f, 80.0f);
    if (R.z > 1e-4f)
    {
        float distToNear = (-nearPlane * 1.05f - posVS.z) / R.z;
        if (distToNear <= 0.02f)
        {
            u_OutSSR[DTid.xy] = float4(0.0f, 0.0f, 0.0f, 0.0f);
            return;
        }
        maxDist = min(maxDist, distToNear);
    }

    // Push start slightly off the surface along normal + reflection direction to prevent
    // immediate self-intersection on curved geometry (e.g., spheres).
    float startBias = max(0.05f, startLinearZ * 0.012f);
    float3 startVS = posVS + normalVS * startBias + R * startBias;
    float3 endVS = startVS + R * maxDist;

    float3 startSS = ProjectViewToScreen(startVS);
    float3 endSS = ProjectViewToScreen(endVS);
    float3 rayDirSS = endSS - startSS;

    float2 pixelDelta = rayDirSS.xy * float2(halfRes);
    float pixelLen = length(pixelDelta);
    if (pixelLen < 1.0f)
    {
        u_OutSSR[DTid.xy] = float4(0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }

    float3 stepSS = rayDirSS / pixelLen;
    float jitter = 0.25f + 0.5f * Bayer4x4(DTid.xy);

    int mip = 0;
    float t = 2.0f + jitter;
    float tPrev = t;
    float maxT = min(pixelLen, 256.0f);

    bool hit = false;
    float3 hitSS = float3(0.0f, 0.0f, 0.0f);
    int2 maxHalf = int2(halfRes) - 1;

    [loop]
    for (int iter = 0; iter < 48; ++iter)
    {
        if (t > maxT)
        {
            break;
        }

        float3 currSS = startSS + stepSS * t;
        if (currSS.x < 0.0f || currSS.x >= 1.0f || currSS.y < 0.0f || currSS.y >= 1.0f || currSS.z <= 0.0f || currSS.z >= 1.0f)
        {
            break;
        }

        int2 mipDims = max(int2(halfRes) >> mip, int2(1, 1));
        int2 cell = clamp(int2(currSS.xy * float2(mipDims)), int2(0, 0), mipDims - 1);
        float hizDepth = t_HiZDepth.Load(int3(cell, mip)).r;

        if (currSS.z >= hizDepth)
        {
            if (mip == 0)
            {
                float rayLinearZ = DeviceDepthToLinearZ(currSS.z);
                float surfLinearZ = DeviceDepthToLinearZ(hizDepth);
                float thickness = max(0.06f, surfLinearZ * 0.03f);
                float diff = rayLinearZ - surfLinearZ;
                if (diff >= 0.0f && diff <= thickness && hizDepth < 0.99999f)
                {
                    // 5-iteration binary search refinement between tPrev and t for crisp hit precision.
                    float lo = tPrev;
                    float hi = t;
                    [unroll]
                    for (int b = 0; b < 5; ++b)
                    {
                        float mid = 0.5f * (lo + hi);
                        float3 midSS = startSS + stepSS * mid;
                        int2 midCell = clamp(int2(midSS.xy * float2(halfRes)), int2(0, 0), maxHalf);
                        float midDepth = t_HiZDepth.Load(int3(midCell, 0)).r;
                        if (midSS.z >= midDepth)
                            hi = mid;
                        else
                            lo = mid;
                    }
                    hitSS = startSS + stepSS * hi;

                    // Reject self/backface hits where hit surface faces the same direction as the ray.
                    int2 hitFullPx = clamp(int2(hitSS.xy * fullRes), int2(0, 0), maxFull);
                    float3 hitNormalVS = OctDecode(t_Normal.Load(int3(hitFullPx, 0)).rg);
                    float hnLenSq = dot(hitNormalVS, hitNormalVS);
                    if (hnLenSq > 1e-6f)
                    {
                        hitNormalVS *= rsqrt(hnLenSq);
                        if (dot(hitNormalVS, R) <= 0.25f)
                        {
                            hit = true;
                            break;
                        }
                    }
                    else
                    {
                        hit = true;
                        break;
                    }
                }
                tPrev = t;
                t += 1.0f;
            }
            else
            {
                mip = max(mip - 1, 0);
            }
        }
        else
        {
            tPrev = t;
            float stride = float(1 << mip);
            t += stride;
            mip = min(mip + 1, int(maxDepthMip));
        }
    }

    if (!hit)
    {
        u_OutSSR[DTid.xy] = float4(0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }

    // Reproject hit point into previous frame HDR color buffer.
    float hitDepth = t_HiZDepth.Load(int3(clamp(int2(hitSS.xy * float2(halfRes)), int2(0, 0), maxHalf), 0)).r;
    float3 hitVS = ReconstructViewPos(hitSS.xy, hitDepth);
    float3 hitWS = mul(viewInv, float4(hitVS, 1.0f)).xyz;
    float4 prevClip = mul(prevViewProj, float4(hitWS, 1.0f));
    float2 prevUV = hitSS.xy;
    if (prevClip.w > 1e-4f)
    {
        float2 prevNDC = prevClip.xy / prevClip.w;
        prevUV = float2(prevNDC.x * 0.5f + 0.5f, 0.5f - prevNDC.y * 0.5f);
    }

    if (prevUV.x < 0.0f || prevUV.x > 1.0f || prevUV.y < 0.0f || prevUV.y > 1.0f)
    {
        u_OutSSR[DTid.xy] = float4(0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }

    float3 radiance = t_PrevColor.SampleLevel(s_LinearClamp, prevUV, 0.0f).rgb;
    radiance = min(max(radiance, float3(0.0f, 0.0f, 0.0f)), float3(16.0f, 16.0f, 16.0f));

    // Smooth edge fade near screen borders.
    float2 edgeDist = min(hitSS.xy, 1.0f - hitSS.xy);
    float edgeFade = saturate(min(edgeDist.x, edgeDist.y) / 0.08f);
    float confidence = saturate(edgeFade * backwardFade);

    u_OutSSR[DTid.xy] = float4(radiance, confidence);
}
