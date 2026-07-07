// Copyright (c) 2026 Evangelion Manuhutu

#include "include/binding_helpers.hlsli"

struct PSInput
{
    float4 position     : SV_POSITION;
    float4 color        : COLOR;
};

struct PSOutput
{
    float4 color : SV_TARGET0;
};

PSOutput main(PSInput input)
{
    // Discard pixel if alpha is zero
    clip(input.color.a == 0.0f ? -1.0f : 1.0f);
    
    PSOutput result;
    result.color = input.color;
    
    return result;
}
