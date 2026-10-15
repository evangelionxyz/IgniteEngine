// Copyright (c) 2026 Evangelion Manuhutu

#include "include/binding_helpers.hlsli"

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

struct ParticleSimCB
{
    float3 emitterPosition;
    float deltaTime;

    float3 emitterVelocity;
    float totalTime;

    float3 velocityMin;
    float lifetimeMin;

    float3 velocityMax;
    float lifetimeMax;

    float3 acceleration;
    float drag;

    float4 startColor;
    float4 endColor;

    float2 startSize;
    float2 endSize;

    float rotationSpeedMin;
    float rotationSpeedMax;
    float spawnRadius;
    float velocityAlignment;

    uint maxParticles;
    uint spawnCount;
    uint spawnOffset;
    uint emitterType;
};

cbuffer SimParams : register(b0)
{
    ParticleSimCB sim;
};

RWStructuredBuffer<GPUParticle> g_Particles : register(u0);

uint WangHash(uint seed)
{
    seed = (seed ^ 61) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    return seed;
}

float RandomFloat(inout uint seed)
{
    seed = WangHash(seed);
    return float(seed) / 4294967295.0;
}

float3 RandomFloat3(inout uint seed)
{
    return float3(RandomFloat(seed), RandomFloat(seed), RandomFloat(seed));
}

[numthreads(64, 1, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID)
{
    uint id = dispatchId.x;
    if (id >= sim.maxParticles)
        return;

    // Check if this particle is in the current spawn window
    bool isSpawned = false;
    if (sim.spawnCount > 0)
    {
        uint start = sim.spawnOffset;
        uint end = sim.spawnOffset + sim.spawnCount;
        if (end <= sim.maxParticles)
        {
            isSpawned = (id >= start && id < end);
        }
        else
        {
            isSpawned = (id >= start || id < (end % sim.maxParticles));
        }
    }

    if (isSpawned)
    {
        uint seed = WangHash(id * 1973u + asuint(sim.totalTime * 1000.0) + asuint(sim.deltaTime * 10000.0));

        GPUParticle p;

        // Position randomized within emitter spawn radius
        float3 rndSphere = RandomFloat3(seed) * 2.0 - 1.0;
        if (length(rndSphere) > 1.0)
            rndSphere = normalize(rndSphere) * RandomFloat(seed);

        p.position = sim.emitterPosition + rndSphere * sim.spawnRadius;

        // Velocity sampled between Min and Max
        float3 rnd3 = RandomFloat3(seed);
        p.velocity = lerp(sim.velocityMin, sim.velocityMax, rnd3) + sim.emitterVelocity;

        // Lifetime
        p.lifetime = 0.0;
        p.maxLifetime = lerp(sim.lifetimeMin, sim.lifetimeMax, RandomFloat(seed));
        p.maxLifetime = max(p.maxLifetime, 0.001);

        // Rotation & angular velocity
        p.rotation = RandomFloat(seed) * 6.2831853;
        p.rotationalVelocity = lerp(sim.rotationSpeedMin, sim.rotationSpeedMax, RandomFloat(seed));

        // Colors & Sizes
        p.startColor = sim.startColor;
        p.endColor = sim.endColor;
        p.startSize = sim.startSize;
        p.endSize = sim.endSize;
        p.color = sim.startColor;
        p.size = sim.startSize;

        p.velocityAlignment = sim.velocityAlignment;
        p.emitterType = sim.emitterType;
        p._pad = float2(0.0, 0.0);

        g_Particles[id] = p;
    }
    else
    {
        GPUParticle p = g_Particles[id];
        if (p.lifetime < p.maxLifetime)
        {
            p.lifetime += sim.deltaTime;
            if (p.lifetime >= p.maxLifetime)
            {
                p.lifetime = p.maxLifetime; // Mark dead
                p.size = float2(0.0, 0.0);
                p.color = float4(0.0, 0.0, 0.0, 0.0);
            }
            else
            {
                float t = saturate(p.lifetime / max(p.maxLifetime, 0.0001));

                // Integrate acceleration and drag
                p.velocity += sim.acceleration * sim.deltaTime;
                p.velocity -= p.velocity * (sim.drag * sim.deltaTime);
                p.position += p.velocity * sim.deltaTime;
                p.rotation += p.rotationalVelocity * sim.deltaTime;

                // Interpolate visual attributes over normalized lifetime
                p.color = lerp(p.startColor, p.endColor, t);
                p.size = lerp(p.startSize, p.endSize, t);

                // Emitter specific behaviors:
                if (p.emitterType == 1) // Fire
                {
                    // Gentle high-frequency flicker
                    float flicker = 0.85 + 0.15 * sin(p.lifetime * 30.0 + float(id * 17u));
                    p.color.rgb *= flicker;
                }
                else if (p.emitterType == 2) // Spark
                {
                    // Sparks glimmer near end of life
                    if (t > 0.7)
                    {
                        float glitter = step(0.3, frac(sin(float(id) * 43758.5453 + p.lifetime * 50.0)));
                        p.color.rgb *= (0.5 + 0.5 * glitter);
                    }
                }
            }

            g_Particles[id] = p;
        }
    }
}
