// Copyright (c) 2026 Evangelion Manuhutu

using Ignite.Core.Component;
using static Ignite.Mathf;

namespace Ignite;

public sealed class BoxColliderComponent : IComponent
{
    public Vector3 Center
    {
        get
        {
            ComponentInternalCalls.BoxColliderComponent_GetCenter(Entity!.ID, out Vector3 result);
            return result;
        }
        set => ComponentInternalCalls.BoxColliderComponent_SetCenter(Entity!.ID, value);
    }

    public Vector3 Scale
    {
        get
        {
            ComponentInternalCalls.BoxColliderComponent_GetScale(Entity!.ID, out Vector3 result);
            return result;
        }
        set => ComponentInternalCalls.BoxColliderComponent_SetScale(Entity!.ID, value);
    }

    public float Friction
    {
        get
        {
            ComponentInternalCalls.BoxColliderComponent_GetFriction(Entity!.ID, out float result);
            return result;
        }
        set => ComponentInternalCalls.BoxColliderComponent_SetFriction(Entity!.ID, value);
    }

    public float Restitution
    {
        get
        {
            ComponentInternalCalls.BoxColliderComponent_GetRestitution(Entity!.ID, out float result);
            return result;
        }
        set => ComponentInternalCalls.BoxColliderComponent_SetRestitution(Entity!.ID, value);
    }

    public float Density
    {
        get
        {
            ComponentInternalCalls.BoxColliderComponent_GetDensity(Entity!.ID, out float result);
            return result;
        }
        set => ComponentInternalCalls.BoxColliderComponent_SetDensity(Entity!.ID, value);
    }
}
