// Copyright (c) 2026 Evangelion Manuhutu

using Ignite.Core.Component;
using static Ignite.Mathf;

namespace Ignite;

public sealed class CapsuleColliderComponent : IComponent
{
    public Vector3 Center
    {
        get
        {
            ComponentInternalCalls.CapsuleColliderComponent_GetCenter(Entity!.ID, out Vector3 result);
            return result;
        }
        set => ComponentInternalCalls.CapsuleColliderComponent_SetCenter(Entity!.ID, value);
    }

    public float Radius
    {
        get
        {
            ComponentInternalCalls.CapsuleColliderComponent_GetRadius(Entity!.ID, out float result);
            return result;
        }
        set => ComponentInternalCalls.CapsuleColliderComponent_SetRadius(Entity!.ID, value);
    }

    public float Height
    {
        get
        {
            ComponentInternalCalls.CapsuleColliderComponent_GetHeight(Entity!.ID, out float result);
            return result;
        }
        set => ComponentInternalCalls.CapsuleColliderComponent_SetHeight(Entity!.ID, value);
    }

    public float Friction
    {
        get
        {
            ComponentInternalCalls.CapsuleColliderComponent_GetFriction(Entity!.ID, out float result);
            return result;
        }
        set => ComponentInternalCalls.CapsuleColliderComponent_SetFriction(Entity!.ID, value);
    }

    public float Restitution
    {
        get
        {
            ComponentInternalCalls.CapsuleColliderComponent_GetRestitution(Entity!.ID, out float result);
            return result;
        }
        set => ComponentInternalCalls.CapsuleColliderComponent_SetRestitution(Entity!.ID, value);
    }

    public float Density
    {
        get
        {
            ComponentInternalCalls.CapsuleColliderComponent_GetDensity(Entity!.ID, out float result);
            return result;
        }
        set => ComponentInternalCalls.CapsuleColliderComponent_SetDensity(Entity!.ID, value);
    }
}
