struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
};

Texture2D sceneTexture : register(t0);
Texture2D uiTexture : register(t1);
Texture2D edgeDetection : register(t2);

SamplerState linearSampler: register(s0);

float4 main(VSOutput input) : SV_Target
{
    float4 sceneColor = sceneTexture.SampleLevel(linearSampler, input.uv, 0.0f);
    float4 uiColor = uiTexture.SampleLevel(linearSampler, input.uv, 0.0f);
    float4 edgeColor = edgeDetection.SampleLevel(linearSampler, input.uv, 0.0f);
    
    float3 finalColor = lerp(sceneColor.rgb, uiColor.rgb, uiColor.a);
    finalColor = lerp(finalColor, edgeColor.rgb, saturate(edgeColor.a));
    return float4(finalColor, 1.0f);
}
