struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
};

Texture2D sourceTexture : register(t0);
SamplerState linearSampler : register(s0);

cbuffer BlurParams : register(b0)
{
    int horizontal;
    float radius;
    float2 _padding;
};

static const float weights[7] =
{
    0.1996756f,
    0.1762131f,
    0.1209854f,
    0.0647588f,
    0.0269955f,
    0.0087643f,
    0.0022077f
};

float4 main(VSOutput input) : SV_Target
{
    uint width, height;
    sourceTexture.GetDimensions(width, height);
    float2 texel = 1.0f / float2(width, height);
    float2 direction = horizontal == 1 ? float2(texel.x, 0.0f) : float2(0.0f, texel.y);
    direction *= max(radius, 0.25f);

    float3 result = sourceTexture.SampleLevel(linearSampler, input.uv, 0.0f).rgb * weights[0];

    [unroll]
    for (int i = 1; i < 7; ++i)
    {
        float2 offset = direction * float(i);
        result += sourceTexture.SampleLevel(linearSampler, saturate(input.uv + offset), 0.0f).rgb * weights[i];
        result += sourceTexture.SampleLevel(linearSampler, saturate(input.uv - offset), 0.0f).rgb * weights[i];
    }

    return float4(result, 1.0f);
}
