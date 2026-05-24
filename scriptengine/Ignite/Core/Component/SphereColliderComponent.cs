// Copyright (c) 2026 Evangelion Manuhutu

using Ignite.Core.Component;
using static Ignite.Mathf;

namespace Ignite;

public sealed class SphereColliderComponent : IComponent
{
    public Vector3 Center
    {
        get
        {
            ComponentInternalCalls.SphereColliderComponent_GetCenter(Entity!.ID, out Vector3 result);
            return result;
        }
        set => ComponentInternalCalls.SphereColliderComponent_SetCenter(Entity!.ID, value);
    }

    public float Radius
    {
        get
        {
            ComponentInternalCalls.SphereColliderComponent_GetRadius(Entity!.ID, out float result);
            return result;
        }
        set => ComponentInternalCalls.SphereColliderComponent_SetRadius(Entity!.ID, value);
    }

    public float Friction
    {
        get
        {
            ComponentInternalCalls.SphereColliderComponent_GetFriction(Entity!.ID, out float result);
            return result;
        }
        set => ComponentInternalCalls.SphereColliderComponent_SetFriction(Entity!.ID, value);
    }

    public float Restitution
    {
        get
        {
            ComponentInternalCalls.SphereColliderComponent_GetRestitution(Entity!.ID, out float result);
            return result;
        }
        set => ComponentInternalCalls.SphereColliderComponent_SetRestitution(Entity!.ID, value);
    }

    public float Density
    {
        get
        {
            ComponentInternalCalls.SphereColliderComponent_GetDensity(Entity!.ID, out float result);
            return result;
        }
        set => ComponentInternalCalls.SphereColliderComponent_SetDensity(Entity!.ID, value);
    }
}
