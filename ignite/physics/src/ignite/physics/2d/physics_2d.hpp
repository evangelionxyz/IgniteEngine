// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_PHYSICS_2D_HPP
#define IGN_PHYSICS_2D_HPP

#include "ignite/core/base.hpp"
#include "ignite/core/types.hpp"
#include "ignite/physics/physics_types.hpp"
#include <box2d/box2d.h>
#include <glm/glm.hpp>

namespace ignite::physics
{
    class IGN_API Physics2D
    {
    public:
        Physics2D();
        ~Physics2D();

        void SimulationStart();
        void SimulationStop();

        void Simulate(float deltaTime);

        b2WorldId GetWorldId() const { return m_WorldId; }

        b2BodyId CreateBody(const b2BodyDef &bodyDef);
        void DestroyBody(b2BodyId bodyId);

        b2ShapeId CreateBoxCollider(b2BodyId bodyId, const b2ShapeDef &shapeDef, const b2Polygon &box);
        b2ShapeId CreateCircleCollider(b2BodyId bodyId, const b2ShapeDef &shapeDef, const b2Circle &circle);
        void DestroyShape(b2ShapeId shapeId, bool updateBodyMass = false);

        bool IsValidBody(b2BodyId bodyId);
        void SetBodyType(b2BodyId bodyId, b2BodyType type);
        void SetPosition(b2BodyId bodyId, const glm::vec2 &position);
        void SetRotation(b2BodyId bodyId, float rotation);
        glm::vec2 GetPosition(b2BodyId bodyId) const;
        float GetRotation(b2BodyId bodyId) const;

        void SetLinearVelocity(b2BodyId bodyId, const glm::vec2 &velocity);
        glm::vec2 GetLinearVelocity(b2BodyId bodyId) const;
        void SetAngularVelocity(b2BodyId bodyId, float velocity);
        float GetAngularVelocity(b2BodyId bodyId) const;

        void ApplyLinearImpulse(b2BodyId bodyId, const glm::vec2 &impulse, const glm::vec2 &point, bool wake);
        void ApplyLinearImpulseToCenter(b2BodyId bodyId, const glm::vec2 &impulse, bool wake);
        void ApplyForce(b2BodyId bodyId, const glm::vec2 &force, const glm::vec2 &point, bool wake);
        void ApplyForceToCenter(b2BodyId bodyId, const glm::vec2 &force, bool wake);
        void ApplyTorque(b2BodyId bodyId, float torque, bool wake);
        void ApplyAngularImpulse(b2BodyId bodyId, float impulse, bool wake);
        void ActivateBody(b2BodyId bodyId);
        void DeactivateBody(b2BodyId bodyId);
        void SetAwake(b2BodyId bodyId, bool awake);
        void SetEnableSleep(b2BodyId bodyId, bool enable);
        void SetGravityScale(b2BodyId bodyId, float scale);
        void SetLinearDamping(b2BodyId bodyId, float damping);
        void SetAngularDamping(b2BodyId bodyId, float damping);
        void SetMotionLock(b2BodyId bodyId, bool lockX, bool lockY, bool lockRotation);
        float GetMass(b2BodyId bodyId);
        bool IsBullet(b2BodyId bodyId);
        void SetBullet(b2BodyId bodyId, bool bullet);

    private:
        b2WorldId m_WorldId{ b2_nullWorldId };
    };
}

#endif
