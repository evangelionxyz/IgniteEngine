// Copyright (c) 2026 Evangelion Manuhutu

using Ignite.Core.Component;
using static Ignite.Mathf;

namespace Ignite;

public class Sprite2DComponent : IComponent
{
    public Vector4 Color
    {
        get
        {
            ComponentInternalCalls.Sprite2DComponent_GetColor(Entity!.ID, out Vector4 result);
            return result;
        }

        set
        {
            ComponentInternalCalls.Sprite2DComponent_SetColor(Entity!.ID, value);
        }
    }

    public Vector2 TilingFactor
    {
        get
        {
            ComponentInternalCalls.Sprite2DComponent_GetTilingFactor(Entity!.ID, out Vector2 result);
            return result;
        }

        set
        {
            ComponentInternalCalls.Sprite2DComponent_SetTilingFactor(Entity!.ID, value);
        }
    }
}
