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
}
