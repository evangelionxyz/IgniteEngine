// Copyright (c) 2026 Evangelion Manuhutu

using Ignite.Core;
namespace Ignite;

public class Circle2D : IComponent
{
    public Vector4 Color
    {
        get
        {
            InternalCalls.Circle2DComponent_GetColor(Entity.ID, out Vector4 result);
            return result;
        }

        set
        {
            InternalCalls.Circle2DComponent_SetColor(Entity.ID, value);
        }
    }
}
