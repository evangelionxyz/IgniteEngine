// Copyright (c) 2026 Evangelion Manuhutu

#include "include/binding_helpers.hlsli"
#include "include/scene.hlsli"

DECLARE_PUSH_CONSTANTS(PushConstants, g_Push, 0, 0);
cbuffer                  CameraBuffer      : register(b1, space0) { Camera camera; }
StructuredBuffer<Object> g_ObjectBuffer    : register(t2, space0);
StructuredBuffer<float4x4> g_Bones         : register(t3, space0);
cbuffer                  SceneBuffer       : register(b4, space0) { Scene scene; }
cbuffer                  CascadesBuffer    : register(b5, space0) { }
cbuffer                  PointLightBuffer  : register(b6, space0) { }
cbuffer                  SpotLightBuffer   : register(b7, space0) { }
cbuffer                  GIBuffer          : register(b8, space0) { }
StructuredBuffer<ReflectionProbeGPU> g_ReflectionProbes : register(t9, space0);

float4 SkinPosition(VertexMeshAnim input, uint objectIndex)
{
    const float totalWeight = dot(input.weights, 1.0f);
    if (totalWeight <= 0.0001f)
    {
        return float4(input.position, 1.0f);
    }

    float4 skinned = float4(0.0f, 0.0f, 0.0f, 0.0f);
    [unroll]
    for (uint i = 0; i < VERTEX_MAX_BONES; ++i)
    {
        const float weight = input.weights[i];
        const uint bone = min(input.boneIDs[i], (uint)(MAX_BONES - 1));
        if (weight > 0.0f)
        {
            uint globalBoneIdx = g_ObjectBuffer[objectIndex].boneOffset + bone;
            skinned += mul(g_Bones[globalBoneIdx], float4(input.position, 1.0f)) * weight;
        }
    }

    if (skinned.w == 0.0f)
    {
        skinned = float4(input.position, 1.0f);
    }
    else
    {
        skinned.w = 1.0f;
    }

    return skinned;
}

PixelVertexInput main(VertexMeshAnim input)
{
    PixelVertexInput pixelInput;
    const uint objectIndex = g_Push.baseInstanceOffset;
    float4 localPosition = SkinPosition(input, objectIndex);
    float4 worldPosition = mul(g_ObjectBuffer[objectIndex].transformMatrix, localPosition);

    pixelInput.normal = input.normal;
    pixelInput.tangent = input.tangent;
    pixelInput.bitangent = input.bitangent;
    pixelInput.worldPos = worldPosition.xyz;
    pixelInput.uv = input.uv;
    pixelInput.color = input.color;
    pixelInput.objectID = g_ObjectBuffer[objectIndex].objectID;
    pixelInput.receiveShadow = g_ObjectBuffer[objectIndex].receiveShadow;
    pixelInput.position = mul(mul(camera.projection, camera.view), worldPosition);
    return pixelInput;
}
