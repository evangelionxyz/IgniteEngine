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
    float4 position      : POSITION;
    float2 localPosition : TEXCOORD;
    float4 color         : COLOR;
    uint objectID        : OBJECTID;
};

struct PSInput
{
    float4 position      : SV_POSITION;
    float2 localPosition : TEXCOORD;
    float4 color         : COLOR;
    uint objectID        : OBJECTID;
};

PSInput main(VSInput input)
{
    PSInput output;
    output.position = mul(mul(camera.projection, camera.view), input.position);
    output.localPosition = input.localPosition;
    output.color = input.color;
    output.objectID = input.objectID;
    return output;
}