#define MAX_BONES 100
#define VERTEX_MAX_BONES 4

struct Camera
{
    float4x4 projection;
    float4x4 view;
    float4 position;
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

struct Scene
{
    float4 lightColor;
    float2 lightAngle;
    float sunAngularRadius;
    int renderMode;
    int debugShadow;
    float exposure;
    float gamma;
    float ambient;
};

struct CascadesShadows
{
    float4x4 lightViewProjection[4];

    float4 cascadeSplits;

    float shadowStrength;
    float minBias;
    float maxBias;
    float pcfRadius;

    int cascadeIndex;
    float padding[3];
};

// Register bindings must match the NVRHI binding set layout (space0):
// b0 = CameraBuffer, b1 = ObjectBuffer, b2 = SkeletonBuffer, b3 = SceneBuffer, b4 = CascadesBuffer
cbuffer CameraBuffer   : register(b0, space0) { Camera camera; }
cbuffer ObjectBuffer   : register(b1, space0) { Object object; }
cbuffer SkeletonBuffer : register(b2, space0) { Skeleton skeleton; }
cbuffer SceneBuffer    : register(b3, space0) { Scene scene; }
cbuffer CascadesBuffer : register(b4, space0) { CascadesShadows csm; }

struct VSInput
{
    float3 position     : POSITION;
    float3 normal       : NORMAL;
    float3 tangent      : TANGENT;
    float3 bitangent    : BITANGENT;
    float2 uv           : TEXCOORD;
    uint4  boneIDs      : BONEIDS;
    float4 weights      : WEIGHTS;
};

struct PSInput
{
    float4 position  : SV_POSITION;
    float3 normal    : NORMAL;
    float3 tangent   : TANGENT;
    float3 bitangent : BITANGENT;
    float3 worldPos  : WORLDPOS;
    float2 uv        : TEXCOORD;
};

float4 SkinPosition(VSInput input)
{
    const float totalWeight = dot(input.weights, 1.0f);
    if (totalWeight <= 0.0001f)
    {
        return float4(input.position, 1.0f);
    }

    float4 skinned = float4(0.0f, 0.0f, 0.0f, 0.0f);
    [unroll]
    for (uint i = 0; i < VERTEX_MAX_BONES; ++i)
    {
        const float weight = input.weights[i];
        const uint bone = min(input.boneIDs[i], (uint)(MAX_BONES - 1));
        if (weight > 0.0f)
        {
            skinned += mul(skeleton.boneTransforms[bone], float4(input.position, 1.0f)) * weight;
        }
    }

    if (skinned.w == 0.0f)
    {
        skinned = float4(input.position, 1.0f);
    }
    else
    {
        skinned.w = 1.0f;
    }

    return skinned;
}

PSInput main(VSInput input)
{
    PSInput output;

    float4 localPosition = SkinPosition(input);
    float4 worldPosition = mul(object.transformMatrix, localPosition);

    output.normal = input.normal;
    output.tangent = input.tangent;
    output.bitangent = input.bitangent;
    output.worldPos = worldPosition.xyz;
    output.uv = input.uv;

    const int cascadeIdx = clamp(csm.cascadeIndex, 0, 3);
    output.position = mul(csm.lightViewProjection[cascadeIdx], worldPosition);

    return output;
}