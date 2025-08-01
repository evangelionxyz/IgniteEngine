#include "include/binding_helpers.hlsli"

struct CameraConstants
{
  float4x4 viewProjection;
  float4 position;
};

DECLARE_PUSH_CONSTANTS(CameraConstants, g_CameraConstants, 0, 0);

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
    output.position = mul(g_CameraConstants.viewProjection, float4(input.position.x, input.position.y, input.position.z, 1.0f));
    return output;
}
