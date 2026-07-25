// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "animator_controller.hpp"

#include "ignite/asset/asset_manager.hpp"
#include "ignite/animation/skeleton.hpp"
#include "ignite/animation/skeletal_animation.hpp"
#include "ignite/animation/blend_space.hpp"
#include "ignite/audio/fmod_sound.hpp"
#include "ignite/core/logger.hpp"

#pragma warning(push)
#pragma warning(disable : 4275 4251)
#include <yaml-cpp/yaml.h>
#pragma warning(pop)

#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace ignite
{
    AnimState::~AnimState()
    {
		AssetManager::GetInstance()->RemoveAssetPin(m_MotionHandle, std::format("animstate.{}.{}", (uint64_t)m_UUID, (uint64_t)m_MotionHandle));
    }

	void AnimState::SetAnimationHandle(const AssetHandle &animationHandle)
	{
		SetMotion(MotionType::SkeletalAnimation, animationHandle);
	}

    void AnimState::SetBlendSpaceHandle(const AssetHandle &blendSpaceHandle)
    {
        SetMotion(MotionType::BlendSpace, blendSpaceHandle);
    }

    void AnimState::SetMotion(MotionType type, const AssetHandle &motionHandle)
    {
        if (m_MotionHandle != AssetHandle(0))
            AssetManager::GetInstance()->RemoveAssetPin(m_MotionHandle, std::format("animstate.{}.{}", (uint64_t)m_UUID, (uint64_t)m_MotionHandle));

        m_MotionType = type;
        m_MotionHandle = motionHandle;
        
        if (m_MotionHandle != AssetHandle(0))
            AssetManager::GetInstance()->AddAssetPin(m_MotionHandle, std::format("animstate.{}.{}", (uint64_t)m_UUID, (uint64_t)m_MotionHandle));
    }

	AnimatorController::~AnimatorController()
	{
        states.clear();
		AssetManager::GetInstance()->RemoveAssetPin(m_SkeletonHandle, std::format("animatorcontroller.{}.{}", (uint64_t)handle, (uint64_t)m_SkeletonHandle));
	}

	Ref<AnimatorController> AnimatorController::Clone(const Ref<AnimatorController> &other)
	{
        Ref<AnimatorController> cloneAnim = CreateRef<AnimatorController>(*other);
        cloneAnim->handle = AssetHandle();
        
        // Copy and create asset pin
        // for skeleton and states
        cloneAnim->SetSkeletonHandle(other->GetSkeletonHandle());
		for (auto &[name, state] : cloneAnim->states)
		{
            // re assign to add asset pin
			state.SetMotion(state.GetMotionType(), state.GetMotionHandle());
		}
        return cloneAnim;
	}

	void AnimatorController::SetSkeletonHandle(const AssetHandle &skeletonHandle)
	{
		m_SkeletonHandle = skeletonHandle;
		if (handle != AssetHandle(0) || m_SkeletonHandle != AssetHandle(0))
		    AssetManager::GetInstance()->AddAssetPin(m_SkeletonHandle, std::format("animatorcontroller.{}.{}", (uint64_t)handle, (uint64_t)m_SkeletonHandle));
	}

	std::string AnimatorController::EvaluateTransitions(const std::string &currentState, float normalizedTime) const
    {
        const AnimTransition *matching = FindMatchingTransition(currentState, normalizedTime);
        return matching ? matching->toState : std::string{};
    }

    const AnimTransition *AnimatorController::FindMatchingTransition(const std::string &currentState, float normalizedTime) const
    {
        for (const auto &tr : transitions)
        {
            // Match: from current state OR "Any State" (empty from)
            const bool fromMatches = tr.fromState.empty() || tr.fromState == currentState;
            if (!fromMatches)
                continue;

            // Exit time check
            if (tr.hasExitTime && normalizedTime < tr.exitTime)
                continue;

            // All conditions must pass
            bool allPass = true;
            for (const auto &cond : tr.conditions)
            {
                if (cond.paramName.empty())
                {
                    LOG_WARN("[AnimatorController] Transition '{}' -> '{}' has a condition with an empty param name — transition is permanently blocked. Fix the animator data.",
                        tr.fromState.empty() ? "AnyState" : tr.fromState, tr.toState);
                    allPass = false;
                    break;
                }

                const AnimParam *param = GetParam(cond.paramName);
                if (!param)
                {
                    LOG_WARN("[AnimatorController] Transition '{}' -> '{}' references unknown param '{}' — transition is permanently blocked. Fix the animator data.",
                        tr.fromState.empty() ? "AnyState" : tr.fromState, tr.toState, cond.paramName);
                    allPass = false;
                    break;
                }

                if (!anim_utils::EvalCondition(cond, param))
                {
                    allPass = false;
                    break;
                }
            }

            if (allPass)
                return &tr;
        }

        return nullptr;
    }

    AnimState *AnimatorController::FindState(const std::string &name)
    {
        auto it = states.find(name);
        if (it != states.end())
            return &it->second;
        return nullptr;
    }

    const AnimState *AnimatorController::FindState(const std::string &name) const
    {
		auto it = states.find(name);
		if (it != states.end())
			return &it->second;
		return nullptr;
    }

    bool AnimatorController::Serialize(const ignite::Path &filepath)
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "AnimatorController" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "DefaultState" << YAML::Value << defaultState;
        out << YAML::Key << "SkeletonHandle" << YAML::Value << static_cast<uint64_t>(GetSkeletonHandle());

        out << YAML::Key << "States" << YAML::Value << YAML::BeginSeq;
        for (const auto &[name, state] : states)
        {
            out << YAML::BeginMap;
            out << YAML::Key << "Name" << YAML::Value << name;
            out << YAML::Key << "MotionType" << YAML::Value << (state.GetMotionType() == AnimState::MotionType::BlendSpace ? "BlendSpace" : "SkeletalAnimation");
            out << YAML::Key << "MotionHandle" << YAML::Value << static_cast<uint64_t>(state.GetMotionHandle());
            // Keep this field for older tools which only understand clipstate.
            out << YAML::Key << "AnimHandle" << YAML::Value << static_cast<uint64_t>(state.GetAnimationAssetHandle());
            out << YAML::Key << "EditorPos" << YAML::Value << YAML::Flow << YAML::BeginSeq << state.editorPos.x << state.editorPos.y << YAML::EndSeq;
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;

        out << YAML::Key << "Params" << YAML::Value << YAML::BeginSeq;
        for (const auto &[name, param] : params)
        {
            out << YAML::BeginMap;
            out << YAML::Key << "Name" << YAML::Value << name;
            out << YAML::Key << "Type" << YAML::Value << anim_utils::ParamTypeToStr(param.type);
            switch (param.type)
            {
                case AnimParam::Type::Float: out << YAML::Key << "Value" << YAML::Value << param.floatVal; break;
                case AnimParam::Type::Int: out << YAML::Key << "Value" << YAML::Value << param.intVal; break;
                case AnimParam::Type::Bool: out << YAML::Key << "Value" << YAML::Value << param.boolVal; break;
                case AnimParam::Type::String: out << YAML::Key << "Value" << YAML::Value << param.strVal; break;
                default: break;
            }
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;

        out << YAML::Key << "Transitions" << YAML::Value << YAML::BeginSeq;
        for (const auto &tr : transitions)
        {
            out << YAML::BeginMap;
            out << YAML::Key << "From" << YAML::Value << tr.fromState;
            out << YAML::Key << "To" << YAML::Value << tr.toState;
            out << YAML::Key << "HasExitTime" << YAML::Value << tr.hasExitTime;
            out << YAML::Key << "ExitTime" << YAML::Value << tr.exitTime;
            out << YAML::Key << "TransitionDuration" << YAML::Value << tr.transitionDuration;

            out << YAML::Key << "Conditions" << YAML::Value << YAML::BeginSeq;
            for (const auto &cond : tr.conditions)
            {
                out << YAML::BeginMap;
                out << YAML::Key << "Param" << YAML::Value << cond.paramName;
                out << YAML::Key << "Op" << YAML::Value << anim_utils::OpToStr(cond.op);

                const AnimParam *param = GetParam(cond.paramName);
                if (param)
                {
                    switch (param->type)
                    {
                        case AnimParam::Type::Float: out << YAML::Key << "Threshold" << YAML::Value << cond.floatThreshold; break;
                        case AnimParam::Type::Int: out << YAML::Key << "Threshold" << YAML::Value << cond.intThreshold; break;
                        case AnimParam::Type::Bool: out << YAML::Key << "Threshold" << YAML::Value << cond.boolThreshold; break;
                        case AnimParam::Type::String: out << YAML::Key << "Threshold" << YAML::Value << cond.strThreshold; break;
                        default: break;
                    }
                }
                else
                {
                    out << YAML::Key << "Threshold" << YAML::Value << cond.floatThreshold;
                }

                out << YAML::EndMap;
            }
            out << YAML::EndSeq;
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;

        out << YAML::EndMap;
        out << YAML::EndMap;

        std::ofstream file(filepath);
        if (!file.is_open())
        {
            LOG_ERROR("[AnimatorController] Failed to open file for writing: {}", filepath.string());
            return false;
        }

        file << out.c_str();
        SetDirtyFlag(false);
        return true;
    }

    Ref<AnimatorController> AnimatorController::Deserialize(const ignite::Path &filepath)
    {
        if (!ignite::Path::exists(filepath))
        {
            LOG_ERROR("[AnimatorController] File does not exists {}", filepath.string());
            return nullptr;
        }

        YAML::Node root;
        try
        {
            root = YAML::LoadFile(filepath.string());
        }
        catch (const YAML::Exception &e)
        {
            LOG_ERROR("[AnimatorController] YAML parse error: {}", e.what());
            return nullptr;
        }

        YAML::Node node = root["AnimatorController"];
        if (!node)
            return nullptr;

        auto assetManager = AssetManager::GetInstance();

        auto ctrl = CreateRef<AnimatorController>();
        if (auto n = node["DefaultState"]) ctrl->defaultState = n.as<std::string>();
        if (auto n = node["SkeletonHandle"]) ctrl->m_SkeletonHandle = AssetHandle(n.as<uint64_t>());

        if (YAML::Node statesNode = node["States"]; statesNode && statesNode.IsSequence())
        {
			// Preallocate states vector to avoid multiple reallocations - destruction of AnimState objects can be expensive due to asset pin management.
            ctrl->states.reserve(statesNode.size());
            for (const auto &sn : statesNode)
            {
                AnimState state;
                if (auto n = sn["Name"]) state.name = n.as<std::string>();

                const auto type = sn["MotionType"] && sn["MotionType"].as<std::string>() == "BlendSpace"
                    ? AnimState::MotionType::BlendSpace : AnimState::MotionType::SkeletalAnimation;

                if (auto n = sn["MotionHandle"]) state.SetMotion(type, AssetHandle(n.as<uint64_t>()));
                else if (auto n = sn["AnimHandle"]) state.SetAnimationHandle(AssetHandle(n.as<uint64_t>()));

                if (auto n = sn["EditorPos"]; n && n.IsSequence() && n.size() == 2)
                    state.editorPos = { n[0].as<float>(), n[1].as<float>() };

                ctrl->states.emplace(state.name, state);
            }
        }

        if (YAML::Node paramsNode = node["Params"]; paramsNode && paramsNode.IsSequence())
        {
            ctrl->params.reserve(paramsNode.size());
            for (const auto &pn : paramsNode)
            {
                AnimParam param;
                if (auto n = pn["Name"]) param.name = n.as<std::string>();
                if (auto n = pn["Type"]) param.type = anim_utils::StrToParamType(n.as<std::string>());
                if (auto n = pn["Value"])
                {
                    switch (param.type)
                    {
                    case AnimParam::Type::Float: param.floatVal = n.as<float>(); break;
                    case AnimParam::Type::Int: param.intVal = n.as<int>(); break;
                    case AnimParam::Type::Bool: param.boolVal = n.as<bool>(); break;
                    case AnimParam::Type::String: param.strVal = n.as<std::string>(); break;
                        default: break;
                    }
                }

                ctrl->params.emplace(param.name, param);
            }
        }

        if (YAML::Node transitionsNode = node["Transitions"]; transitionsNode && transitionsNode.IsSequence())
        {
            for (const auto &tn : transitionsNode)
            {
                AnimTransition tr;
                if (auto n = tn["From"]) tr.fromState = n.as<std::string>();
                if (auto n = tn["To"]) tr.toState = n.as<std::string>();
                if (auto n = tn["HasExitTime"]) tr.hasExitTime = n.as<bool>();
                if (auto n = tn["ExitTime"]) tr.exitTime = n.as<float>();
                if (auto n = tn["TransitionDuration"]) tr.transitionDuration = n.as<float>();

                if (YAML::Node condsNode = tn["Conditions"]; condsNode && condsNode.IsSequence())
                {
                    for (const auto &cn : condsNode)
                    {
                        AnimCondition cond;
                        if (auto n = cn["Param"]) cond.paramName = n.as<std::string>();
                        if (auto n = cn["Op"]) cond.op = anim_utils::StrToOp(n.as<std::string>());

                        const AnimParam *param = ctrl->GetParam(cond.paramName);
                        if (auto n = cn["Threshold"])
                        {
                            if (param)
                            {
                                switch (param->type)
                                {
                                    case AnimParam::Type::Float: cond.floatThreshold = n.as<float>(); break;
                                    case AnimParam::Type::Int: cond.intThreshold = n.as<int>(); break;
                                    case AnimParam::Type::Bool: cond.boolThreshold = n.as<bool>(); break;
                                    case AnimParam::Type::String: cond.strThreshold = n.as<std::string>(); break;
                                    default: break;
                                }
                            }
                            else
                            {
                                try { cond.floatThreshold = n.as<float>(); } catch (...) {}
                            }
                        }

                        tr.conditions.push_back(cond);
                    }
                }

                ctrl->transitions.push_back(tr);
            }
        }

        ctrl->SetDirtyFlag(false);
        return ctrl;
    }

    Ref<AnimatorController> AnimatorController::Create()
    {
        auto ctrl = CreateRef<AnimatorController>();
        return ctrl;
    }

    bool AnimatorController::UpdateSkeleton(float deltaTime, AnimatorControllerRuntime &runtime, AssetManager *assetManager)
    {
        if (!assetManager || GetSkeletonHandle() == AssetHandle(0))
            return false;

        Ref<Skeleton> skeleton = assetManager->GetAsset<Skeleton>(GetSkeletonHandle());
        if (!skeleton)
            return false;

        if (runtime.currentStateName.empty())
        {
            runtime.currentStateName = !defaultState.empty() 
                ? defaultState 
                : (states.empty() ? std::string{} : states.begin()->first); // get key of state
            runtime.stateElapsed = 0.0f;
            runtime.stateNormalized = 0.0f;
        }

        const AnimState *state = FindState(runtime.currentStateName);
        if (!state && !states.empty())
        {
            runtime.currentStateName = states.begin()->first; // get key of state
            state = FindState(runtime.currentStateName);
            runtime.stateElapsed = 0.0f;
            runtime.stateNormalized = 0.0f;
        }

        auto resolveMotion = [&](const AnimState *motionState, std::vector<std::pair<Ref<SkeletalAnimation>, float>> &outContrib) -> bool
        {
            outContrib.clear();
            if (!motionState || motionState->GetMotionHandle() == AssetHandle(0))
                return false;

            if (motionState->GetMotionType() == AnimState::MotionType::SkeletalAnimation)
            {
                Ref<SkeletalAnimation> animation = assetManager->GetAsset<SkeletalAnimation>(motionState->GetMotionHandle());
                if (animation) outContrib.emplace_back(animation, 1.0f);
            }
            else
            {
                Ref<BlendSpace> blendSpace = assetManager->GetAsset<BlendSpace>(motionState->GetMotionHandle());
                if (!blendSpace) return false;
                const AnimParam *x = GetParam(blendSpace->axisXName);
                const AnimParam *y = GetParam(blendSpace->axisYName);
                const glm::vec2 input(x && x->type == AnimParam::Type::Float ? x->floatVal : 0.0f,
                    y && y->type == AnimParam::Type::Float ? y->floatVal : 0.0f);
                for (const BlendSpaceWeight &weight : blendSpace->Evaluate(input))
                {
                    Ref<SkeletalAnimation> animation = assetManager->GetAsset<SkeletalAnimation>(weight.GetAnimationAssetHandle());
                    if (animation && animation->duration > 0.0f)
                        outContrib.emplace_back(animation, weight.weight);
                }
            }
            return !outContrib.empty();
        };

        std::vector<std::pair<Ref<SkeletalAnimation>, float>> sourceContribs;
        if (!resolveMotion(state, sourceContribs))
            return false;

        runtime.previousStateNormalized = runtime.stateNormalized;
        runtime.stateElapsed += deltaTime;
        
        // Calculate effective weighted duration of current motion
        float effectiveDuration = 0.0f;
        for (const auto &[anim, weight] : sourceContribs)
        {
            const float durSec = anim->ticksPerSeconds > 0.0f ? (anim->duration / anim->ticksPerSeconds) : 0.0f;
            effectiveDuration += durSec * weight;
        }

        if (effectiveDuration > 0.0001f)
        {
            runtime.stateNormalized = std::fmod(runtime.stateNormalized + (deltaTime / effectiveDuration), 1.0f);
            if (runtime.stateNormalized < 0.0f)
                runtime.stateNormalized += 1.0f;
        }
        else
        {
            runtime.stateNormalized = 0.0f;
        }

        // Trigger Animation Events (e.g., Audio actions)
        const float prevNorm = runtime.previousStateNormalized;
        const float currNorm = runtime.stateNormalized;
        for (const auto &contrib : sourceContribs)
        {
            if (!contrib.first)
                continue;

            for (const auto &evt : contrib.first->timelineEvents)
            {
                bool triggered = false;
                if (currNorm >= prevNorm)
                {
                    triggered = (evt.normalizedTime >= prevNorm && evt.normalizedTime <= currNorm && prevNorm != currNorm);
                }
                else
                {
                    triggered = (evt.normalizedTime >= prevNorm || evt.normalizedTime <= currNorm);
                }

                if (triggered)
                {
                    if (evt.action == AnimationTimelineEvent::Action::Audio && evt.GetAudioHandle() != AssetHandle(0))
                    {
                        if (auto sound = assetManager->GetAsset<FmodSound>(evt.GetAudioHandle()))
                        {
                            sound->Play();
                        }
                    }
                }
            }
        }

        // State Transition Handling
        if (runtime.isTransitioning)
        {
            runtime.transitionElapsed += deltaTime;
            if (runtime.transitionDuration > 0.0001f && runtime.transitionElapsed >= runtime.transitionDuration)
            {
                runtime.currentStateName = runtime.transitionTargetState;
                runtime.stateElapsed = 0.0f;
                // Preserve phase synchronization when transition finishes
                runtime.previousStateNormalized = runtime.stateNormalized;
                runtime.isTransitioning = false;
                runtime.transitionTargetState.clear();
                runtime.transitionElapsed = 0.0f;

                state = FindState(runtime.currentStateName);
                if (!resolveMotion(state, sourceContribs))
                    return false;
            }
        }
        else
        {
            const AnimTransition *matchingTr = FindMatchingTransition(runtime.currentStateName, runtime.stateNormalized);
            if (matchingTr && matchingTr->toState != runtime.currentStateName)
            {
                if (const AnimState *nextState = FindState(matchingTr->toState); nextState && nextState->GetMotionHandle() != AssetHandle(0))
                {
                    if (matchingTr->transitionDuration > 0.0001f)
                    {
                        runtime.isTransitioning = true;
                        runtime.transitionTargetState = matchingTr->toState;
                        runtime.transitionDuration = matchingTr->transitionDuration;
                        runtime.transitionElapsed = 0.0f;
                    }
                    else
                    {
                        runtime.currentStateName = matchingTr->toState;
                        runtime.stateElapsed = 0.0f;
                        runtime.previousStateNormalized = runtime.stateNormalized;
                        state = nextState;
                        if (!resolveMotion(state, sourceContribs))
                            return false;
                    }
                }
            }
        }

        // Evaluate target motion if transitioning
        std::vector<std::pair<Ref<SkeletalAnimation>, float>> targetContribs;
        if (runtime.isTransitioning)
        {
            const AnimState *targetState = FindState(runtime.transitionTargetState);
            resolveMotion(targetState, targetContribs);
        }

        // Timeline events are evaluated from dominant clip
        runtime.triggeredEventIndices.clear();
        runtime.eventSourceAnimation = AssetHandle(0);
        if (!sourceContribs.empty())
        {
            const Ref<SkeletalAnimation> &eventAnimation = sourceContribs.front().first;
            runtime.eventSourceAnimation = eventAnimation->handle;
            const float previous = runtime.previousStateNormalized;
            const float current = runtime.stateNormalized;
            for (uint32_t i = 0; i < static_cast<uint32_t>(eventAnimation->timelineEvents.size()); ++i)
            {
                const float marker = eventAnimation->timelineEvents[i].normalizedTime;
                const bool crossed = current >= previous
                    ? (marker > previous && marker <= current)
                    : (marker > previous || marker <= current);
                if (crossed)
                    runtime.triggeredEventIndices.push_back(i);
            }
        }

        const size_t jointCount = skeleton->joints.size();
        if (runtime.localPoses.size() != jointCount)
        {
            runtime.localPoses.resize(jointCount);
            runtime.globalPoses.resize(jointCount);
            runtime.finalTransforms.resize(jointCount);
        }

        for (size_t i = 0; i < jointCount; ++i)
        {
            runtime.localPoses[i] = skeleton->joints[i].defaultTransform;
        }

        const float blendAlpha = runtime.isTransitioning && runtime.transitionDuration > 0.0001f
            ? std::clamp(runtime.transitionElapsed / runtime.transitionDuration, 0.0f, 1.0f)
            : 0.0f;

        // Calculate joint local poses (with transition cross-fading if active)
        for (size_t jointIndex = 0; jointIndex < jointCount; ++jointIndex)
        {
            const Joint &joint = skeleton->joints[jointIndex];

            auto evaluatePoseFromContribs = [&](const std::vector<std::pair<Ref<SkeletalAnimation>, float>> &contribs, float normTime) -> Transform
            {
                glm::vec3 translation(0.0f), scale(0.0f);
                glm::quat rotationSum(0.0f, 0.0f, 0.0f, 0.0f);
                glm::quat referenceRotation = joint.defaultTransform.rotation;
                bool hasReference = false;

                for (const auto &[sampleAnimation, weight] : contribs)
                {
                    const float sampleTime = std::fmod(normTime * sampleAnimation->duration, sampleAnimation->duration);
                    Transform pose = joint.defaultTransform;
                    if (const auto channel = sampleAnimation->channels.find(static_cast<int>(jointIndex)); channel != sampleAnimation->channels.end())
                        pose = channel->second.Calculate(sampleTime, joint.defaultTransform);

                    glm::quat rotation = pose.rotation;
                    if (hasReference && glm::dot(referenceRotation, rotation) < 0.0f)
                        rotation = -rotation;
                    else if (!hasReference)
                    {
                        referenceRotation = rotation;
                        hasReference = true;
                    }
                    translation += pose.translation * weight;
                    scale += pose.scale * weight;
                    rotationSum.w += rotation.w * weight;
                    rotationSum.x += rotation.x * weight;
                    rotationSum.y += rotation.y * weight;
                    rotationSum.z += rotation.z * weight;
                }

                if (glm::length(rotationSum) > 0.000001f)
                    rotationSum = glm::normalize(rotationSum);
                else
                    rotationSum = joint.defaultTransform.rotation;

                return Transform{ translation, rotationSum, scale };
            };

            Transform poseSource = evaluatePoseFromContribs(sourceContribs, runtime.stateNormalized);
            if (runtime.isTransitioning && !targetContribs.empty())
            {
                const float targetNormTime = runtime.stateNormalized;
                Transform poseTarget = evaluatePoseFromContribs(targetContribs, targetNormTime);

                glm::quat rotA = poseSource.rotation;
                glm::quat rotB = poseTarget.rotation;
                if (glm::dot(rotA, rotB) < 0.0f) rotB = -rotB;

                Transform blended;
                blended.translation = glm::mix(poseSource.translation, poseTarget.translation, blendAlpha);
                blended.rotation = glm::normalize(glm::slerp(rotA, rotB, blendAlpha));
                blended.scale = glm::mix(poseSource.scale, poseTarget.scale, blendAlpha);
                runtime.localPoses[jointIndex] = blended;
            }
            else
            {
                runtime.localPoses[jointIndex] = poseSource;
            }
        }

        // Compute global poses
        for (size_t i = 0; i < jointCount; ++i)
        {
            const Joint &joint = skeleton->joints[i];
            if (joint.parentJointId == -1)
            {
                runtime.globalPoses[i] = runtime.localPoses[i];
            }
            else
            {
                const Transform& parent = runtime.globalPoses[joint.parentJointId];
                Transform &global = runtime.globalPoses[i];
                global.translation = parent.translation + parent.rotation * (parent.scale * runtime.localPoses[i].translation);
                global.rotation = parent.rotation * runtime.localPoses[i].rotation;
                global.scale = parent.scale * runtime.localPoses[i].scale;
            }
        }

        // Compute GPU-ready final transforms
        for (size_t i = 0; i < jointCount; ++i)
        {
            runtime.finalTransforms[i] = runtime.globalPoses[i].GetMatrix() * skeleton->joints[i].inverseBindPose;
        }

        return true;
    }
}
