#include "include/binding_helpers.hlsli"
#include "include/lighting.hlsli"

// ==============================
// set 0
// ==============================
DECLARE_PUSH_CONSTANTS(PushConstants, g_Push, 0, 0); // b0
cbuffer                              CameraBuffer       : register(b1, space0) { Camera camera; }
StructuredBuffer<Object>             g_ObjectBuffer     : register(t2, space0);
StructuredBuffer<uint>               g_InstanceIndices  : register(t3, space0);
cbuffer                              SceneBuffer        : register(b4, space0) { Scene scene; }
cbuffer                              CascadesBuffer     : register(b5, space0) { CascadesShadows csm; }
cbuffer                              PointLightBuffer   : register(b6, space0) { PointLight pointLights[MAX_POINT_LIGHTS]; }
cbuffer                              SpotLightBuffer    : register(b7, space0) { SpotLight spotLights[MAX_SPOT_LIGHTS]; }
cbuffer                              GIBuffer           : register(b8, space0) { GIData gi; }
StructuredBuffer<ReflectionProbeGPU> g_ReflectionProbes : register(t9, space0);

// ==============================
// set 1
// ==============================
cbuffer        MaterialBuffer        : register(b0, space1) { Material material; }
Texture2D      environmentMapTexture : register(t0, space1);
Texture2DArray shadowMap             : register(t1, space1);
SamplerState   sampler0              : register(s0, space1);
SamplerState   shadowSampler         : register(s1, space1);

PSOutput main(PixelVertexInput input, bool isFrontFace : SV_IsFrontFace)
{
    return EvaluateMeshPixel(
        input,
        isFrontFace,
        camera,
        scene,
        csm,
        pointLights,
        spotLights,
        gi,
        g_ReflectionProbes,
        material,
        environmentMapTexture,
        shadowMap,
        sampler0,
        shadowSampler);
}
