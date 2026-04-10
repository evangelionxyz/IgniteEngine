struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
};

Texture2D lowResTexture : register(t0);
Texture2D highResTexture : register(t1);
SamplerState linearSampler : register(s0);

cbuffer UpsampleParams : register(b0)
{
    float radius;
    float3 _padding;
};

float4 main(VSOutput input) : SV_Target
{
    uint width, height;
    lowResTexture.GetDimensions(width, height);
    float2 texel = 1.0f / float2(width, height);

    float2 offsets[9] =
    {
        float2(0.0f, 0.0f),
        float2(-texel.x * radius, 0.0f),
        float2( texel.x * radius, 0.0f),
        float2(0.0f, -texel.y * radius),
        float2(0.0f,  texel.y * radius),
        float2(-texel.x * radius, -texel.y * radius),
        float2( texel.x * radius, -texel.y * radius),
        float2(-texel.x * radius,  texel.y * radius),
        float2( texel.x * radius,  texel.y * radius)
    };

    float weights[9] =
    {
        4.0f,
        2.0f, 2.0f,
        2.0f, 2.0f,
        1.0f, 1.0f, 1.0f, 1.0f
    };

    float3 sum = float3(0.0f, 0.0f, 0.0f);

    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        float2 sampleUV = saturate(input.uv + offsets[i]);
        float3 sampleColor = lowResTexture.SampleLevel(linearSampler, sampleUV, 0.0f).rgb;
        sampleColor = clamp(sampleColor, 0.0f, 100.0f);
        sum += sampleColor * weights[i];
    }

    float3 upsampled = sum / 16.0f;

    float3 highRes = highResTexture.SampleLevel(linearSampler, input.uv, 0.0f).rgb;
    highRes = clamp(highRes, 0.0f, 100.0f);

    float3 result = clamp(upsampled + highRes, 0.0f, 100.0f);
    return float4(result, 1.0f);
}
