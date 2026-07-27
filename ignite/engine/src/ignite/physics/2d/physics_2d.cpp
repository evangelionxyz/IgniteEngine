// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "physics_2d.hpp"
#include "ignite/scene/scene.hpp"

#include "ignite/scene/component.hpp"
#include "ignite/scene/scene_manager.hpp"
#include "ignite/core/profiler/profiler.hpp"

#include "ignite/math/transform.hpp"

namespace ignite::physics
{
    Physics2D::Physics2D(Scene *scene)
        : m_Scene(scene)
    {
        b2WorldDef worldDef = b2DefaultWorldDef();
        m_WorldId = b2CreateWorld(&worldDef);
    }

    Physics2D::~Physics2D()
    {
        if (B2_IS_NULL(m_WorldId) == false)
            b2DestroyWorld(m_WorldId);

        m_WorldId = b2_nullWorldId;
        m_Scene = nullptr;
    }

    void Physics2D::SetScene(Scene *scene)
    {
        m_Scene = scene;
    }

    void Physics2D::SimulationStart(Scene *scene)
    {
        IGN_PROFILE_FUNCTION();

        m_Scene = scene;

        entt::registry *reg = m_Scene->registry;
        auto view = reg->view<Rigidbody2DComponent>();
        for (entt::entity e : view)
        {
            IDComponent &id          = reg->get<IDComponent>(e);
            Rigidbody2DComponent &rb = reg->get<Rigidbody2DComponent>(e);
            TransformComponent &tr   = reg->get<TransformComponent>(e);

            b2BodyDef bodyDef        = b2DefaultBodyDef();
            bodyDef.type             = static_cast<b2BodyType>(rb.bodyType);
            bodyDef.position         = { tr.world.translation.x, tr.world.translation.y };
            bodyDef.rotation         = b2MakeRot(glm::eulerAngles(tr.world.rotation).z);
            bodyDef.angularVelocity  = rb.angularVelocity;
            bodyDef.linearVelocity.x = rb.linearVelocity.x;
            bodyDef.linearVelocity.y = rb.linearVelocity.y;
            bodyDef.gravityScale     = rb.gravityScale;
            bodyDef.angularDamping   = rb.angularDamping;
            bodyDef.linearDamping    = rb.linearDamping;
            bodyDef.isEnabled        = rb.isEnabled;
            bodyDef.isAwake          = rb.isAwake;
            bodyDef.motionLocks.angularZ = rb.fixedRotation;
            bodyDef.allowFastRotation = rb.allowFastRotation;

            rb.bodyId = b2CreateBody(m_WorldId, &bodyDef);
            b2Body_SetUserData(rb.bodyId, static_cast<void *>(&e));

            // create box collider
            if (reg->any_of<BoxCollider2DComponent>(e))
            {
                auto &bc = reg->get<BoxCollider2DComponent>(e);
                CreateBoxCollider(&bc, rb.bodyId, b2Vec2(bc.size.x * tr.world.scale.x, bc.size.y * tr.world.scale.y));
                b2Shape_SetUserData(bc.shapeId, static_cast<void *>(&e));
            }

            // create circle collider
            if (reg->any_of<CircleCollider2DComponent>(e))
            {
                auto &cc = reg->get<CircleCollider2DComponent>(e);
                CreateCircleCollider(&cc, rb.bodyId, glm::max(tr.world.scale.x, tr.world.scale.y));
                b2Shape_SetUserData(cc.shapeId, static_cast<void *>(&e));
            }
        }
    }

    void Physics2D::SimulationStop()
    {
        IGN_PROFILE_FUNCTION();

        if (!m_Scene)
            return;

        for (entt::entity e : m_Scene->registry->view<Rigidbody2DComponent>())
        {
            DestroyEntity(Entity{ e, m_Scene });
        }

        m_Scene = nullptr;
    }

    void Physics2D::InstantiateEntity(Entity entity)
    {
        if (!entity.HasComponent<Rigidbody2DComponent>())
            return;

        auto &id = entity.GetComponent<IDComponent>();
        auto &tr = entity.GetComponent<TransformComponent>();

        auto &rb = entity.GetComponent<Rigidbody2DComponent>();

        b2BodyDef bodyDef        = b2DefaultBodyDef();
        bodyDef.type             = static_cast<b2BodyType>(rb.bodyType);
        bodyDef.position         = { tr.world.translation.x, tr.world.translation.y };
        bodyDef.rotation         = b2MakeRot(glm::eulerAngles(tr.world.rotation).z);
        bodyDef.angularVelocity  = rb.angularVelocity;
        bodyDef.linearVelocity.x = rb.linearVelocity.x;
        bodyDef.linearVelocity.y = rb.linearVelocity.y;
        bodyDef.gravityScale     = rb.gravityScale;
        bodyDef.angularDamping   = rb.angularDamping;
        bodyDef.linearDamping    = rb.linearDamping;
        bodyDef.isEnabled        = rb.isEnabled;
        bodyDef.isAwake          = rb.isAwake;
        bodyDef.motionLocks.angularZ = rb.fixedRotation;
        bodyDef.allowFastRotation = rb.allowFastRotation;

        rb.bodyId = b2CreateBody(m_WorldId, &bodyDef);
        b2Body_SetUserData(rb.bodyId, static_cast<void *>(&entity));

        // create box collider
        if (entity.HasComponent<BoxCollider2DComponent>())
        {
            auto &bc = entity.GetComponent<BoxCollider2DComponent>();
            CreateBoxCollider(&bc, rb.bodyId, b2Vec2(bc.size.x * tr.world.scale.x, bc.size.y * tr.world.scale.y));
            b2Shape_SetUserData(bc.shapeId, static_cast<void *>(&entity));
        }

        // create circle collider
        if (entity.HasComponent<CircleCollider2DComponent>())
        {
            auto &cc = entity.GetComponent<CircleCollider2DComponent>();
            CreateCircleCollider(&cc, rb.bodyId, glm::max(tr.world.scale.x, tr.world.scale.y));
            b2Shape_SetUserData(cc.shapeId, static_cast<void *>(&entity));
        }
    }

    void Physics2D::DestroyEntity(Entity entity)
    {
        if (!m_Scene || !b2World_IsValid(m_WorldId))
            return;

        if (entity.HasComponent<BoxCollider2DComponent>())
        {
            auto &c = entity.GetComponent<BoxCollider2DComponent>();
            if (b2Shape_IsValid(c.shapeId))
                b2DestroyShape(c.shapeId, false);
        }

        if (entity.HasComponent<CircleCollider2DComponent>())
        {
            auto &c = entity.GetComponent<CircleCollider2DComponent>();
            if (b2Shape_IsValid(c.shapeId))
                b2DestroyShape(c.shapeId, false);
        }

        if (entity.HasComponent<Rigidbody2DComponent>())
        {
            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            if (b2Body_IsValid(rb.bodyId))
                b2DestroyBody(rb.bodyId);
        }
    }

    void Physics2D::Simulate(float deltaTime)
    {
        IGN_PROFILE_FUNCTION();
        constexpr i32 subStepCount = 12;
        {
            IGN_PROFILE_SCOPE("Physics2D::StepWorld");
            b2World_Step(m_WorldId, deltaTime, subStepCount);
        }

        const auto reg = m_Scene->registry;
       {
            IGN_PROFILE_SCOPE("Physics2D::SyncEntities");
            for (const auto e : reg->view<Rigidbody2DComponent>())
            {
                TransformComponent &tr = reg->get<TransformComponent>(e);
                Rigidbody2DComponent &rb = reg->get<Rigidbody2DComponent>(e);

                if (rb.isGizmoDragging)
                    continue;

                if (rb.dirty)
                {
                    b2Body_SetLinearVelocity(rb.bodyId, { rb.linearVelocity.x, rb.linearVelocity.y });
                    b2Body_SetAngularVelocity(rb.bodyId, rb.angularVelocity);
                    b2Body_SetGravityScale(rb.bodyId, rb.gravityScale);
                    b2Body_SetLinearDamping(rb.bodyId, rb.linearDamping);
                    b2Body_SetAngularDamping(rb.bodyId, rb.angularDamping);
                    b2Body_SetAwake(rb.bodyId, rb.isAwake);
                    rb.isEnabled ? b2Body_Enable(rb.bodyId) : b2Body_Disable(rb.bodyId);
                    b2Body_EnableSleep(rb.bodyId, rb.isEnableSleep);

                    b2MotionLocks ml;
                    ml.angularZ = rb.fixedRotation;
                    b2Body_SetMotionLocks(rb.bodyId, ml);

                    rb.dirty = false;
                }

                if (reg->any_of<BoxCollider2DComponent>(e))
                {
                    BoxCollider2DComponent &bc = reg->get<BoxCollider2DComponent>(e);
                    if (bc.dirty)
                    {
                        b2Shape_SetFriction(bc.shapeId, bc.friction);
                        b2Shape_SetDensity(bc.shapeId, bc.density, true);
                        b2Shape_SetRestitution(bc.shapeId, bc.restitution);

                        float width = glm::abs(bc.size.x * tr.world.scale.x);
                        float height = glm::abs(bc.size.y * tr.world.scale.y);

                        width = glm::max(width, glm::epsilon<float>());
                        height = glm::max(height, glm::epsilon<float>());
                        const b2Vec2 offset = { bc.offset.x * tr.world.scale.x, bc.offset.y * tr.world.scale.y };
                        const b2Polygon boxShape = b2MakeOffsetBox(width, height, offset, b2MakeRot(0.0f));
                        b2Shape_SetPolygon(bc.shapeId, &boxShape);
                        bc.dirty = false;
                    }
                }

                if (reg->any_of<CircleCollider2DComponent>(e))
                {
                    CircleCollider2DComponent &cc = reg->get<CircleCollider2DComponent>(e);
                    if (cc.dirty)
                    {
                        b2Shape_SetFriction(cc.shapeId, cc.friction);
                        b2Shape_SetDensity(cc.shapeId, cc.density, true);
                        b2Shape_SetRestitution(cc.shapeId, cc.restitution);

                        const b2Circle circleShape = {
                            .center = {cc.center.x, cc.center.y},
                            .radius = cc.radius
                        };
                        b2Shape_SetCircle(cc.shapeId, &circleShape);
                        cc.dirty = false;
                    }
                }

                // first, calculate the local transform
                const auto [x, y] = b2Body_GetPosition(rb.bodyId);
                const b2Rot rotation = b2Body_GetRotation(rb.bodyId);

                glm::vec3 worldTranslation = { x, y, tr.world.translation.z };
                glm::quat worldRotation = glm::quat({ 0.0f, 0.0f, b2Rot_GetAngle(rotation) });

                IDComponent &idc = reg->get<IDComponent>(e);
                if (idc.parent != 0)
                {
                    Entity parentEntity = SceneManager::GetEntity(m_Scene, idc.parent);
                    if (parentEntity.IsValid())
                    {
                        const auto &parentTr = parentEntity.GetComponent<TransformComponent>();
                        glm::mat4 parentWorldMatrix = parentTr.world.GetMatrix();
                        glm::mat4 invParentWorldMatrix = glm::inverse(parentWorldMatrix);
                        
                        glm::mat4 childWorldMatrix = glm::translate(glm::mat4(1.0f), worldTranslation) * glm::toMat4(worldRotation) * glm::scale(glm::mat4(1.0f), tr.world.scale);
                        glm::mat4 childLocalMatrix = invParentWorldMatrix * childWorldMatrix;
                        
                        glm::vec3 savedLocalScale = tr.local.scale;
                        Transform::Decompose(childLocalMatrix, tr.local);
                        tr.local.scale = savedLocalScale;
                        
                        tr.world.translation = worldTranslation;
                        tr.world.rotation = worldRotation;
                    }
                    else
                    {
                        tr.local.translation = worldTranslation;
                        tr.local.rotation = worldRotation;
                        tr.world.translation = worldTranslation;
                        tr.world.rotation = worldRotation;
                    }
                }
                else
                {
                    tr.local.translation = worldTranslation;
                    tr.local.rotation = worldRotation;
                    tr.world.translation = worldTranslation;
                    tr.world.rotation = worldRotation;
                }
            }
        }
    }

    void Physics2D::CreateBoxCollider(BoxCollider2DComponent *box, b2BodyId bodyId, b2Vec2 size)
    {
        float scaleX = 1.0f;
        float scaleY = 1.0f;
        if (glm::abs(box->size.x) > glm::epsilon<float>())
            scaleX = size.x / box->size.x;
        if (glm::abs(box->size.y) > glm::epsilon<float>())
            scaleY = size.y / box->size.y;

        const float width = glm::max(glm::abs(size.x), glm::epsilon<float>());
        const float height = glm::max(glm::abs(size.y), glm::epsilon<float>());

        const b2Vec2 offset = { box->offset.x * scaleX, box->offset.y * scaleY };
        const b2Polygon boxShape = b2MakeOffsetBox(width, height, offset, b2MakeRot(0.0f));

        b2ShapeDef shapeDef           = b2DefaultShapeDef();
        shapeDef.density              = box->density;
        shapeDef.material.friction    = box->friction;
        shapeDef.material.restitution = box->restitution;
        shapeDef.isSensor             = box->isSensor;
        box->shapeId                  = b2CreatePolygonShape(bodyId, &shapeDef, &boxShape);
    }

    void Physics2D::CreateCircleCollider(CircleCollider2DComponent *circle, b2BodyId bodyId, float size)
    {
        b2Circle circleShape =
        {
            .center = {circle->center.x, circle->center.y},
            .radius = circle->radius * size
        };

        b2ShapeDef shapeDef = b2DefaultShapeDef();
        shapeDef.density = circle->density;
        shapeDef.material.friction = circle->friction;
        shapeDef.material.restitution = circle->restitution;
        shapeDef.isSensor = circle->isSensor;
        circle->shapeId = b2CreateCircleShape(bodyId, &shapeDef, &circleShape);
    }

	bool Physics2D::IsValidBody(b2BodyId bodyId)
	{
        return b2Body_IsValid(bodyId);
	}

    void Physics2D::SetBodyType(b2BodyId bodyId, b2BodyType type)
    {
        b2Body_SetType(bodyId, type);
    }

	void Physics2D::SetPosition(b2BodyId bodyId, const glm::vec2 &position)
    {
        b2Body_SetTransform(bodyId, { position.x, position.y }, b2Body_GetRotation(bodyId));
    }

    void Physics2D::SetRotation(b2BodyId bodyId, float rotation)
    {
        b2Body_SetTransform(bodyId, b2Body_GetPosition(bodyId), b2MakeRot(glm::radians(rotation)));
    }

	void Physics2D::SetLinearVelocity(b2BodyId bodyId, const glm::vec2 &velocity)
	{
		b2Body_SetLinearVelocity(bodyId, { velocity.x, velocity.y });
	}

	void Physics2D::SetAngularVelocity(b2BodyId bodyId, float velocity)
	{
		b2Body_SetAngularVelocity(bodyId, velocity);
	}

	void Physics2D::ApplyLinearImpulse(b2BodyId bodyId, const glm::vec2 &impulse, const glm::vec2 &point, bool wake)
	{
        b2Body_ApplyLinearImpulse(bodyId, { impulse.x, impulse.y }, { point.x, point.y }, wake);
	}

    void Physics2D::ApplyLinearImpulseToCenter(b2BodyId bodyId, const glm::vec2 &impulse, bool wake)
    {
        b2Body_ApplyLinearImpulseToCenter(bodyId, { impulse.x, impulse.y }, wake);
    }

	void Physics2D::ApplyForce(b2BodyId bodyId, const glm::vec2 &force, const glm::vec2 &point, bool wake)
	{
        b2Body_ApplyForce(bodyId, { force.x, force.y }, { point.x, point.y }, wake);
	}

	void Physics2D::ApplyForceToCenter(b2BodyId bodyId, const glm::vec2 &force, bool wake)
	{
        b2Body_ApplyForceToCenter(bodyId, { force.x, force.y }, wake);
	}

	void Physics2D::ApplyTorque(b2BodyId bodyId, float torque, bool wake)
	{
        b2Body_ApplyTorque(bodyId, torque, wake);
	}

	void Physics2D::ApplyAngularImpulse(b2BodyId bodyId, float impulse, bool wake)
	{
		b2Body_ApplyAngularImpulse(bodyId, impulse, wake);
	}

	void Physics2D::ActivateBody(b2BodyId bodyId)
	{
		b2Body_Enable(bodyId);
	}

	void Physics2D::DeactivateBody(b2BodyId bodyId)
	{
        b2Body_Disable(bodyId);
	}

	void Physics2D::SetAwake(b2BodyId bodyId, bool awake)
	{
        b2Body_SetAwake(bodyId, awake);
	}

    void Physics2D::SetEnableSleep(b2BodyId bodyId, bool enable)
    {
        b2Body_EnableSleep(bodyId, enable);
    }

	void Physics2D::SetGravityScale(b2BodyId bodyId, float scale)
	{
        b2Body_SetGravityScale(bodyId, scale);
	}

	void Physics2D::SetLinearDamping(b2BodyId bodyId, float damping)
	{
        b2Body_SetLinearDamping(bodyId, damping);
	}

	void Physics2D::SetAngularDamping(b2BodyId bodyId, float damping)
	{
		b2Body_SetAngularDamping(bodyId, damping);
	}

	void Physics2D::SetMotionLock(b2BodyId bodyId, bool lockX, bool lockY, bool lockRotation)
	{
		b2Body_SetMotionLocks(bodyId, { lockX, lockY, lockRotation });
	}

    float Physics2D::GetMass(b2BodyId bodyId)
    {
        return b2Body_GetMass(bodyId);
    }

    bool Physics2D::IsBullet(b2BodyId bodyId)
    {
        return b2Body_IsBullet(bodyId);
    }

    void Physics2D::SetBullet(b2BodyId bodyId, bool bullet)
    {
        b2Body_SetBullet(bodyId, bullet);
    }

} // namespace ignite
