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
    nointerpolation uint probeIndex : PROBEINDEX;
};

PSInput main(VertexMesh input, uint instanceID : SV_InstanceID)
{
    DebugProbeGPU probe = g_DebugProbes[instanceID];
    float3 worldPos = probe.posAndRadius.xyz + input.position * probe.posAndRadius.w;

    PSInput output;
    output.position = mul(mul(camera.projection, camera.view), float4(worldPos, 1.0f));
    output.worldNormal = normalize(input.normal);
    output.probeIndex = instanceID;
    return output;
}

