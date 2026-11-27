// Copyright (c) 2026 Evangelion Manuhutu

#include "include/binding_helpers.hlsli"
#include "include/helpers.hlsli"
#include "include/scene.hlsli"
#include "include/sh.hlsli"

cbuffer      CameraBuffer   : register(b1, space0) { Camera camera; }
cbuffer      MaterialBuffer : register(b0, space1) { Material material; }
SamplerState sampler0       : register(s0, space1);

#ifdef IGN_DEPTH_PREPASS_OPAQUE
[earlydepthstencil]
#endif
float2 main(PixelVertexInput input, bool isFrontFace : SV_IsFrontFace) : SV_TARGET0
{
#ifndef IGN_DEPTH_PREPASS_OPAQUE
    float2 tiledUV = input.uv * material.tilingFactor;

    // Masked materials run alpha-test discard in the depth prepass only;
    // the subsequent color pass uses LESS_EQUAL / EQUAL without depth write.
    if (material.blendMode == 2)
    {
        Texture2D baseColorTex = ResourceDescriptorHeap[SafeBindlessIndex(material.baseColorTextureIndex)];
        float4 rawBase = baseColorTex.Sample(sampler0, tiledUV);
        float finalAlpha = rawBase.a * input.color.a * material.baseColorFactor.a;
        float alphaCutoff = (material.alphaCutoff > 0.0f) ? material.alphaCutoff : 0.5f;
        if (finalAlpha < alphaCutoff)
        {
            discard;
        }
    }
#endif

    // Output clean geometric vertex normals (without tangent normal-map bumps) so GTAO
    // evaluates against the same smooth macro geometry represented by the depth buffer.
    float3 finalNormal = normalize(input.normal);
    if (isFrontFace)
    {
        finalNormal = -finalNormal;
    }

    float3 viewN = mul((float3x3)camera.view, finalNormal);
    float lenSq = dot(viewN, viewN);
    viewN = (lenSq > 1e-8f) ? (viewN * rsqrt(lenSq)) : float3(0.0f, 0.0f, 1.0f);
    return OctEncode(viewN);
}
