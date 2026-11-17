// Copyright (c) 2026 Evangelion Manuhutu

using Ignite.Core.Component;
using static Ignite.Mathf;

namespace Ignite;

public sealed class PlaneColliderComponent : IComponent
{
    public Vector3 Center
    {
        get
        {
            ComponentInternalCalls.PlaneColliderComponent_GetCenter(Entity!.ID, out Vector3 result);
            return result;
        }
        set => ComponentInternalCalls.PlaneColliderComponent_SetCenter(Entity!.ID, value);
    }

    public Vector3 Scale
    {
        get
        {
            ComponentInternalCalls.PlaneColliderComponent_GetScale(Entity!.ID, out Vector3 result);
            return result;
        }
        set => ComponentInternalCalls.PlaneColliderComponent_SetScale(Entity!.ID, value);
    }
}

