// Copyright (c) 2026 Evangelion Manuhutu

using Ignite.Core.Component;
using static Ignite.Mathf;

namespace Ignite;

public enum ParticleEmitterPreset : int
{
    Smoke = 0,
    Fire = 1,
    Spark = 2,
    Custom = 3,
    Plasma = 4,
    MuzzleFlash = 5,
    Shockwave = 6
}

public sealed class ParticleSystemComponent : IComponent
{
    public bool IsPlaying
    {
        get => ComponentInternalCalls.ParticleSystemComponent_GetIsPlaying(Entity!.ID);
        set => ComponentInternalCalls.ParticleSystemComponent_SetIsPlaying(Entity!.ID, value);
    }

    public float EmissionRate
    {
        get => ComponentInternalCalls.ParticleSystemComponent_GetEmissionRate(Entity!.ID);
        set => ComponentInternalCalls.ParticleSystemComponent_SetEmissionRate(Entity!.ID, value);
    }

    public uint MaxParticles
    {
        get => ComponentInternalCalls.ParticleSystemComponent_GetMaxParticles(Entity!.ID);
        set => ComponentInternalCalls.ParticleSystemComponent_SetMaxParticles(Entity!.ID, value);
    }

    public Vector4 StartColor
    {
        get => ComponentInternalCalls.ParticleSystemComponent_GetStartColor(Entity!.ID);
        set => ComponentInternalCalls.ParticleSystemComponent_SetStartColor(Entity!.ID, value);
    }

    public Vector4 EndColor
    {
        get => ComponentInternalCalls.ParticleSystemComponent_GetEndColor(Entity!.ID);
        set => ComponentInternalCalls.ParticleSystemComponent_SetEndColor(Entity!.ID, value);
    }

    public Vector2 StartSize
    {
        get => ComponentInternalCalls.ParticleSystemComponent_GetStartSize(Entity!.ID);
        set => ComponentInternalCalls.ParticleSystemComponent_SetStartSize(Entity!.ID, value);
    }

    public Vector2 EndSize
    {
        get => ComponentInternalCalls.ParticleSystemComponent_GetEndSize(Entity!.ID);
        set => ComponentInternalCalls.ParticleSystemComponent_SetEndSize(Entity!.ID, value);
    }

    public void Play() => ComponentInternalCalls.ParticleSystemComponent_Play(Entity!.ID);
    public void Stop() => ComponentInternalCalls.ParticleSystemComponent_Stop(Entity!.ID);
    public void Restart() => ComponentInternalCalls.ParticleSystemComponent_Restart(Entity!.ID);
    public void ApplyPreset(ParticleEmitterPreset preset) => ComponentInternalCalls.ParticleSystemComponent_SetPreset(Entity!.ID, (int)preset);
    public void EmitBurst(uint count) => ComponentInternalCalls.ParticleSystemComponent_SetPreset(Entity!.ID, 1000 + (int)(count > 4000u ? 4000u : count));
}

