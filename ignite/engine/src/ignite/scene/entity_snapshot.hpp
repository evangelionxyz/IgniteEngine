/* MIT License
 * 
 * Copyright (c) 2026 Evangelion Manuhutu
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include "component_group.hpp"
#include "entity.hpp"
#include "scene_manager.hpp"

#include <optional>
#include <vector>

namespace ignite
{
    // ---------------------------------------------------------------------------
    // EntitySnapshot — a by-value copy of all editor-relevant entity data.
    // Runtime-only physics handles (b2BodyId, JPH::Body*) are intentionally
    // excluded; they will be recreated when the scene starts playing.
    // ---------------------------------------------------------------------------
    struct EntitySnapshot
    {
        // Identity
        UUID       uuid;
        std::string name;
        EntityType type = EntityType_Node;
        UUID       parent = UUID(0);
        std::vector<UUID> children;

        // Components (optional so we only store the ones the entity actually has)
        std::optional<TransformComponent>      transform;
        std::optional<Sprite2DComponent>       sprite2D;
        std::optional<Circle2DComponent>       circle2D;
        std::optional<CameraComponent>         camera;
        std::optional<WorldEnvironment>        worldEnv;
        std::optional<AudioSourceComponent>    audioSource;
        std::optional<ScriptComponent>         script;

        // 2D Physics — body / shape IDs are runtime-only, we copy editable fields only
        struct Rigidbody2DSnapshot
        {
            physics::BodyType bodyType = physics::BodyType::Static;
            glm::vec2 linearVelocity = { 0.0f, 0.0f };
            float angularVelocity = 0.0f;
            float gravityScale = 1.0f;
            float linearDamping = 0.6f;
            float angularDamping = 0.2f;
            bool isAwake = true;
            bool isEnabled = true;
            bool isEnableSleep = false;
            bool allowFastRotation = true;
            bool fixedRotation = false;
        };

        std::optional<Rigidbody2DSnapshot> rigidbody2D;

        struct BoxCollider2DSnapshot
        {
            glm::vec2 size = { 0.5f, 0.5f };
            glm::vec2 offset = { 0.0f, 0.0f };
            float     restitution = 0.1f;
            float     friction = 0.5f;
            float     density = 1.0f;
            bool      isSensor = false;
        };

        std::optional<BoxCollider2DSnapshot> boxCollider2D;

        struct CircleCollider2DSnapshot
        {
            glm::vec2 center = { 0.0f, 0.0f };
            float     radius = 0.5f;
            float     restitution = 0.1f;
            float     friction = 0.5f;
            float     density = 1.0f;
            bool      isSensor = false;
        };
        std::optional<CircleCollider2DSnapshot> circleCollider2D;

        struct RigidbodySnapshot
        {
			physics::BodyType bodyType = physics::BodyType::Static;
            bool  useGravity = true;
            bool  rotateX = true, rotateY = true, rotateZ = true;
            bool  moveX = true, moveY = true, moveZ = true;
            float mass = 1.0f;
            bool  allowSleeping = true;
            bool  retainAcceleration = false;
            float gravityFactor = 1.0f;
            glm::vec3 centerMass = { 0.0f, 0.0f, 0.0f };
        };
        std::optional<RigidbodySnapshot> rigidbody;

        std::optional<BoxColliderComponent>    boxCollider;
        std::optional<SphereColliderComponent> sphereCollider;
        std::optional<CapsuleColliderComponent> capsuleCollider;
        // MeshColliderComponent intentionally omitted (large vertex/index data)
    };

    // ---------------------------------------------------------------------------
    // SnapshotEntity — captures all copyable state from an entity.
    // Call this BEFORE destroying the entity.
    // ---------------------------------------------------------------------------
    inline EntitySnapshot SnapshotEntity(Scene *scene, Entity entity)
    {
        EntitySnapshot snap;

        if (!entity.IsValid())
            return snap;

        const IDComponent &id = entity.GetComponent<IDComponent>();
        snap.uuid     = id.uuid;
        snap.name     = id.name;
        snap.type     = id.type;
        snap.parent   = id.parent;
        snap.children = id.children;

        if (entity.HasComponent<TransformComponent>())
            snap.transform = entity.GetComponent<TransformComponent>();

        if (entity.HasComponent<Sprite2DComponent>())
            snap.sprite2D = entity.GetComponent<Sprite2DComponent>();

        if (entity.HasComponent<Circle2DComponent>())
            snap.circle2D = entity.GetComponent<Circle2DComponent>();

        if (entity.HasComponent<CameraComponent>())
            snap.camera = entity.GetComponent<CameraComponent>();

        if (entity.HasComponent<WorldEnvironment>())
            snap.worldEnv = entity.GetComponent<WorldEnvironment>();

        if (entity.HasComponent<AudioSourceComponent>())
            snap.audioSource = entity.GetComponent<AudioSourceComponent>();

        if (entity.HasComponent<ScriptComponent>())
            snap.script = entity.GetComponent<ScriptComponent>();

        if (entity.HasComponent<Rigidbody2DComponent>())
        {
            const Rigidbody2DComponent &rb = entity.GetComponent<Rigidbody2DComponent>();
            EntitySnapshot::Rigidbody2DSnapshot s;
            s.bodyType         = rb.bodyType;
            s.linearVelocity   = rb.linearVelocity;
            s.angularVelocity  = rb.angularVelocity;
            s.gravityScale     = rb.gravityScale;
            s.linearDamping    = rb.linearDamping;
            s.angularDamping   = rb.angularDamping;
            s.isAwake          = rb.isAwake;
            s.isEnabled        = rb.isEnabled;
            s.isEnableSleep    = rb.isEnableSleep;
            s.allowFastRotation = rb.allowFastRotation;
            s.fixedRotation    = rb.fixedRotation;
            snap.rigidbody2D   = s;
        }

        if (entity.HasComponent<BoxCollider2DComponent>())
        {
            const BoxCollider2DComponent &bc = entity.GetComponent<BoxCollider2DComponent>();
            EntitySnapshot::BoxCollider2DSnapshot s;
            s.size        = bc.size;
            s.offset      = bc.offset;
            s.restitution = bc.restitution;
            s.friction    = bc.friction;
            s.density     = bc.density;
            s.isSensor    = bc.isSensor;
            snap.boxCollider2D = s;
        }

		if (entity.HasComponent<CircleCollider2DComponent>())
		{
			const CircleCollider2DComponent &cc = entity.GetComponent<CircleCollider2DComponent>();
			EntitySnapshot::CircleCollider2DSnapshot s;
			s.center = cc.center;
			s.radius = cc.radius;
			s.restitution = cc.restitution;
			s.friction = cc.friction;
			s.density = cc.density;
			s.isSensor = cc.isSensor;
			snap.circleCollider2D = s;
		}

        if (entity.HasComponent<RigidbodyComponent>())
        {
            const RigidbodyComponent &rb = entity.GetComponent<RigidbodyComponent>();
            EntitySnapshot::RigidbodySnapshot s;
            s.bodyType           = rb.bodyType;
            s.useGravity         = rb.useGravity;
            s.rotateX            = rb.rotateX;
            s.rotateY            = rb.rotateY;
            s.rotateZ            = rb.rotateZ;
            s.moveX              = rb.moveX;
            s.moveY              = rb.moveY;
            s.moveZ              = rb.moveZ;
            s.mass               = rb.mass;
            s.allowSleeping      = rb.allowSleeping;
            s.retainAcceleration = rb.retainAcceleration;
            s.gravityFactor      = rb.gravityFactor;
            s.centerMass         = rb.centerMass;
            snap.rigidbody       = s;
        }

        if (entity.HasComponent<BoxColliderComponent>())
            snap.boxCollider = entity.GetComponent<BoxColliderComponent>();

        if (entity.HasComponent<SphereColliderComponent>())
            snap.sphereCollider = entity.GetComponent<SphereColliderComponent>();

        if (entity.HasComponent<CapsuleColliderComponent>())
            snap.capsuleCollider = entity.GetComponent<CapsuleColliderComponent>();

        return snap;
    }

    // ---------------------------------------------------------------------------
    // RestoreEntity — recreates an entity from a snapshot.
    // This is used by Undo of EntityDestroyCommand.
    // NOTE: children must be restored separately (the destroy command snapshots
    //       the whole hierarchy as a flat list and restores them in order).
    // ---------------------------------------------------------------------------
    inline Entity RestoreEntity(Scene *scene, const EntitySnapshot &snap)
    {
        // Re-create with the SAME uuid so existing references stay valid
        Entity e = SceneManager::CreateEntity(scene, snap.name, snap.type, snap.uuid);

        IDComponent &id = e.GetComponent<IDComponent>();
        id.parent   = snap.parent;
        id.children = snap.children;

        if (snap.transform)
        {
            e.GetComponent<TransformComponent>() = *snap.transform;
        }

        if (snap.sprite2D)
            e.AddOrReplaceComponent<Sprite2DComponent>(*snap.sprite2D);

		if (snap.circle2D)
			e.AddOrReplaceComponent<Circle2DComponent>(*snap.circle2D);

        if (snap.camera)
            e.AddOrReplaceComponent<CameraComponent>(*snap.camera);

        if (snap.worldEnv)
            e.AddOrReplaceComponent<WorldEnvironment>(*snap.worldEnv);

        if (snap.audioSource)
            e.AddOrReplaceComponent<AudioSourceComponent>(*snap.audioSource);

        if (snap.script)
            e.AddOrReplaceComponent<ScriptComponent>(*snap.script);

        if (snap.rigidbody2D)
        {
            const auto &s = *snap.rigidbody2D;
            Rigidbody2DComponent rb;
            rb.bodyType              = s.bodyType;
            rb.linearVelocity    = s.linearVelocity;
            rb.angularVelocity   = s.angularVelocity;
            rb.gravityScale      = s.gravityScale;
            rb.linearDamping     = s.linearDamping;
            rb.angularDamping    = s.angularDamping;
            rb.isAwake           = s.isAwake;
            rb.isEnabled         = s.isEnabled;
            rb.isEnableSleep     = s.isEnableSleep;
            rb.allowFastRotation = s.allowFastRotation;
            rb.fixedRotation     = s.fixedRotation;
            // bodyId is left default — physics system initialises it at runtime
            e.AddOrReplaceComponent<Rigidbody2DComponent>(rb);
        }

        if (snap.boxCollider2D)
        {
            const auto &s = *snap.boxCollider2D;
            BoxCollider2DComponent bc;
            bc.size        = s.size;
            bc.offset      = s.offset;
            bc.restitution = s.restitution;
            bc.friction    = s.friction;
            bc.density     = s.density;
            bc.isSensor    = s.isSensor;
            e.AddOrReplaceComponent<BoxCollider2DComponent>(bc);
        }

		if (snap.circleCollider2D)
		{
			const auto &s = *snap.circleCollider2D;
			CircleCollider2DComponent cc;
			cc.center = s.center;
			cc.radius = s.radius;
			cc.restitution = s.restitution;
			cc.friction = s.friction;
			cc.density = s.density;
			cc.isSensor = s.isSensor;
			e.AddOrReplaceComponent<CircleCollider2DComponent>(cc);
		}

        if (snap.rigidbody)
        {
            const auto &s = *snap.rigidbody;
            RigidbodyComponent rb;
            rb.bodyType           = s.bodyType;
            rb.useGravity         = s.useGravity;
            rb.rotateX            = s.rotateX;
            rb.rotateY            = s.rotateY;
            rb.rotateZ            = s.rotateZ;
            rb.moveX              = s.moveX;
            rb.moveY              = s.moveY;
            rb.moveZ              = s.moveZ;
            rb.mass               = s.mass;
            rb.allowSleeping      = s.allowSleeping;
            rb.retainAcceleration = s.retainAcceleration;
            rb.gravityFactor      = s.gravityFactor;
            rb.centerMass         = s.centerMass;
            // body ptr is left null — physics system assigns it at runtime
            e.AddOrReplaceComponent<RigidbodyComponent>(rb);
        }

        if (snap.boxCollider)
            e.AddOrReplaceComponent<BoxColliderComponent>(*snap.boxCollider);

        if (snap.sphereCollider)
            e.AddOrReplaceComponent<SphereColliderComponent>(*snap.sphereCollider);

        if (snap.capsuleCollider)
            e.AddOrReplaceComponent<CapsuleColliderComponent>(*snap.capsuleCollider);

        return e;
    }
}
