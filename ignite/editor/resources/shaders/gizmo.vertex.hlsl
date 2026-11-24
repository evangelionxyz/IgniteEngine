// Copyright (c) 2026 Evangelion Manuhutu

#include "include/binding_helpers.hlsli"

struct GizmoPushConstants
{
    uint baseInstance;
    uint drawMode; // 0 = instanced unit mesh, 1 = dynamic world-space triangles
    float2 _pad;
};

struct GizmoInstance
{
    float4x4 worldMatrix;
    float4 color;
    float4 ringCenterAndFlag; // xyz = ring center, w = 1 if horizon fade enabled
    float hoverFactor;
    uint shadingMode;
    float2 _pad;
};

struct GizmoPointLight
{
    float4 positionAndRange;
    float4 color;
};

struct GizmoSpotLight
{
    float4 positionAndRange;
    float4 directionAndCosOuter;
    float4 color;
    float4 cosInnerAndPad;
};

struct GizmoFrameConstants
{
    float4x4 viewProjection;
    float4 cameraPosAndIsOrtho;
    float4 cameraForwardAndNear;
    float4 viewportAndDepthSize;
    float4 occlusionParams;
    float4 sunDirection;
    float4 sunColor;
    float4 lightingCounts;
    float4 skySH[9];
    GizmoPointLight pointLights[8];
    GizmoSpotLight spotLights[4];
};

DECLARE_PUSH_CONSTANTS(GizmoPushConstants, g_Push, 0, 0);
cbuffer GizmoFrameBuffer : register(b1, space0) { GizmoFrameConstants u_Frame; }
StructuredBuffer<GizmoInstance> g_Instances : register(t0, space0);

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR;
    float2 params : TEXCOORD0;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPos : WORLDPOS;
    float3 worldNormal : NORMAL;
    float4 color : COLOR;
    float4 ringCenterAndFlag : TEXCOORD0;
    float hoverFactor : TEXCOORD1;
    nointerpolation uint shadingMode : TEXCOORD2;
};

PSInput main(VSInput input, uint instanceID : SV_InstanceID)
{
    PSInput output;

    if (g_Push.drawMode == 0u)
    {
        GizmoInstance inst = g_Instances[g_Push.baseInstance + instanceID];
        float4 worldPos = mul(inst.worldMatrix, float4(input.position, 1.0f));
        float3 worldN = mul((float3x3)inst.worldMatrix, input.normal);
        float nLenSq = dot(worldN, worldN);
        worldN = (nLenSq > 1e-8f) ? (worldN * rsqrt(nLenSq)) : float3(0.0f, 1.0f, 0.0f);

        output.position = mul(u_Frame.viewProjection, worldPos);
        output.worldPos = worldPos.xyz;
        output.worldNormal = worldN;
        output.color = inst.color * input.color;
        output.ringCenterAndFlag = inst.ringCenterAndFlag;
        output.hoverFactor = inst.hoverFactor;
        output.shadingMode = inst.shadingMode;
    }
    else
    {
        float4 worldPos = float4(input.position, 1.0f);
        float nLenSq = dot(input.normal, input.normal);
        float3 worldN = (nLenSq > 1e-8f) ? (input.normal * rsqrt(nLenSq)) : float3(0.0f, 1.0f, 0.0f);

        output.position = mul(u_Frame.viewProjection, worldPos);
        output.worldPos = worldPos.xyz;
        output.worldNormal = worldN;
        output.color = input.color;
        output.ringCenterAndFlag = float4(0.0f, 0.0f, 0.0f, 0.0f);
        output.hoverFactor = input.params.x;
        output.shadingMode = asuint(input.params.y);
    }

    return output;
}

