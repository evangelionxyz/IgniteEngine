// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_SCENE_HLSLI
#define IGN_SCENE_HLSLI

#define VERTEX_MAX_BONES 4
#define MAX_BONES 100
#define MAX_POINT_LIGHTS 16
#define MAX_SPOT_LIGHTS 16
#define MAX_REFLECTION_PROBES 32

#define RENDER_MODE_COLOR 0
#define RENDER_MODE_DIFFUSE 1
#define RENDER_MODE_NORMALS 2
#define RENDER_MODE_METALLIC 3
#define RENDER_MODE_ROUGHNESS 4
#define RENDER_MODE_SUBSURFACE 5
#define RENDER_MODE_TRANSMISSION 6
#define RENDER_MODE_COAT 7
#define RENDER_MODE_FUZZ 8
#define RENDER_MODE_GI_DIFFUSE 9
#define RENDER_MODE_GI_SPECULAR 10
#define RENDER_MODE_AO 11
#define RENDER_MODE_PROBE_WEIGHTS 12

struct PushConstants
{
    // For instanced draws: base index into the InstanceIndexBuffer.
    // Shader reads: objectIndex = g_InstanceIndices[baseInstanceOffset + SV_InstanceID]
    // For non-instanced (skeletal / transparent) draws: baseInstanceOffset is the
    // objectIndex directly in the legacy path.
    uint baseInstanceOffset;
};

struct Object
{
    float4x4 transformMatrix;
    float4x4 normalMatrix;
    uint objectID;
    uint boneOffset;
    uint receiveShadow;
    uint _padding;
};

struct Camera
{
    float4x4 projection;
    float4x4 view;
    float4 position;
};

struct Scene
{
    float4 lightColor; // w component can store lightIntensity
    float2 lightAngle;
    float sunAngularRadius;
    int renderMode;
    int debugShadow;
    float exposure;
    float gamma;
    float ambient;
    int numPointLights;
    int numSpotLights;
    int skyType;
    int enableIBL;
    float4 sunDirection;
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
    int blendMode; // 0 = Opaque, 1 = Transparent
    float2 tilingFactor;
    int baseColorTextureIndex;
    int emissiveTextureIndex;
    int metallicTextureIndex;
    int roughnessTextureIndex;
    int normalTextureIndex;
    int occlusionTextureIndex;
    float2 _pad;

    // OpenPBR Surface core
    float baseWeight;
    float specularWeight;
    float specularIOR;
    float coatWeight;
    float4 specularColor;
    float4 coatColor;
    float coatRoughness;
    float coatIOR;
    float coatDarkening;
    float specularAnisotropy;

    // OpenPBR subsurface
    float subsurfaceWeight;
    float subsurfaceScale;
    float2 _padSS;
    float4 subsurfaceColor;
    float4 subsurfaceRadius;

    // OpenPBR transmission
    float transmissionWeight;
    float transmissionDepth;
    float specularRotation;
    float transmissionScatter;
    float4 transmissionColor;

    // OpenPBR fuzz
    float fuzzWeight;
    float fuzzRoughness;
    float2 _padFZ;
    float4 fuzzColor;

    // Advanced
    float useMultiScatter;
    float emissionLuminance;
    float alphaCutoff;
    float normalScale;
};

struct Skeleton
{
    float4x4 boneTransforms[MAX_BONES];
};

// Vertex
struct VertexMesh
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
    float2 uv : TEXCOORD;
    float4 color : COLOR;
};

struct VertexMeshAnim
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
    float2 uv : TEXCOORD;
    float4 color : COLOR;
    uint4 boneIDs : BONEIDS;
    float4 weights : WEIGHTS;
};

// Vertex Input
struct PixelVertexInput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
    float3 worldPos : WORLDPOS;
    float2 uv : TEXCOORD;
    float4 color : COLOR;
    nointerpolation uint objectID : OBJECTID;
    nointerpolation uint receiveShadow : RECEIVESHADOW;
};

// Lighting
struct PointLight
{
    float4 positionAndRange; // xyz = position, w = range
    float4 color; // rgb = color, a = intensity
    float4 attenuation; // x = constant, y = linear, z = quadratic, w = unused
    float4 shadowParams; // x = shadow slice index (<0 disabled), y = bias, z = pcfRadius, w = shadowStrength
};

struct SpotLight
{
    float4 positionAndRange; // xyz = position, w = range
    float4 directionAndAngle; // xyz = direction, w = cos(outerConeAngle)
    float4 color; // rgb = color, a = intensity
    float4 attenuation; // x = constant, y = linear, z = quadratic, w = cos(innerConeAngle)
    float4 shadowParams; // x = shadow slice index (<0 disabled), y = bias, z = pcfRadius, w = shadowStrength
};

struct GIData
{
    float4 skySH[9];

    int skyPrefilterCubeIndex;
    int skyPrefilterMipCount;
    int captureMode;
    int giMode;

    float4 gridMinAndIntensity;
    float4 gridSizeAndSpacing;
    int4 gridDimsAndOrder;
    float4 biasAndFade;
    int4 shTextureIndices0;
    int4 shTextureIndices1;
    int4 visibilityParams;

    int reflectionCubeArrayIndex;
    int reflectionProbeCount;
    int reflectionMipCount;
    int bakePointShadowCubeArrayIndex;

    int gtaoTextureIndex;
    int aoMode;
    float aoIntensity;
    float aoPower;

    float4 viewportInvSizeAndFlags;

    int dynamicPointShadowCubeArrayIndex;
    int skyVisibilitySplit;
    int skyVisibilityTexIndex;
    int _giPad0;
};

struct ReflectionProbeGPU
{
    float4x4 worldToLocal;
    float4x4 localToWorld;
    float4 boxMinAndSlice;
    float4 boxMaxAndBlend;
    float4 influenceMinAndPrio;
    float4 influenceMaxAndInt;
    float4 capturePosAndNorm;
    float4 captureSH[4];
};

#endif
