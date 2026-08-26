// Copyright (c) 2026 Evangelion Manuhutu

#include "physics_2d.hpp"
#include "ignite/core/logger.hpp"

namespace ignite::physics
{
    Physics2D::Physics2D()
    {
    }

    Physics2D::~Physics2D()
    {
        SimulationStop();
    }

    void Physics2D::SimulationStart()
    {
        if (b2World_IsValid(m_WorldId))
        {
            b2DestroyWorld(m_WorldId);
        }

        b2WorldDef worldDef = b2DefaultWorldDef();
        m_WorldId = b2CreateWorld(&worldDef);

        LOG_ASSERT(b2World_IsValid(m_WorldId), "[Physics 2D] Failed to create b2Body");
        LOG_TRACE("[Physics 2D] World Created");
        LOG_TRACE("[Physics 2D] Simulation Started");
    }

    void Physics2D::SimulationStop()
    {
        if (b2World_IsValid(m_WorldId))
        {
            b2DestroyWorld(m_WorldId);
        }
        m_WorldId = b2_nullWorldId;

        LOG_TRACE("[Physics 2D] Simulation Stopped");
    }

    void Physics2D::Simulate(float deltaTime)
    {
        if (!b2World_IsValid(m_WorldId))
            return;

        constexpr i32 subStepCount = 12;
        b2World_Step(m_WorldId, deltaTime, subStepCount);
    }

    b2BodyId Physics2D::CreateBody(const b2BodyDef &bodyDef)
    {
        if (!b2World_IsValid(m_WorldId))
        {
            LOG_ERROR("[Physics 2D] Failed to create body");
            return b2_nullBodyId;
        }

        return b2CreateBody(m_WorldId, &bodyDef);
    }

    void Physics2D::DestroyBody(b2BodyId bodyId)
    {
        if (b2Body_IsValid(bodyId))
        {
            b2DestroyBody(bodyId);
        }
    }

    b2ShapeId Physics2D::CreateBoxCollider(b2BodyId bodyId, const b2ShapeDef &shapeDef, const b2Polygon &box)
    {
        if (!b2Body_IsValid(bodyId))
        {
            LOG_ERROR("[Physics 2D] Failed to create box collider");
            return b2_nullShapeId;
        }

        return b2CreatePolygonShape(bodyId, &shapeDef, &box);
    }

    b2ShapeId Physics2D::CreateCircleCollider(b2BodyId bodyId, const b2ShapeDef &shapeDef, const b2Circle &circle)
    {
        if (!b2Body_IsValid(bodyId))
        {
            LOG_ERROR("[Physics 2D] Failed to create circle collider");
            return b2_nullShapeId;
        }

        return b2CreateCircleShape(bodyId, &shapeDef, &circle);
    }

    void Physics2D::DestroyShape(b2ShapeId shapeId, bool updateBodyMass)
    {
        if (b2Shape_IsValid(shapeId))
        {
            b2DestroyShape(shapeId, updateBodyMass);
        }
    }

    bool Physics2D::IsValidBody(b2BodyId bodyId)
    {
        return b2Body_IsValid(bodyId);
    }

    void Physics2D::SetBodyType(b2BodyId bodyId, b2BodyType type)
    {
        if (b2Body_IsValid(bodyId))
            b2Body_SetType(bodyId, type);
    }

    void Physics2D::SetPosition(b2BodyId bodyId, const glm::vec2 &position)
    {
        if (b2Body_IsValid(bodyId))
            b2Body_SetTransform(bodyId, { position.x, position.y }, b2Body_GetRotation(bodyId));
    }

    void Physics2D::SetRotation(b2BodyId bodyId, float rotation)
    {
        if (b2Body_IsValid(bodyId))
            b2Body_SetTransform(bodyId, b2Body_GetPosition(bodyId), b2MakeRot(glm::radians(rotation)));
    }

    glm::vec2 Physics2D::GetPosition(b2BodyId bodyId) const
    {
        if (b2Body_IsValid(bodyId))
        {
            b2Vec2 pos = b2Body_GetPosition(bodyId);
            return { pos.x, pos.y };
        }
        return { 0.0f, 0.0f };
    }

    float Physics2D::GetRotation(b2BodyId bodyId) const
    {
        if (b2Body_IsValid(bodyId))
        {
            b2Rot rot = b2Body_GetRotation(bodyId);
            return b2Rot_GetAngle(rot);
        }
        return 0.0f;
    }

    void Physics2D::SetLinearVelocity(b2BodyId bodyId, const glm::vec2 &velocity)
    {
        if (b2Body_IsValid(bodyId))
            b2Body_SetLinearVelocity(bodyId, { velocity.x, velocity.y });
    }

    glm::vec2 Physics2D::GetLinearVelocity(b2BodyId bodyId) const
    {
        if (b2Body_IsValid(bodyId))
        {
            b2Vec2 vel = b2Body_GetLinearVelocity(bodyId);
            return { vel.x, vel.y };
        }
        return { 0.0f, 0.0f };
    }

    void Physics2D::SetAngularVelocity(b2BodyId bodyId, float velocity)
    {
        if (b2Body_IsValid(bodyId))
            b2Body_SetAngularVelocity(bodyId, velocity);
    }

    float Physics2D::GetAngularVelocity(b2BodyId bodyId) const
    {
        if (b2Body_IsValid(bodyId))
            return b2Body_GetAngularVelocity(bodyId);
        return 0.0f;
    }

    void Physics2D::ApplyLinearImpulse(b2BodyId bodyId, const glm::vec2 &impulse, const glm::vec2 &point, bool wake)
    {
        if (b2Body_IsValid(bodyId))
            b2Body_ApplyLinearImpulse(bodyId, { impulse.x, impulse.y }, { point.x, point.y }, wake);
    }

    void Physics2D::ApplyLinearImpulseToCenter(b2BodyId bodyId, const glm::vec2 &impulse, bool wake)
    {
        if (b2Body_IsValid(bodyId))
            b2Body_ApplyLinearImpulseToCenter(bodyId, { impulse.x, impulse.y }, wake);
    }

    void Physics2D::ApplyForce(b2BodyId bodyId, const glm::vec2 &force, const glm::vec2 &point, bool wake)
    {
        if (b2Body_IsValid(bodyId))
            b2Body_ApplyForce(bodyId, { force.x, force.y }, { point.x, point.y }, wake);
    }

    void Physics2D::ApplyForceToCenter(b2BodyId bodyId, const glm::vec2 &force, bool wake)
    {
        if (b2Body_IsValid(bodyId))
            b2Body_ApplyForceToCenter(bodyId, { force.x, force.y }, wake);
    }

    void Physics2D::ApplyTorque(b2BodyId bodyId, float torque, bool wake)
    {
        if (b2Body_IsValid(bodyId))
            b2Body_ApplyTorque(bodyId, torque, wake);
    }

    void Physics2D::ApplyAngularImpulse(b2BodyId bodyId, float impulse, bool wake)
    {
        if (b2Body_IsValid(bodyId))
            b2Body_ApplyAngularImpulse(bodyId, impulse, wake);
    }

    void Physics2D::ActivateBody(b2BodyId bodyId)
    {
        if (b2Body_IsValid(bodyId))
            b2Body_Enable(bodyId);
    }

    void Physics2D::DeactivateBody(b2BodyId bodyId)
    {
        if (b2Body_IsValid(bodyId))
            b2Body_Disable(bodyId);
    }

    void Physics2D::SetAwake(b2BodyId bodyId, bool awake)
    {
        if (b2Body_IsValid(bodyId))
            b2Body_SetAwake(bodyId, awake);
    }

    void Physics2D::SetEnableSleep(b2BodyId bodyId, bool enable)
    {
        if (b2Body_IsValid(bodyId))
            b2Body_EnableSleep(bodyId, enable);
    }

    void Physics2D::SetGravityScale(b2BodyId bodyId, float scale)
    {
        if (b2Body_IsValid(bodyId))
            b2Body_SetGravityScale(bodyId, scale);
    }

    void Physics2D::SetLinearDamping(b2BodyId bodyId, float damping)
    {
        if (b2Body_IsValid(bodyId))
            b2Body_SetLinearDamping(bodyId, damping);
    }

    void Physics2D::SetAngularDamping(b2BodyId bodyId, float damping)
    {
        if (b2Body_IsValid(bodyId))
            b2Body_SetAngularDamping(bodyId, damping);
    }

    void Physics2D::SetMotionLock(b2BodyId bodyId, bool lockX, bool lockY, bool lockRotation)
    {
        if (b2Body_IsValid(bodyId))
            b2Body_SetMotionLocks(bodyId, { lockX, lockY, lockRotation });
    }

    float Physics2D::GetMass(b2BodyId bodyId)
    {
        if (b2Body_IsValid(bodyId))
            return b2Body_GetMass(bodyId);
        return 0.0f;
    }

    bool Physics2D::IsBullet(b2BodyId bodyId)
    {
        if (b2Body_IsValid(bodyId))
            return b2Body_IsBullet(bodyId);
        return false;
    }

    void Physics2D::SetBullet(b2BodyId bodyId, bool bullet)
    {
        if (b2Body_IsValid(bodyId))
            b2Body_SetBullet(bodyId, bullet);
    }

} // namespace ignite::physics
