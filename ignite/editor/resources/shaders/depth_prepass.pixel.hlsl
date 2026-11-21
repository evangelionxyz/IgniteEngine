// Copyright (c) 2026 Evangelion Manuhutu

#include "include/binding_helpers.hlsli"
#include "include/helpers.hlsli"
#include "include/scene.hlsli"
#include "include/sh.hlsli"

cbuffer      CameraBuffer   : register(b1, space0) { Camera camera; }
cbuffer      MaterialBuffer : register(b0, space1) { Material material; }
SamplerState sampler0       : register(s0, space1);

float2 main(PixelVertexInput input, bool isFrontFace : SV_IsFrontFace) : SV_TARGET0
{
    float2 tiledUV = input.uv * material.tilingFactor;

    // Masked materials run alpha-test discard in the depth prepass only;
    // the subsequent color pass uses LESS_EQUAL / EQUAL without depth write.
    if (material.blendMode == 2)
    {
        Texture2D baseColorTex = ResourceDescriptorHeap[SafeBindlessIndex(material.baseColorTextureIndex)];
        float4 rawBase = baseColorTex.Sample(sampler0, tiledUV);
        float finalAlpha = rawBase.a * input.color.a * material.baseColorFactor.a;
        if (finalAlpha < 0.5f)
        {
            discard;
        }
    }

    float3 N = normalize(input.normal);
    float3 finalNormal = N;

    if (material.normalTextureIndex >= 0)
    {
        float3 T = input.tangent - N * dot(N, input.tangent);
        float tLenSq = dot(T, T);
        if (tLenSq > 1e-6f)
        {
            T *= rsqrt(tLenSq);
        }
        else
        {
            float3 bFallback;
            BuildOrthonormalBasis(N, T, bFallback);
        }
        float3 bitangent = cross(N, T);
        float3 B = dot(bitangent, input.bitangent) < 0.0f ? -bitangent : bitangent;

        Texture2D normalMapTex = ResourceDescriptorHeap[SafeBindlessIndex(material.normalTextureIndex)];
        float3 tangentNormal = normalMapTex.Sample(sampler0, tiledUV).rgb * 2.0f - 1.0f;
        finalNormal = normalize(T * tangentNormal.x + B * tangentNormal.y + N * tangentNormal.z);
    }

    if (!isFrontFace)
    {
        finalNormal = -finalNormal;
    }

    float3 viewN = mul((float3x3)camera.view, finalNormal);
    float lenSq = dot(viewN, viewN);
    viewN = (lenSq > 1e-8f) ? (viewN * rsqrt(lenSq)) : float3(0.0f, 0.0f, 1.0f);
    return OctEncode(viewN);
}
