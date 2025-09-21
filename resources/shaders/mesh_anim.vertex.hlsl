#include "include/binding_helpers.hlsli"

#define VERTEX_MAX_BONES 4 // bone influences
#define MAX_BONES 100

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
    float4x4 boneTransforms[MAX_BONES];
};

cbuffer CameraBuffer : register(b0, space0)
{
    Camera camera; 
}

cbuffer ObjectBuffer : register(b1, space0)
{ 
    Object object;
}

struct VSInput
{
    float3 position     : POSITION;
    float3 normal       : NORMAL;
    float3 tangent      : TANGENT;
    float3 bitangent    : BITANGENT;
    float2 uv           : TEXCOORD;
    uint4 boneIDs       : BONEIDS;
    float4 weights      : WEIGHTS;
};

struct PSInput
{
    float4 position     : SV_POSITION;
    float3 normal       : NORMAL;
    float3 tangent      : TANGENT;
    float3 bitangent    : BITANGENT;
    float3 worldPos     : WORLDPOS;
    float2 uv           : TEXCOORD;
};

PSInput main(VSInput input)
{
    PSInput output;

    // Initialize with zero
    float4 posL = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float3 normalL = float3(0.0f, 0.0f, 0.0f);
    float3 tangentL = float3(0.0f, 0.0f, 0.0f);
    float3 bitangentL = float3(0.0f, 0.0f, 0.0f);

    // Calculate skinned position and normal
    for (int i = 0; i < VERTEX_MAX_BONES; ++i)
    {
        float weight = input.weights[i];
        if (weight > 0.0f)
        {
            uint boneId = input.boneIDs[i];
            float4x4 transform = object.boneTransforms[boneId];

            posL += weight * mul(transform, float4(input.position, 1.0));
            normalL += weight * mul((float3x3)transform, input.normal);
            tangentL += weight * mul((float3x3)transform, input.tangent);
            bitangentL += weight * mul((float3x3)transform, input.bitangent);
        }
    }

    // Ensure we have a valid position
    if (length(posL) < 0.00001f)
    {
        // Fallback to no skinning if weights don't sum to a significant value
        posL = float4(input.position, 1.0f);
        normalL = input.normal;
        tangentL = input.tangent;
        bitangentL = input.bitangent;
    }

    float4 worldPos    = mul(object.transformMatrix, posL);

    output.position     = mul(mul(camera.projection, camera.view), worldPos);
    // Use normal matrix for correct inverse-transpose transform of direction vectors
    float3x3 N = (float3x3)object.normalMatrix;
    output.normal       = normalize(mul(N, normalL));
    output.tangent      = normalize(mul(N, tangentL));
    output.bitangent    = normalize(mul(N, bitangentL));
    output.worldPos     = worldPos.xyz;
    output.uv           = input.uv;
    return output;
}