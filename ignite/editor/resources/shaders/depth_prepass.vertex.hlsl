// Copyright (c) 2026 Evangelion Manuhutu

#include "include/binding_helpers.hlsli"
#include "include/scene.hlsli"

DECLARE_PUSH_CONSTANTS(PushConstants, g_Push, 0, 0);         // b0 - base instance offset
cbuffer CameraBuffer                    : register(b1, space0) { Camera camera; }
StructuredBuffer<Object>   g_ObjectBuffer    : register(t2, space0);
StructuredBuffer<uint>     g_InstanceIndices : register(t3, space0);
cbuffer SceneBuffer                     : register(b4, space0) { Scene scene; }
cbuffer CSMBuffer                       : register(b5, space0) { }
cbuffer PointLightBuffer                : register(b6, space0) { }
cbuffer SpotLightBuffer                 : register(b7, space0) { }
cbuffer GIBuffer                        : register(b8, space0) { }
StructuredBuffer<ReflectionProbeGPU> g_ReflectionProbes : register(t9, space0);

// Preserves all 6 VertexMesh inputs and outputs identical PixelVertexInput as mesh_static.vertex.hlsl
// so reflected vertex buffer stride matches sizeof(VertexMeshStatic) when extendedDynamicState is absent.
PixelVertexInput main(VertexMesh input, uint instanceID : SV_InstanceID)
{
    PixelVertexInput pixelInput;

    const uint objectIndex = g_InstanceIndices[g_Push.baseInstanceOffset + instanceID];

    float4 posL = float4(input.position, 1.0f);
    float3 normalL = input.normal;
    float3 tangentL = input.tangent;
    float3 bitangentL = input.bitangent;

    float4 worldPos = mul(g_ObjectBuffer[objectIndex].transformMatrix, posL);

    pixelInput.position = mul(mul(camera.projection, camera.view), worldPos);
    float3x3 N = (float3x3)g_ObjectBuffer[objectIndex].normalMatrix;
    float3 worldN = mul(N, normalL);
    pixelInput.normal = dot(worldN, worldN) > 1e-8f ? normalize(worldN) : float3(0.0f, 1.0f, 0.0f);

    float3 worldT = mul(N, tangentL);
    worldT = worldT - pixelInput.normal * dot(pixelInput.normal, worldT);
    if (dot(worldT, worldT) > 1e-8f)
    {
        pixelInput.tangent = normalize(worldT);
    }
    else
    {
        float3 fallbackAxis = abs(pixelInput.normal.y) < 0.999f ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f);
        pixelInput.tangent = normalize(cross(fallbackAxis, pixelInput.normal));
    }

    float3 worldB = mul(N, bitangentL);
    float3 computedB = cross(pixelInput.normal, pixelInput.tangent);
    pixelInput.bitangent = (dot(worldB, worldB) > 1e-8f && dot(computedB, worldB) < 0.0f) ? -computedB : computedB;
    pixelInput.worldPos = worldPos.xyz;
    pixelInput.uv = input.uv;
    pixelInput.color = input.color;
    pixelInput.objectID = g_ObjectBuffer[objectIndex].objectID;
    pixelInput.receiveShadow = g_ObjectBuffer[objectIndex].receiveShadow;
    return pixelInput;
}
