#include "include/helpers.hlsli"

struct Scene
{
    float4 lightColor;        // w component can store lightIntensity
    float2 lightAngle;
    float sunAngularRadius;
    int renderMode;
    
    float exposure;
    float gamma;
    float ambient;
    float padding;            // Explicit padding for 16-byte alignment
};

cbuffer SceneBuffer : register(b1) { Scene scene; }

struct PSInput
{
    float4 position : SV_POSITION;
    float3 UVW      : UVW;
};

Texture2D texture0 : register(t0);
SamplerState sampler0 : register(s0);

struct PSOutput
{
    float4 color : SV_TARGET0;
};

PSOutput main(PSInput input)
{
    PSOutput result;
    float3 dir = normalize(input.UVW);
    float3 color = SampleSphericalMap(texture0, sampler0, dir);

    result.color = float4(FilmicTonemap(color, scene.exposure, scene.gamma), 1.0f);

    return result;
}