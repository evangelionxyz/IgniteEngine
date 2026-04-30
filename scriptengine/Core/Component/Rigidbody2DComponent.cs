// Copyright (c) 2026 Evangelion Manuhutu

using Ignite.Core;
namespace Ignite;

public enum Body2DType
{
    Static = 0,
    Dynamic = 1,
    Kinematic = 2
}

public sealed class Rigidbody2DComponent : IComponent
{
    public Body2DType Type
    {
        get
        {
            InternalCalls.Rigidbody2DComponent_GetType(Entity.ID, out int result);
            return (Body2DType)result;
        }
        set => InternalCalls.Rigidbody2DComponent_SetType(Entity.ID, (int)value);
    }

    public Vector2 LinearVelocity
    {
        get
        {
            InternalCalls.Rigidbody2DComponent_GetLinearVelocity(Entity.ID, out Vector2 result);
            return result;
        }
        set => InternalCalls.Rigidbody2DComponent_SetLinearVelocity(Entity.ID, value);
    }

    public float AngularVelocity
    {
        get
        {
            InternalCalls.Rigidbody2DComponent_GetAngularVelocity(Entity.ID, out float result);
            return result;
        }
        set => InternalCalls.Rigidbody2DComponent_SetAngularVelocity(Entity.ID, value);
    }

    public float GravityScale
    {
        get
        {
            InternalCalls.Rigidbody2DComponent_GetGravityScale(Entity.ID, out float result);
            return result;
        }
        set => InternalCalls.Rigidbody2DComponent_SetGravityScale(Entity.ID, value);
    }

    public float LinearDamping
    {
        get
        {
            InternalCalls.Rigidbody2DComponent_GetLinearDamping(Entity.ID, out float result);
            return result;
        }
        set => InternalCalls.Rigidbody2DComponent_SetLinearDamping(Entity.ID, value);
    }

    public float AngularDamping
    {
        get
        {
            InternalCalls.Rigidbody2DComponent_GetAngularDamping(Entity.ID, out float result);
            return result;
        }
        set => InternalCalls.Rigidbody2DComponent_SetAngularDamping(Entity.ID, value);
    }

    public bool IsAwake
    {
        get
        {
            InternalCalls.Rigidbody2DComponent_GetIsAwake(Entity.ID, out bool result);
            return result;
        }
        set => InternalCalls.Rigidbody2DComponent_SetIsAwake(Entity.ID, value);
    }

    public bool IsEnabled
    {
        get
        {
            InternalCalls.Rigidbody2DComponent_GetIsEnabled(Entity.ID, out bool result);
            return result;
        }
        set => InternalCalls.Rigidbody2DComponent_SetIsEnabled(Entity.ID, value);
    }

    public bool IsEnableSleep
    {
        get
        {
            InternalCalls.Rigidbody2DComponent_GetIsEnableSleep(Entity.ID, out bool result);
            return result;
        }
        set => InternalCalls.Rigidbody2DComponent_SetIsEnableSleep(Entity.ID, value);
    }

    public bool IsBullet
    {
        get
        {
            InternalCalls.Rigidbody2DComponent_GetIsBullet(Entity.ID, out bool result);
            return result;
        }
        set => InternalCalls.Rigidbody2DComponent_SetIsBullet(Entity.ID, value);
    }

    public float Mass
    {
        get
        {
            InternalCalls.Rigidbody2DComponent_GetMass(Entity.ID, out float result);
            return result;
        }
    }

    public void ApplyForce(Vector2 force, Vector2 point, bool wake)
    {
        InternalCalls.Rigidbody2DComponent_ApplyForce(Entity.ID, force, point, wake);
    }

    public void ApplyForceToCenter(Vector2 force, bool wake)
    {
        InternalCalls.Rigidbody2DComponent_ApplyForceToCenter(Entity.ID, force, wake);
    }

    public void ApplyLinearImpulse(Vector2 impulse, Vector2 point, bool wake)
    {
        InternalCalls.Rigidbody2DComponent_ApplyLinearImpulse(Entity.ID, impulse, point, wake);
    }

    public void ApplyLinearImpulseToCenter(Vector2 impulse, bool wake)
    {
        InternalCalls.Rigidbody2DComponent_ApplyLinearImpulseToCenter(Entity.ID, impulse, wake);
    }

    public void ApplyAngularImpulse(float impulse, bool wake)
    {
        InternalCalls.Rigidbody2DComponent_ApplyAngularImpulse(Entity.ID, impulse, wake);
    }

    public void ApplyTorque(float torque, bool wake)
    {
        InternalCalls.Rigidbody2DComponent_ApplyTorque(Entity.ID, torque, wake);
    }
}
