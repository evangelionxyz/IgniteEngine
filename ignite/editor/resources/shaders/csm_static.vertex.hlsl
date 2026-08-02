#include "include/binding_helpers.hlsli"
#include "include/scene.hlsli"
#include "include/shadow.hlsli"

DECLARE_PUSH_CONSTANTS(PushConstants, g_Push, 0, 0);         // b0 - base instance offset
cbuffer CameraBuffer                    : register(b1, space0) { Camera camera; }
StructuredBuffer<Object>   g_ObjectBuffer    : register(t2, space0);
StructuredBuffer<uint>     g_InstanceIndices : register(t3, space0); // Instance index indirection
cbuffer SceneBuffer                     : register(b4, space0) { Scene scene; }
cbuffer CascadesBuffer                  : register(b5, space0) { CascadesShadows csm; }
cbuffer PointLightBuffer                : register(b6, space0) { } // unused in VS
cbuffer SpotLightBuffer                 : register(b7, space0) { } // unused in VS

PixelVertexInput main(VertexMesh input, uint instanceID : SV_InstanceID)
{
    PixelVertexInput pixelInput;

    // Resolve per-instance object index via the instance index buffer
    const uint objectIndex = g_InstanceIndices[g_Push.baseInstanceOffset + instanceID];

    float4 worldPosition = mul(g_ObjectBuffer[objectIndex].transformMatrix, float4(input.position, 1.0f));
    
    pixelInput.normal = input.normal;
    pixelInput.tangent = input.tangent;
    pixelInput.bitangent = input.bitangent;
    pixelInput.worldPos = worldPosition.xyz;
    pixelInput.uv = input.uv;
    pixelInput.color = input.color;
    pixelInput.objectID = g_ObjectBuffer[objectIndex].objectID;

    const int cascadeIdx = clamp(csm.cascadeIndex, 0, 3);
    pixelInput.position = mul(csm.lightViewProjection[cascadeIdx], worldPosition);

    return pixelInput;
}