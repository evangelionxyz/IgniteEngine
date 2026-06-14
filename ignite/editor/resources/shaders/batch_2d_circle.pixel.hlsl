#include "include/binding_helpers.hlsli" 
struct PSInput 
{ 
    float4 position : SV_POSITION; 
    float2 localPosition : TEXCOORD; 
    float4 color : COLOR; 
    uint objectID : OBJECTID;
}; 

struct PSOutput 
{ 
    float4 color : SV_TARGET0; 
    uint objectID : SV_TARGET1;
}; 

PSOutput main(PSInput input) 
{
    float d = 1.0 - length(input.localPosition); 
    float circleShape = smoothstep(0.5, 0.5, d);

    PSOutput result;
    result.color = input.color * circleShape;
    result.objectID = input.objectID;
    clip(circleShape <= 0.0001f ? -1.0 : 1.0);

    return result;
}