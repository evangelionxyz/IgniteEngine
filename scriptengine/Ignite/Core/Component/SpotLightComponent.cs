// Copyright (c) 2026 Evangelion Manuhutu

using Ignite.Core.Component;
using static Ignite.Mathf;

namespace Ignite;

public sealed class SpotLightComponent : IComponent
{
    public Vector4 Color
    {
        get => ComponentInternalCalls.SpotLightComponent_GetColor(Entity!.ID);
        set => ComponentInternalCalls.SpotLightComponent_SetColor(Entity!.ID, value);
    }

    public float Intensity
    {
        get => ComponentInternalCalls.SpotLightComponent_GetIntensity(Entity!.ID);
        set => ComponentInternalCalls.SpotLightComponent_SetIntensity(Entity!.ID, value);
    }

    public float Range
    {
        get => ComponentInternalCalls.SpotLightComponent_GetRange(Entity!.ID);
        set => ComponentInternalCalls.SpotLightComponent_SetRange(Entity!.ID, value);
    }

    public bool Enabled
    {
        get => ComponentInternalCalls.SpotLightComponent_GetEnabled(Entity!.ID);
        set => ComponentInternalCalls.SpotLightComponent_SetEnabled(Entity!.ID, value);
    }

    public float InnerConeAngle
    {
        get => ComponentInternalCalls.SpotLightComponent_GetInnerConeAngle(Entity!.ID);
        set => ComponentInternalCalls.SpotLightComponent_SetInnerConeAngle(Entity!.ID, value);
    }

    public float OuterConeAngle
    {
        get => ComponentInternalCalls.SpotLightComponent_GetOuterConeAngle(Entity!.ID);
        set => ComponentInternalCalls.SpotLightComponent_SetOuterConeAngle(Entity!.ID, value);
    }

    public float ConstantAttenuation
    {
        get => ComponentInternalCalls.SpotLightComponent_GetConstantAttenuation(Entity!.ID);
        set => ComponentInternalCalls.SpotLightComponent_SetConstantAttenuation(Entity!.ID, value);
    }

    public float LinearAttenuation
    {
        get => ComponentInternalCalls.SpotLightComponent_GetLinearAttenuation(Entity!.ID);
        set => ComponentInternalCalls.SpotLightComponent_SetLinearAttenuation(Entity!.ID, value);
    }

    public float QuadraticAttenuation
    {
        get => ComponentInternalCalls.SpotLightComponent_GetQuadraticAttenuation(Entity!.ID);
        set => ComponentInternalCalls.SpotLightComponent_SetQuadraticAttenuation(Entity!.ID, value);
    }
}

