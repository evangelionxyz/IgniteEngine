#include "include/binding_helpers.hlsli"
#include "include/materials/mat2d.hlsli"
#include "include/materials/mat2d_unlit.pixel.hlsli"
#include "include/materials/mat2d_lit.pixel.hlsli"

struct PSInput
{
    float4 position     : SV_POSITION;
    float3 worldPosition: WORLDPOS;
    float2 texCoord     : TEXCOORD;
    float2 tilingFactor : TILINGFACTOR;
    float4 color        : COLOR;
    float4 additiveColor: ADDITIVECOLOR;
    uint texIndex       : TEXINDEX;
    uint materialType   : MATTYPE;
};

cbuffer Material2DLightingBuffer : register(b1, space0)
{
    Material2DLighting material2DLighting;
}

Texture2D textures[]    : register(t0);
SamplerState samplerState : register(s0);

struct PSOutput
{
    float4 color : SV_TARGET0;
};

PSOutput main(PSInput input)
{
    float4 texColor = textures[input.texIndex].Sample(samplerState, input.texCoord * input.tilingFactor);
    Material2D material;
    material.baseColor = input.color;
    material.additiveColor = input.additiveColor;
    material.tiling = input.tilingFactor;
    material.materialType = input.materialType;

    float4 finalColor = (material.materialType == MATERIAL_2D_LIT)
        ? ComputeMaterial2DLit(material, material2DLighting, input.worldPosition, texColor)
        : ComputeMaterial2DUnlit(material, texColor);
    
    // Discard pixel if alpha is zero
    clip(finalColor.a == 0.0f ? -1.0f : 1.0f);
    
    PSOutput result;
    result.color = finalColor;

    return result;
}