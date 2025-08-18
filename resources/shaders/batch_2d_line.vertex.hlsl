#include "include/binding_helpers.hlsli"

struct CameraConstants
{
  float4x4 viewProjection;
  float4 position;
};

DECLARE_PUSH_CONSTANTS(CameraConstants, g_CameraConstants, 0, 0);

struct VSInput
{
    float3 position     : POSITION;
    float4 color        : COLOR;
};

struct PSInput
{
    float4 position     : SV_POSITION;
    float4 color        : COLOR;
};

PSInput main(VSInput input)
{
    PSInput output;
    float4 pos          = float4(input.position.x, input.position.y, input.position.z, 1.0f);
    output.position     = mul(g_CameraConstants.viewProjection, pos);
    output.color        = input.color;
    return output;
}