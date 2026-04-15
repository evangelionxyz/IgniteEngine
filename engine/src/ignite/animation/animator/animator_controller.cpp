// Copyright (c) 2026 Evangelion Manuhutu

#include "animator_controller.hpp"

#include "ignite/asset/asset_manager.hpp"
#include "ignite/animation/skeleton.hpp"
#include "ignite/animation/skeletal_animation.hpp"
#include "ignite/core/logger.hpp"

#pragma warning(push)
#pragma warning(disable : 4275 4251)
#include <yaml-cpp/yaml.h>
#pragma warning(pop)

#include <fstream>
#include <algorithm>
#include <cmath>

#include <glm/gtx/quaternion.hpp>

namespace ignite
{

    std::string AnimatorController::EvaluateTransitions(const std::string &currentState, float normalizedTime) const
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
                return tr.toState;
        }

        return {};
    }

    AnimState *AnimatorController::FindState(const std::string &name)
    {
        auto it = std::find_if(states.begin(), states.end(), [&name](const AnimState &s) { return s.name == name; });
        return it != states.end() ? &(*it) : nullptr;
    }

    const AnimState *AnimatorController::FindState(const std::string &name) const
    {
        auto it = std::find_if(states.begin(), states.end(), [&name](const AnimState &s) { return s.name == name; });
        return it != states.end() ? &(*it) : nullptr;
    }

    bool AnimatorController::Serialize(const std::filesystem::path &filepath)
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "AnimatorController" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "DefaultState" << YAML::Value << defaultState;
        out << YAML::Key << "SkeletonHandle" << YAML::Value << static_cast<uint64_t>(skeletonHandle);

        out << YAML::Key << "States" << YAML::Value << YAML::BeginSeq;
        for (const auto &s : states)
        {
            out << YAML::BeginMap;
            out << YAML::Key << "Name" << YAML::Value << s.name;
            out << YAML::Key << "AnimHandle" << YAML::Value << static_cast<uint64_t>(s.animHandle);
            out << YAML::Key << "EditorPos" << YAML::Value << YAML::Flow << YAML::BeginSeq << s.editorPos.x << s.editorPos.y << YAML::EndSeq;
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;

        out << YAML::Key << "Params" << YAML::Value << YAML::BeginSeq;
        for (const auto &p : params)
        {
            out << YAML::BeginMap;
            out << YAML::Key << "Name" << YAML::Value << p.name;
            out << YAML::Key << "Type" << YAML::Value << anim_utils::ParamTypeToStr(p.type);
            switch (p.type)
            {
                case AnimParam::Type::Float: out << YAML::Key << "Value" << YAML::Value << p.floatVal; break;
                case AnimParam::Type::Int: out << YAML::Key << "Value" << YAML::Value << p.intVal; break;
                case AnimParam::Type::Bool: out << YAML::Key << "Value" << YAML::Value << p.boolVal; break;
                case AnimParam::Type::String: out << YAML::Key << "Value" << YAML::Value << p.strVal; break;
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

    Ref<AnimatorController> AnimatorController::Deserialize(const std::filesystem::path &filepath)
    {
        if (!std::filesystem::exists(filepath))
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

        auto ctrl = CreateRef<AnimatorController>();
        if (auto n = node["DefaultState"]) ctrl->defaultState = n.as<std::string>();
        if (auto n = node["SkeletonHandle"]) ctrl->skeletonHandle = AssetHandle(n.as<uint64_t>());

        if (YAML::Node statesNode = node["States"]; statesNode && statesNode.IsSequence())
        {
            for (const auto &sn : statesNode)
            {
                AnimState s;
                if (auto n = sn["Name"]) s.name = n.as<std::string>();
                if (auto n = sn["AnimHandle"]) s.animHandle = AssetHandle(n.as<uint64_t>());
                if (auto n = sn["EditorPos"]; n && n.IsSequence() && n.size() == 2)
                    s.editorPos = { n[0].as<float>(), n[1].as<float>() };
                ctrl->states.push_back(s);
            }
        }

        if (YAML::Node paramsNode = node["Params"]; paramsNode && paramsNode.IsSequence())
        {
            for (const auto &pn : paramsNode)
            {
                AnimParam p;
                if (auto n = pn["Name"]) p.name = n.as<std::string>();
                if (auto n = pn["Type"]) p.type = anim_utils::StrToParamType(n.as<std::string>());
                if (auto n = pn["Value"])
                {
                    switch (p.type)
                    {
                        case AnimParam::Type::Float: p.floatVal = n.as<float>(); break;
                        case AnimParam::Type::Int: p.intVal = n.as<int>(); break;
                        case AnimParam::Type::Bool: p.boolVal = n.as<bool>(); break;
                        case AnimParam::Type::String: p.strVal = n.as<std::string>(); break;
                        default: break;
                    }
                }
                ctrl->params.push_back(p);
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
        ctrl->SetReadyFlag(true);
        return ctrl;
    }

    Ref<AnimatorController> AnimatorController::Create()
    {
        auto ctrl = CreateRef<AnimatorController>();
        ctrl->SetReadyFlag(true);
        return ctrl;
    }

    bool AnimatorController::UpdateSkeleton(float deltaTime, AnimatorControllerRuntime &runtime, AssetManager *assetManager)
    {
        if (!assetManager || skeletonHandle == AssetHandle(0))
            return false;

        Ref<Asset> skeletonAsset = assetManager->GetAsset(skeletonHandle);
        if (!skeletonAsset)
            skeletonAsset = assetManager->GetAssetImmediate(skeletonHandle);
        Ref<Skeleton> skeleton = skeletonAsset ? skeletonAsset->As<Skeleton>() : nullptr;
        if (!skeleton)
            return false;

        if (runtime.currentStateName.empty())
        {
            runtime.currentStateName = !defaultState.empty() ? defaultState : (states.empty() ? std::string{} : states.front().name);
            runtime.stateElapsed = 0.0f;
            runtime.stateNormalized = 0.0f;
        }

        const AnimState *state = FindState(runtime.currentStateName);
        if (!state && !states.empty())
        {
            runtime.currentStateName = states.front().name;
            state = FindState(runtime.currentStateName);
            runtime.stateElapsed = 0.0f;
            runtime.stateNormalized = 0.0f;
        }

        if (!state || state->animHandle == AssetHandle(0))
            return false;

        Ref<Asset> animationAsset = assetManager->GetAsset(state->animHandle);
        if (!animationAsset)
            animationAsset = assetManager->GetAssetImmediate(state->animHandle);
        Ref<SkeletalAnimation> animation = animationAsset ? animationAsset->As<SkeletalAnimation>() : nullptr;
        if (!animation || animation->duration <= 0.0f)
            return false;

        runtime.stateElapsed += deltaTime;
        const float durationSeconds = animation->ticksPerSeconds > 0.0f ? (animation->duration / animation->ticksPerSeconds) : 0.0f;
        if (durationSeconds > 0.0f)
        {
            runtime.stateNormalized = std::fmod(runtime.stateElapsed, durationSeconds) / durationSeconds;
        }
        else
        {
            runtime.stateNormalized = 0.0f;
        }

        const std::string nextStateName = EvaluateTransitions(runtime.currentStateName, runtime.stateNormalized);
        if (!nextStateName.empty() && nextStateName != runtime.currentStateName)
        {
            if (const AnimState *nextState = FindState(nextStateName); nextState && nextState->animHandle != AssetHandle(0))
            {
                runtime.currentStateName = nextStateName;
                runtime.stateElapsed = 0.0f;
                runtime.stateNormalized = 0.0f;

                state = nextState;
                animationAsset = assetManager->GetAsset(state->animHandle);
                if (!animationAsset)
                    animationAsset = assetManager->GetAssetImmediate(state->animHandle);
                animation = animationAsset ? animationAsset->As<SkeletalAnimation>() : nullptr;
                if (!animation || animation->duration <= 0.0f)
                    return false;
            }
        }

        const float animTime = std::fmod(runtime.stateElapsed * animation->ticksPerSeconds, animation->duration);
        const size_t jointCount = skeleton->joints.size();

        // Build per-instance local transforms from skeleton defaults (read-only)
        std::vector<glm::mat4> localTransforms(jointCount);
        for (size_t i = 0; i < jointCount; ++i)
        {
            const Joint &j = skeleton->joints[i];
            localTransforms[i] = glm::translate(glm::mat4(1.0f), j.defaultTranslation)
                * glm::toMat4(j.defaultRotation)
                * glm::scale(glm::mat4(1.0f), j.defaultScale);
        }

        // Apply animation channels to per-instance local transforms
        for (auto &[nodeName, channel] : animation->channels)
        {
            if (const auto it = skeleton->nameToJointMap.find(nodeName); it != skeleton->nameToJointMap.end())
            {
                const i32 jointIndex = it->second;
                const Joint &j = skeleton->joints[jointIndex];
                localTransforms[jointIndex] = channel.CalculateTransform(animTime,
                    j.defaultTranslation, j.defaultRotation, j.defaultScale);
            }
        }

        // Compute per-instance global transforms using read-only skeleton hierarchy
        std::vector<glm::mat4> globalTransforms(jointCount);
        for (size_t i = 0; i < jointCount; ++i)
        {
            const Joint &j = skeleton->joints[i];
            globalTransforms[i] = (j.parentJointId == -1)
                ? localTransforms[i]
                : globalTransforms[j.parentJointId] * localTransforms[i];
        }

        // Compute GPU-ready final transforms: globalTransform * inverseBindPose
        runtime.finalTransforms.resize(jointCount);
        for (size_t i = 0; i < jointCount; ++i)
            runtime.finalTransforms[i] = globalTransforms[i] * skeleton->joints[i].inverseBindPose;

        return true;
    }

}
