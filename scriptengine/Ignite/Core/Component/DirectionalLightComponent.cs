// Copyright (c) 2026 Evangelion Manuhutu

using Ignite.Core.Component;
using static Ignite.Mathf;

namespace Ignite;

public sealed class DirectionalLightComponent : IComponent
{
    public Vector4 Color
    {
        get => ComponentInternalCalls.DirectionalLightComponent_GetColor(Entity!.ID);
        set => ComponentInternalCalls.DirectionalLightComponent_SetColor(Entity!.ID, value);
    }

    public float Intensity
    {
        get => ComponentInternalCalls.DirectionalLightComponent_GetIntensity(Entity!.ID);
        set => ComponentInternalCalls.DirectionalLightComponent_SetIntensity(Entity!.ID, value);
    }

    public float AngularRadius
    {
        get => ComponentInternalCalls.DirectionalLightComponent_GetAngularRadius(Entity!.ID);
        set => ComponentInternalCalls.DirectionalLightComponent_SetAngularRadius(Entity!.ID, value);
    }

    public float ShadowStrength
    {
        get => ComponentInternalCalls.DirectionalLightComponent_GetShadowStrength(Entity!.ID);
        set => ComponentInternalCalls.DirectionalLightComponent_SetShadowStrength(Entity!.ID, value);
    }

    public float ShadowDistance
    {
        get => ComponentInternalCalls.DirectionalLightComponent_GetShadowDistance(Entity!.ID);
        set => ComponentInternalCalls.DirectionalLightComponent_SetShadowDistance(Entity!.ID, value);
    }

    public int ShadowResolution
    {
        get => ComponentInternalCalls.DirectionalLightComponent_GetShadowResolution(Entity!.ID);
        set => ComponentInternalCalls.DirectionalLightComponent_SetShadowResolution(Entity!.ID, value);
    }

    public bool CascadeShadow
    {
        get => ComponentInternalCalls.DirectionalLightComponent_GetCascadeShadow(Entity!.ID);
        set => ComponentInternalCalls.DirectionalLightComponent_SetCascadeShadow(Entity!.ID, value);
    }

    public bool SoftShadows
    {
        get => ComponentInternalCalls.DirectionalLightComponent_GetSoftShadows(Entity!.ID);
        set => ComponentInternalCalls.DirectionalLightComponent_SetSoftShadows(Entity!.ID, value);
    }
}

