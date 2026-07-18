// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_SCENE_HLSLI
#define IGN_SCENE_HLSLI

#define VERTEX_MAX_BONES 4
#define MAX_BONES 100
#define MAX_POINT_LIGHTS 16
#define MAX_SPOT_LIGHTS 16

#define RENDER_MODE_COLOR 0
#define RENDER_MODE_DIFFUSE 1
#define RENDER_MODE_NORMALS 2
#define RENDER_MODE_METALLIC 3
#define RENDER_MODE_ROUGHNESS 4

struct PushConstants
{
    uint objectIndex;
};

struct Object
{
    float4x4 transformMatrix;
    float4x4 normalMatrix;
    uint objectID;
    uint boneOffset;
    float2 _padding;
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
    float3 _pad;
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
};

// Lighting
struct PointLight
{
    float4 positionAndRange; // xyz = position, w = range
    float4 color; // rgb = color, a = intensity
    float4 attenuation; // x = constant, y = linear, z = quadratic, w = unused
};

struct SpotLight
{
    float4 positionAndRange; // xyz = position, w = range
    float4 directionAndAngle; // xyz = direction, w = cos(outerConeAngle)
    float4 color; // rgb = color, a = intensity
    float4 attenuation; // x = constant, y = linear, z = quadratic, w = cos(innerConeAngle)
};

#endif
