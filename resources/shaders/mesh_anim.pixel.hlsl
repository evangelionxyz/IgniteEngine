#include "include/helpers.hlsli"
#include "include/pbr.hlsli"
#include "include/binding_helpers.hlsli"

#define RENDER_MODE_COLOR 0
#define RENDER_MODE_DIFFUSE 1
#define RENDER_MODE_NORMALS 2
#define RENDER_MODE_METALLIC 3
#define RENDER_MODE_ROUGHNESS 4

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
    
    float exposure;
    float gamma;
    float ambient;
    float padding;            // Explicit padding for 16-byte alignment
};

struct Object
{
    float4x4 transformMatrix;
    float4x4 normalMatrix;
};

struct Material
{
    float4 baseColorFactor;
    float4 emissiveFactor;
    float metallicFactor;
    float roughnessFactor;
    float occlusionStrength;
};

// push constant buffers

// set 0
cbuffer CameraBuffer      : register(b0, space0) { Camera camera; }
cbuffer ObjectBuffer      : register(b1, space0) { Object object; }

cbuffer SceneBuffer       : register(b2, space0) { Scene scene; }

// set 1
cbuffer MaterialBuffer    : register(b0, space1) { Material material; }
Texture2D baseColorTexture         : register(t0, space1);
Texture2D emissiveTexture          : register(t1, space1);
Texture2D metallicRoughnessTexture : register(t2, space1);
Texture2D normalMapTexture         : register(t3, space1);
Texture2D occlusionTexture         : register(t4, space1);
Texture2D environmentMapTexture    : register(t5, space1);
SamplerState sampler0              : register(s0, space1);

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
};

float3 CalcDirLight(float3 ldirection, float3 lcolor, float3 normal, float3 viewDirection, float3 diffTexColor, float shadow)
{
    float3 lightDirection = normalize(-ldirection);
    float3 normalizedViewDir = normalize(viewDirection);
    float ambientStrength = 0.1f;
    float3 ambientColor = ambientStrength * lcolor;
    float diffuse = max(dot(normal, lightDirection), 0.0f);
    float3 diffuseColor = diffuse * lcolor;

    float specularStrength = 0.5f;
    float3 reflectDir = reflect(-lightDirection, normal);
    float specular = pow(max(dot(normalizedViewDir, reflectDir), 0.0f), 32.0f);
    float3 specularColor = specularStrength * specular * lcolor;
    return (ambientColor + diffuseColor + specularColor) * diffTexColor;
}

float3 GenNormalFromMap(float3x3 TBN, float2 uv)
{
    float3 normalMap = normalMapTexture.Sample(sampler0, uv).rgb * 2.0f - 1.0f;
    return normalize(mul(TBN, normalMap));
}

PSOutput main(PSInput input)
{
    PSOutput result;
    
    float3 N = normalize(input.normal);
    float3 T = normalize(input.tangent);
    float3 B = normalize(input.bitangent);
    
    float3x3 TBN = transpose(float3x3(T, B, N));

    float3 viewDirection = normalize(camera.position.xyz - input.worldPos);

    // calculate sun direction
    float azimuth = scene.lightAngle.x;
    float elevation = scene.lightAngle.y;
    float3 sunDirection = float3(
        cos(elevation) * cos(azimuth),
        sin(elevation),
        cos(elevation) * sin(azimuth)
    );

    float3 lightDirection = normalize(sunDirection);
    float sunAngularRadius = scene.sunAngularRadius;
    float sunSolidAngle = 2.0 * M_PI * (1.0 - cos(sunAngularRadius)); // steradians

    float3 emissiveColor = emissiveTexture.Sample(sampler0, input.uv).rgb * material.emissiveFactor.rgb;
    float3 metallicRoughnessColor = metallicRoughnessTexture.Sample(sampler0, input.uv).rgb;
    float3 normalMap = normalMapTexture.Sample(sampler0, input.uv).rgb;

    float metallic = clamp(metallicRoughnessColor.b * material.metallicFactor, 0.0f, 1.0f);
    float roughness = clamp(metallicRoughnessColor.g * material.roughnessFactor, 0.0f, 1.0f);

    if (scene.renderMode == RENDER_MODE_COLOR)
    {
        float3 baseColor = baseColorTexture.Sample(sampler0, input.uv).rgb * material.baseColorFactor.rgb;
        float3 diffuseColor = baseColor * (1.0f - metallic);
        float3 specularColor = lerp(float3(0.04f, 0.04f, 0.04f), baseColor, metallic);

        // User normal mapping if available
        float3 finalNormal = normalize(input.normal * normalMap);
        // if (length(normalMap) > 0.01)
        // {
        //     finalNormal = GenNormalFromMap(TBN, input.uv);
        // }

        float3 reflectDirection = reflect(-viewDirection, finalNormal);
        float3 reflectRadiance = SampleSphericalMap(environmentMapTexture, sampler0, reflectDirection);
        reflectRadiance = reflectRadiance / (reflectRadiance + 1.0f);

        float reflectionStrength = lerp(0.01f, 1.0f, metallic) * (1.0f - roughness);
        float3 F = SchlickFresnel(viewDirection, finalNormal, specularColor);
        float NdotR = saturate(dot(finalNormal, reflectDirection));
        float3 reflectedSpecular = GGXReflect(finalNormal, reflectDirection, viewDirection, reflectRadiance, specularColor, roughness);
        reflectedSpecular *= reflectionStrength * NdotR * F;

        // direct light
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

        // Ambient lighting
        float3 ambient = diffuseColor * scene.ambient;

        float3 finalColor = directLighting + ambient + reflectedSpecular;
        if (length(emissiveColor) > 0.01f)
        {
            finalColor += emissiveColor * material.emissiveFactor.rgb * material.emissiveFactor.a;
        }

        result.color = float4(finalColor, 1.0f);
    }
    else if (scene.renderMode == RENDER_MODE_DIFFUSE)
    {
        float3 diffuse = baseColorTexture.Sample(sampler0, input.uv).rgb;
        result.color = float4(diffuse, 1.0f);
    }
    else if (scene.renderMode == RENDER_MODE_NORMALS)
    {
        float3 finalNormal = N * 0.5 + 0.5f;
        if (length(normalMap) > 0.01)
        {
            finalNormal = GenNormalFromMap(TBN, input.uv) * N * 0.5f + 0.5f;
        }
        result.color = float4(finalNormal, 1.0f);
    }
    else if (scene.renderMode == RENDER_MODE_METALLIC)
    {
        float metallic = metallicRoughnessTexture.Sample(sampler0, input.uv).b;
        result.color = float4(metallic, metallic, metallic, 1.0f);
    }
    else if (scene.renderMode == RENDER_MODE_ROUGHNESS)
    {
        float roughness = metallicRoughnessTexture.Sample(sampler0, input.uv).g;
        result.color = float4(roughness, roughness, roughness, 1.0f);
    }

    return result;
}