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

static void SampleEnvAndProbesAtDir(
    float3 worldPos,
    float3 N,
    float3 dir,
    float rough,
    float3 coatDir,
    float coatRough,
    bool evaluateCoat,
    float3 combinedDiff,
    float cameraAltitudeKm,
    Scene scene,
    GIData gi,
    StructuredBuffer<ReflectionProbeGPU> reflectionProbes,
    Texture2D environmentMapTexture,
    SamplerState sampler0,
    out float3 outBase,
    out float3 outCoat)
{
    if (scene.enableIBL == 0 && (gi.reflectionProbeCount <= 0 || gi.reflectionCubeArrayIndex < 0))
    {
        outBase = float3(0.0f, 0.0f, 0.0f);
        outCoat = float3(0.0f, 0.0f, 0.0f);
        return;
    }
    float3 skyB = float3(0.0f, 0.0f, 0.0f);
    float3 skyC = float3(0.0f, 0.0f, 0.0f);
    if (scene.enableIBL != 0)
    {
        if (gi.skyPrefilterCubeIndex >= 0)
        {
            TextureCube skyPrefilterCube = ResourceDescriptorHeap[SafeBindlessIndex(gi.skyPrefilterCubeIndex)];
            float maxSkyMip = float(max(gi.skyPrefilterMipCount - 1, 0));
            skyB = skyPrefilterCube.SampleLevel(sampler0, dir, saturate(rough) * maxSkyMip).rgb;
            if (evaluateCoat)
            {
                skyC = skyPrefilterCube.SampleLevel(sampler0, coatDir, saturate(coatRough) * maxSkyMip).rgb;
            }
        }
        else
        {
            float3 skySharpB = SampleEnvironmentMap(environmentMapTexture, sampler0, dir, scene.skyType, cameraAltitudeKm);
            float3 skyBlurB = SampleRoughEnvironmentSpecular(environmentMapTexture, sampler0, dir, rough, scene.skyType, cameraAltitudeKm);
            skyB = lerp(skySharpB, skyBlurB, smoothstep(0.0f, 0.6f, rough));

            if (evaluateCoat)
            {
                float3 skySharpC = SampleEnvironmentMap(environmentMapTexture, sampler0, coatDir, scene.skyType, cameraAltitudeKm);
                float3 skyBlurC = SampleRoughEnvironmentSpecular(environmentMapTexture, sampler0, coatDir, coatRough, scene.skyType, cameraAltitudeKm);
                skyC = lerp(skySharpC, skyBlurC, smoothstep(0.0f, 0.6f, coatRough));
            }
        }
    }

    outBase = skyB;
    outCoat = skyC;
    if (gi.reflectionProbeCount > 0 && gi.reflectionCubeArrayIndex >= 0 && gi.viewportInvSizeAndFlags.w > 0.5f)
    {
        float3 probeB, probeC;
        float probeWeight = 0.0f;
        EvaluateReflectionProbes(worldPos, N, dir, rough, coatRough, evaluateCoat,
            combinedDiff, gi, reflectionProbes, sampler0, probeB, probeC, probeWeight);
        outBase = lerp(skyB, probeB, probeWeight);
        if (evaluateCoat)
        {
            outCoat = lerp(skyC, probeC, probeWeight);
        }
    }
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

    float3 lightDirection = (dot(scene.sunDirection.xyz, scene.sunDirection.xyz) > 1e-6f)
        ? scene.sunDirection.xyz
        : normalize(float3(
            cos(scene.lightAngle.y) * sin(scene.lightAngle.x),
            sin(scene.lightAngle.y),
            cos(scene.lightAngle.y) * cos(scene.lightAngle.x)));

    // Sample base color early so alpha-tested/discarded fragments avoid extra texture fetches
    float4 rawBaseColor = float4(1.0f, 1.0f, 1.0f, 1.0f);
    if (material.baseColorTextureIndex >= 0)
    {
        Texture2D baseColorTex = ResourceDescriptorHeap[SafeBindlessIndex(material.baseColorTextureIndex)];
        rawBaseColor = baseColorTex.Sample(sampler0, tiledUV);
    }
    float4 baseColorSample = float4(SRGBToLinear(rawBaseColor.rgb), rawBaseColor.a) * input.color;
    float finalAlpha = baseColorSample.a * material.baseColorFactor.a;

#ifndef IGN_OPAQUE_NO_DISCARD
    // Discard nearly transparent fragments in transparent mode, or below cutoff in masked mode
    float alphaCutoff = (material.alphaCutoff > 0.0f) ? material.alphaCutoff : 0.5f;
    if ((material.blendMode == 1 && finalAlpha < 0.001f) || (material.blendMode == 2 && finalAlpha < alphaCutoff))
    {
        discard;
    }
#endif

    // Irradiance probe capture renders with cull NONE and proj[1][1] *= -1; backfaces emit alpha = 0
    // so the SH projection compute shader can measure geometry enclosure / backface fraction.
    if (gi.captureMode == 1 && !isFrontFace)
    {
        result.color = float4(0.0f, 0.0f, 0.0f, 0.0f);
        return result;
    }

    float3 emissiveColor = float3(0.0f, 0.0f, 0.0f);
    if (any(material.emissiveFactor.rgb > 0.0f) && material.emissiveFactor.a > 0.0f && material.emissionLuminance > 0.0f)
    {
        if (material.emissiveTextureIndex >= 0)
        {
            Texture2D emissiveTex = ResourceDescriptorHeap[SafeBindlessIndex(material.emissiveTextureIndex)];
            emissiveColor = SRGBToLinear(emissiveTex.Sample(sampler0, tiledUV).rgb) * material.emissiveFactor.rgb;
        }
        else
        {
            emissiveColor = material.emissiveFactor.rgb;
        }
    }

    float metallic = 0.0f;
    if (material.metallicFactor > 0.001f)
    {
        if (material.metallicTextureIndex >= 0)
        {
            Texture2D metallicTex = ResourceDescriptorHeap[SafeBindlessIndex(material.metallicTextureIndex)];
            float4 metallicColor = metallicTex.Sample(sampler0, tiledUV);
            metallic = saturate(SelectChannel(metallicColor, material.metallicChannel) * material.metallicFactor);
        }
        else
        {
            metallic = saturate(material.metallicFactor);
        }
    }

    float roughness = 0.0f;
    if (material.roughnessFactor > 0.001f)
    {
        if (material.roughnessTextureIndex >= 0)
        {
            Texture2D roughnessTex = ResourceDescriptorHeap[SafeBindlessIndex(material.roughnessTextureIndex)];
            float4 roughnessColor = roughnessTex.Sample(sampler0, tiledUV);
            roughness = saturate(SelectChannel(roughnessColor, material.roughnessChannel) * material.roughnessFactor);
        }
        else
        {
            roughness = saturate(material.roughnessFactor);
        }
    }

    float materialAO = 1.0f;
    if (material.occlusionStrength > 0.001f && material.occlusionTextureIndex >= 0)
    {
        Texture2D occlusionTex = ResourceDescriptorHeap[SafeBindlessIndex(material.occlusionTextureIndex)];
        float occlusionSample = occlusionTex.Sample(sampler0, tiledUV).r;
        materialAO = lerp(1.0f, occlusionSample, saturate(material.occlusionStrength));
    }
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

    // Tangent space normal mapping with safe orthonormal basis fallback.
    // Under frontCounterClockwise = false + negative viewport height, double-sided backfaces
    // in the main color pass arrive with isFrontFace == true; flip them toward the viewer.
    if ((gi.captureMode == 0 && isFrontFace) || (gi.captureMode == 2 && !isFrontFace))
    {
        N = -N;
    }
    float3 finalNormal = N;
    float3 T = float3(1.0f, 0.0f, 0.0f);
    float3 B = float3(0.0f, 1.0f, 0.0f);
    float normalMapLength = 1.0f;
    const bool needsTangentFrame = (material.normalTextureIndex >= 0) || (material.specularAnisotropy > 1e-4f);
    if (needsTangentFrame)
    {
        T = input.tangent - N * dot(N, input.tangent);
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
        B = dot(bitangent, input.bitangent) < 0.0f ? -bitangent : bitangent;
        if (material.normalTextureIndex >= 0)
        {
            Texture2D normalMapTex = ResourceDescriptorHeap[SafeBindlessIndex(material.normalTextureIndex)];
            float3 tangentNormal = normalMapTex.Sample(sampler0, tiledUV).rgb * 2.0f - 1.0f;
            normalMapLength = length(tangentNormal);
            tangentNormal.xy *= material.normalScale;
            finalNormal = normalize(T * tangentNormal.x + B * tangentNormal.y + N * tangentNormal.z);
        }
    }

    if (gi.captureMode == 0 && (gi.renderFeatureFlags & 1u) != 0u)
    {
        roughness = ComputeGeometricSpecularAA(N, roughness, 0.25f, 0.18f);
        if (material.normalTextureIndex >= 0)
        {
            roughness = ComputeGeometricSpecularAA(finalNormal, roughness, 0.12f, 0.12f);
            roughness = ComputeToksvigSpecularAA(roughness, normalMapLength);
        }
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
        float coatRough = max(saturate(material.coatRoughness), 0.05f);
        if (gi.captureMode == 0 && (gi.renderFeatureFlags & 1u) != 0u && surface.coatWeight > 0.001f)
        {
            coatRough = ComputeGeometricSpecularAA(N, coatRough, 0.25f, 0.18f);
        }
        surface.coatRoughness = coatRough;
        surface.coatIOR = max(material.coatIOR, 1.0f);
        surface.coatDarkening = saturate(material.coatDarkening);
        surface.specularAnisotropy = saturate(material.specularAnisotropy);
        surface.specularRotation = material.specularRotation;
        surface.subsurfaceWeight = saturate(material.subsurfaceWeight);
        surface.subsurfaceColor = material.subsurfaceColor.rgb;
        surface.subsurfaceRadius = material.subsurfaceRadius.rgb;
        surface.subsurfaceScale = material.subsurfaceScale;
        surface.transmissionWeight = saturate(material.transmissionWeight);
        surface.transmissionColor = material.transmissionColor.rgb;
        surface.transmissionDepth = material.transmissionDepth;
        surface.transmissionScatter = saturate(material.transmissionScatter);
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
        else if (gi.captureMode == 1 && gi.biasAndFade.w > 0.5f
            && gi.skyVisibilitySplit != 0 && gi.skyVisibilityTexIndex >= 0 && gi.skySH[0].w > 0.5f)
        {
            // Bounce >= 1 when SkyVisibilitySplit is enabled: include sky diffuse attenuated by
            // previous-bounce sky visibility so skylight bounces into enclosed arches and corridors.
            skyDiffuseIrradiance = EvalSHIrradiance4(gi.skySH, finalNormal);
        }

        const bool receiveShadow = (input.receiveShadow != 0u);

        float3 directLighting = float3(0.0f, 0.0f, 0.0f);
        float shadowTerm = 1.0f;
        float sunNdotL = dot(finalNormal, lightDirection);
        if (sunNdotL > 0.0f || scene.debugShadow == 2)
        {
            if (receiveShadow)
            {
                shadowTerm = SampleShadow(csm, shadowMap, shadowSampler, camera.view,
                    input.worldPos, finalNormal, lightDirection, input.position.xy);
            }
            if (sunNdotL > 0.0f && shadowTerm > 1e-4f)
            {
                float3 irradiance = scene.lightColor.rgb * (scene.lightColor.w * shadowTerm);
                directLighting = OpenPBRDirect(surface, finalNormal, lightDirection, viewDirection, T, B, irradiance);
            }
        }

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
            if (dot(finalNormal, toLight) <= 0.0f)
                continue;

            float localShadow = 1.0f;
            if (receiveShadow)
            {
                localShadow = SampleLocalPointOrSpotShadow(
                    gi.bakePointShadowCubeArrayIndex,
                    gi.dynamicPointShadowCubeArrayIndex,
                    sampler0,
                    pointLights[pi].shadowParams,
                    lightPos,
                    range,
                    input.worldPos,
                    finalNormal,
                    input.position.xy);
                if (localShadow <= 1e-4f)
                    continue;
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
            if (dot(finalNormal, toLight) <= 0.0f)
                continue;

            float theta = dot(-toLight, spotDir);
            if (theta < cosOuter)
                continue;

            float localShadow = 1.0f;
            if (receiveShadow)
            {
                localShadow = SampleLocalPointOrSpotShadow(
                    gi.bakePointShadowCubeArrayIndex,
                    gi.dynamicPointShadowCubeArrayIndex,
                    sampler0,
                    spotLights[si].shadowParams,
                    lightPos,
                    range,
                    input.worldPos,
                    finalNormal,
                    input.position.xy);
                if (localShadow <= 1e-4f)
                    continue;
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
        float skyVis = 1.0f;
        float3 volIrradiance = float3(0.0f, 0.0f, 0.0f);
        if (gi.giMode > 0 && gi.biasAndFade.w > 0.5f && gi.viewportInvSizeAndFlags.w > 0.5f)
        {
            volIrradiance = SampleGIIrradianceAndSkyVis(input.worldPos, finalNormal, viewDirection, gi, sampler0, volW, skyVis) * gi.gridMinAndIntensity.w;
        }

        float NdotV = saturate(dot(finalNormal, viewDirection));
        float3 kS = OpenPBRBaseSpecularAlbedo(surface, NdotV);
        float3 kD = (1.0f - kS) * (1.0f - surface.metalness);

        if (gi.captureMode == 0)
        {
            const bool hasVol = (gi.giMode > 0 && gi.biasAndFade.w > 0.5f && gi.viewportInvSizeAndFlags.w > 0.5f);
            float3 combinedDiff = skyDiffuseIrradiance;
            if (hasVol)
            {
                if (gi.skyVisibilitySplit != 0 && gi.skyVisibilityTexIndex >= 0)
                {
                    combinedDiff = volIrradiance * volW + skyDiffuseIrradiance * lerp(1.0f, skyVis, volW);
                }
                else
                {
                    combinedDiff = lerp(skyDiffuseIrradiance, volIrradiance, volW);
                }
            }

            // Diffuse indirect (attenuated by transmission on transmissive dielectrics)
            float3 diffuseIndirect = kD * surface.baseWeight * surface.baseColor * combinedDiff * MultiBounceAO(ao, surface.baseColor);

            const bool hasCoat = (surface.coatWeight > 1e-4f);
            const bool hasFuzz = (surface.fuzzWeight > 1e-4f);
            float baseEffRough = OpenPBRCoatRoughenedRoughness(surface.specularRoughness, surface.coatRoughness, surface.coatWeight);
            float3 baseR = OpenPBRAnisotropicReflectionDir(viewDirection, finalNormal, T, B, baseEffRough, surface.specularAnisotropy, surface.specularRotation);
            float3 coatR = hasCoat ? reflect(-viewDirection, finalNormal) : baseR;

            float3 finalBaseRad = float3(0.0f, 0.0f, 0.0f);
            float3 finalCoatRad = float3(0.0f, 0.0f, 0.0f);
            SampleEnvAndProbesAtDir(input.worldPos, finalNormal, baseR, baseEffRough, coatR, surface.coatRoughness, hasCoat,
                combinedDiff, cameraAltitudeKm, scene, gi, reflectionProbes, environmentMapTexture, sampler0,
                finalBaseRad, finalCoatRad);

            if ((gi.renderFeatureFlags & 2u) != 0u && gi.ssrTextureIndex >= 0 && material.blendMode != 1)
            {
                float maxR = max(gi.ssrMaxRoughness, 0.05f);
                float roughFade = 1.0f - smoothstep(maxR * 0.6f, maxR, baseEffRough);
                if (roughFade > 1e-4f && gi.ssrIntensity > 1e-4f)
                {
                    float2 halfTexelSSR = gi.viewportInvSizeAndFlags.xy;
                    float2 ssrUV = clamp(input.position.xy * gi.viewportInvSizeAndFlags.xy, halfTexelSSR, 1.0f - halfTexelSSR);
                    Texture2D ssrTex = ResourceDescriptorHeap[SafeBindlessIndex(gi.ssrTextureIndex)];
                    float4 ssrSample = ssrTex.SampleLevel(sampler0, ssrUV, 0.0f);
                    float ssrWeight = saturate(ssrSample.a * roughFade * gi.ssrIntensity);
                    finalBaseRad = lerp(finalBaseRad, ssrSample.rgb, ssrWeight);
                }
            }

            // Indirect OpenPBR transmission along refracted vector when transmissionWeight > 0
            if (surface.transmissionWeight > 0.001f && (scene.enableIBL != 0 || (gi.reflectionProbeCount > 0 && gi.reflectionCubeArrayIndex >= 0)))
            {
                float3 refrDir = refract(-viewDirection, finalNormal, 1.0f / max(surface.specularIOR, 1.01f));
                if (dot(refrDir, refrDir) < 1e-4f)
                {
                    refrDir = -viewDirection;
                }
                float transRough = saturate(baseEffRough * 0.5f + surface.transmissionScatter * 0.5f);
                float3 transBaseRad = float3(0.0f, 0.0f, 0.0f);
                float3 transUnusedCoat = float3(0.0f, 0.0f, 0.0f);
                SampleEnvAndProbesAtDir(input.worldPos, finalNormal, refrDir, transRough, coatR, surface.coatRoughness, false,
                    combinedDiff, cameraAltitudeKm, scene, gi, reflectionProbes, environmentMapTexture, sampler0,
                    transBaseRad, transUnusedCoat);

                float3 beer = TransmissionBeerLaw(surface.transmissionColor, surface.transmissionDepth, NdotV);
                float3 scatterTint = lerp(beer, combinedDiff * surface.transmissionColor, surface.transmissionScatter * 0.5f);
                float3 transmittedIndirect = (1.0f - surface.metalness) * (1.0f - kS) * transBaseRad * scatterTint;
                diffuseIndirect = lerp(diffuseIndirect, transmittedIndirect, surface.transmissionWeight);
            }
            ambient = diffuseIndirect;

            if (scene.enableIBL != 0 || (gi.reflectionProbeCount > 0 && gi.reflectionCubeArrayIndex >= 0))
            {
                float baseSO = ComputeBentNormalSpecularOcclusion(finalNormal, viewDirection, baseR, ao, baseEffRough);
                float3 specAccum = kS * finalBaseRad * baseSO;

                if (hasCoat)
                {
                    float coatF0 = DielectricF0(max(surface.coatIOR, 1.0f));
                    float3 coatF = SchlickFresnelCos(NdotV, coatF0);
                    float2 coatAB = EnvBRDFApprox(saturate(surface.coatRoughness), NdotV);
                    float3 coatSpec = surface.coatWeight * saturate(coatF0 * coatAB.x + coatAB.y);
                    float3 throughCoat = OpenPBRThroughCoat(NdotV, coatF, surface);
                    float coatSO = ComputeBentNormalSpecularOcclusion(finalNormal, viewDirection, coatR, ao, surface.coatRoughness);
                    specAccum = coatSpec * finalCoatRad * coatSO + specAccum * throughCoat;
                }

                if (hasFuzz)
                {
                    float3 fuzz = surface.fuzzWeight * surface.fuzzColor * EnvSheenApprox(surface.fuzzRoughness, NdotV) * combinedDiff * MultiBounceAO(ao, surface.fuzzColor);
                    reflectedSpecular = fuzz + (1.0f - surface.fuzzWeight) * specAccum;
                }
                else
                {
                    reflectedSpecular = specAccum;
                }
            }
        }
        else if (gi.captureMode == 1)
        {
            if (gi.biasAndFade.w > 0.5f)
            {
                float3 combinedDiff = volIrradiance * volW;
                if (gi.skyVisibilitySplit != 0 && gi.skyVisibilityTexIndex >= 0 && gi.skySH[0].w > 0.5f)
                {
                    combinedDiff += skyDiffuseIrradiance * lerp(1.0f, skyVis, volW);
                }
                ambient = (1.0f - surface.metalness) * surface.baseWeight * surface.baseColor * combinedDiff * MultiBounceAO(ao, surface.baseColor);
            }
        }
        else if (gi.captureMode == 2)
        {
            const bool hasVol = (gi.giMode > 0 && gi.biasAndFade.w > 0.5f);
            float3 combinedDiff = skyDiffuseIrradiance;
            if (hasVol)
            {
                if (gi.skyVisibilitySplit != 0 && gi.skyVisibilityTexIndex >= 0)
                {
                    combinedDiff = volIrradiance * volW + skyDiffuseIrradiance * lerp(1.0f, skyVis, volW);
                }
                else
                {
                    combinedDiff = lerp(skyDiffuseIrradiance, volIrradiance, volW);
                }
            }
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
        result.color = float4(rawBaseColor.rgb, 1.0f);
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
