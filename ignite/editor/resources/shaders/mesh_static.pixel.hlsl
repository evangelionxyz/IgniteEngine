#include "include/binding_helpers.hlsli"
#include "include/helpers.hlsli"
#include "include/pbr.hlsli"
#include "include/scene.hlsli"
#include "include/shadow.hlsli"

// ==============================
// set 0
// ==============================

DECLARE_PUSH_CONSTANTS(PushConstants, g_Push, 0, 0); // b0
cbuffer CameraBuffer                    : register(b1, space0) { Camera camera; }
StructuredBuffer<Object>   g_ObjectBuffer    : register(t2, space0);
StructuredBuffer<uint>     g_InstanceIndices : register(t3, space0);
cbuffer SceneBuffer                     : register(b4, space0) { Scene scene; }
cbuffer CascadesBuffer                  : register(b5, space0) { CascadesShadows csm; }
cbuffer PointLightBuffer                : register(b6, space0) { PointLight pointLights[MAX_POINT_LIGHTS]; }
cbuffer SpotLightBuffer                 : register(b7, space0) { SpotLight spotLights[MAX_SPOT_LIGHTS]; }

// ==============================
// set 1
// ==============================
cbuffer MaterialBuffer          : register(b0, space1) { Material material; }
Texture2D environmentMapTexture : register(t0, space1);
Texture2DArray shadowMap        : register(t1, space1);
SamplerState sampler0           : register(s0, space1);
SamplerState shadowSampler      : register(s1, space1); // linear sampler for shadow map PCF

struct PSOutput
{
    float4 color : SV_TARGET0;
    uint objectID : SV_TARGET1;
};

float3 GenNormalFromMap(float3x3 TBN, float2 uv)
{
    float2 tiledUV = uv * material.tilingFactor;
    Texture2D normalMapTex = ResourceDescriptorHeap[material.normalTextureIndex];
    float3 normalMap = normalMapTex.Sample(sampler0, tiledUV).rgb * 2.0f - 1.0f;
    return normalize(mul(TBN, normalMap));
}

float SelectChannel(float4 value, int channel)
{
    if (channel == 1)
        return value.g;
    if (channel == 2)
        return value.b;
    if (channel == 3)
        return value.a;
    return value.r;
}

PSOutput main(PixelVertexInput input)
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

    Texture2D baseColorTex = ResourceDescriptorHeap[material.baseColorTextureIndex];
    Texture2D normalMapTex = ResourceDescriptorHeap[material.normalTextureIndex];
    Texture2D emissiveTex = ResourceDescriptorHeap[material.emissiveTextureIndex];
    Texture2D metallicTex = ResourceDescriptorHeap[material.metallicTextureIndex];
    Texture2D roughnessTex = ResourceDescriptorHeap[material.roughnessTextureIndex];

    float3 emissiveColor = emissiveTex.Sample(sampler0, tiledUV).rgb * material.emissiveFactor.rgb;
    float4 metallicColor = metallicTex.Sample(sampler0, tiledUV);
    float4 roughnessColor = roughnessTex.Sample(sampler0, tiledUV);
    float3 normalMap = normalMapTex.Sample(sampler0, tiledUV).rgb;

    float metallic = clamp(SelectChannel(metallicColor, material.metallicChannel) * material.metallicFactor, 0.0f, 1.0f);
    float roughness = clamp(SelectChannel(roughnessColor, material.roughnessChannel) * material.roughnessFactor, 0.0f, 1.0f);

    // Sample base color with alpha for transparency support
    float4 baseColorSample = baseColorTex.Sample(sampler0, tiledUV) * input.color;
    float finalAlpha = baseColorSample.a * material.baseColorFactor.a;

    // Discard nearly transparent fragments in transparent mode
    if (material.blendMode == 1 && finalAlpha < 0.001f)
    {
        discard;
    }

    if (scene.renderMode == RENDER_MODE_COLOR)
    {
        float3 baseColor = baseColorSample.rgb * material.baseColorFactor.rgb;
        float3 diffuseColor = baseColor * (1.0f - metallic);
        float3 specularColor = lerp(float3(0.04f, 0.04f, 0.04f), baseColor, metallic);

        float3 finalNormal = normalize(N * normalMap);

        float3 reflectDirection = reflect(-viewDirection, finalNormal);
        float3 reflectRadiance = SampleEnvironmentMap(environmentMapTexture, sampler0, reflectDirection, scene.skyType, camera.position.y * 0.001f);
        float3 reflectedSpecular = float3(0.0f, 0.0f, 0.0f);
        
        if (length(reflectRadiance) > 0.01f)
        {
            float reflectionStrength = lerp(0.01f, 1.0f, metallic) * (1.0f - roughness);
            float3 F = SchlickFresnel(viewDirection, finalNormal, specularColor);
            float NdotR = saturate(dot(finalNormal, reflectDirection));
            reflectedSpecular = GGXReflect(finalNormal, reflectDirection, viewDirection,
                reflectRadiance, specularColor, roughness) * reflectionStrength * NdotR * F;
        }

        float3 irradiance = scene.lightColor.rgb * scene.lightColor.w;
        float3 directLighting = GGX(
            finalNormal,
            lightDirection,
            viewDirection,
            irradiance,
            diffuseColor,
            specularColor,
            roughness
        );

        float shadowTerm = SampleShadow(csm, shadowMap, shadowSampler, camera.view, 
            input.worldPos, finalNormal, lightDirection, input.position.xy);
        
        directLighting *= shadowTerm;

        // Point Lights PBR
        for (int pi = 0; pi < scene.numPointLights; ++pi)
        {
            float3 lightPos = pointLights[pi].positionAndRange.xyz;
            float range = pointLights[pi].positionAndRange.w;
            float3 color = pointLights[pi].color.rgb;
            float intensity = pointLights[pi].color.a;
            float constAtt = pointLights[pi].attenuation.x;
            float linAtt = pointLights[pi].attenuation.y;
            float quadAtt = pointLights[pi].attenuation.z;

            float3 lightVec = lightPos - input.worldPos;
            float d = length(lightVec);
            if (d > range)
                continue;

            float3 toLight = normalize(lightVec);
            float atten = 1.0f / (constAtt + linAtt * d + quadAtt * (d * d));
            atten *= saturate(1.0f - (d / range));

            float3 ptIrradiance = color * intensity * atten;
            directLighting += GGX(
                finalNormal,
                toLight,
                viewDirection,
                ptIrradiance,
                diffuseColor,
                specularColor,
                roughness
            );
        }

        // Spot Lights PBR
        for (int si = 0; si < scene.numSpotLights; ++si)
        {
            float3 lightPos = spotLights[si].positionAndRange.xyz;
            float range = spotLights[si].positionAndRange.w;
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
            if (d > range)
                continue;

            float3 toLight = normalize(lightVec);
            float theta = dot(-toLight, spotDir);
            if (theta < cosOuter)
                continue;

            float atten = 1.0f / (constAtt + linAtt * d + quadAtt * (d * d));
            atten *= saturate(1.0f - (d / range));

            float coneFactor = saturate((theta - cosOuter) / max(0.0001f, cosInner - cosOuter));
            atten *= coneFactor;

            float3 spIrradiance = color * intensity * atten;
            directLighting += GGX(
                finalNormal,
                toLight,
                viewDirection,
                spIrradiance,
                diffuseColor,
                specularColor,
                roughness
            );
        }

        float baseAmbient = 0.03f;
        float occScale = 0.35f;
        float shadowAmbientFactor = lerp(0.0f, 1.0f, shadowTerm);

        float3 ambient = diffuseColor * (baseAmbient + occScale) * shadowAmbientFactor;
        reflectedSpecular *= shadowTerm;

        float3 finalColor = directLighting + ambient + reflectedSpecular;

        if (length(emissiveColor) > 0.01f)
        {
            finalColor += emissiveColor * material.emissiveFactor.rgb * material.emissiveFactor.a;
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
        float3 finalNormal = normalize(N * normalMap) * 0.5f + 0.5f;
        result.color = float4(finalNormal, 1.0f);
    }
    else if (scene.renderMode == RENDER_MODE_METALLIC)
    {
        result.color = float4(metallic, metallic, metallic, 1.0f);
    }
    else if (scene.renderMode == RENDER_MODE_ROUGHNESS)
    {
        result.color = float4(roughness, roughness, roughness, 1.0f);
    }

    return result;
}
