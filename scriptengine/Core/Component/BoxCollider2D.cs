// Copyright (c) 2026 Evangelion Manuhutu

using Ignite.Core;
namespace Ignite;

public sealed class BoxCollider2D : IComponent
{
    public Vector2 Size
    {
        get
        {
            InternalCalls.BoxCollider2DComponent_GetSize(Entity.ID, out Vector2 result);
            return result;
        }
        set => InternalCalls.BoxCollider2DComponent_SetSize(Entity.ID, value);
    }

    public Vector2 Offset
    {
        get
        {
            InternalCalls.BoxCollider2DComponent_GetOffset(Entity.ID, out Vector2 result);
            return result;
        }
        set => InternalCalls.BoxCollider2DComponent_SetOffset(Entity.ID, value);
    }

    public float Restitution
    {
        get
        {
            InternalCalls.BoxCollider2DComponent_GetRestitution(Entity.ID, out float result);
            return result;
        }
        set => InternalCalls.BoxCollider2DComponent_SetRestitution(Entity.ID, value);
    }

    public float Friction
    {
        get
        {
            InternalCalls.BoxCollider2DComponent_GetFriction(Entity.ID, out float result);
            return result;
        }
        set => InternalCalls.BoxCollider2DComponent_SetFriction(Entity.ID, value);
    }

    public float Density
    {
        get
        {
            InternalCalls.BoxCollider2DComponent_GetDensity(Entity.ID, out float result);
            return result;
        }
        set => InternalCalls.BoxCollider2DComponent_SetDensity(Entity.ID, value);
    }

    public bool IsSensor
    {
        get
        {
            InternalCalls.BoxCollider2DComponent_GetIsSensor(Entity.ID, out bool result);
            return result;
        }
        set => InternalCalls.BoxCollider2DComponent_SetIsSensor(Entity.ID, value);
    }
}
