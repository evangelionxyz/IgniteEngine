// Copyright (c) 2026 Evangelion Manuhutu

using Ignite.Core.Component;
using static Ignite.Mathf;

namespace Ignite;

public sealed class PointLightComponent : IComponent
{
    public Vector4 Color
    {
        get => ComponentInternalCalls.PointLightComponent_GetColor(Entity!.ID);
        set => ComponentInternalCalls.PointLightComponent_SetColor(Entity!.ID, value);
    }

    public float Intensity
    {
        get => ComponentInternalCalls.PointLightComponent_GetIntensity(Entity!.ID);
        set => ComponentInternalCalls.PointLightComponent_SetIntensity(Entity!.ID, value);
    }

    public float Range
    {
        get => ComponentInternalCalls.PointLightComponent_GetRange(Entity!.ID);
        set => ComponentInternalCalls.PointLightComponent_SetRange(Entity!.ID, value);
    }

    public bool Enabled
    {
        get => ComponentInternalCalls.PointLightComponent_GetEnabled(Entity!.ID);
        set => ComponentInternalCalls.PointLightComponent_SetEnabled(Entity!.ID, value);
    }

    public float ConstantAttenuation
    {
        get => ComponentInternalCalls.PointLightComponent_GetConstantAttenuation(Entity!.ID);
        set => ComponentInternalCalls.PointLightComponent_SetConstantAttenuation(Entity!.ID, value);
    }

    public float LinearAttenuation
    {
        get => ComponentInternalCalls.PointLightComponent_GetLinearAttenuation(Entity!.ID);
        set => ComponentInternalCalls.PointLightComponent_SetLinearAttenuation(Entity!.ID, value);
    }

    public float QuadraticAttenuation
    {
        get => ComponentInternalCalls.PointLightComponent_GetQuadraticAttenuation(Entity!.ID);
        set => ComponentInternalCalls.PointLightComponent_SetQuadraticAttenuation(Entity!.ID, value);
    }
}

