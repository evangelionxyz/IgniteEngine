// Copyright (c) 2026 Evangelion Manuhutu

using Ignite.Core.Component;
using static Ignite.Mathf;

namespace Ignite;

public sealed class CircleCollider2DComponent : IComponent
{
    public Vector2 Center
    {
        get
        {
            ComponentInternalCalls.CircleCollider2DComponent_GetCenter(Entity.ID, out Vector2 result);
            return result;
        }
        set => ComponentInternalCalls.CircleCollider2DComponent_SetCenter(Entity.ID, value);
    }

    public float Radius
    {
        get
        {
            ComponentInternalCalls.CircleCollider2DComponent_GetRadius(Entity.ID, out float result);
            return result;
        }
        set => ComponentInternalCalls.CircleCollider2DComponent_SetRadius(Entity.ID, value);
    }

    public float Restitution
    {
        get
        {
            ComponentInternalCalls.CircleCollider2DComponent_GetRestitution(Entity.ID, out float result);
            return result;
        }
        set => ComponentInternalCalls.CircleCollider2DComponent_SetRestitution(Entity.ID, value);
    }

    public float Friction
    {
        get
        {
            ComponentInternalCalls.CircleCollider2DComponent_GetFriction(Entity.ID, out float result);
            return result;
        }
        set => ComponentInternalCalls.CircleCollider2DComponent_SetFriction(Entity.ID, value);
    }

    public float Density
    {
        get
        {
            ComponentInternalCalls.CircleCollider2DComponent_GetDensity(Entity.ID, out float result);
            return result;
        }
        set => ComponentInternalCalls.CircleCollider2DComponent_SetDensity(Entity.ID, value);
    }

    public bool IsSensor
    {
        get
        {
            ComponentInternalCalls.CircleCollider2DComponent_GetIsSensor(Entity.ID, out bool result);
            return result;
        }
        set => ComponentInternalCalls.CircleCollider2DComponent_SetIsSensor(Entity.ID, value);
    }
}
