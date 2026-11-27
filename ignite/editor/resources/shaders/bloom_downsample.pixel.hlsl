struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
};

Texture2D sourceTexture : register(t0);
SamplerState linearSampler : register(s0);

cbuffer DownsampleParams : register(b0)
{
    float threshold;
    float intensity;
    float knee;
    float padding;
};

float3 Prefilter(float3 color)
{
    float brightness = max(max(color.r, color.g), color.b);

    // Soft knee
    float rq = clamp(brightness - threshold + knee, 0.0f, 2.0f * knee);
    rq = (rq * rq) / (4.0f * knee + 1e-4f);

    float weight = max(rq, brightness - threshold) / max(brightness, 1e-4f);
    return color * saturate(weight);
}

float4 main(VSOutput input) : SV_Target
{
    uint width, height;
    sourceTexture.GetDimensions(width, height);
    float2 texel = 1.0f / float2(width, height);

    float3 sum = float3(0.0f, 0.0f, 0.0f);
    if (threshold > 0.0f)
    {
        sum += Prefilter(sourceTexture.SampleLevel(linearSampler, input.uv, 0.0f).rgb) * 0.125f;

        sum += Prefilter(sourceTexture.SampleLevel(linearSampler, input.uv + texel * float2(-0.5f, -0.5f), 0.0f).rgb) * 0.125f;
        sum += Prefilter(sourceTexture.SampleLevel(linearSampler, input.uv + texel * float2( 0.5f, -0.5f), 0.0f).rgb) * 0.125f;
        sum += Prefilter(sourceTexture.SampleLevel(linearSampler, input.uv + texel * float2(-0.5f,  0.5f), 0.0f).rgb) * 0.125f;
        sum += Prefilter(sourceTexture.SampleLevel(linearSampler, input.uv + texel * float2( 0.5f,  0.5f), 0.0f).rgb) * 0.125f;

        sum += Prefilter(sourceTexture.SampleLevel(linearSampler, input.uv + texel * float2(-1.0f,  0.0f), 0.0f).rgb) * 0.0625f;
        sum += Prefilter(sourceTexture.SampleLevel(linearSampler, input.uv + texel * float2( 1.0f,  0.0f), 0.0f).rgb) * 0.0625f;
        sum += Prefilter(sourceTexture.SampleLevel(linearSampler, input.uv + texel * float2( 0.0f, -1.0f), 0.0f).rgb) * 0.0625f;
        sum += Prefilter(sourceTexture.SampleLevel(linearSampler, input.uv + texel * float2( 0.0f,  1.0f), 0.0f).rgb) * 0.0625f;

        sum += Prefilter(sourceTexture.SampleLevel(linearSampler, input.uv + texel * float2(-1.0f, -1.0f), 0.0f).rgb) * 0.03125f;
        sum += Prefilter(sourceTexture.SampleLevel(linearSampler, input.uv + texel * float2( 1.0f, -1.0f), 0.0f).rgb) * 0.03125f;
        sum += Prefilter(sourceTexture.SampleLevel(linearSampler, input.uv + texel * float2(-1.0f,  1.0f), 0.0f).rgb) * 0.03125f;
        sum += Prefilter(sourceTexture.SampleLevel(linearSampler, input.uv + texel * float2( 1.0f,  1.0f), 0.0f).rgb) * 0.03125f;
    }
    else
    {
        sum += sourceTexture.SampleLevel(linearSampler, input.uv, 0.0f).rgb * 0.125f;

        sum += sourceTexture.SampleLevel(linearSampler, input.uv + texel * float2(-0.5f, -0.5f), 0.0f).rgb * 0.125f;
        sum += sourceTexture.SampleLevel(linearSampler, input.uv + texel * float2( 0.5f, -0.5f), 0.0f).rgb * 0.125f;
        sum += sourceTexture.SampleLevel(linearSampler, input.uv + texel * float2(-0.5f,  0.5f), 0.0f).rgb * 0.125f;
        sum += sourceTexture.SampleLevel(linearSampler, input.uv + texel * float2( 0.5f,  0.5f), 0.0f).rgb * 0.125f;

        sum += sourceTexture.SampleLevel(linearSampler, input.uv + texel * float2(-1.0f,  0.0f), 0.0f).rgb * 0.0625f;
        sum += sourceTexture.SampleLevel(linearSampler, input.uv + texel * float2( 1.0f,  0.0f), 0.0f).rgb * 0.0625f;
        sum += sourceTexture.SampleLevel(linearSampler, input.uv + texel * float2( 0.0f, -1.0f), 0.0f).rgb * 0.0625f;
        sum += sourceTexture.SampleLevel(linearSampler, input.uv + texel * float2( 0.0f,  1.0f), 0.0f).rgb * 0.0625f;

        sum += sourceTexture.SampleLevel(linearSampler, input.uv + texel * float2(-1.0f, -1.0f), 0.0f).rgb * 0.03125f;
        sum += sourceTexture.SampleLevel(linearSampler, input.uv + texel * float2( 1.0f, -1.0f), 0.0f).rgb * 0.03125f;
        sum += sourceTexture.SampleLevel(linearSampler, input.uv + texel * float2(-1.0f,  1.0f), 0.0f).rgb * 0.03125f;
        sum += sourceTexture.SampleLevel(linearSampler, input.uv + texel * float2( 1.0f,  1.0f), 0.0f).rgb * 0.03125f;
    }

    return float4(sum * intensity, 1.0f);
}
