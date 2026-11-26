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
Texture2D gizmoTexture : register(t9);

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

    float4 taaParams; // x=enableTAA, y=currentFrameWeight, z=historyValid, w=enableOutline
    float4 taaJitter; // xy=currJitterUV, zw=prevJitterUV

    float4x4 projectionInv;
    float4x4 taaReprojection;
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

float3 ApplyFogWithDistance(float3 color, float viewDistance, float2 uv)
{
    if (fogDensity <= 0.0f)
        return color;

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

float CalculateViewDistance(float depth, float2 uv)
{
    float4 clipSpace = float4(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, depth, 1.0f);
    float4 viewSpace = mul(projectionInv, clipSpace);
    viewSpace /= viewSpace.w;
    return abs(viewSpace.z);
}

float3 RGBToYCoCg(float3 rgb)
{
    float Y  = dot(rgb, float3( 0.25f, 0.50f,  0.25f));
    float Co = dot(rgb, float3( 0.50f, 0.00f, -0.50f));
    float Cg = dot(rgb, float3(-0.25f, 0.50f, -0.25f));
    return float3(Y, Co, Cg);
}

float3 YCoCgToRGB(float3 ycocg)
{
    float t = ycocg.x - ycocg.z;
    return float3(t + ycocg.y, ycocg.x + ycocg.z, t - ycocg.y);
}

// Ray-box clip in YCoCg space toward neighborhood mean to prevent oil-painting posterization
float3 ClipAABB(float3 aabbMin, float3 aabbMax, float3 history, float3 avg)
{
    float3 pClip = 0.5f * (aabbMax + aabbMin);
    float3 eClip = max(0.5f * (aabbMax - aabbMin), float3(1e-5f, 1e-5f, 1e-5f));
    float3 vClip = history - pClip;
    float3 vUnit = vClip / eClip;
    float3 aUnit = abs(vUnit);
    float maUnit = max(aUnit.x, max(aUnit.y, aUnit.z));
    if (maUnit > 1.0f)
        return pClip + vClip / maUnit;
    return history;
}

// 5-tap Catmull-Rom bicubic history lookup (eliminates bilinear feedback blur)
float3 SampleHistoryCatmullRom(float2 uv, float2 texSize)
{
    float2 samplePos = uv * texSize;
    float2 texPos1 = floor(samplePos - 0.5f) + 0.5f;
    float2 f = samplePos - texPos1;

    float2 w0 = f * (-0.5f + f * (1.0f - 0.5f * f));
    float2 w1 = 1.0f + f * f * (-2.5f + 1.5f * f);
    float2 w2 = f * (0.5f + f * (2.0f - 1.5f * f));
    float2 w3 = f * f * (-0.5f + 0.5f * f);

    float2 w12 = w1 + w2;
    float2 offset12 = w2 / max(w12, float2(1e-5f, 1e-5f));

    float2 invTexSize = 1.0f / max(texSize, float2(1.0f, 1.0f));
    float2 texPos0  = (texPos1 - 1.0f) * invTexSize;
    float2 texPos3  = (texPos1 + 2.0f) * invTexSize;
    float2 texPos12 = (texPos1 + offset12) * invTexSize;

    float3 result = 0.0f;
    result += taaHistoryTexture.SampleLevel(linearSampler, clamp(float2(texPos12.x, texPos0.y),  0.0f, 1.0f), 0.0f).rgb * (w12.x * w0.y);
    result += taaHistoryTexture.SampleLevel(linearSampler, clamp(float2(texPos0.x,  texPos12.y), 0.0f, 1.0f), 0.0f).rgb * (w0.x  * w12.y);
    result += taaHistoryTexture.SampleLevel(linearSampler, clamp(float2(texPos12.x, texPos12.y), 0.0f, 1.0f), 0.0f).rgb * (w12.x * w12.y);
    result += taaHistoryTexture.SampleLevel(linearSampler, clamp(float2(texPos3.x,  texPos12.y), 0.0f, 1.0f), 0.0f).rgb * (w3.x  * w12.y);
    result += taaHistoryTexture.SampleLevel(linearSampler, clamp(float2(texPos12.x, texPos3.y),  0.0f, 1.0f), 0.0f).rgb * (w12.x * w3.y);
    return max(result, 0.0f);
}

float4 main(VSOutput input) : SV_Target
{
    const bool taaActive = (taaParams.x > 0.5f);
    float3 sceneColor = SampleSceneWithChromAb(input.uv);

    const bool hasFog = (fogDensity > 0.0f);
    const bool hasDOF = (enableDOF > 0);
    float centerDepth = 1.0f;
    bool depthSampled = false;

    if (hasDOF)
    {
        centerDepth = depthTexture.SampleLevel(linearSampler, input.uv, 0.0f).r;
        depthSampled = true;
        float dist = CalculateViewDistance(centerDepth, input.uv);

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
            static const float2 k_PoissonDof[12] =
            {
                float2(-0.326212f, -0.405810f),
                float2(-0.840144f, -0.073580f),
                float2(-0.695914f,  0.457137f),
                float2(-0.203345f,  0.620716f),
                float2( 0.962340f, -0.194983f),
                float2( 0.473434f, -0.480026f),
                float2( 0.519456f,  0.767022f),
                float2( 0.185461f, -0.893124f),
                float2( 0.507431f,  0.064425f),
                float2( 0.896420f,  0.412458f),
                float2(-0.321940f, -0.932615f),
                float2(-0.791559f, -0.597710f)
            };

            float2 texSize;
            sceneTexture.GetDimensions(texSize.x, texSize.y);
            float2 radius = (coc * 2.5f) / max(texSize, float2(1.0f, 1.0f));

            float3 accum = sceneColor;
            float totalWeight = 1.0f;

            [unroll]
            for (int i = 0; i < 12; ++i)
            {
                float2 suv = saturate(input.uv + k_PoissonDof[i] * radius);
                float3 sampleColor = sceneTexture.SampleLevel(linearSampler, suv, 0.0f).rgb;
                accum += sampleColor;
                totalWeight += 1.0f;
            }
            sceneColor = accum / totalWeight;
        }

        if (hasFog)
        {
            sceneColor = ApplyFogWithDistance(sceneColor, dist, input.uv);
        }
    }
    else
    {
        if (hasFog)
        {
            centerDepth = depthTexture.SampleLevel(linearSampler, input.uv, 0.0f).r;
            depthSampled = true;
            float dist = CalculateViewDistance(centerDepth, input.uv);
            sceneColor = ApplyFogWithDistance(sceneColor, dist, input.uv);
        }
    }

    float3 bloomContribution = 0.0f;
    if (flags.x > 0.5f)
    {
        bloomContribution = bloomTexture.SampleLevel(linearSampler, input.uv, 0.0f).rgb * flags.y;
        sceneColor += bloomContribution;
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

    if (taaActive && taaParams.z > 0.5f)
    {
        float2 sceneTexSize;
        sceneTexture.GetDimensions(sceneTexSize.x, sceneTexSize.y);
        float2 sceneTexelSize = 1.0f / max(sceneTexSize, float2(1.0f, 1.0f));

        if (!depthSampled)
            centerDepth = depthTexture.SampleLevel(linearSampler, input.uv, 0.0f).r;

        // Dilated closest-depth lookup across 3x3 cross to stabilize silhouette edges
        float closestDepth = centerDepth;
        float2 closestUV = input.uv;
        static const int2 kCross[4] = { int2(-1, -1), int2(1, -1), int2(-1, 1), int2(1, 1) };
        [unroll]
        for (int c = 0; c < 4; ++c)
        {
            float2 cUV = saturate(input.uv + float2(kCross[c]) * sceneTexelSize);
            float d = depthTexture.SampleLevel(linearSampler, cUV, 0.0f).r;
            if (d < closestDepth)
            {
                closestDepth = d;
                closestUV = cUV;
            }
        }

        // Camera depth reprojection using dilated closest-depth velocity vector
        float4 currClip = float4(closestUV.x * 2.0f - 1.0f, 1.0f - closestUV.y * 2.0f, closestDepth, 1.0f);
        float4 prevClip = mul(taaReprojection, currClip);
        float2 historyUV = input.uv;
        bool validHistoryUV = (prevClip.w > 1e-5f);
        if (validHistoryUV)
        {
            float2 prevNdc = prevClip.xy / prevClip.w;
            float2 prevClosestUV = float2(prevNdc.x * 0.5f + 0.5f, 0.5f - prevNdc.y * 0.5f);
            float2 velocityUV = prevClosestUV - closestUV;
            historyUV = input.uv + velocityUV;
            validHistoryUV = all(historyUV >= 0.0f) && all(historyUV <= 1.0f);
        }

        if (validHistoryUV)
        {
            float2 historyTexSize;
            taaHistoryTexture.GetDimensions(historyTexSize.x, historyTexSize.y);
            float3 historyColor = SampleHistoryCatmullRom(historyUV, historyTexSize);

            // Compute 3x3 YCoCg variance and min/max bounds around the exact current texel
            // without pre-blurring the current frame (pre-blurring + 8-bit history causes oil-painting softness).
            float3 centerRGB = saturate(tonemappedScene);
            float3 centerYCoCg = RGBToYCoCg(centerRGB);
            float3 m1 = centerYCoCg;
            float3 m2 = centerYCoCg * centerYCoCg;
            float3 minYCoCg = centerYCoCg;
            float3 maxYCoCg = centerYCoCg;
            float3 minRGB = centerRGB;
            float3 maxRGB = centerRGB;
            float3 crossSumRGB = 0.0f;

            [unroll]
            for (int ny = -1; ny <= 1; ++ny)
            {
                [unroll]
                for (int nx = -1; nx <= 1; ++nx)
                {
                    if (nx == 0 && ny == 0)
                        continue;

                    float2 nUV = saturate(input.uv + float2(nx, ny) * sceneTexelSize);
                    float3 nLinear = SampleSceneWithChromAb(nUV) + bloomContribution;
                    float3 nTonemapped = saturate(ApplyTonemap(nLinear, tonemapMode, exposure, gamma));
                    float3 sYCoCg = RGBToYCoCg(nTonemapped);

                    m1 += sYCoCg;
                    m2 += sYCoCg * sYCoCg;
                    minYCoCg = min(minYCoCg, sYCoCg);
                    maxYCoCg = max(maxYCoCg, sYCoCg);
                    minRGB = min(minRGB, nTonemapped);
                    maxRGB = max(maxRGB, nTonemapped);
                    if (abs(nx) + abs(ny) == 1)
                        crossSumRGB += nTonemapped;
                }
            }

            float3 mu = m1 * (1.0f / 9.0f);
            float3 sigma = sqrt(max(m2 * (1.0f / 9.0f) - mu * mu, float3(0.0f, 0.0f, 0.0f)));
            sigma = max(sigma, float3(0.001f, 0.0005f, 0.0005f));

            float motionPixels = length((historyUV - input.uv) * historyTexSize);
            float varianceGamma = lerp(1.25f, 0.75f, saturate(motionPixels * 0.5f));
            float3 clipMin = max(minYCoCg, mu - varianceGamma * sigma);
            float3 clipMax = min(maxYCoCg, mu + varianceGamma * sigma);

            float3 historyYCoCg = RGBToYCoCg(historyColor);
            float3 clippedYCoCg = ClipAABB(clipMin, clipMax, historyYCoCg, mu);
            float3 clampedHistory = saturate(YCoCgToRGB(clippedYCoCg));

            // Boost current-frame weight under motion while honoring low user blendFactor at rest,
            // using Karis luma weighting to suppress specular flicker without blurring fine detail.
            float baseWeight = saturate(taaParams.y);
            float motionResponse = saturate(motionPixels * 0.05f);
            float alpha = max(baseWeight, lerp(baseWeight, max(baseWeight, 0.30f), motionResponse));

            float currLuma = dot(centerRGB, float3(0.2126f, 0.7152f, 0.0722f));
            float histLuma = dot(clampedHistory, float3(0.2126f, 0.7152f, 0.0722f));
            float wCurr = alpha / (1.0f + currLuma);
            float wHist = (1.0f - alpha) / (1.0f + histLuma);
            tonemappedScene = saturate((centerRGB * wCurr + clampedHistory * wHist) / max(wCurr + wHist, 1e-5f));
        }
    }

    float4 uiColor = uiTexture.SampleLevel(linearSampler, input.uv, 0.0f);
    float4 debugColor = debugTexture.SampleLevel(linearSampler, input.uv, 0.0f);
    float4 gizmoColor = gizmoTexture.SampleLevel(linearSampler, input.uv, 0.0f);
    
    float3 finalColor = lerp(tonemappedScene, debugColor.rgb, debugColor.a);
    finalColor = lerp(finalColor, uiColor.rgb, uiColor.a);
    if (taaParams.w > 0.5f)
    {
        float4 edgeColor = edgeDetection.SampleLevel(linearSampler, input.uv, 0.0f);
        finalColor = lerp(finalColor, edgeColor.rgb, saturate(edgeColor.a));
    }
    finalColor = finalColor * (1.0f - saturate(gizmoColor.a)) + gizmoColor.rgb;
    return float4(finalColor, 1.0f);
}
