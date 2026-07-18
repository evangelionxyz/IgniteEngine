#include "include/binding_helpers.hlsli"
#include "include/scene.hlsli"
#include "include/shadow.hlsli"

DECLARE_PUSH_CONSTANTS(PushConstants, g_Push, 0, 0); // b0
cbuffer CameraBuffer                    : register(b1, space0) { Camera camera; }
StructuredBuffer<Object> g_ObjectBuffer : register(t2, space0);
cbuffer SceneBuffer                     : register(b3, space0) { Scene scene; }
cbuffer CascadesBuffer                  : register(b4, space0) { CascadesShadows csm; }

PixelVertexInput main(VertexMeshAnim input)
{
    PixelVertexInput pixelInput;
    
    float4 worldPosition = mul(
        g_ObjectBuffer[g_Push.objectIndex].transformMatrix,
        float4(input.position, 1.0f));
    
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