#include "include/binding_helpers.hlsli"
#include "include/scene.hlsli"

cbuffer CameraBuffer : register(b0, space0) { Camera camera; }
cbuffer ObjectBuffer : register(b1, space0) { Object object; }
cbuffer SkeletonBuffer : register(b2, space0) { Skeleton skeleton; }

PixelVertexInput main(VertexMeshAnim input)
{
    PixelVertexInput pixelInput;

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
            uint boneId = min(input.boneIDs[i], (uint)(MAX_BONES - 1));
            float4x4 transform = skeleton.boneTransforms[boneId];

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

    pixelInput.position     = mul(mul(camera.projection, camera.view), worldPos);
    // Use normal matrix for correct inverse-transpose transform of direction vectors
    float3x3 N = (float3x3)object.normalMatrix;
    pixelInput.normal       = normalize(mul(N, normalL));
    pixelInput.tangent      = normalize(mul(N, tangentL));
    pixelInput.bitangent    = normalize(mul(N, bitangentL));
    pixelInput.worldPos     = worldPos.xyz;
    pixelInput.uv           = input.uv;
    pixelInput.color        = input.color;
    return pixelInput;
}