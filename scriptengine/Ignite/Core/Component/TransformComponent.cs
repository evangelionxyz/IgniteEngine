// Copyright (c) 2026 Evangelion Manuhutu

using Ignite.Core.Component;
using static Ignite.Mathf;

namespace Ignite;

public class TransformComponent : IComponent
{
    public Vector3 Forward
    {
        get
        {
            ComponentInternalCalls.TransformComponent_GetForward(Entity.ID, out Vector3 forward);
            return forward;
        }

        set
        {
            ComponentInternalCalls.TransformComponent_SetForward(Entity.ID, value);
        }
    }

    public Vector3 Right
    {
        get
        {
            ComponentInternalCalls.TransformComponent_GetRight(Entity.ID, out Vector3 right);
            return right;
        }

        set
        {
            ComponentInternalCalls.TransformComponent_SetRight(Entity.ID, value);
        }
    }

    public Vector3 Up
    {
        get
        {
            ComponentInternalCalls.TransformComponent_GetUp(Entity.ID, out Vector3 up);
            return up;
        }

        set
        {
            ComponentInternalCalls.TransformComponent_SetUp(Entity.ID, value);
        }
    }

    public Vector3 Translation
    {
        get
        {
            ComponentInternalCalls.TransformComponent_GetTranslation(Entity.ID, out Vector3 translation);
            return translation;
        }
        set
        {
            ComponentInternalCalls.TransformComponent_SetTranslation(Entity.ID, value);
        }
    }

    public Quaternion Rotation
    {
        get
        {
            ComponentInternalCalls.TransformComponent_GetRotation(Entity.ID, out Quaternion quat);
            return quat;
        }
        set
        {
            ComponentInternalCalls.TransformComponent_SetRotation(Entity.ID, value);
        }
    }

    public Vector3 EulerAngles
    {
        get
        {
            ComponentInternalCalls.TransformComponent_GetEulerAngles(Entity.ID, out Vector3 quat);
            return quat;
        }
        set
        {
            ComponentInternalCalls.TransformComponent_SetEulerAngles(Entity.ID, value);
        }
    }

    public Vector3 Scale
    {
        get
        {
            ComponentInternalCalls.TransformComponent_GetScale(Entity.ID, out Vector3 scale);
            return scale;
        }
        set
        {
            ComponentInternalCalls.TransformComponent_SetScale(Entity.ID, value);
        }
    }
}