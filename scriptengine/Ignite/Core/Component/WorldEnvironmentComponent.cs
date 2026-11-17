// Copyright (c) 2026 Evangelion Manuhutu

using Ignite.Core.Component;
using static Ignite.Mathf;

namespace Ignite;

public sealed class WorldEnvironmentComponent : IComponent
{
    public float Exposure
    {
        get => ComponentInternalCalls.WorldEnvironmentComponent_GetExposure(Entity!.ID);
        set => ComponentInternalCalls.WorldEnvironmentComponent_SetExposure(Entity!.ID, value);
    }

    public float Gamma
    {
        get => ComponentInternalCalls.WorldEnvironmentComponent_GetGamma(Entity!.ID);
        set => ComponentInternalCalls.WorldEnvironmentComponent_SetGamma(Entity!.ID, value);
    }

    public float Ambient
    {
        get => ComponentInternalCalls.WorldEnvironmentComponent_GetAmbient(Entity!.ID);
        set => ComponentInternalCalls.WorldEnvironmentComponent_SetAmbient(Entity!.ID, value);
    }

    public bool EnableIBL
    {
        get => ComponentInternalCalls.WorldEnvironmentComponent_GetEnableIBL(Entity!.ID);
        set => ComponentInternalCalls.WorldEnvironmentComponent_SetEnableIBL(Entity!.ID, value);
    }

    public float FogDensity
    {
        get => ComponentInternalCalls.WorldEnvironmentComponent_GetFogDensity(Entity!.ID);
        set => ComponentInternalCalls.WorldEnvironmentComponent_SetFogDensity(Entity!.ID, value);
    }

    public Vector4 FogColor
    {
        get => ComponentInternalCalls.WorldEnvironmentComponent_GetFogColor(Entity!.ID);
        set => ComponentInternalCalls.WorldEnvironmentComponent_SetFogColor(Entity!.ID, value);
    }

    public float FogStart
    {
        get => ComponentInternalCalls.WorldEnvironmentComponent_GetFogStart(Entity!.ID);
        set => ComponentInternalCalls.WorldEnvironmentComponent_SetFogStart(Entity!.ID, value);
    }

    public float FogEnd
    {
        get => ComponentInternalCalls.WorldEnvironmentComponent_GetFogEnd(Entity!.ID);
        set => ComponentInternalCalls.WorldEnvironmentComponent_SetFogEnd(Entity!.ID, value);
    }

    public void MarkDirty()
        => ComponentInternalCalls.WorldEnvironmentComponent_MarkDirty(Entity!.ID);
}

