#include "include/helpers.hlsli"

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
};

Texture2D sceneTexture : register(t0);
Texture2D uiTexture : register(t1);
Texture2D edgeDetection : register(t2);
Texture2D bloomTexture : register(t3);
Texture2D ssaoTexture : register(t4);
Texture2D depthTexture : register(t5);
Texture2D debugTexture : register(t6);
Texture2D<uint> objectIDTexture : register(t7);
Texture2D taaHistoryTexture : register(t8);

SamplerState linearSampler : register(s0);

cbuffer CompositePostProcess : register(b0)
{
    float4 flags; // x=enableBloom y=bloomIntensity z=enableVignette w=enableChromAb
    float4 vignetteParams; // x=radius y=softness z=intensity w=chromAbAmount
    float4 chromAbParams; // x=chromAbRadial, y=enableSSAO, z=ssaoIntensity
    float4 vignetteColor;
    
    int tonemapMode;
    float exposure;
    float gamma;
    int enableDOF;

    float focalLength;
    float focalDistance;
    float fStop;
    float focusRange;

    float blurAmount;
    float3 padding_dof;

    float4 fogColor;
    float fogDensity;
    float fogStart;
    float fogEnd;
    float padding_fog;

    float4 taaParams; // x=enableTAA, y=currentFrameWeight, z=historyValid

    float4x4 projectionInv;
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

float3 ApplyFog(float3 color, float depth, float2 uv)
{
    if (fogDensity <= 0.0f)
        return color;

    // Convert depth to view space distance
    float4 clipSpace = float4(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, depth, 1.0f);
    float4 viewSpace = mul(projectionInv, clipSpace);
    viewSpace /= viewSpace.w;

    float viewDistance = abs(viewSpace.z);
    
    // 1. Linear fog for base distance fade
    float linearFog = saturate((viewDistance - fogStart) / max(fogEnd - fogStart, 1e-4f));
    
    // 2. Exponential fog for atmospheric depth
    float expFog = 1.0f - exp(-fogDensity * viewDistance);
    
    // 3. Exponential squared fog for dense atmosphere
    float exp2Fog = 1.0f - exp(-pow(fogDensity * viewDistance * 0.5f, 2.0f));
    
    // 4. Height-based fog simulation (assuming ground plane at y=0)
    float2 ndcPos = uv * 2.0f - 1.0f;
    float heightFactor = max(0.0f, 1.0f - abs(ndcPos.y) * 0.3f); // More fog at horizon
    
    // Combine fog models with weighted blending
    float combinedFog = lerp(linearFog, expFog, 0.6f) + exp2Fog * 0.3f;
    combinedFog *= (1.0f + heightFactor * 0.4f); // Enhance horizon fog
    
    // Distance-based fog color variation (cooler colors at distance)
    float3 distantFogColor = lerp(fogColor.rgb, fogColor.rgb * float3(0.8f, 0.9f, 1.1f), saturate(viewDistance / max(fogEnd, 1e-4f)));
    
    // Atmospheric perspective (slight blue shift at distance)
    float atmosphericStrength = saturate(viewDistance / max(fogEnd * 2.0f, 1e-4f));
    float3 atmosphericColor = lerp(distantFogColor, distantFogColor * float3(0.7f, 0.8f, 1.2f), atmosphericStrength * 0.3f);
    
    float fogFactor = 1.0f - saturate(combinedFog);
    return lerp(atmosphericColor, color, fogFactor);
}

float4 main(VSOutput input) : SV_Target
{
    float3 sceneColor = SampleSceneWithChromAb(input.uv);

    float depth = depthTexture.SampleLevel(linearSampler, input.uv, 0.0f).r;
    
    // Check if there is a transparent object on far plane
    uint2 texDims;
    objectIDTexture.GetDimensions(texDims.x, texDims.y);
    int2 pixelCoords = clamp(int2(input.uv * texDims), int2(0, 0), int2(texDims.x - 1, texDims.y - 1));
    uint objID = objectIDTexture.Load(int3(pixelCoords, 0));
    if (objID != 0xFFFFFFFFu && depth >= 0.999f)
    {
        depth = 0.0f; // Disable fog for transparent object in empty sky
    }
    
    // Convert depth to view space distance
    float4 clipSpace = float4(input.uv.x * 2.0f - 1.0f, 1.0f - input.uv.y * 2.0f, depth, 1.0f);
    float4 viewSpace = mul(projectionInv, clipSpace);
    viewSpace /= viewSpace.w;
    float dist = abs(viewSpace.z);

    if (enableDOF > 0)
    {
        // Calculate circle of confusion (CoC)
        float distanceFromFocus = abs(dist - focalDistance);
        float coc = 0.0f;
        if (distanceFromFocus > focusRange)
        {
            coc = (distanceFromFocus - focusRange) * focalLength / (fStop * max(dist, 0.1f));
        }
        coc = min(coc, blurAmount);

        if (coc >= 0.1f)
        {
            float4 accum = float4(0.0f, 0.0f, 0.0f, 0.0f);
            float totalWeight = 0.0f;
            int samples = 9; // balance quality/perf
            float2 texSize;
            sceneTexture.GetDimensions(texSize.x, texSize.y);
            
            for (int i = -samples/2; i <= samples/2; ++i)
            {
                for (int j = -samples/2; j <= samples/2; ++j)
                {
                    float2 offset = float2(i, j) * coc / texSize;
                    float2 suv = input.uv + offset;
                    if (all(suv >= 0.0f) && all(suv <= 1.0f))
                    {
                        float3 sampleColor = SampleSceneWithChromAb(suv);
                        
                        if (chromAbParams.y > 0.5f)
                        {
                            float ao = ssaoTexture.SampleLevel(linearSampler, suv, 0.0f).r;
                            sampleColor *= lerp(1.0f, ao, chromAbParams.z);
                        }

                        float sampleDepth = depthTexture.SampleLevel(linearSampler, suv, 0.0f).r;
                        int2 sampleCoords = clamp(int2(suv * texDims), int2(0, 0), int2(texDims.x - 1, texDims.y - 1));
                        uint sampleObjID = objectIDTexture.Load(int3(sampleCoords, 0));
                        if (sampleObjID != 0xFFFFFFFFu && sampleDepth >= 0.999f)
                        {
                            sampleDepth = 0.0f;
                        }
                        sampleColor = ApplyFog(sampleColor, sampleDepth, suv);
                        accum += float4(sampleColor, 1.0f);
                        totalWeight += 1.0f;
                    }
                }
            }
            if (totalWeight > 0.0f)
                sceneColor = accum.rgb / totalWeight;
        }
        else
        {
            if (chromAbParams.y > 0.5f)
            {
                float ao = ssaoTexture.SampleLevel(linearSampler, input.uv, 0.0f).r;
                sceneColor *= lerp(1.0f, ao, chromAbParams.z);
            }
            sceneColor = ApplyFog(sceneColor, depth, input.uv);
        }
    }
    else
    {
        if (chromAbParams.y > 0.5f)
        {
            float ao = ssaoTexture.SampleLevel(linearSampler, input.uv, 0.0f).r;
            sceneColor *= lerp(1.0f, ao, chromAbParams.z);
        }
        sceneColor = ApplyFog(sceneColor, depth, input.uv);
    }

    if (flags.x > 0.5f)
    {
        float3 bloom = bloomTexture.SampleLevel(linearSampler, input.uv, 0.0f).rgb;
        sceneColor += bloom * flags.y;
    }

    if (flags.z > 0.5f)
    {
        float2 fromCenter = input.uv - float2(0.5f, 0.5f);
        float distCenter = length(fromCenter);
        float inner = max(vignetteParams.x - vignetteParams.y, 0.0f);
        float vignetteMask = smoothstep(inner, max(vignetteParams.x, inner + 1e-4f), distCenter);
        float vignetteAmount = saturate(vignetteMask * vignetteParams.z);
        sceneColor = lerp(sceneColor, vignetteColor.rgb, vignetteAmount);
    }

    // Apply tonemapping
    float3 tonemappedScene = ApplyTonemap(sceneColor, tonemapMode, exposure, gamma);

    if (taaParams.x > 0.5f && taaParams.z > 0.5f)
    {
        float3 historyColor = taaHistoryTexture.SampleLevel(linearSampler, input.uv, 0.0f).rgb;

        // Neighborhood color clamping: build a 3x3 AABB in tonemapped space and clamp
        // the history into it. This kills ghosting when the camera moves without needing
        // motion vectors.
        float2 sceneTexelSize;
        sceneTexture.GetDimensions(sceneTexelSize.x, sceneTexelSize.y);
        sceneTexelSize = 1.0f / sceneTexelSize;

        float3 neighborMin = tonemappedScene;
        float3 neighborMax = tonemappedScene;

        [unroll]
        for (int ny = -1; ny <= 1; ++ny)
        {
            [unroll]
            for (int nx = -1; nx <= 1; ++nx)
            {
                if (nx == 0 && ny == 0) continue;
                float2 nUV = saturate(input.uv + float2(nx, ny) * sceneTexelSize);
                float3 nColor = ApplyTonemap(SampleSceneWithChromAb(nUV), tonemapMode, exposure, gamma);
                neighborMin = min(neighborMin, nColor);
                neighborMax = max(neighborMax, nColor);
            }
        }

        // Clamp history into the current-frame neighborhood before blending
        float3 clampedHistory = clamp(historyColor, neighborMin, neighborMax);
        tonemappedScene = lerp(clampedHistory, tonemappedScene, saturate(taaParams.y));
    }

    float4 uiColor = uiTexture.SampleLevel(linearSampler, input.uv, 0.0f);
    float4 edgeColor = edgeDetection.SampleLevel(linearSampler, input.uv, 0.0f);
    float4 debugColor = debugTexture.SampleLevel(linearSampler, input.uv, 0.0f);
    
    float3 finalColor = lerp(tonemappedScene, debugColor.rgb, debugColor.a);
    finalColor = lerp(finalColor, uiColor.rgb, uiColor.a);
    finalColor = lerp(finalColor, edgeColor.rgb, saturate(edgeColor.a));
    return float4(finalColor, 1.0f);
}
