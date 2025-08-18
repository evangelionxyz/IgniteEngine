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
    float2 texCoord     : TEXCOORD;
    float2 tilingFactor : TILINGFACTOR;
    float4 color        : COLOR;
    uint texIndex       : TEXINDEX;
};

struct PSInput
{
    float4 position     : SV_POSITION;
    float2 texCoord     : TEXCOORD;
    float2 tilingFactor : TILINGFACTOR;
    float4 color        : COLOR;
    uint texIndex       : TEXINDEX;
};

PSInput main(VSInput input)
{
    PSInput output;
    float4 pos          = float4(input.position.x, input.position.y, input.position.z, 1.0f);
    output.position     = mul(g_CameraConstants.viewProjection, pos);
    output.color        = input.color;
    output.tilingFactor = input.tilingFactor;
    output.texCoord     = input.texCoord;
    output.texIndex     = input.texIndex;
    
    return output;
}