// Copyright (c) 2026 Evangelion Manuhutu

#include "include/scene.hlsli"

struct DebugProbeGPU
{
    float4 posAndRadius;
    float4 sh[9];
    float4 stateAndFlags; // x: 0 = invalid, 1 = valid, 2 = relocated
};

cbuffer CameraBuffer : register(b0, space0)
{
    Camera camera;
};

StructuredBuffer<DebugProbeGPU> g_DebugProbes : register(t1, space0);

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldNormal : NORMAL;
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
    float2 uv : TEXCOORD;
    float4 color : COLOR;
    nointerpolation uint probeIndex : PROBEINDEX;
};

// Preserves all 6 VertexMesh inputs in SPIR-V reflection so the pipeline stride
// matches sizeof(VertexMeshStatic) when extendedDynamicState is unavailable.
PSInput main(VertexMesh input, uint instanceID : SV_InstanceID)
{
    DebugProbeGPU probe = g_DebugProbes[instanceID];
    // EMeshType::SPHERE is built with radius 0.5 (unit diameter); scale by 2*radius.
    float3 worldPos = probe.posAndRadius.xyz + input.position * (probe.posAndRadius.w * 2.0f);

    PSInput output;
    output.position = mul(mul(camera.projection, camera.view), float4(worldPos, 1.0f));
    output.worldNormal = normalize(input.normal);
    output.tangent = input.tangent;
    output.bitangent = input.bitangent;
    output.uv = input.uv;
    output.color = input.color;
    output.probeIndex = instanceID;
    return output;
}
