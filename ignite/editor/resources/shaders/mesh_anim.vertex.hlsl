#include "include/binding_helpers.hlsli"
#include "include/scene.hlsli"

DECLARE_PUSH_CONSTANTS(PushConstants, g_Push, 0, 0); // b0 - for skeletal: baseInstanceOffset == objectIndex (instancing deferred)
cbuffer CameraBuffer                    : register(b1, space0) { Camera camera; }
StructuredBuffer<Object>   g_ObjectBuffer : register(t2, space0);
StructuredBuffer<float4x4> g_Bones       : register(t3, space0);
cbuffer SceneBuffer                     : register(b4, space0) { Scene scene; }
cbuffer CSMBuffer                       : register(b5, space0) { } // unused in color VS
cbuffer PointLightBuffer                : register(b6, space0) { } // unused in VS
cbuffer SpotLightBuffer                 : register(b7, space0) { } // unused in VS

PixelVertexInput main(VertexMeshAnim input)
{
    PixelVertexInput pixelInput;

    // For skeletal meshes, baseInstanceOffset is used directly as the object index
    // (skeletal instancing is deferred to a later phase)
    const uint objectIndex = g_Push.baseInstanceOffset;

    // Initialize with zero
    float4 posL = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float3 normalL = float3(0.0f, 0.0f, 0.0f);
    float3 tangentL = float3(0.0f, 0.0f, 0.0f);
    float3 bitangentL = float3(0.0f, 0.0f, 0.0f);

    // Calculate skinned position and normal
    const float totalWeight = dot(input.weights, 1.0f);
    if (totalWeight <= 0.0001f)
    {
        posL = float4(input.position, 1.0f);
        normalL = input.normal;
        tangentL = input.tangent;
        bitangentL = input.bitangent;
    }
    else
    {
        for (int i = 0; i < VERTEX_MAX_BONES; ++i)
        {
            float weight = input.weights[i];
            if (weight > 0.0f)
            {
                uint boneId = min(input.boneIDs[i], (uint)(MAX_BONES - 1));
                uint globalBoneIdx = g_ObjectBuffer[objectIndex].boneOffset + boneId;
                float4x4 transform = g_Bones[globalBoneIdx];

                posL += weight * mul(transform, float4(input.position, 1.0));
                normalL += weight * mul((float3x3)transform, input.normal);
                tangentL += weight * mul((float3x3)transform, input.tangent);
                bitangentL += weight * mul((float3x3)transform, input.bitangent);
            }
        }
    }

    float4 worldPos    = mul(g_ObjectBuffer[objectIndex].transformMatrix, posL);

    pixelInput.position     = mul(mul(camera.projection, camera.view), worldPos);
    // Use normal matrix for correct inverse-transpose transform of direction vectors
    float3x3 N = (float3x3)g_ObjectBuffer[objectIndex].normalMatrix;
    pixelInput.normal       = normalize(mul(N, normalL));
    pixelInput.tangent      = normalize(mul(N, tangentL));
    pixelInput.bitangent    = normalize(mul(N, bitangentL));
    pixelInput.worldPos     = worldPos.xyz;
    pixelInput.uv           = input.uv;
    pixelInput.color        = input.color;
    pixelInput.objectID     = g_ObjectBuffer[objectIndex].objectID;
    return pixelInput;
}