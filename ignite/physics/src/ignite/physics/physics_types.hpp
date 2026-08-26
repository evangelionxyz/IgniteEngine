// Copyright (c) 2026 Evangelion Manuhutu
#pragma once
#ifndef IGN_PHYSICS_TYPES_HPP
#define IGN_PHYSICS_TYPES_HPP

#include "ignite/core/base.hpp"
#include "ignite/core/types.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstdint>
#include <string>
#include <array>
#include <vector>

namespace ignite::physics
{
	enum class Physics3DType : uint8_t
	{
		Jolt = 0,
		Box3D = 1,
		PhysX = 2,
	};

	enum class BodyType : uint8_t
	{
		Static = 0,
		Kinematic = 1,
		Dynamic = 2,
	};

	enum class MotionQuality : uint8_t
	{
		Discrete = 0,
		LinearCast = 1,
	};

	enum class ColliderType : uint8_t
	{
		Box = 0,
		Sphere = 1,
		Capsule = 2,
		Mesh = 3,
		Plane = 4,
		HeightField = 5,
	};

	enum class CollisionEventType : uint8_t
	{
		Enter = 0,
		Stay = 1,
		Exit = 2,
	};

	enum class ActivationEventType : uint8_t
	{
		Activated = 0,
		Deactivated = 1,
	};

	struct PhysicsTransformData
	{
		glm::vec3 position = { 0.0f, 0.0f, 0.0f };
		glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	};

	static constexpr uint32_t MAX_PHYSICS_LAYERS = 32;

	struct Physics3DSettings
	{
		glm::vec3 gravity = glm::vec3(0.0f, -9.81f, 0.0f);
		uint32_t layerCount = 16;
		std::array<std::string, MAX_PHYSICS_LAYERS> layerNames = {
			"Default", "Static", "Kinematic", "Player", "Enemy", "Trigger", "Vehicle", "Water",
			"Layer 8", "Layer 9", "Layer 10", "Layer 11", "Layer 12", "Layer 13", "Layer 14", "Layer 15",
			"Layer 16", "Layer 17", "Layer 18", "Layer 19", "Layer 20", "Layer 21", "Layer 22", "Layer 23",
			"Layer 24", "Layer 25", "Layer 26", "Layer 27", "Layer 28", "Layer 29", "Layer 30", "Layer 31"
		};
		std::array<uint32_t, MAX_PHYSICS_LAYERS> collisionMasks = []() {
			std::array<uint32_t, MAX_PHYSICS_LAYERS> masks{};
			for (size_t i = 0; i < MAX_PHYSICS_LAYERS; ++i)
				masks[i] = 0xFFFFFFFF;
			return masks;
		}();

		bool CanLayersCollide(uint32_t layerA, uint32_t layerB) const
		{
			if (layerA >= MAX_PHYSICS_LAYERS || layerB >= MAX_PHYSICS_LAYERS)
				return false;
			return (collisionMasks[layerA] & (1u << layerB)) != 0;
		}

		void SetLayerCollision(uint32_t layerA, uint32_t layerB, bool canCollide)
		{
			if (layerA >= MAX_PHYSICS_LAYERS || layerB >= MAX_PHYSICS_LAYERS)
				return;
			if (canCollide)
			{
				collisionMasks[layerA] |= (1u << layerB);
				collisionMasks[layerB] |= (1u << layerA);
			}
			else
			{
				collisionMasks[layerA] &= ~(1u << layerB);
				collisionMasks[layerB] &= ~(1u << layerA);
			}
		}

		uint32_t GetLayerMask(uint32_t layer) const
		{
			if (layer >= MAX_PHYSICS_LAYERS)
				return 0xFFFFFFFF;
			return collisionMasks[layer];
		}
	};

	struct CollisionEvent
	{
		CollisionEventType type = CollisionEventType::Enter;
		uint64_t userDataA = 0;
		uint64_t userDataB = 0;
		glm::vec3 contactPoint = { 0.0f, 0.0f, 0.0f };
		glm::vec3 contactNormal = { 0.0f, 1.0f, 0.0f };
	};

	struct BodyActivationEvent
	{
		ActivationEventType type = ActivationEventType::Activated;
		uint64_t userData = 0;
	};

	// ---------------------------------------------------------
	// Descriptors for creation (No Scene or Component dependencies)
	// ---------------------------------------------------------

	struct RigidBodyDesc
	{
		BodyType bodyType = BodyType::Dynamic;
		MotionQuality motionQuality = MotionQuality::Discrete;
		uint32_t layer = 0;

		bool useGravity = true;
		bool rotateX = true, rotateY = true, rotateZ = true;
		bool moveX = true, moveY = true, moveZ = true;
		bool allowSleeping = true;
		bool isSensor = false;
		bool applyGyroscopicForce = false;

		float mass = 1.0f;
		float gravityFactor = 1.0f;
		float linearDamping = 0.0f;
		float angularDamping = 0.05f;
		float friction = 0.2f;
		float restitution = 0.0f;
		float maxLinearVelocity = 500.0f;
		float maxAngularVelocity = 47.1238898f; // ~15 * PI

		glm::vec3 linearVelocity = { 0.0f, 0.0f, 0.0f };
		glm::vec3 angularVelocity = { 0.0f, 0.0f, 0.0f };
		glm::vec3 centerMass = { 0.0f, 0.0f, 0.0f };
	};

	struct BoxColliderDesc
	{
		glm::vec3 center = { 0.0f, 0.0f, 0.0f };
		glm::vec3 halfExtents = { 0.5f, 0.5f, 0.5f };
	};

	struct SphereColliderDesc
	{
		glm::vec3 center = { 0.0f, 0.0f, 0.0f };
		float radius = 0.5f;
	};

	struct CapsuleColliderDesc
	{
		glm::vec3 center = { 0.0f, 0.0f, 0.0f };
		float radius = 0.5f;
		float halfHeight = 2.0f;
	};

	struct PlaneColliderDesc
	{
		glm::vec3 center = { 0.0f, 0.0f, 0.0f };
		glm::vec3 scale = { 1.0f, 1.0f, 1.0f };
	};

	struct MeshColliderDesc
	{
		glm::vec3 center = { 0.0f, 0.0f, 0.0f };
		std::vector<glm::vec3> vertices;
		std::vector<uint32_t> indices;
		bool isConvex = false;
	};

	struct HeightFieldColliderDesc
	{
		glm::vec3 center = { 0.0f, 0.0f, 0.0f };
		glm::vec3 scale = { 1.0f, 1.0f, 1.0f };
		uint32_t sampleCount = 0;
		std::vector<float> heights;
	};

	struct CharacterControllerDesc
	{
		glm::vec3 center = { 0.0f, 0.0f, 0.0f };
		float radius = 0.5f;
		float halfHeight = 2.0f;
		float maxStepHeight = 0.4f;
		float maxSlopeAngle = 45.0f; // in degrees
		float mass = 80.0f;
		float friction = 0.2f;
		float gravityFactor = 1.0f;
		glm::vec3 up = { 0.0f, 1.0f, 0.0f };
		glm::vec3 linearVelocity = { 0.0f, 0.0f, 0.0f };
	};
}

#endif
