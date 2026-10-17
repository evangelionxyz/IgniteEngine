// Copyright (c) 2026 Evangelion Manuhutu

#include "jolt_joint.hpp"

#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/SliderConstraint.h>
#include <Jolt/Physics/Constraints/ConeConstraint.h>

#include "ignite/core/logger.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace ignite::physics
{
	static JPH::Vec3 GlmToJoltVec3(const glm::vec3 &v)
	{
		return { v.x, v.y, v.z };
	}

	static JPH::RVec3 GlmToJoltRVec3(const glm::vec3 &v)
	{
		return { v.x, v.y, v.z };
	}

	static JPH::EMotorState ToJoltMotorState(const JointMotorState state)
	{
		switch (state)
		{
			case JointMotorState::Off:      return JPH::EMotorState::Off;
			case JointMotorState::Velocity: return JPH::EMotorState::Velocity;
			case JointMotorState::Position: return JPH::EMotorState::Position;
		}
		return JPH::EMotorState::Off;
	}

	static void ComputeOrthonormalAxes(const glm::vec3 &inAxis, const glm::vec3 &inNormal, JPH::Vec3 &outAxis, JPH::Vec3 &outNormal)
	{
		JPH::Vec3 axis = GlmToJoltVec3(inAxis);
		if (axis.LengthSq() < 1e-6f)
		{
			axis = JPH::Vec3::sAxisX();
		}
		else
		{
			axis = axis.Normalized();
		}

		JPH::Vec3 normal = GlmToJoltVec3(inNormal);
		// Orthogonalize normal against axis
		normal = normal - normal.Dot(axis) * axis;
		if (normal.LengthSq() < 1e-6f)
		{
			normal = axis.GetNormalizedPerpendicular();
		}
		else
		{
			normal = normal.Normalized();
		}

		outAxis = axis;
		outNormal = normal;
	}

	JoltJoint::JoltJoint(
		const JointDesc &desc,
		uint64_t userData,
		JPH::PhysicsSystem *physicsSystem,
		JPH::BodyInterface *bodyInterface,
		JPH::BodyID bodyID1,
		JPH::BodyID bodyID2,
		const TransformDesc &transformA,
		const TransformDesc &transformB
	)
		: m_UserData(userData)
		, m_JointType(desc.type)
		, m_BreakForce(desc.breakForce)
		, m_BreakTorque(desc.breakTorque)
		, m_PhysicsSystem(physicsSystem)
		, m_BodyInterface(bodyInterface)
	{
		LOG_ASSERT(m_PhysicsSystem && m_BodyInterface, "[Jolt Physics] PhysicsSystem or BodyInterface is null!");
		if (bodyID1.IsInvalid() || bodyID1 == bodyID2)
		{
			LOG_WARN("[Jolt Physics] Cannot create joint with invalid or identical body IDs!");
			return;
		}

		const bool hasBodyB = !bodyID2.IsInvalid();

		glm::vec3 worldPoint1 = desc.localAnchorA;
		glm::vec3 worldAxis1 = desc.localAxisA;
		glm::vec3 worldNormal1 = desc.localNormalA;

		glm::vec3 worldPoint2 = desc.localAnchorB;
		glm::vec3 worldAxis2 = desc.localAxisB;
		glm::vec3 worldNormal2 = desc.localNormalB;

		if (desc.space == JointSpace::Local)
		{
			worldPoint1 = transformA.position + transformA.rotation * (transformA.scale * desc.localAnchorA);
			worldAxis1 = transformA.rotation * desc.localAxisA;
			worldNormal1 = transformA.rotation * desc.localNormalA;

			if (desc.autoConfigureConnected)
			{
				if (desc.type == JointType::Distance && hasBodyB)
				{
					worldPoint2 = transformB.position + transformB.rotation * (transformB.scale * desc.localAnchorB);
				}
				else
				{
					worldPoint2 = worldPoint1;
				}
				worldAxis2 = worldAxis1;
				worldNormal2 = worldNormal1;
			}
			else if (hasBodyB)
			{
				worldPoint2 = transformB.position + transformB.rotation * (transformB.scale * desc.localAnchorB);
				worldAxis2 = transformB.rotation * desc.localAxisB;
				worldNormal2 = transformB.rotation * desc.localNormalB;
			}
		}
		else // JointSpace::World
		{
			if (desc.autoConfigureConnected && desc.type != JointType::Distance)
			{
				worldPoint2 = worldPoint1;
				worldAxis2 = worldAxis1;
				worldNormal2 = worldNormal1;
			}
		}

		JPH::Vec3 joltAxis1, joltNormal1;
		JPH::Vec3 joltAxis2, joltNormal2;
		ComputeOrthonormalAxes(worldAxis1, worldNormal1, joltAxis1, joltNormal1);
		ComputeOrthonormalAxes(worldAxis2, worldNormal2, joltAxis2, joltNormal2);

		JPH::Ref<JPH::TwoBodyConstraintSettings> settings;

		switch (desc.type)
		{
			case JointType::Fixed:
			{
				auto *fixedSettings = new JPH::FixedConstraintSettings();
				fixedSettings->mSpace = JPH::EConstraintSpace::WorldSpace;
				fixedSettings->mAutoDetectPoint = false;
				fixedSettings->mPoint1 = GlmToJoltRVec3(worldPoint1);
				fixedSettings->mAxisX1 = joltAxis1;
				fixedSettings->mAxisY1 = joltNormal1;
				fixedSettings->mPoint2 = GlmToJoltRVec3(worldPoint2);
				fixedSettings->mAxisX2 = joltAxis2;
				fixedSettings->mAxisY2 = joltNormal2;
				settings = fixedSettings;
				break;
			}
			case JointType::Distance:
			{
				auto *distSettings = new JPH::DistanceConstraintSettings();
				distSettings->mSpace = JPH::EConstraintSpace::WorldSpace;
				distSettings->mPoint1 = GlmToJoltRVec3(worldPoint1);
				distSettings->mPoint2 = GlmToJoltRVec3(worldPoint2);
				distSettings->mMinDistance = std::max(0.0f, desc.minDistance);
				distSettings->mMaxDistance = desc.maxDistance;
				if (desc.enableSpring)
				{
					distSettings->mLimitsSpringSettings.mMode = JPH::ESpringMode::FrequencyAndDamping;
					distSettings->mLimitsSpringSettings.mFrequency = std::max(0.0f, desc.springFrequency);
					distSettings->mLimitsSpringSettings.mDamping = std::max(0.0f, desc.springDamping);
				}
				settings = distSettings;
				break;
			}
			case JointType::Point:
			{
				auto *pointSettings = new JPH::PointConstraintSettings();
				pointSettings->mSpace = JPH::EConstraintSpace::WorldSpace;
				pointSettings->mPoint1 = GlmToJoltRVec3(worldPoint1);
				pointSettings->mPoint2 = GlmToJoltRVec3(worldPoint2);
				settings = pointSettings;
				break;
			}
			case JointType::Hinge:
			{
				auto *hingeSettings = new JPH::HingeConstraintSettings();
				hingeSettings->mSpace = JPH::EConstraintSpace::WorldSpace;
				hingeSettings->mPoint1 = GlmToJoltRVec3(worldPoint1);
				hingeSettings->mHingeAxis1 = joltAxis1;
				hingeSettings->mNormalAxis1 = joltNormal1;
				hingeSettings->mPoint2 = GlmToJoltRVec3(worldPoint2);
				hingeSettings->mHingeAxis2 = joltAxis2;
				hingeSettings->mNormalAxis2 = joltNormal2;

				if (desc.enableLimits)
				{
					float minRad = glm::clamp(glm::radians(desc.limitsMin), -JPH::JPH_PI, 0.0f);
					float maxRad = glm::clamp(glm::radians(desc.limitsMax), 0.0f, JPH::JPH_PI);
					if (std::abs(maxRad - minRad) < 1e-4f)
					{
						maxRad = std::min(JPH::JPH_PI, minRad + 1e-3f);
					}
					hingeSettings->mLimitsMin = minRad;
					hingeSettings->mLimitsMax = maxRad;
				}
				else
				{
					hingeSettings->mLimitsMin = -JPH::JPH_PI;
					hingeSettings->mLimitsMax = JPH::JPH_PI;
				}

				if (desc.enableSpring)
				{
					hingeSettings->mLimitsSpringSettings.mMode = JPH::ESpringMode::FrequencyAndDamping;
					hingeSettings->mLimitsSpringSettings.mFrequency = std::max(0.0f, desc.springFrequency);
					hingeSettings->mLimitsSpringSettings.mDamping = std::max(0.0f, desc.springDamping);
				}

				hingeSettings->mMaxFrictionTorque = std::max(0.0f, desc.maxFriction);
				hingeSettings->mMotorSettings.mSpringSettings.mMode = JPH::ESpringMode::FrequencyAndDamping;
				hingeSettings->mMotorSettings.mSpringSettings.mFrequency = std::max(0.0f, desc.motorFrequency);
				hingeSettings->mMotorSettings.mSpringSettings.mDamping = std::max(0.0f, desc.motorDamping);
				hingeSettings->mMotorSettings.SetTorqueLimit(std::max(0.0f, desc.maxMotorForce));
				settings = hingeSettings;
				break;
			}
			case JointType::Slider:
			{
				auto *sliderSettings = new JPH::SliderConstraintSettings();
				sliderSettings->mSpace = JPH::EConstraintSpace::WorldSpace;
				sliderSettings->mAutoDetectPoint = false;
				sliderSettings->mPoint1 = GlmToJoltRVec3(worldPoint1);
				sliderSettings->mSliderAxis1 = joltAxis1;
				sliderSettings->mNormalAxis1 = joltNormal1;
				sliderSettings->mPoint2 = GlmToJoltRVec3(worldPoint2);
				sliderSettings->mSliderAxis2 = joltAxis2;
				sliderSettings->mNormalAxis2 = joltNormal2;

				if (desc.enableLimits)
				{
					float minLim = std::min(0.0f, desc.limitsMin);
					float maxLim = std::max(0.0f, desc.limitsMax);
					if (std::abs(maxLim - minLim) < 1e-4f)
					{
						maxLim = minLim + 1e-3f;
					}
					sliderSettings->mLimitsMin = minLim;
					sliderSettings->mLimitsMax = maxLim;
				}
				else
				{
					sliderSettings->mLimitsMin = -FLT_MAX;
					sliderSettings->mLimitsMax = FLT_MAX;
				}

				if (desc.enableSpring)
				{
					sliderSettings->mLimitsSpringSettings.mMode = JPH::ESpringMode::FrequencyAndDamping;
					sliderSettings->mLimitsSpringSettings.mFrequency = std::max(0.0f, desc.springFrequency);
					sliderSettings->mLimitsSpringSettings.mDamping = std::max(0.0f, desc.springDamping);
				}

				sliderSettings->mMaxFrictionForce = std::max(0.0f, desc.maxFriction);
				sliderSettings->mMotorSettings.mSpringSettings.mMode = JPH::ESpringMode::FrequencyAndDamping;
				sliderSettings->mMotorSettings.mSpringSettings.mFrequency = std::max(0.0f, desc.motorFrequency);
				sliderSettings->mMotorSettings.mSpringSettings.mDamping = std::max(0.0f, desc.motorDamping);
				sliderSettings->mMotorSettings.SetForceLimit(std::max(0.0f, desc.maxMotorForce));
				settings = sliderSettings;
				break;
			}
			case JointType::Cone:
			{
				auto *coneSettings = new JPH::ConeConstraintSettings();
				coneSettings->mSpace = JPH::EConstraintSpace::WorldSpace;
				coneSettings->mPoint1 = GlmToJoltRVec3(worldPoint1);
				coneSettings->mTwistAxis1 = joltAxis1;
				coneSettings->mPoint2 = GlmToJoltRVec3(worldPoint2);
				coneSettings->mTwistAxis2 = joltAxis2;
				coneSettings->mHalfConeAngle = glm::clamp(glm::radians(desc.halfConeAngle), 0.0f, JPH::JPH_PI);
				settings = coneSettings;
				break;
			}
		}

		if (!settings)
		{
			return;
		}

		settings->mEnabled = desc.enabled;
		settings->mUserData = userData;

		m_Constraint = m_BodyInterface->CreateConstraint(settings.GetPtr(), bodyID1, bodyID2);
		if (m_Constraint)
		{
			m_PhysicsSystem->AddConstraint(m_Constraint.GetPtr());
			m_AddedToSystem = true;

			// Apply motor state for Hinge / Slider
			UpdateFromDesc(desc);
		}
	}

	JoltJoint::~JoltJoint()
	{
		DestroyJoint();
	}

	void JoltJoint::DestroyJoint()
	{
		if (m_Constraint && m_PhysicsSystem && m_AddedToSystem)
		{
			m_PhysicsSystem->RemoveConstraint(m_Constraint.GetPtr());
			m_AddedToSystem = false;
		}
		m_Constraint = nullptr;
	}

	void JoltJoint::SetEnabled(bool enabled)
	{
		if (m_Constraint && !m_IsBroken)
		{
			m_Constraint->SetEnabled(enabled);
			if (m_BodyInterface)
			{
				m_BodyInterface->ActivateConstraint(m_Constraint.GetPtr());
			}
		}
	}

	bool JoltJoint::IsEnabled() const
	{
		return m_Constraint && m_Constraint->GetEnabled() && !m_IsBroken;
	}

	void JoltJoint::UpdateFromDesc(const JointDesc &desc)
	{
		if (!m_Constraint)
		{
			return;
		}

		m_BreakForce = desc.breakForce;
		m_BreakTorque = desc.breakTorque;

		if (!m_IsBroken)
		{
			m_Constraint->SetEnabled(desc.enabled);
		}

		switch (m_JointType)
		{
			case JointType::Distance:
			{
				auto *dist = static_cast<JPH::DistanceConstraint *>(m_Constraint.GetPtr());
				float minDist = std::max(0.0f, desc.minDistance);
				float maxDist = desc.maxDistance >= 0.0f ? std::max(minDist, desc.maxDistance) : dist->GetMaxDistance();
				dist->SetDistance(minDist, maxDist);

				JPH::SpringSettings spring;
				spring.mMode = JPH::ESpringMode::FrequencyAndDamping;
				spring.mFrequency = desc.enableSpring ? std::max(0.0f, desc.springFrequency) : 0.0f;
				spring.mDamping = desc.enableSpring ? std::max(0.0f, desc.springDamping) : 0.0f;
				dist->SetLimitsSpringSettings(spring);
				break;
			}
			case JointType::Hinge:
			{
				auto *hinge = static_cast<JPH::HingeConstraint *>(m_Constraint.GetPtr());
				if (desc.enableLimits)
				{
					float minRad = glm::clamp(glm::radians(desc.limitsMin), -JPH::JPH_PI, 0.0f);
					float maxRad = glm::clamp(glm::radians(desc.limitsMax), 0.0f, JPH::JPH_PI);
					if (std::abs(maxRad - minRad) < 1e-4f)
					{
						maxRad = std::min(JPH::JPH_PI, minRad + 1e-3f);
					}
					hinge->SetLimits(minRad, maxRad);
				}
				else
				{
					hinge->SetLimits(-JPH::JPH_PI, JPH::JPH_PI);
				}

				JPH::SpringSettings spring;
				spring.mMode = JPH::ESpringMode::FrequencyAndDamping;
				spring.mFrequency = desc.enableSpring ? std::max(0.0f, desc.springFrequency) : 0.0f;
				spring.mDamping = desc.enableSpring ? std::max(0.0f, desc.springDamping) : 0.0f;
				hinge->SetLimitsSpringSettings(spring);

				hinge->SetMaxFrictionTorque(std::max(0.0f, desc.maxFriction));

				JPH::MotorSettings &motor = hinge->GetMotorSettings();
				motor.mSpringSettings.mMode = JPH::ESpringMode::FrequencyAndDamping;
				motor.mSpringSettings.mFrequency = std::max(0.0f, desc.motorFrequency);
				motor.mSpringSettings.mDamping = std::max(0.0f, desc.motorDamping);
				motor.SetTorqueLimit(std::max(0.0f, desc.maxMotorForce));

				hinge->SetMotorState(ToJoltMotorState(desc.motorState));
				if (desc.motorState == JointMotorState::Velocity)
				{
					hinge->SetTargetAngularVelocity(glm::radians(desc.motorTargetVelocity));
				}
				else if (desc.motorState == JointMotorState::Position)
				{
					hinge->SetTargetAngle(glm::radians(desc.motorTargetPosition));
				}
				break;
			}
			case JointType::Slider:
			{
				auto *slider = static_cast<JPH::SliderConstraint *>(m_Constraint.GetPtr());
				if (desc.enableLimits)
				{
					float minLim = std::min(0.0f, desc.limitsMin);
					float maxLim = std::max(0.0f, desc.limitsMax);
					if (std::abs(maxLim - minLim) < 1e-4f)
					{
						maxLim = minLim + 1e-3f;
					}
					slider->SetLimits(minLim, maxLim);
				}
				else
				{
					slider->SetLimits(-FLT_MAX, FLT_MAX);
				}

				JPH::SpringSettings spring;
				spring.mMode = JPH::ESpringMode::FrequencyAndDamping;
				spring.mFrequency = desc.enableSpring ? std::max(0.0f, desc.springFrequency) : 0.0f;
				spring.mDamping = desc.enableSpring ? std::max(0.0f, desc.springDamping) : 0.0f;
				slider->SetLimitsSpringSettings(spring);

				slider->SetMaxFrictionForce(std::max(0.0f, desc.maxFriction));

				JPH::MotorSettings &motor = slider->GetMotorSettings();
				motor.mSpringSettings.mMode = JPH::ESpringMode::FrequencyAndDamping;
				motor.mSpringSettings.mFrequency = std::max(0.0f, desc.motorFrequency);
				motor.mSpringSettings.mDamping = std::max(0.0f, desc.motorDamping);
				motor.SetForceLimit(std::max(0.0f, desc.maxMotorForce));

				slider->SetMotorState(ToJoltMotorState(desc.motorState));
				if (desc.motorState == JointMotorState::Velocity)
				{
					slider->SetTargetVelocity(desc.motorTargetVelocity);
				}
				else if (desc.motorState == JointMotorState::Position)
				{
					slider->SetTargetPosition(desc.motorTargetPosition);
				}
				break;
			}
			case JointType::Cone:
			{
				auto *cone = static_cast<JPH::ConeConstraint *>(m_Constraint.GetPtr());
				cone->SetHalfConeAngle(glm::clamp(glm::radians(desc.halfConeAngle), 0.0f, JPH::JPH_PI));
				break;
			}
			default:
				break;
		}

		if (m_BodyInterface && !m_IsBroken && desc.enabled)
		{
			m_BodyInterface->ActivateConstraint(m_Constraint.GetPtr());
		}
	}

	bool JoltJoint::CheckBreakCondition(float deltaTime)
	{
		if (!m_Constraint || m_IsBroken || !m_Constraint->GetEnabled() || deltaTime <= 1e-6f)
		{
			return false;
		}

		if (m_BreakForce <= 0.0f && m_BreakTorque <= 0.0f)
		{
			return false;
		}

		float linearImpulse = 0.0f;
		float angularImpulse = 0.0f;

		switch (m_JointType)
		{
			case JointType::Fixed:
			{
				const auto *fixed = static_cast<const JPH::FixedConstraint *>(m_Constraint.GetPtr());
				linearImpulse = fixed->GetTotalLambdaPosition().Length();
				angularImpulse = fixed->GetTotalLambdaRotation().Length();
				break;
			}
			case JointType::Distance:
			{
				const auto *dist = static_cast<const JPH::DistanceConstraint *>(m_Constraint.GetPtr());
				linearImpulse = std::abs(dist->GetTotalLambdaPosition());
				break;
			}
			case JointType::Point:
			{
				const auto *point = static_cast<const JPH::PointConstraint *>(m_Constraint.GetPtr());
				linearImpulse = point->GetTotalLambdaPosition().Length();
				break;
			}
			case JointType::Hinge:
			{
				const auto *hinge = static_cast<const JPH::HingeConstraint *>(m_Constraint.GetPtr());
				linearImpulse = hinge->GetTotalLambdaPosition().Length();
				const auto rotVec = hinge->GetTotalLambdaRotation();
				const float rotLimits = hinge->GetTotalLambdaRotationLimits();
				const float rotMotor = hinge->GetTotalLambdaMotor();
				angularImpulse = std::sqrt(rotVec[0] * rotVec[0] + rotVec[1] * rotVec[1] + rotLimits * rotLimits + rotMotor * rotMotor);
				break;
			}
			case JointType::Slider:
			{
				const auto *slider = static_cast<const JPH::SliderConstraint *>(m_Constraint.GetPtr());
				const auto posVec = slider->GetTotalLambdaPosition();
				const float posLimits = slider->GetTotalLambdaPositionLimits();
				const float posMotor = slider->GetTotalLambdaMotor();
				linearImpulse = std::sqrt(posVec[0] * posVec[0] + posVec[1] * posVec[1] + posLimits * posLimits + posMotor * posMotor);
				angularImpulse = slider->GetTotalLambdaRotation().Length();
				break;
			}
			case JointType::Cone:
			{
				const auto *cone = static_cast<const JPH::ConeConstraint *>(m_Constraint.GetPtr());
				linearImpulse = cone->GetTotalLambdaPosition().Length();
				angularImpulse = std::abs(cone->GetTotalLambdaRotation());
				break;
			}
		}

		const float force = linearImpulse / deltaTime;
		const float torque = angularImpulse / deltaTime;

		if ((m_BreakForce > 0.0f && force > m_BreakForce) || (m_BreakTorque > 0.0f && torque > m_BreakTorque))
		{
			m_IsBroken = true;
			m_Constraint->SetEnabled(false);
			if (m_BodyInterface)
			{
				m_BodyInterface->ActivateConstraint(m_Constraint.GetPtr());
			}
			return true;
		}

		return false;
	}
}
