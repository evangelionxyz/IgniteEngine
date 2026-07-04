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
    uint objectID;
    float3 _padding;
};

struct Skeleton
{
    float4x4 boneTransforms[MAX_BONES];
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
    int blendMode;      // 0 = Opaque, 1 = Transparent
    float2 tilingFactor;
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
cbuffer SkeletonBuffer    : register(b2, space0) { Skeleton skeleton; }
cbuffer SceneBuffer       : register(b3, space0) { Scene scene; }
cbuffer CascadesBuffer    : register(b4, space0) { CascadesShadows csm; }

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
SamplerState shadowSampler         : register(s1, space1); // linear sampler for shadow map PCF

struct PSInput
{
    float4 position  : SV_POSITION;
    float3 normal    : NORMAL;
    float3 tangent   : TANGENT;
    float3 bitangent : BITANGENT;
    float3 worldPos  : WORLDPOS;
    float2 uv        : TEXCOORD;
    float4 color     : COLOR;
};

struct PSOutput
{
    float4 color : SV_TARGET0;
    uint objectID : SV_TARGET1;
};

float3 GenNormalFromMap(float3x3 TBN, float2 uv)
{
    float2 tiledUV = uv * material.tilingFactor;
    float3 normalMap = normalMapTexture.Sample(sampler0, tiledUV).rgb * 2.0f - 1.0f;
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

// ---------------------------------------------------------------------------
// 16-tap Poisson disk in unit-disk space.
// Samples are hand-tuned for good coverage without visible patterns.
// ---------------------------------------------------------------------------
static const float2 k_PoissonDisk[16] =
{
    float2(-0.94201624f, -0.39906216f),
    float2( 0.94558609f, -0.76890725f),
    float2(-0.09418410f, -0.92938870f),
    float2( 0.34495938f,  0.29387760f),
    float2(-0.91588581f,  0.45771432f),
    float2(-0.81544232f, -0.87912464f),
    float2(-0.38277543f,  0.27676845f),
    float2( 0.97484398f,  0.75648379f),
    float2( 0.44323325f, -0.97511554f),
    float2( 0.53742981f, -0.47373420f),
    float2(-0.26496911f, -0.41893023f),
    float2( 0.79197514f,  0.19090188f),
    float2(-0.24188840f,  0.99706507f),
    float2(-0.81409955f,  0.91437590f),
    float2( 0.19984126f,  0.78641367f),
    float2( 0.14383161f, -0.14100790f)
};

// ---------------------------------------------------------------------------
// Interleaved-gradient noise hash — fast, low-correlation per-pixel rotation
// angle used to decorrelate Poisson taps across neighbouring pixels, which
// removes the regular banding you get from an unrotated disk.
// Reference: Jimenez 2014, "Next-Generation Post Processing in Call of Duty"
// ---------------------------------------------------------------------------
float InterleavedGradientNoise(float2 screenPos)
{
    float3 magic = float3(0.06711056f, 0.00583715f, 52.9829189f);
    return frac(magic.z * frac(dot(screenPos, magic.xy)));
}

float SampleShadow(float3 worldPos, float3 normal, float3 lightDirection, float2 screenPos)
{
    float3 viewPos = mul(camera.view, float4(worldPos, 1.0f)).xyz;
    float viewDepth = -viewPos.z;
    int cascadeIdx = GetCascadeIndex(viewDepth);
    cascadeIdx = clamp(cascadeIdx, 0, NUM_CASCADES - 1);

    float4 lightSpace = mul(csm.lightViewProjection[cascadeIdx], float4(worldPos, 1.0f));
    float3 ndc = lightSpace.xyz / lightSpace.w;
    float2 shadowUV = ndc.xy * 0.5f + 0.5f;
    float shadowDepth = ndc.z; // orthoZO outputs z in [0,1] for both D3D12 and Vulkan
    // Vulkan texture UV origin is top-left while NDC Y+ is upward, so we must
    // flip Y to convert from NDC-space to texture-space correctly.
    shadowUV.y = 1.0f - shadowUV.y;

    if (shadowUV.x < 0.0f || shadowUV.x > 1.0f ||
        shadowUV.y < 0.0f || shadowUV.y > 1.0f)
    {
        return 1.0f;
    }

    // Slope-scaled bias: steep surfaces get more bias to avoid acne.
    // Per-cascade scale: far cascades have lower depth precision so they need
    // proportionally more bias. cascade 0 = 1x, cascade 1 = 1.5x, ...
    float cosTheta = saturate(dot(normal, -lightDirection));
    float baseBias = lerp(csm.maxBias, csm.minBias, cosTheta);
    float cascadeBiasScale = 1.0f + float(cascadeIdx) * 0.5f;
    float bias = baseBias * cascadeBiasScale;
    float compareDepth = shadowDepth - bias;

    // Texel size in UV space.
    uint width, height, layers;
    shadowMap.GetDimensions(width, height, layers);
    float2 texelSize = 1.0f / float2(width, height);

    // ---------------------------------------------------------------------------
    // PCF radius scales with cascade: keep cascade 0 sharp, relax for far ones.
    // csm.pcfRadius acts as the base radius in texels for cascade 0.
    // ---------------------------------------------------------------------------
    float cascadeScale = 1.0f + float(cascadeIdx) * 0.5f;
    float filterRadius = max(csm.pcfRadius, 0.5f) * cascadeScale;

    // ---------------------------------------------------------------------------
    // Per-pixel rotation of the Poisson disk using interleaved-gradient noise.
    // This breaks the structured pattern and eliminates banding without TAA.
    // ---------------------------------------------------------------------------
    float angle  = InterleavedGradientNoise(screenPos) * 6.28318530718f; // 2*PI
    float sinA   = sin(angle);
    float cosA   = cos(angle);
    float2x2 rot = float2x2(cosA, -sinA, sinA, cosA);

    // ---------------------------------------------------------------------------
    // 16-tap Poisson-disk PCF with hardware comparison sampler.
    // SampleCmpLevelZero performs a bilinear 2x2 gather and returns the
    // average of the four per-texel comparisons — 4x quality for free.
    // ---------------------------------------------------------------------------
    float visibility = 0.0f;
    [unroll]
    for (int i = 0; i < 16; ++i)
    {
        float2 tapOffset = mul(rot, k_PoissonDisk[i]) * texelSize * filterRadius;
        float2 sampleUV  = clamp(shadowUV + tapOffset, 0.0f, 1.0f);
        float storedDepth = shadowMap.SampleLevel(
            shadowSampler,
            float3(sampleUV, float(cascadeIdx)),
            0.0f).r;
        // Lit when fragment depth (with bias) is less than the stored occluder depth.
        visibility += (compareDepth < storedDepth) ? 1.0f : 0.0f;
    }
    visibility /= 16.0f;

    return saturate(lerp(1.0f, visibility, csm.shadowStrength));
}

PSOutput main(PSInput input)
{
    PSOutput result;
    result.objectID = object.objectID;

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

    float3 emissiveColor = emissiveTexture.Sample(sampler0, tiledUV).rgb * material.emissiveFactor.rgb;
    float4 metallicColor = metallicTexture.Sample(sampler0, tiledUV);
    float4 roughnessColor = roughnessTexture.Sample(sampler0, tiledUV);
    float3 normalMap = normalMapTexture.Sample(sampler0, tiledUV).rgb;

    float metallic = clamp(SelectChannel(metallicColor, material.metallicChannel) * material.metallicFactor, 0.0f, 1.0f);
    float roughness = clamp(SelectChannel(roughnessColor, material.roughnessChannel) * material.roughnessFactor, 0.0f, 1.0f);

    // Sample base color with alpha for transparency support
    float4 baseColorSample = baseColorTexture.Sample(sampler0, tiledUV) * input.color;
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

        float shadowTerm = SampleShadow(input.worldPos, finalNormal, lightDirection, input.position.xy);
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

        // Output alpha: 1.0 for opaque, actual alpha for transparent
        float outputAlpha = (material.blendMode == 1) ? finalAlpha : 1.0f;
        result.color = float4(finalColor, outputAlpha);
    }
    else if (scene.renderMode == RENDER_MODE_DIFFUSE)
    {
        float3 diffuse = baseColorTexture.Sample(sampler0, tiledUV).rgb;
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