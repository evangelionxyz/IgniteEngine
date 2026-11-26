// Copyright (c) 2026 Evangelion Manuhutu

#include "include/binding_helpers.hlsli"
#include "include/scene.hlsli"

DECLARE_PUSH_CONSTANTS(PushConstants, g_Push, 0, 0);         // b0 - base instance offset
cbuffer                  CameraBuffer      : register(b1, space0) { Camera camera; }
StructuredBuffer<Object> g_ObjectBuffer    : register(t2, space0);
StructuredBuffer<uint>   g_InstanceIndices : register(t3, space0);
cbuffer                  SceneBuffer       : register(b4, space0) { Scene scene; }
cbuffer                  CascadesBuffer    : register(b5, space0) { }
cbuffer                  PointLightBuffer  : register(b6, space0) { }
cbuffer                  SpotLightBuffer   : register(b7, space0) { }
cbuffer                  GIBuffer          : register(b8, space0) { }
StructuredBuffer<ReflectionProbeGPU> g_ReflectionProbes : register(t9, space0);

PixelVertexInput main(VertexMesh input, uint instanceID : SV_InstanceID)
{
    PixelVertexInput pixelInput;
    const uint objectIndex = g_InstanceIndices[g_Push.baseInstanceOffset + instanceID];
    float4 worldPosition = mul(g_ObjectBuffer[objectIndex].transformMatrix, float4(input.position, 1.0f));

    // Populate all PixelVertexInput fields so DXC preserves the full VertexMeshStatic stride in SPIR-V reflection
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
