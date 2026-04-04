struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
};

Texture2D sceneTexture : register(t0);
Texture2D uiTexture : register(t1);
Texture2D edgeDetection : register(t2);
Texture2D bloomTexture : register(t3);

SamplerState linearSampler: register(s0);

cbuffer CompositePostProcess : register(b0)
{
    float4 flags; // x=enableBloom y=bloomIntensity z=enableVignette w=enableChromAb
    float4 vignetteParams; // x=radius y=softness z=intensity w=chromAbAmount
    float4 chromAbParams; // x=chromAbRadial
    float4 vignetteColor;
}

float3 SampleSceneWithChromAb(float2 uv)
{
    if (flags.w < 0.5f)
    {
        return sceneTexture.SampleLevel(linearSampler, uv, 0.0f).rgb;
    }

    float2 center = float2(0.5f, 0.5f);
    float2 dir = uv - center;
    float dist = length(dir);
    float2 offset = normalize(dir + 1e-6f) * vignetteParams.w * (1.0f + chromAbParams.x * dist);

    float r = sceneTexture.SampleLevel(linearSampler, saturate(uv + offset), 0.0f).r;
    float g = sceneTexture.SampleLevel(linearSampler, uv, 0.0f).g;
    float b = sceneTexture.SampleLevel(linearSampler, saturate(uv - offset), 0.0f).b;
    return float3(r, g, b);
}

float4 main(VSOutput input) : SV_Target
{
    float3 sceneColor = SampleSceneWithChromAb(input.uv);

    if (flags.x > 0.5f)
    {
        float3 bloom = bloomTexture.SampleLevel(linearSampler, input.uv, 0.0f).rgb;
        sceneColor += bloom * flags.y;
    }

    if (flags.z > 0.5f)
    {
        float2 fromCenter = input.uv - float2(0.5f, 0.5f);
        float dist = length(fromCenter);
        float inner = max(vignetteParams.x - vignetteParams.y, 0.0f);
        float vignetteMask = smoothstep(inner, max(vignetteParams.x, inner + 1e-4f), dist);
        float vignetteAmount = saturate(vignetteMask * vignetteParams.z);
        sceneColor = lerp(sceneColor, vignetteColor.rgb, vignetteAmount);
    }

    float4 uiColor = uiTexture.SampleLevel(linearSampler, input.uv, 0.0f);
    float4 edgeColor = edgeDetection.SampleLevel(linearSampler, input.uv, 0.0f);
    
    float3 finalColor = lerp(sceneColor, uiColor.rgb, uiColor.a);
    finalColor = lerp(finalColor, edgeColor.rgb, saturate(edgeColor.a));
    return float4(finalColor, 1.0f);
}
