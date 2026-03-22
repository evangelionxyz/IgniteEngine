#include "include/binding_helpers.hlsli" 
struct PSInput 
{ 
    float4 position : SV_POSITION; 
    float2 localPosition : TEXCOORD; 
    float4 color : COLOR; 
}; 

struct PSOutput 
{ 
    float4 color : SV_TARGET0; 
}; 

PSOutput main(PSInput input) 
{
    float d = 1.0 - length(input.localPosition); 
    float circleShape = smoothstep(0.5, 0.5, d);

    PSOutput result;
    result.color = input.color * circleShape;
    clip(circleShape <= 0.0001f ? -1.0 : 1.0);

    return result;
}