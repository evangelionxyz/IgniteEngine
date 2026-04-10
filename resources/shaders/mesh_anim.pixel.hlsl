#include "include/helpers.hlsli"
#include "include/pbr.hlsli"
#include "include/binding_helpers.hlsli"

#define RENDER_MODE_COLOR 0
#define RENDER_MODE_DIFFUSE 1
#define RENDER_MODE_NORMALS 2
#define RENDER_MODE_METALLIC 3
#define RENDER_MODE_ROUGHNESS 4

#define MAX_BONES 100

#define NUM_CASCADES 4

struct Camera
{
    float4x4 projection;
    float4x4 view;
    float4 position;
};

struct Scene
{
    float4 lightColor;        // w component can store lightIntensity
    float2 lightAngle;
    float sunAngularRadius;
    int renderMode;
    int debugShadow;
    float exposure;
    float gamma;
    float ambient;
};

struct Object
{
    float4x4 transformMatrix;
    float4x4 normalMatrix;
    float4x4 boneTransforms[MAX_BONES];
    uint objectID;
    float3 _padding;
};

struct Material
{
    float4 baseColorFactor;
    float4 emissiveFactor;
    float metallicFactor;
    float roughnessFactor;
    float occlusionStrength;
    int metallicChannel;
    int roughnessChannel;
    float padding[3];
};

struct CascadesShadows
{
    float4x4 lightViewProjection[NUM_CASCADES];
    
    float4 cascadeSplits; // view-space distances (camera space z positive forward magnitude)
    
    float shadowStrength;
    float minBias;
    float maxBias;
    float pcfRadius;

    int cascadeIndex;
    float padding[3];
};

// push constant buffers

// set 0
cbuffer CameraBuffer      : register(b0, space0) { Camera camera; }
cbuffer ObjectBuffer      : register(b1, space0) { Object object; }
cbuffer SceneBuffer       : register(b2, space0) { Scene scene; }
cbuffer CascadesBuffer    : register(b3, space0) { CascadesShadows csm; }

// set 1
cbuffer MaterialBuffer    : register(b0, space1) { Material material; }

Texture2D baseColorTexture         : register(t0, space1);
Texture2D emissiveTexture          : register(t1, space1);
Texture2D metallicTexture          : register(t2, space1);
Texture2D roughnessTexture         : register(t3, space1);
Texture2D normalMapTexture         : register(t4, space1);
Texture2D occlusionTexture         : register(t5, space1);
Texture2D environmentMapTexture    : register(t6, space1);
Texture2DArray shadowMap           : register(t7, space1);
SamplerState sampler0              : register(s0, space1);
SamplerState sampler1              : register(s1, space1);

struct PSInput
{
    float4 position  : SV_POSITION;
    float3 normal    : NORMAL;
    float3 tangent   : TANGENT;
    float3 bitangent : BITANGENT;
    float3 worldPos  : WORLDPOS;
    float2 uv        : TEXCOORD;
};

struct PSOutput
{
    float4 color : SV_TARGET0;
    uint objectID : SV_TARGET1;
};

float3 GenNormalFromMap(float3x3 TBN, float2 uv)
{
    float3 normalMap = normalMapTexture.Sample(sampler0, uv).rgb * 2.0f - 1.0f;
    return normalize(mul(TBN, normalMap));
}

float SelectChannel(float4 value, int channel)
{
    if (channel == 1) return value.g;
    if (channel == 2) return value.b;
    if (channel == 3) return value.a;
    return value.r;
}

int GetCascadeIndex(float viewDepth)
{
    if (viewDepth < csm.cascadeSplits.x) return 0;
    if (viewDepth < csm.cascadeSplits.y) return 1;
    if (viewDepth < csm.cascadeSplits.z) return 2;
    return 3;
}

float SampleShadow(float3 worldPos, float3 normal, float3 lightDirection)
{
    float3 viewPos = mul(camera.view, float4(worldPos, 1.0f)).xyz;
    float viewDepth = -viewPos.z;
    int cascadeIdx = GetCascadeIndex(viewDepth);
    cascadeIdx = clamp(cascadeIdx, 0, NUM_CASCADES - 1);

    float4 lightSpace = mul(csm.lightViewProjection[cascadeIdx], float4(worldPos, 1.0f));
    float3 ndc = lightSpace.xyz / lightSpace.w;
    float2 shadowUV = ndc.xy * 0.5f + 0.5f;
#if defined(TARGET_VULKAN) || defined(SPIRV)
    shadowUV.y = 1.0f - shadowUV.y;
    float shadowDepth = ndc.z;
#else
    float shadowDepth = ndc.z * 0.5f + 0.5f;
#endif
    float3 shadowCoord = float3(shadowUV, shadowDepth);

    if (shadowCoord.x < 0.0f || shadowCoord.x > 1.0f ||
        shadowCoord.y < 0.0f || shadowCoord.y > 1.0f)
    {
        return 1.0f;
    }

    float cosTheta = saturate(dot(normal, -lightDirection));
    float bias = lerp(csm.maxBias, csm.minBias, cosTheta);

    uint width, height, layers;
    shadowMap.GetDimensions(width, height, layers);
    float2 texelSize = 1.0f / float2(width, height);
    float sampleRadius = max(csm.pcfRadius, 0.0f);
    int kernelRadius = max(2, (int)ceil(sampleRadius));

    float visibility = 0.0f;
    float sampleCount = 0.0f;

    for (int x = -kernelRadius; x <= kernelRadius; ++x)
    {
        for (int y = -kernelRadius; y <= kernelRadius; ++y)
        {
            float2 offset = float2(x, y) * texelSize * sampleRadius;
            float2 sampleUV = clamp(shadowCoord.xy + offset, 0.0f, 1.0f);
            float depth = shadowMap.SampleLevel(sampler0, float3(sampleUV, cascadeIdx), 0.0f).r;
            visibility += (shadowCoord.z - bias <= depth) ? 1.0f : 0.0f;
            sampleCount += 1.0f;
        }
    }

    visibility /= max(sampleCount, 1.0f);
    return saturate(lerp(1.0f, visibility, csm.shadowStrength));
}

PSOutput main(PSInput input)
{
    PSOutput result;
    result.objectID = object.objectID;

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

    float3 emissiveColor = emissiveTexture.Sample(sampler0, input.uv).rgb * material.emissiveFactor.rgb;
    float4 metallicColor = metallicTexture.Sample(sampler0, input.uv);
    float4 roughnessColor = roughnessTexture.Sample(sampler0, input.uv);
    float3 normalMap = normalMapTexture.Sample(sampler0, input.uv).rgb;

    float metallic = clamp(SelectChannel(metallicColor, material.metallicChannel) * material.metallicFactor, 0.0f, 1.0f);
    float roughness = clamp(SelectChannel(roughnessColor, material.roughnessChannel) * material.roughnessFactor, 0.0f, 1.0f);

    if (scene.renderMode == RENDER_MODE_COLOR)
    {
        float3 baseColor = baseColorTexture.Sample(sampler0, input.uv).rgb * material.baseColorFactor.rgb;
        float3 diffuseColor = baseColor * (1.0f - metallic);
        float3 specularColor = lerp(float3(0.04f, 0.04f, 0.04f), baseColor, metallic);

        float3 finalNormal = normalize(N * normalMap);

        float3 reflectDirection = reflect(-viewDirection, finalNormal);
        float3 reflectRadiance = SampleSphericalMap(environmentMapTexture, sampler0, reflectDirection);
        float3 reflectedSpecular = float3(0.0f, 0.0f, 0.0f);
        
        if (length(reflectRadiance) > 0.01f)
        {
            reflectRadiance = reflectRadiance / (reflectRadiance + 1.0f);
            
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

        float shadowTerm = SampleShadow(input.worldPos, finalNormal, lightDirection);
        directLighting *= shadowTerm;

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
            int ci = GetCascadeIndex(viewDepth);
            float3 dbg = ci == 0 ? float3(1.0f, 0.0f, 0.0f)
                : (ci == 1 ? float3(0.0f, 1.0f, 0.0f)
                : (ci == 2 ? float3(0.0f, 0.0f, 1.0f)
                : float3(1.0f, 1.0f, 0.0f)));
            finalColor *= dbg;
        }

        result.color = float4(FilmicTonemap(finalColor, scene.exposure, scene.gamma), 1.0f);
    }
    else if (scene.renderMode == RENDER_MODE_DIFFUSE)
    {
        float3 diffuse = baseColorTexture.Sample(sampler0, input.uv).rgb;
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