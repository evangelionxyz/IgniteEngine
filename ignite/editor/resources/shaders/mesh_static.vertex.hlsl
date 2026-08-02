#include "include/binding_helpers.hlsli"
#include "include/scene.hlsli"

DECLARE_PUSH_CONSTANTS(PushConstants, g_Push, 0, 0);         // b0 - base instance offset
cbuffer CameraBuffer                    : register(b1, space0) { Camera camera; }
StructuredBuffer<Object>   g_ObjectBuffer    : register(t2, space0);
StructuredBuffer<uint>     g_InstanceIndices : register(t3, space0); // Instance index indirection
cbuffer SceneBuffer                     : register(b4, space0) { Scene scene; }
cbuffer CSMBuffer                       : register(b5, space0) { }  // unused in color pass but kept for layout
cbuffer PointLightBuffer                : register(b6, space0) { }  // unused in VS
cbuffer SpotLightBuffer                 : register(b7, space0) { }  // unused in VS

PixelVertexInput main(VertexMesh input, uint instanceID : SV_InstanceID)
{
    PixelVertexInput pixelInput;

    // Resolve per-instance object index via the instance index buffer
    const uint objectIndex = g_InstanceIndices[g_Push.baseInstanceOffset + instanceID];

    float4 posL = float4(input.position, 1.0f);
    float3 normalL = input.normal;
    float3 tangentL = input.tangent;
    float3 bitangentL = input.bitangent;

    float4 worldPos = mul(g_ObjectBuffer[objectIndex].transformMatrix, posL);

    pixelInput.position = mul(mul(camera.projection, camera.view), worldPos);
    // Use normal matrix for correct inverse-transpose transform of direction vectors
    float3x3 N = (float3x3)g_ObjectBuffer[objectIndex].normalMatrix;
    pixelInput.normal = normalize(mul(N, normalL));
    pixelInput.tangent = normalize(mul(N, tangentL));
    pixelInput.bitangent = normalize(mul(N, bitangentL));
    pixelInput.worldPos = worldPos.xyz;
    pixelInput.uv = input.uv;
    pixelInput.color = input.color;
    pixelInput.objectID = g_ObjectBuffer[objectIndex].objectID;
    return pixelInput;
}