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
    float3 position : POSITION;
};

struct PSInput
{
    float4 position : SV_POSITION;
};

PSInput main(VSInput input)
{
    PSInput output;
    output.position = mul(mul(camera.projection, camera.view), float4(input.position.x, input.position.y, input.position.z, 1.0f));
    return output;
}
