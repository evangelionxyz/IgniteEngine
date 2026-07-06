#include "include/scene.hlsli"
#include "include/shadow.hlsli"

cbuffer CameraBuffer   : register(b0, space0) { Camera camera; }
cbuffer ObjectBuffer   : register(b1, space0) { Object object; }
cbuffer SceneBuffer    : register(b2, space0) { Scene scene; }
cbuffer CascadesBuffer : register(b3, space0) { CascadesShadows csm; }

PixelVertexInput main(VertexMeshAnim input)
{
    PixelVertexInput pixelInput;
    
    float4 worldPosition = mul(object.transformMatrix, float4(input.position, 1.0f));
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