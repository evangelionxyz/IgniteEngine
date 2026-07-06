#include "include/binding_helpers.hlsli"
#include "include/scene.hlsli"

cbuffer CameraBuffer : register(b0, space0) { Camera camera; }
cbuffer ObjectBuffer : register(b1, space0) { Object object; }

PixelVertexInput main(VertexMesh input)
{
    PixelVertexInput pixelInput;

    float4 posL = float4(input.position, 1.0f);
    float3 normalL = input.normal;
    float3 tangentL = input.tangent;
    float3 bitangentL = input.bitangent;

    float4 worldPos = mul(object.transformMatrix, posL);

    pixelInput.position = mul(mul(camera.projection, camera.view), worldPos);
    // Use normal matrix for correct inverse-transpose transform of direction vectors
    float3x3 N = (float3x3) object.normalMatrix;
    pixelInput.normal = normalize(mul(N, normalL));
    pixelInput.tangent = normalize(mul(N, tangentL));
    pixelInput.bitangent = normalize(mul(N, bitangentL));
    pixelInput.worldPos = worldPos.xyz;
    pixelInput.uv = input.uv;
    pixelInput.color = input.color;
    return pixelInput;
}