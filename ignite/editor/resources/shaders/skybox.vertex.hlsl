#include "include/binding_helpers.hlsli"

struct Camera
{
    float4x4 projection;
    float4x4 view;
    float4 position;
};

cbuffer CamerBuffer : register(b0, space0)
{
    Camera camera;
}

struct VSInput
{
    float3 position : POSITION;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 UVW : UVW;
};

PSInput main(VSInput input)
{
    PSInput output;
    // Rotate view direction with zero translation
    float3 viewPos = mul(camera.view, float4(input.position, 0.0f)).xyz;
    float4 pos = mul(camera.projection, float4(viewPos, 1.0f));

    // Push to far plane (depth = 1.0 in Vulkan NDC [0, 1])
    output.position = pos.xyww;
    output.UVW = input.position;

    return output;
}
