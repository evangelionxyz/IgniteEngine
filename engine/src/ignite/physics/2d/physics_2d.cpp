// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "physics_2d.hpp"
#include <ignite/scene/scene.hpp>

#include "ignite/scene/component.hpp"
#include "ignite/scene/scene_manager.hpp"
#include "ignite/core/profiler/profiler.hpp"

namespace ignite
{
    Physics2D::Physics2D(Scene *scene)
        : m_Scene(scene)
    {
    }

    Physics2D::~Physics2D()
    {
		SimulationStop();
    }

    void Physics2D::SimulationStart()
    {
        IGN_PROFILE_FUNCTION();
        b2WorldDef worldDef = b2DefaultWorldDef();
        m_WorldId = b2CreateWorld(&worldDef);

        entt::registry *reg = m_Scene->registry;
        auto view = reg->view<Rigidbody2DComponent>();
        for (entt::entity e : view)
        {
            IDComponent &id          = reg->get<IDComponent>(e);
            Rigidbody2DComponent &rb = reg->get<Rigidbody2DComponent>(e);
            TransformComponent &tr   = reg->get<TransformComponent>(e);

            b2BodyDef bodyDef        = b2DefaultBodyDef();
            bodyDef.type             = GetB2BodyType(rb.type);
            bodyDef.position         = { tr.translation.x, tr.translation.y };
            bodyDef.rotation         = b2MakeRot(eulerAngles(tr.rotation).z);
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
                CreateBoxCollider(&bc, rb.bodyId, b2Vec2(bc.size.x * tr.scale.x, bc.size.y * tr.scale.y));
                b2Shape_SetUserData(bc.shapeId, static_cast<void *>(&e));
            }

            // create circle collider
            if (reg->any_of<CircleCollider2DComponent>(e))
            {
                auto &cc = reg->get<CircleCollider2DComponent>(e);
				CreateCircleCollider(&cc, rb.bodyId, glm::max(tr.scale.x, tr.scale.y));
				b2Shape_SetUserData(cc.shapeId, static_cast<void *>(&e));
            }
        }
    }

    void Physics2D::SimulationStop()
    {
        IGN_PROFILE_FUNCTION();
        if (B2_IS_NULL(m_WorldId) == false)
            b2DestroyWorld(m_WorldId);

        m_WorldId = b2_nullWorldId;
    }

    void Physics2D::InstantiateEntity(Entity entity)
    {
        if (!entity.HasComponent<Rigidbody2DComponent>())
            return;

        auto &id = entity.GetComponent<IDComponent>();
        auto &tr = entity.GetComponent<TransformComponent>();

        auto &rb = entity.GetComponent<Rigidbody2DComponent>();

        b2BodyDef bodyDef        = b2DefaultBodyDef();
        bodyDef.type             = GetB2BodyType(rb.type);
        bodyDef.position         = { tr.translation.x, tr.translation.y };
        bodyDef.rotation         = b2MakeRot(eulerAngles(tr.rotation).z);
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
            CreateBoxCollider(&bc, rb.bodyId, b2Vec2(bc.size.x * tr.scale.x, bc.size.y * tr.scale.y));
            b2Shape_SetUserData(bc.shapeId, static_cast<void *>(&entity));
        }

		// create circle collider
		if (entity.HasComponent<CircleCollider2DComponent>())
		{
			auto &cc = entity.GetComponent<CircleCollider2DComponent>();
			CreateCircleCollider(&cc, rb.bodyId, glm::max(tr.scale.x, tr.scale.y));
			b2Shape_SetUserData(cc.shapeId, static_cast<void *>(&entity));
		}
    }

	void Physics2D::DestroyEntity(Entity entity)
    {
		if (entity.HasComponent<BoxCollider2DComponent>())
		{
			auto &c = entity.GetComponent<BoxCollider2DComponent>();

			// check b2world is already created
			if (b2World_IsValid(m_WorldId))
			{
				b2DestroyShape(c.shapeId, false);
			}
		}

		if (entity.HasComponent<CircleCollider2DComponent>())
		{
			auto &c = entity.GetComponent<CircleCollider2DComponent>();

			// check b2world is already created
			if (b2World_IsValid(m_WorldId))
			{
				b2DestroyShape(c.shapeId, false);
			}
		}

        if (entity.HasComponent<Rigidbody2DComponent>())
        {
			auto &rb = entity.GetComponent<Rigidbody2DComponent>();

			// check b2world is already created
			if (b2World_IsValid(m_WorldId))
			{
				b2DestroyBody(rb.bodyId);
			}
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

					float width = glm::abs(bc.size.x * tr.scale.x);
					float height = glm::abs(bc.size.y * tr.scale.y);

					width = glm::max(width, glm::epsilon<float>());
					height = glm::max(height, glm::epsilon<float>());
                    const b2Vec2 offset = { bc.offset.x * tr.scale.x, bc.offset.y * tr.scale.y };
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
            const b2Vec2 linearVelocity = b2Body_GetLinearVelocity(rb.bodyId);

            tr.localTranslation = { x, y, tr.translation.z };
			tr.localRotation = glm::quat({ 0.0f, 0.0f, b2Rot_GetAngle(rotation) });

            tr.translation = tr.localTranslation;
            tr.rotation = tr.localRotation;

            rb.linearVelocity = { linearVelocity.x, linearVelocity.y };
            rb.angularVelocity = b2Body_GetAngularVelocity(rb.bodyId);
            rb.isAwake = b2Body_IsAwake(rb.bodyId);
             rb.isEnabled = b2Body_IsEnabled(rb.bodyId);
            }
        }
    }

    void Physics2D::CreateBoxCollider(BoxCollider2DComponent *box, b2BodyId bodyId, b2Vec2 size)
    {
        float width = glm::abs(size.x);
        float height = glm::abs(size.y);
        width = glm::max(width, glm::epsilon<float>());
        height = glm::max(height, glm::epsilon<float>());
        
        box->currentSize = { width, height };

        float scaleX = 1.0f;
        float scaleY = 1.0f;
        if (glm::abs(box->size.x) > glm::epsilon<float>())
            scaleX = size.x / box->size.x;
        if (glm::abs(box->size.y) > glm::epsilon<float>())
            scaleY = size.y / box->size.y;

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

} // namespace ignite
