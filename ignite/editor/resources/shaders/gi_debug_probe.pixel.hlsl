// Copyright (c) 2026 Evangelion Manuhutu

#include "include/scene.hlsli"
#include "include/sh.hlsli"

struct DebugProbeGPU
{
    float4 posAndRadius;
    float4 sh[9];
    float4 stateAndFlags; // x: 0 = invalid, 1 = valid, 2 = relocated
};

StructuredBuffer<DebugProbeGPU> g_DebugProbes : register(t1, space0);

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldNormal : NORMAL;
    nointerpolation uint probeIndex : PROBEINDEX;
};

struct PSOutput
{
    float4 color : SV_TARGET0;
    uint objectID : SV_TARGET1;
};

PSOutput main(PSInput input)
{
    DebugProbeGPU probe = g_DebugProbes[input.probeIndex];
    float3 N = normalize(input.worldNormal);

    float state = probe.stateAndFlags.x;
    float3 color = EvalSHIrradiance4(probe.sh, N);

    if (state < 0.5f)
    {
        // Invalid probe -> bright red marker
        color = float3(3.0f, 0.05f, 0.05f);
    }
    else if (state > 1.5f)
    {
        // Relocated valid probe -> subtle yellow indicator blended at silhouette
        color = lerp(color, float3(2.5f, 2.0f, 0.1f), 0.35f);
    }

    PSOutput output;
    output.color = float4(color, 1.0f);
    output.objectID = 0xFFFFFFFFu;
    return output;
}

