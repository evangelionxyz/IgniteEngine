// Copyright (c) 2026 Evangelion Manuhutu

using Ignite.Core.Component;
using static Ignite.Mathf;

namespace Ignite;

public sealed class PointLight2DComponent : IComponent
{
    public Vector4 Color
    {
        get => ComponentInternalCalls.PointLight2DComponent_GetColor(Entity!.ID);
        set => ComponentInternalCalls.PointLight2DComponent_SetColor(Entity!.ID, value);
    }

    public float Radius
    {
        get => ComponentInternalCalls.PointLight2DComponent_GetRadius(Entity!.ID);
        set => ComponentInternalCalls.PointLight2DComponent_SetRadius(Entity!.ID, value);
    }

    public float Intensity
    {
        get => ComponentInternalCalls.PointLight2DComponent_GetIntensity(Entity!.ID);
        set => ComponentInternalCalls.PointLight2DComponent_SetIntensity(Entity!.ID, value);
    }

    public bool Enabled
    {
        get => ComponentInternalCalls.PointLight2DComponent_GetEnabled(Entity!.ID);
        set => ComponentInternalCalls.PointLight2DComponent_SetEnabled(Entity!.ID, value);
    }
}

