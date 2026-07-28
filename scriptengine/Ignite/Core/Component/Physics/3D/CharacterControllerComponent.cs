// Copyright (c) 2026 Evangelion Manuhutu

using Ignite.Core.Component;
using static Ignite.Mathf;

namespace Ignite;

public sealed class CharacterControllerComponent : IComponent
{
    public Vector3 Center
    {
        get
        {
            ComponentInternalCalls.CharacterControllerComponent_GetCenter(Entity!.ID, out Vector3 result);
            return result;
        }
        set => ComponentInternalCalls.CharacterControllerComponent_SetCenter(Entity!.ID, value);
    }

    public float Radius
    {
        get
        {
            ComponentInternalCalls.CharacterControllerComponent_GetRadius(Entity!.ID, out float result);
            return result;
        }
        set => ComponentInternalCalls.CharacterControllerComponent_SetRadius(Entity!.ID, value);
    }

    public float Height
    {
        get
        {
            ComponentInternalCalls.CharacterControllerComponent_GetHeight(Entity!.ID, out float result);
            return result;
        }
        set => ComponentInternalCalls.CharacterControllerComponent_SetHeight(Entity!.ID, value);
    }

    public float MaxStepHeight
    {
        get
        {
            ComponentInternalCalls.CharacterControllerComponent_GetMaxStepHeight(Entity!.ID, out float result);
            return result;
        }
        set => ComponentInternalCalls.CharacterControllerComponent_SetMaxStepHeight(Entity!.ID, value);
    }

    public float MaxSlopeAngle
    {
        get
        {
            ComponentInternalCalls.CharacterControllerComponent_GetMaxSlopeAngle(Entity!.ID, out float result);
            return result;
        }
        set => ComponentInternalCalls.CharacterControllerComponent_SetMaxSlopeAngle(Entity!.ID, value);
    }

    public float Mass
    {
        get
        {
            ComponentInternalCalls.CharacterControllerComponent_GetMass(Entity!.ID, out float result);
            return result;
        }
        set => ComponentInternalCalls.CharacterControllerComponent_SetMass(Entity!.ID, value);
    }

    public float Friction
    {
        get
        {
            ComponentInternalCalls.CharacterControllerComponent_GetFriction(Entity!.ID, out float result);
            return result;
        }
        set => ComponentInternalCalls.CharacterControllerComponent_SetFriction(Entity!.ID, value);
    }

    public float GravityFactor
    {
        get
        {
            ComponentInternalCalls.CharacterControllerComponent_GetGravityFactor(Entity!.ID, out float result);
            return result;
        }
        set => ComponentInternalCalls.CharacterControllerComponent_SetGravityFactor(Entity!.ID, value);
    }

    public Vector3 Up
    {
        get
        {
            ComponentInternalCalls.CharacterControllerComponent_GetUp(Entity!.ID, out Vector3 result);
            return result;
        }
        set => ComponentInternalCalls.CharacterControllerComponent_SetUp(Entity!.ID, value);
    }

    public Vector3 LinearVelocity
    {
        get
        {
            ComponentInternalCalls.CharacterControllerComponent_GetLinearVelocity(Entity!.ID, out Vector3 result);
            return result;
        }
        set => ComponentInternalCalls.CharacterControllerComponent_SetLinearVelocity(Entity!.ID, value);
    }

    public bool IsOnGround
    {
        get
        {
            ComponentInternalCalls.CharacterControllerComponent_IsOnGround(Entity!.ID, out bool result);
            return result;
        }
    }

    public Vector3 GroundNormal
    {
        get
        {
            ComponentInternalCalls.CharacterControllerComponent_GetGroundNormal(Entity!.ID, out Vector3 result);
            return result;
        }
    }

    public Vector3 Position
    {
        get
        {
            ComponentInternalCalls.CharacterControllerComponent_GetPosition(Entity!.ID, out Vector3 result);
            return result;
        }
        set => ComponentInternalCalls.CharacterControllerComponent_SetPosition(Entity!.ID, value);
    }

    public Quaternion Rotation
    {
        get
        {
            ComponentInternalCalls.CharacterControllerComponent_GetRotation(Entity!.ID, out Quaternion result);
            return result;
        }
        set => ComponentInternalCalls.CharacterControllerComponent_SetRotation(Entity!.ID, value);
    }

    public void Move(Vector3 displacement, float deltaTime = 1.0f / 60.0f)
    {
        ComponentInternalCalls.CharacterControllerComponent_Move(Entity!.ID, displacement, deltaTime);
    }
}
