// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_LIGHTING_HLSLI
#define IGN_LIGHTING_HLSLI

#include "binding_helpers.hlsli"
#include "helpers.hlsli"
#include "pbr.hlsli"
#include "scene.hlsli"
#include "shadow.hlsli"
#include "sh.hlsli"
#include "gi.hlsli"
#include "reflection_probes.hlsli"

struct PSOutput
{
    float4 color : SV_TARGET0;
    uint objectID : SV_TARGET1;
};

static float SelectChannel(float4 value, int channel)
{
    if (channel == 1)
        return value.g;
    if (channel == 2)
        return value.b;
    if (channel == 3)
        return value.a;
    return value.r;
}

static float3 MultiBounceAO(float ao, float3 albedo)
{
    float3 a =  2.0404f * albedo - 0.3324f;
    float3 b = -4.7951f * albedo + 0.6417f;
    float3 c =  2.7552f * albedo + 0.6903f;
    float3 x = float3(ao, ao, ao);
    return saturate(max(x, ((x * a + b) * x + c) * x));
}

static PSOutput EvaluateMeshPixel(
    PixelVertexInput input,
    bool isFrontFace,
    Camera camera,
    Scene scene,
    CascadesShadows csm,
    PointLight pointLights[MAX_POINT_LIGHTS],
    SpotLight spotLights[MAX_SPOT_LIGHTS],
    GIData gi,
    StructuredBuffer<ReflectionProbeGPU> reflectionProbes,
    Material material,
    Texture2D environmentMapTexture,
    Texture2DArray shadowMap,
    SamplerState sampler0,
    SamplerState shadowSampler)
{
    PSOutput result;
    result.objectID = input.objectID;

    float2 tiledUV = input.uv * material.tilingFactor;

    float3 N = normalize(input.normal);
    float3 viewDirection = normalize(camera.position.xyz - input.worldPos);

    float azimuth = scene.lightAngle.x;
    float elevation = scene.lightAngle.y;
    float3 sunDirection = float3(
        cos(elevation) * sin(azimuth),
        sin(elevation),
        cos(elevation) * cos(azimuth)
    );

    float3 lightDirection = normalize(sunDirection);

    Texture2D baseColorTex = ResourceDescriptorHeap[SafeBindlessIndex(material.baseColorTextureIndex)];
    Texture2D emissiveTex = ResourceDescriptorHeap[SafeBindlessIndex(material.emissiveTextureIndex)];
    Texture2D metallicTex = ResourceDescriptorHeap[SafeBindlessIndex(material.metallicTextureIndex)];
    Texture2D roughnessTex = ResourceDescriptorHeap[SafeBindlessIndex(material.roughnessTextureIndex)];
    Texture2D occlusionTex = ResourceDescriptorHeap[SafeBindlessIndex(material.occlusionTextureIndex)];

    float3 emissiveColor = SRGBToLinear(emissiveTex.Sample(sampler0, tiledUV).rgb) * material.emissiveFactor.rgb;
    float4 metallicColor = metallicTex.Sample(sampler0, tiledUV);
    float4 roughnessColor = roughnessTex.Sample(sampler0, tiledUV);
    float occlusionSample = occlusionTex.Sample(sampler0, tiledUV).r;
    float materialAO = lerp(1.0f, occlusionSample, saturate(material.occlusionStrength));
    float screenAO = 1.0f;
    if (gi.captureMode == 0 && gi.aoMode == 2 && gi.gtaoTextureIndex >= 0 && material.blendMode != 1)
    {
        float2 halfTexel = gi.viewportInvSizeAndFlags.xy;
        float2 screenUV = clamp(input.position.xy * gi.viewportInvSizeAndFlags.xy, halfTexel, 1.0f - halfTexel);
        Texture2D gtaoTex = ResourceDescriptorHeap[SafeBindlessIndex(gi.gtaoTextureIndex)];
        float rawGtao = gtaoTex.SampleLevel(sampler0, screenUV, 0.0f).r;
        float expFactor = max(gi.aoPower * gi.aoIntensity, 0.0f);
        screenAO = saturate(pow(max(rawGtao, 1e-4f), expFactor));
    }
    float ao = min(materialAO, screenAO);

    float metallic = clamp(SelectChannel(metallicColor, material.metallicChannel) * material.metallicFactor, 0.0f, 1.0f);
    float roughness = clamp(SelectChannel(roughnessColor, material.roughnessChannel) * material.roughnessFactor, 0.0f, 1.0f);

    // Sample base color with sRGB-to-linear conversion and alpha for transparency support
    float4 rawBaseColor = baseColorTex.Sample(sampler0, tiledUV);
    float4 baseColorSample = float4(SRGBToLinear(rawBaseColor.rgb), rawBaseColor.a) * input.color;
    float finalAlpha = baseColorSample.a * material.baseColorFactor.a;

    // Discard nearly transparent fragments in transparent mode, or below cutoff in masked mode
    if ((material.blendMode == 1 && finalAlpha < 0.001f) || (material.blendMode == 2 && finalAlpha < 0.5f))
    {
        discard;
    }

    // Irradiance probe capture renders with cull NONE and proj[1][1] *= -1; backfaces emit alpha = 0
    // so the SH projection compute shader can measure geometry enclosure / backface fraction.
    if (gi.captureMode == 1 && !isFrontFace)
    {
        result.color = float4(0.0f, 0.0f, 0.0f, 0.0f);
        return result;
    }

    // Tangent space normal mapping with safe orthonormal basis fallback
    float3 finalNormal = N;
    float3 T = input.tangent - N * dot(N, input.tangent);
    float tLenSq = dot(T, T);
    if (tLenSq > 1e-6f)
    {
        T *= rsqrt(tLenSq);
    }
    else
    {
        float3 bFallback;
        BuildOrthonormalBasis(N, T, bFallback);
    }
    float3 bitangent = cross(N, T);
    float3 B = dot(bitangent, input.bitangent) < 0.0f ? -bitangent : bitangent;
    if (material.normalTextureIndex >= 0)
    {
        Texture2D normalMapTex = ResourceDescriptorHeap[SafeBindlessIndex(material.normalTextureIndex)];
        float3 tangentNormal = normalMapTex.Sample(sampler0, tiledUV).rgb * 2.0f - 1.0f;
        finalNormal = normalize(T * tangentNormal.x + B * tangentNormal.y + N * tangentNormal.z);
    }

    if (scene.renderMode == RENDER_MODE_COLOR
        || scene.renderMode == RENDER_MODE_GI_DIFFUSE
        || scene.renderMode == RENDER_MODE_GI_SPECULAR
        || scene.renderMode == RENDER_MODE_AO
        || scene.renderMode == RENDER_MODE_PROBE_WEIGHTS)
    {
        OpenPBRSurface surface;
        surface.baseColor = baseColorSample.rgb * material.baseColorFactor.rgb;
        surface.baseWeight = saturate(material.baseWeight);
        surface.metalness = metallic;
        surface.specularWeight = material.specularWeight;
        surface.specularColor = material.specularColor.rgb;
        surface.specularRoughness = max(roughness, 0.05f);
        surface.specularIOR = max(material.specularIOR, 1.0f);
        surface.coatWeight = saturate(material.coatWeight);
        surface.coatColor = material.coatColor.rgb;
        surface.coatRoughness = max(saturate(material.coatRoughness), 0.05f);
        surface.coatIOR = max(material.coatIOR, 1.0f);
        surface.coatDarkening = saturate(material.coatDarkening);
        surface.specularAnisotropy = saturate(material.specularAnisotropy);
        surface.subsurfaceWeight = saturate(material.subsurfaceWeight);
        surface.subsurfaceColor = material.subsurfaceColor.rgb;
        surface.subsurfaceRadius = material.subsurfaceRadius.rgb;
        surface.subsurfaceScale = material.subsurfaceScale;
        surface.transmissionWeight = saturate(material.transmissionWeight);
        surface.transmissionColor = material.transmissionColor.rgb;
        surface.transmissionDepth = material.transmissionDepth;
        surface.fuzzWeight = saturate(material.fuzzWeight);
        surface.fuzzColor = material.fuzzColor.rgb;
        surface.fuzzRoughness = max(saturate(material.fuzzRoughness), 0.05f);
        surface.useMultiScatter = saturate(material.useMultiScatter);

        const float cameraAltitudeKm = camera.position.y * 0.001f;
        float3 skyDiffuseIrradiance = float3(0.0f, 0.0f, 0.0f);
        if (gi.captureMode != 1 && scene.enableIBL != 0)
        {
            skyDiffuseIrradiance = (gi.skySH[0].w > 0.5f)
                ? EvalSHIrradiance4(gi.skySH, finalNormal)
                : SampleEnvironmentIrradiance(environmentMapTexture, sampler0, finalNormal, scene.skyType, cameraAltitudeKm);
        }

        float3 irradiance = scene.lightColor.rgb * scene.lightColor.w;
        float3 directLighting = OpenPBRDirect(surface, finalNormal, lightDirection, viewDirection, T, B, irradiance);

        float shadowTerm = SampleShadow(csm, shadowMap, shadowSampler, camera.view,
            input.worldPos, finalNormal, lightDirection, input.position.xy);

        directLighting *= shadowTerm;

        // Point Lights PBR
        for (int pi = 0; pi < scene.numPointLights; ++pi)
        {
            float3 lightPos = pointLights[pi].positionAndRange.xyz;
            float range = max(pointLights[pi].positionAndRange.w, 1e-4f);
            float3 color = pointLights[pi].color.rgb;
            float intensity = pointLights[pi].color.a;
            float constAtt = pointLights[pi].attenuation.x;
            float linAtt = pointLights[pi].attenuation.y;
            float quadAtt = pointLights[pi].attenuation.z;

            float3 lightVec = lightPos - input.worldPos;
            float d = length(lightVec);
            if (d > range || d < 1e-5f)
                continue;

            float3 toLight = lightVec / d;
            float localShadow = 1.0f;
            if (gi.captureMode != 0 && gi.bakePointShadowCubeArrayIndex >= 0 && pi < 8)
            {
                TextureCubeArray<float4> localShadowCubes = ResourceDescriptorHeap[SafeBindlessIndex(gi.bakePointShadowCubeArrayIndex)];
                float stored = localShadowCubes.SampleLevel(sampler0, float4(-toLight, float(pi)), 0.0f).r;
                float bias = 0.006f + 0.012f * (1.0f - saturate(dot(finalNormal, toLight)));
                localShadow = (((d / range) - bias) <= stored) ? 1.0f : 0.0f;
            }

            float atten = 1.0f / max(constAtt + linAtt * d + quadAtt * (d * d), 1e-4f);
            atten *= saturate(1.0f - (d / range));

            float3 ptIrradiance = color * intensity * atten * localShadow;
            directLighting += OpenPBRDirect(surface, finalNormal, toLight, viewDirection, T, B, ptIrradiance);
        }

        // Spot Lights PBR
        for (int si = 0; si < scene.numSpotLights; ++si)
        {
            float3 lightPos = spotLights[si].positionAndRange.xyz;
            float range = max(spotLights[si].positionAndRange.w, 1e-4f);
            float3 spotDir = spotLights[si].directionAndAngle.xyz;
            float cosOuter = spotLights[si].directionAndAngle.w;
            float3 color = spotLights[si].color.rgb;
            float intensity = spotLights[si].color.a;
            float constAtt = spotLights[si].attenuation.x;
            float linAtt = spotLights[si].attenuation.y;
            float quadAtt = spotLights[si].attenuation.z;
            float cosInner = spotLights[si].attenuation.w;

            float3 lightVec = lightPos - input.worldPos;
            float d = length(lightVec);
            if (d > range || d < 1e-5f)
                continue;

            float3 toLight = lightVec / d;
            float theta = dot(-toLight, spotDir);
            if (theta < cosOuter)
                continue;

            float localShadow = 1.0f;
            if (gi.captureMode != 0 && gi.bakePointShadowCubeArrayIndex >= 0 && si < 4)
            {
                TextureCubeArray<float4> localShadowCubes = ResourceDescriptorHeap[SafeBindlessIndex(gi.bakePointShadowCubeArrayIndex)];
                float stored = localShadowCubes.SampleLevel(sampler0, float4(-toLight, float(8 + si)), 0.0f).r;
                float bias = 0.006f + 0.012f * (1.0f - saturate(dot(finalNormal, toLight)));
                localShadow = (((d / range) - bias) <= stored) ? 1.0f : 0.0f;
            }

            float atten = 1.0f / max(constAtt + linAtt * d + quadAtt * (d * d), 1e-4f);
            atten *= saturate(1.0f - (d / range));

            float coneFactor = saturate((theta - cosOuter) / max(0.0001f, cosInner - cosOuter));
            atten *= coneFactor;

            float3 spIrradiance = color * intensity * atten * localShadow;
            directLighting += OpenPBRDirect(surface, finalNormal, toLight, viewDirection, T, B, spIrradiance);
        }

        // Diffuse indirect (GI volume / sky irradiance) & Specular indirect (reflection probes + sky IBL).
        // During Irradiance capture (captureMode == 1), never add unoccluded sky irradiance; only include previous bounce GI when enabled.
        float3 ambient = float3(0.0f, 0.0f, 0.0f);
        float3 reflectedSpecular = float3(0.0f, 0.0f, 0.0f);
        float volW = 0.0f;
        float3 volIrradiance = float3(0.0f, 0.0f, 0.0f);
        if (gi.giMode > 0 && gi.biasAndFade.w > 0.5f && gi.viewportInvSizeAndFlags.w > 0.5f)
        {
            volIrradiance = SampleGIIrradiance(input.worldPos, finalNormal, viewDirection, gi, sampler0, volW) * gi.gridMinAndIntensity.w;
        }

        float NdotV = saturate(dot(finalNormal, viewDirection));
        float3 kS = OpenPBRBaseSpecularAlbedo(surface, NdotV);
        float3 kD = (1.0f - kS) * (1.0f - surface.metalness);

        if (gi.captureMode == 0)
        {
            float3 combinedDiff = (gi.giMode > 0 && gi.biasAndFade.w > 0.5f && gi.viewportInvSizeAndFlags.w > 0.5f)
                ? lerp(skyDiffuseIrradiance, volIrradiance, volW)
                : skyDiffuseIrradiance;
            ambient = kD * surface.baseWeight * surface.baseColor * combinedDiff * MultiBounceAO(ao, surface.baseColor);

            // Specular indirect radiance for base and coat roughness lobes
            float3 R = reflect(-viewDirection, finalNormal);
            float3 skyBaseRad = float3(0.0f, 0.0f, 0.0f);
            float3 skyCoatRad = float3(0.0f, 0.0f, 0.0f);
            if (scene.enableIBL != 0)
            {
                if (gi.skyPrefilterCubeIndex >= 0)
                {
                    TextureCube skyPrefilterCube = ResourceDescriptorHeap[SafeBindlessIndex(gi.skyPrefilterCubeIndex)];
                    float maxSkyMip = float(max(gi.skyPrefilterMipCount - 1, 0));
                    skyBaseRad = skyPrefilterCube.SampleLevel(sampler0, R, saturate(surface.specularRoughness) * maxSkyMip).rgb;
                    skyCoatRad = skyPrefilterCube.SampleLevel(sampler0, R, saturate(surface.coatRoughness) * maxSkyMip).rgb;
                }
                else
                {
                    float3 skySharp = SampleEnvironmentMap(environmentMapTexture, sampler0, R, scene.skyType, cameraAltitudeKm);
                    float3 skyBlurBase = SampleRoughEnvironmentSpecular(environmentMapTexture, sampler0, R, surface.specularRoughness, scene.skyType, cameraAltitudeKm);
                    float3 skyBlurCoat = SampleRoughEnvironmentSpecular(environmentMapTexture, sampler0, R, surface.coatRoughness, scene.skyType, cameraAltitudeKm);
                    skyBaseRad = lerp(skySharp, skyBlurBase, smoothstep(0.0f, 0.6f, surface.specularRoughness));
                    skyCoatRad = lerp(skySharp, skyBlurCoat, smoothstep(0.0f, 0.6f, surface.coatRoughness));
                }
            }

            float3 finalBaseRad = skyBaseRad;
            float3 finalCoatRad = skyCoatRad;
            if (gi.reflectionProbeCount > 0 && gi.reflectionCubeArrayIndex >= 0 && gi.viewportInvSizeAndFlags.w > 0.5f)
            {
                float3 probeBaseRad, probeCoatRad;
                float probeWeight = 0.0f;
                EvaluateReflectionProbes(input.worldPos, finalNormal, R, surface.specularRoughness, surface.coatRoughness,
                    combinedDiff, gi, reflectionProbes, sampler0, probeBaseRad, probeCoatRad, probeWeight);
                finalBaseRad = lerp(skyBaseRad, probeBaseRad, probeWeight);
                finalCoatRad = lerp(skyCoatRad, probeCoatRad, probeWeight);
            }

            if (scene.enableIBL != 0 || (gi.reflectionProbeCount > 0 && gi.reflectionCubeArrayIndex >= 0))
            {
                float3 coatF = SchlickFresnelCos(NdotV, DielectricF0(max(surface.coatIOR, 1.0f)));
                float3 coatSpec = surface.coatWeight * coatF;
                float3 throughCoat = lerp(float3(1.0f, 1.0f, 1.0f), saturate(surface.coatColor) * (1.0f - coatF),
                    surface.coatWeight * surface.coatDarkening);
                float3 fuzz = surface.fuzzWeight * surface.fuzzColor * combinedDiff * MultiBounceAO(ao, surface.fuzzColor);

                float baseSO = ComputeSpecularOcclusion(NdotV, ao, surface.specularRoughness);
                float coatSO = ComputeSpecularOcclusion(NdotV, ao, surface.coatRoughness);

                reflectedSpecular = fuzz + (1.0f - surface.fuzzWeight) *
                    (coatSpec * finalCoatRad * coatSO + kS * throughCoat * finalBaseRad * baseSO);
            }
        }
        else if (gi.captureMode == 1)
        {
            if (gi.biasAndFade.w > 0.5f)
            {
                ambient = kD * surface.baseWeight * surface.baseColor * volIrradiance * volW * ao;
            }
        }
        else if (gi.captureMode == 2)
        {
            float3 combinedDiff = (gi.giMode > 0 && gi.biasAndFade.w > 0.5f)
                ? lerp(skyDiffuseIrradiance, volIrradiance, volW)
                : skyDiffuseIrradiance;
            ambient = kD * surface.baseWeight * surface.baseColor * combinedDiff * MultiBounceAO(ao, surface.baseColor);
        }

        if (gi.captureMode != 0)
        {
            float3 captureColor = directLighting + ambient;
            if (length(emissiveColor) > 0.01f)
            {
                captureColor += emissiveColor * material.emissiveFactor.a * material.emissionLuminance;
            }
            result.color = float4(captureColor, 1.0f);
            return result;
        }

        if (scene.renderMode == RENDER_MODE_GI_DIFFUSE)
        {
            result.color = float4(ambient, 1.0f);
            return result;
        }
        if (scene.renderMode == RENDER_MODE_GI_SPECULAR)
        {
            result.color = float4(reflectedSpecular, 1.0f);
            return result;
        }
        if (scene.renderMode == RENDER_MODE_AO)
        {
            result.color = float4(ao, ao, ao, 1.0f);
            return result;
        }
        if (scene.renderMode == RENDER_MODE_PROBE_WEIGHTS)
        {
            result.color = float4(volW, 1.0f - volW, (gi.giMode > 1) ? 0.5f : 0.0f, 1.0f);
            return result;
        }

        float3 finalColor = directLighting + ambient + reflectedSpecular;

        if (length(emissiveColor) > 0.01f)
        {
            // emissiveColor already includes emissiveFactor.rgb
            finalColor += emissiveColor * material.emissiveFactor.a * material.emissionLuminance;
        }

        if (scene.debugShadow == 2)
        {
            result.color = float4(shadowTerm, shadowTerm, shadowTerm, 1.0f);
            return result;
        }

        if (scene.debugShadow == 1)
        {
            float3 viewPos = mul(camera.view, float4(input.worldPos, 1.0f)).xyz;
            float viewDepth = -viewPos.z;
            int ci = GetCascadeIndex(csm.cascadeSplits, viewDepth);
            float3 dbg = ci == 0 ? float3(1.0f, 0.0f, 0.0f)
                : (ci == 1 ? float3(0.0f, 1.0f, 0.0f)
                : (ci == 2 ? float3(0.0f, 0.0f, 1.0f)
                : float3(1.0f, 1.0f, 0.0f)));
            finalColor *= dbg;
        }

        // Output alpha: 1.0 for opaque, actual alpha for transparent
        float outputAlpha = (material.blendMode == 1) ? finalAlpha : 1.0f;
        result.color = float4(finalColor, outputAlpha);
    }
    else if (scene.renderMode == RENDER_MODE_DIFFUSE)
    {
        float3 diffuse = baseColorTex.Sample(sampler0, tiledUV).rgb;
        result.color = float4(diffuse, 1.0f);
    }
    else if (scene.renderMode == RENDER_MODE_NORMALS)
    {
        result.color = float4(finalNormal * 0.5f + 0.5f, 1.0f);
    }
    else if (scene.renderMode == RENDER_MODE_METALLIC)
    {
        result.color = float4(metallic, metallic, metallic, 1.0f);
    }
    else if (scene.renderMode == RENDER_MODE_ROUGHNESS)
    {
        result.color = float4(roughness, roughness, roughness, 1.0f);
    }
    else if (scene.renderMode == RENDER_MODE_SUBSURFACE)
    {
        float ss = material.subsurfaceWeight;
        result.color = float4(material.subsurfaceColor.rgb * ss, 1.0f);
    }
    else if (scene.renderMode == RENDER_MODE_TRANSMISSION)
    {
        float tr = material.transmissionWeight;
        result.color = float4(material.transmissionColor.rgb * tr, 1.0f);
    }
    else if (scene.renderMode == RENDER_MODE_COAT)
    {
        float ct = material.coatWeight;
        result.color = float4(material.coatColor.rgb * ct, 1.0f);
    }
    else if (scene.renderMode == RENDER_MODE_FUZZ)
    {
        float fz = material.fuzzWeight;
        result.color = float4(material.fuzzColor.rgb * fz, 1.0f);
    }

    return result;
}

#endif
