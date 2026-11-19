// Copyright (c) 2026 Evangelion Manuhutu

#include "include/cubemap_helpers.hlsli"

static const float kPI = 3.14159265359f;

cbuffer PrefilterGGXParams : register(b0)
{
    uint dstSize;
    uint srcSize;
    float roughness;
    uint sampleCount;
};

TextureCube<float4> srcCube : register(t0);
SamplerState linearSampler : register(s0);

#ifdef TARGET_VULKAN
    [[vk::image_format("rgba16f")]]
#endif
RWTexture2DArray<float4> dstMip : register(u0);

static float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10f;
}

static float2 Hammersley(uint i, uint N)
{
    return float2(float(i) / float(max(N, 1u)), RadicalInverse_VdC(i));
}

static float DistributionGGX(float NdotH, float alpha)
{
    float a2 = alpha * alpha;
    float denom = (NdotH * NdotH) * (a2 - 1.0f) + 1.0f;
    return a2 / max(kPI * denom * denom, 1e-6f);
}

static float3 ImportanceSampleGGX(float2 xi, float3 N, float r)
{
    float a = r * r;
    float phi = 2.0f * kPI * xi.x;
    float cosTheta = sqrt(saturate((1.0f - xi.y) / max(1.0f + (a * a - 1.0f) * xi.y, 1e-6f)));
    float sinTheta = sqrt(saturate(1.0f - cosTheta * cosTheta));

    float3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    float3 up = abs(N.z) < 0.999f ? float3(0.0f, 0.0f, 1.0f) : float3(1.0f, 0.0f, 0.0f);
    float3 tangent = normalize(cross(up, N));
    float3 bitangent = cross(N, tangent);

    return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint face = dispatchThreadID.z;
    if (dispatchThreadID.x >= dstSize || dispatchThreadID.y >= dstSize || face >= 6u)
    {
        return;
    }

    float2 uv = (float2(dispatchThreadID.xy) + 0.5f) / float(dstSize);
    float3 N = CubemapUVToDir(face, uv);
    float3 R = N;
    float3 V = R;

    if (roughness <= 1e-4f)
    {
        float3 mirrorColor = srcCube.SampleLevel(linearSampler, R, 0.0f).rgb;
        dstMip[uint3(dispatchThreadID.xy, face)] = float4(mirrorColor, 1.0f);
        return;
    }

    float r = max(roughness, 0.045f);
    float alpha = r * r;
    float saTexel = (4.0f * kPI) / (6.0f * float(max(srcSize * srcSize, 1u)));

    uint N_samples = max(sampleCount, 1u);
    float3 prefilteredColor = float3(0.0f, 0.0f, 0.0f);
    float totalWeight = 0.0f;

    for (uint i = 0u; i < N_samples; ++i)
    {
        float2 xi = Hammersley(i, N_samples);
        float3 H = ImportanceSampleGGX(xi, N, r);
        float3 L = normalize(2.0f * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0f);
        if (NdotL > 0.0f)
        {
            float NdotH = max(dot(N, H), 0.0f);
            float HdotV = max(dot(H, V), 0.0f);
            float D = DistributionGGX(NdotH, alpha);
            float pdf = max((D * NdotH) / max(4.0f * HdotV, 1e-5f), 1e-5f);
            float saSample = 1.0f / (float(N_samples) * pdf);
            float mipLevel = max(0.5f * log2(saSample / saTexel), 0.0f);

            prefilteredColor += srcCube.SampleLevel(linearSampler, L, mipLevel).rgb * NdotL;
            totalWeight += NdotL;
        }
    }

    float3 outColor = (totalWeight > 1e-5f)
        ? (prefilteredColor / totalWeight)
        : srcCube.SampleLevel(linearSampler, R, 0.0f).rgb;
    dstMip[uint3(dispatchThreadID.xy, face)] = float4(outColor, 1.0f);
}

