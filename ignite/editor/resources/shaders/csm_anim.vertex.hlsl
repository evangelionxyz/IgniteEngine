#include "include/scene.hlsli"
#include "include/shadow.hlsli"

cbuffer CameraBuffer   : register(b0, space0) { Camera camera; }
cbuffer ObjectBuffer   : register(b1, space0) { Object object; }
cbuffer SkeletonBuffer : register(b2, space0) { Skeleton skeleton; }
cbuffer SceneBuffer    : register(b3, space0) { Scene scene; }
cbuffer CascadesBuffer : register(b4, space0) { CascadesShadows csm; }

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

PixelVertexInput main(VertexMeshAnim input)
{
    PixelVertexInput pixelInput;

    float4 localPosition = SkinPosition(input);
    float4 worldPosition = mul(object.transformMatrix, localPosition);

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