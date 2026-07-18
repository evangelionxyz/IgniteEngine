#include "include/binding_helpers.hlsli"
#include "include/scene.hlsli"
#include "include/shadow.hlsli"

DECLARE_PUSH_CONSTANTS(PushConstants, g_Push, 0, 0); // b0
cbuffer CameraBuffer                    : register(b1, space0) { Camera camera; }
StructuredBuffer<Object> g_ObjectBuffer : register(t2, space0);
StructuredBuffer<float4x4> g_Bones      : register(t3, space0);
cbuffer SceneBuffer                     : register(b4, space0) { Scene scene; }
cbuffer CascadesBuffer                  : register(b5, space0) { CascadesShadows csm; }

float4 SkinPosition(VertexMeshAnim input)
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
            uint globalBoneIdx = g_ObjectBuffer[g_Push.objectIndex].boneOffset + bone;
            skinned += mul(g_Bones[globalBoneIdx], float4(input.position, 1.0f)) * weight;
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

PixelVertexInput main(VertexMeshAnim input)
{
    PixelVertexInput pixelInput;

    float4 localPosition = SkinPosition(input);
    float4 worldPosition = mul(g_ObjectBuffer[g_Push.objectIndex].transformMatrix, localPosition);

    pixelInput.normal = input.normal;
    pixelInput.tangent = input.tangent;
    pixelInput.bitangent = input.bitangent;
    pixelInput.worldPos = worldPosition.xyz;
    pixelInput.uv = input.uv;
    pixelInput.color = input.color;

    const int cascadeIdx = clamp(csm.cascadeIndex, 0, 3);
    pixelInput.position = mul(csm.lightViewProjection[cascadeIdx], worldPosition);

    return pixelInput;
}