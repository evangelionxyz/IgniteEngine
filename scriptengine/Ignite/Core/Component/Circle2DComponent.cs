// Copyright (c) 2026 Evangelion Manuhutu

using Ignite.Core.Component;
using static Ignite.Mathf;

namespace Ignite;

public class Circle2DComponent : IComponent
{
    public Vector4 Color
    {
        get
        {
            ComponentInternalCalls.Circle2DComponent_GetColor(Entity!.ID, out Vector4 result);
            return result;
        }

        set
        {
            ComponentInternalCalls.Circle2DComponent_SetColor(Entity!.ID, value);
        }
    }
}
