struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

Texture2D sceneTexture : register(t0);
SamplerState linearSampler : register(s0);

float4 main(VSOutput input) : SV_Target
{
    return sceneTexture.SampleLevel(linearSampler, input.uv, 0.0f);
}
