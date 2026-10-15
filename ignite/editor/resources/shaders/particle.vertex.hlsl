// Copyright (c) 2026 Evangelion Manuhutu

#include "include/binding_helpers.hlsli"

struct Camera
{
    float4x4 projection;
    float4x4 view;
    float4 position;
};

struct ParticleRenderCB
{
    float4x4 invView;
    float2 viewportSize;
    uint useTexture;
    uint objectID;
};

struct GPUParticle
{
    float3 position;
    float lifetime;

    float3 velocity;
    float maxLifetime;

    float4 color;

    float2 size;
    float rotation;
    float rotationalVelocity;

    float4 startColor;
    float4 endColor;
    float2 startSize;
    float2 endSize;

    float velocityAlignment;
    uint emitterType;
    float2 _pad;
};

cbuffer                       CameraBuffer   : register(b0) { Camera camera; };
cbuffer                       ParticleParams : register(b1) { ParticleRenderCB renderParams; };
StructuredBuffer<GPUParticle> g_Particles : register(t0);

static const float2 quadOffsets[4] =
{
    float2(-0.5, -0.5),
    float2( 0.5, -0.5),
    float2( 0.5,  0.5),
    float2(-0.5,  0.5)
};

static const float2 quadUVs[4] =
{
    float2(0.0, 1.0),
    float2(1.0, 1.0),
    float2(1.0, 0.0),
    float2(0.0, 0.0)
};

static const int quadIndices[6] = { 0, 1, 2, 0, 2, 3 };

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
    nointerpolation uint emitterType : TYPE0;
    nointerpolation uint objectID : OBJECTID;
};

VSOutput main(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
    VSOutput output;

    GPUParticle p = g_Particles[instanceID];

    // Discard inactive particles
    if (p.lifetime >= p.maxLifetime || p.size.x <= 0.0 || p.size.y <= 0.0 || p.color.a <= 0.001)
    {
        output.position = float4(0.0, 0.0, 0.0, 0.0);
        output.uv = float2(0.0, 0.0);
        output.color = float4(0.0, 0.0, 0.0, 0.0);
        output.emitterType = 0;
        output.objectID = uint(-1);
        return output;
    }

    int cornerIndex = quadIndices[vertexID % 6];
    float2 corner = quadOffsets[cornerIndex];
    float2 uv = quadUVs[cornerIndex];

    // 1. Transform particle center to View Space (Camera Space)
    // Generating billboards in view space guarantees they are ALWAYS facing the camera
    // and CANNOT flatten or degenerate to an edge-on line at any camera angle.
    float4 viewCenter = mul(camera.view, float4(p.position, 1.0));

    // 2. Standard billboard rotation (e.g. for smoke, fire, or unaligned particles)
    float c = cos(p.rotation);
    float s = sin(p.rotation);
    float2 standardOffset = float2(corner.x * c - corner.y * s, corner.x * s + corner.y * c) * p.size;

    // 3. Velocity-aligned billboard (e.g. for sparks, rain, embers)
    float2 finalOffset = standardOffset;

    if (p.velocityAlignment > 0.001)
    {
        // Transform 3D velocity to View Space
        float3 viewVel = mul(camera.view, float4(p.velocity, 0.0)).xyz;
        float2 velDir2D = viewVel.xy;
        float speed2D = length(velDir2D);

        // When moving across screen, stretch along direction of motion on screen
        if (speed2D > 1e-4)
        {
            float2 sparkUp = velDir2D / speed2D;
            float2 sparkRight = float2(-sparkUp.y, sparkUp.x); // Exactly 90 deg orthogonal in screen space

            // Foreshortening factor: if particle flies directly at camera, contract length smoothly
            float speed3D = max(length(viewVel), 1e-4);
            float foreshorten = clamp(speed2D / speed3D, 0.2, 1.0);

            float2 velOffset = sparkRight * (corner.x * p.size.x) + sparkUp * (corner.y * p.size.y * foreshorten);

            finalOffset = lerp(standardOffset, velOffset, p.velocityAlignment);
        }
    }

    // 4. Compute final view-space vertex position
    float4 viewPos = viewCenter;
    viewPos.xy += finalOffset;

    // 5. Transform from View Space to Clip Space
    output.position = mul(camera.projection, viewPos);
    output.uv = uv;
    output.color = p.color;
    output.emitterType = p.emitterType;
    output.objectID = renderParams.objectID;

    return output;
}
