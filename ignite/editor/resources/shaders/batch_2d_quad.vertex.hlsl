// Copyright (c) 2026 Evangelion Manuhutu

#include "include/binding_helpers.hlsli"

struct Camera
{
    float4x4 projection;
    float4x4 view;
    float4 position;
};

cbuffer CameraBuffer : register(b0, space0)
{
    Camera camera;
}

struct VSInput
{
    float3 position     : POSITION;
    float2 texCoord     : TEXCOORD;
    float2 tilingFactor : TILINGFACTOR;
    float4 color        : COLOR;
    float4 additiveColor: ADDITIVECOLOR;
    uint texIndex       : TEXINDEX;
    uint materialType   : MATTYPE;
    uint objectID       : OBJECTID;
};

struct PSInput
{
    float4 position     : SV_POSITION;
    float3 worldPosition: WORLDPOS;
    float2 texCoord     : TEXCOORD;
    float2 tilingFactor : TILINGFACTOR;
    float4 color        : COLOR;
    float4 additiveColor: ADDITIVECOLOR;
    nointerpolation uint texIndex       : TEXINDEX;
    nointerpolation uint materialType   : MATTYPE;
    nointerpolation uint objectID       : OBJECTID;
};

PSInput main(VSInput input)
{
    PSInput output;
    float4 pos          = float4(input.position.x, input.position.y, input.position.z, 1.0f);
    output.position     = mul(mul(camera.projection, camera.view), pos);
    output.worldPosition = input.position;
    output.color        = input.color;
    output.additiveColor = input.additiveColor;
    output.tilingFactor = input.tilingFactor;
    output.texCoord     = input.texCoord;
    output.texIndex     = input.texIndex;
    output.materialType = input.materialType;
    output.objectID     = input.objectID;
    
    return output;
}