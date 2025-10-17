struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
};

Texture2D mainTexture : register(t0);
SamplerState linearSampler: register(s0);

float4 main(VSOutput input) : SV_Target
{
    return mainTexture.SampleLevel(linearSampler, input.uv, 0.0f);
}
