// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_PHYSICS_2D_HPP
#define IGN_PHYSICS_2D_HPP

#include "physics_2d_component.hpp"
#include <box2d/box2d.h>
#include <entt/entt.hpp>

#include "ignite/core/types.hpp"
#include "ignite/scene/entity.hpp"

namespace ignite
{
    class Scene;
}

namespace ignite::physics
{
    class IGN_API Physics2D
    {
    public:
        Physics2D(Scene *scene = nullptr);
        ~Physics2D();

        void SetScene(Scene *scene);
        Scene *GetScene() const { return m_Scene; }

        void SimulationStart(Scene *scene);
        void SimulationStop();

        void InstantiateEntity(Entity entity);
        void DestroyEntity(Entity entity);

        void Simulate(float deltaTime);
        void CreateBoxCollider(BoxCollider2DComponent *box, b2BodyId bodyId, b2Vec2 size);
        void CreateCircleCollider(CircleCollider2DComponent *circle, b2BodyId bodyId, float size);

        bool IsValidBody(b2BodyId bodyId);
        void SetBodyType(b2BodyId bodyId, b2BodyType type);
		void SetPosition(b2BodyId bodyId, const glm::vec2 &position);
		void SetRotation(b2BodyId bodyId, float rotation);
        void SetLinearVelocity(b2BodyId bodyId, const glm::vec2 &velocity);
		void SetAngularVelocity(b2BodyId bodyId, float velocity);
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
        Scene *m_Scene;
        b2WorldId m_WorldId{ b2_nullWorldId };
    };
}

#endif
