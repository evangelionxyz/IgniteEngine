// Copyright (c) 2026 Evangelion Manuhutu

using Ignite.Core.Component;
using static Ignite.Mathf;

namespace Ignite;

public sealed class RigidbodyComponent : IComponent
{
    public BodyType Type
    {
        get
        {
            ComponentInternalCalls.RigidbodyComponent_GetType(Entity!.ID, out int result);
            return (BodyType)result;
        }
        set => ComponentInternalCalls.RigidbodyComponent_SetType(Entity!.ID, (int)value);
    }

    public MotionQuality MotionQuality
    {
        get
        {
            ComponentInternalCalls.RigidbodyComponent_GetMotionQuality(Entity!.ID, out int result);
            return (MotionQuality)result;
        }
    }

    public MotionQuality GetMotionQuality() 
    {
        return MotionQuality;
    }

    public bool UseGravity
    {
        get
        {
            ComponentInternalCalls.RigidbodyComponent_GetUseGravity(Entity!.ID, out bool result);
            return result;
        }
        set => ComponentInternalCalls.RigidbodyComponent_SetUseGravity(Entity!.ID, value);
    }

    public float Mass
    {
        get
        {
            ComponentInternalCalls.RigidbodyComponent_GetMass(Entity!.ID, out float result);
            return result;
        }
    }

    public float GetMass()
    {
        return Mass;
    }

    public float GravityFactor
    {
        get
        {
            ComponentInternalCalls.RigidbodyComponent_GetGravityFactor(Entity!.ID, out float result);
            return result;
        }
        set => ComponentInternalCalls.RigidbodyComponent_SetGravityFactor(Entity!.ID, value);
    }

    public Vector3 Position
    {
        get
        {
            ComponentInternalCalls.RigidbodyComponent_GetPosition(Entity!.ID, out Vector3 result);
            return result;
        }
        set => ComponentInternalCalls.RigidbodyComponent_SetPosition(Entity!.ID, value);
    }

    public Quaternion Rotation
    {
        get
        {
            ComponentInternalCalls.RigidbodyComponent_GetRotation(Entity!.ID, out Quaternion result);
            return result;
        }
        set => ComponentInternalCalls.RigidbodyComponent_SetRotation(Entity!.ID, value);
    }

    public Vector3 LinearVelocity
    {
        get
        {
            ComponentInternalCalls.RigidbodyComponent_GetLinearVelocity(Entity!.ID, out Vector3 result);
            return result;
        }
        set => ComponentInternalCalls.RigidbodyComponent_SetLinearVelocity(Entity!.ID, value);
    }

    public Vector3 AngularVelocity
    {
        get
        {
            ComponentInternalCalls.RigidbodyComponent_GetAngularVelocity(Entity!.ID, out Vector3 result);
            return result;
        }
    }

    public Vector3 CenterOfMass
    {
        get
        {
            ComponentInternalCalls.RigidbodyComponent_GetCenterOfMass(Entity!.ID, out Vector3 result);
            return result;
        }
    }

    public bool IsActive
    {
        get
        {
            ComponentInternalCalls.RigidbodyComponent_IsActive(Entity!.ID, out bool result);
            return result;
        }
    }

    public void ApplyForce(Vector3 force, Vector3 point)
    {
        ComponentInternalCalls.RigidbodyComponent_ApplyForce(Entity!.ID, force, point);
    }

    public void ApplyForceToCenter(Vector3 force)
    {
        ComponentInternalCalls.RigidbodyComponent_ApplyForceToCenter(Entity!.ID, force);
    }

    public void ApplyTorque(Vector3 torque)
    {
        ComponentInternalCalls.RigidbodyComponent_ApplyTorque(Entity!.ID, torque);
    }

    public void ApplyLinearImpulse(Vector3 impulse, Vector3 point)
    {
        ComponentInternalCalls.RigidbodyComponent_ApplyLinearImpulse(Entity!.ID, impulse, point);
    }

    public void ApplyLinearImpulseToCenter(Vector3 impulse)
    {
        ComponentInternalCalls.RigidbodyComponent_ApplyLinearImpulseToCenter(Entity!.ID, impulse);
    }

    public void ApplyAngularImpulse(Vector3 impulse)
    {
        ComponentInternalCalls.RigidbodyComponent_ApplyAngularImpulse(Entity!.ID, impulse);
    }

    public void Activate()
    {
        ComponentInternalCalls.RigidbodyComponent_Activate(Entity!.ID);
    }

    public void Deactivate()
    {
        ComponentInternalCalls.RigidbodyComponent_Deactivate(Entity!.ID);
    }

    public void MoveKinematic(Vector3 targetPosition, Vector3 targetRotation, float deltaTime)
    {
        ComponentInternalCalls.RigidbodyComponent_MoveKinematic(Entity!.ID, targetPosition, targetRotation, deltaTime);
    }
}
