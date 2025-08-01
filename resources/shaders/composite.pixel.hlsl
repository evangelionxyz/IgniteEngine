struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
};

Texture2D sceneTexture : register(t0);
Texture2D edgeDetection : register(t1);

SamplerState linearSampler: register(s0);

float4 main(VSOutput input) : SV_Target
{
    float4 sceneColor = sceneTexture.SampleLevel(linearSampler, input.uv, 0.0f);
    float4 edgeDetectionColor = edgeDetection.SampleLevel(linearSampler, input.uv, 0.0f);
    return sceneColor + edgeDetectionColor;
    // return float4(1.0f, 0.0f, 0.0f, 1.0f);
}
