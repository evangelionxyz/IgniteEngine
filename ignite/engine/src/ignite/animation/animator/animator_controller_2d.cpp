// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "ignite/asset/asset_manager.hpp"
#include "animator_controller_2d.hpp"
#include "ignite/core/logger.hpp"

#pragma warning(push)
#pragma warning(disable : 4275 4251)
#include <yaml-cpp/yaml.h>
#pragma warning(pop)


namespace ignite
{
	void AnimState2D::SetAnimationHandle(const AssetHandle &animationHandle)
	{
		m_AnimHandle = animationHandle;
		AssetManager::GetInstance()->AddAssetPin(m_AnimHandle, std::format("animstate2d.{}.{}", (uint64_t)m_UUID, (uint64_t)m_AnimHandle));
	}

    AnimState2D::~AnimState2D()
    {
		AssetManager::GetInstance()->RemoveAssetPin(m_AnimHandle, std::format("animstate2d.{}.{}", (uint64_t)m_UUID, (uint64_t)m_AnimHandle));
    }

    std::string AnimatorController2D::EvaluateTransitions(const std::string &currentState, float normalizedTime) const
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
                const AnimParam *param = GetParam(cond.paramName);
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

    AnimState2D *AnimatorController2D::FindState(const std::string &name)
    {
        auto it = std::find_if(states.begin(), states.end(), [&name](const AnimState2D &s) { return s.name == name; });
        return it != states.end() ? &(*it) : nullptr;
    }

    const AnimState2D *AnimatorController2D::FindState(const std::string &name) const
    {
        auto it = std::find_if(states.begin(), states.end(), [&name](const AnimState2D &s) { return s.name == name; });
        return it != states.end() ? &(*it) : nullptr;
    }

    // -----------------------------------------------------------------------
    // Serialize
    // -----------------------------------------------------------------------
    bool AnimatorController2D::Serialize(const ignite::Path &filepath)
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "AnimatorController2D" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "DefaultState" << YAML::Value << defaultState;

        // States
        out << YAML::Key << "States" << YAML::Value << YAML::BeginSeq;
        for (const auto &s : states)
        {
            out << YAML::BeginMap;
            out << YAML::Key << "Name"       << YAML::Value << s.name;
            out << YAML::Key << "AnimHandle" << YAML::Value << static_cast<uint64_t>(s.GetAnimationAssetHandle());
            out << YAML::Key << "EditorPos"  << YAML::Value << YAML::Flow << YAML::BeginSeq << s.editorPos.x << s.editorPos.y << YAML::EndSeq;
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;

        // Params
        out << YAML::Key << "Params" << YAML::Value << YAML::BeginSeq;
        for (const auto &p : params)
        {
            out << YAML::BeginMap;
            out << YAML::Key << "Name" << YAML::Value << p.name;
            out << YAML::Key << "Type" << YAML::Value << anim_utils::ParamTypeToStr(p.type);
            switch (p.type)
            {
            case AnimParam::Type::Float:  out << YAML::Key << "Value" << YAML::Value << p.floatVal; break;
            case AnimParam::Type::Int:    out << YAML::Key << "Value" << YAML::Value << p.intVal;   break;
            case AnimParam::Type::Bool:   out << YAML::Key << "Value" << YAML::Value << p.boolVal;  break;
            case AnimParam::Type::String: out << YAML::Key << "Value" << YAML::Value << p.strVal;   break;
            default: break;
            }
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;

        // Transitions
        out << YAML::Key << "Transitions" << YAML::Value << YAML::BeginSeq;
        for (const auto &tr : transitions)
        {
            out << YAML::BeginMap;
            out << YAML::Key << "From"        << YAML::Value << tr.fromState;
            out << YAML::Key << "To"          << YAML::Value << tr.toState;
            out << YAML::Key << "HasExitTime" << YAML::Value << tr.hasExitTime;
            out << YAML::Key << "ExitTime"    << YAML::Value << tr.exitTime;

            out << YAML::Key << "Conditions" << YAML::Value << YAML::BeginSeq;
            for (const auto &cond : tr.conditions)
            {
                out << YAML::BeginMap;
                out << YAML::Key << "Param" << YAML::Value << cond.paramName;
                out << YAML::Key << "Op"    << YAML::Value << anim_utils::OpToStr(cond.op);

                // Find param type for threshold serialization
                const AnimParam *param = GetParam(cond.paramName);
                if (param)
                {
                    switch (param->type)
                    {
                    case AnimParam::Type::Float:  out << YAML::Key << "Threshold" << YAML::Value << cond.floatThreshold; break;
                    case AnimParam::Type::Int:    out << YAML::Key << "Threshold" << YAML::Value << cond.intThreshold;   break;
                    case AnimParam::Type::Bool:   out << YAML::Key << "Threshold" << YAML::Value << cond.boolThreshold;  break;
                    case AnimParam::Type::String: out << YAML::Key << "Threshold" << YAML::Value << cond.strThreshold;   break;
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

        out << YAML::EndMap; // AnimatorController2D
        out << YAML::EndMap; // root

        std::ofstream file(filepath);
        if (!file.is_open())
        {
            LOG_ERROR("[AnimCtrl2D] Failed to open file for writing: {}", filepath.string());
            return false;
        }

        file << out.c_str();
        SetDirtyFlag(false);
        LOG_INFO("[AnimCtrl2D] Serialized to {}", filepath.string());
        return true;
    }

    // -----------------------------------------------------------------------
    // Deserialize
    // -----------------------------------------------------------------------
    Ref<AnimatorController2D> AnimatorController2D::Deserialize(const ignite::Path &filepath)
    {
        if (!ignite::Path::exists(filepath))
        {
            LOG_ERROR("[AnimCtrl2D] File does not exist: {}", filepath.string());
            return nullptr;
        }

        YAML::Node root;
        try
        {
            root = YAML::LoadFile(filepath.string());
        }
        catch (const YAML::Exception &e)
        {
            LOG_ERROR("[AnimCtrl2D] YAML parse error: {}", e.what());
            return nullptr;
        }

        YAML::Node node = root["AnimatorController2D"];
        if (!node)
            return nullptr;

        auto ctrl = CreateRef<AnimatorController2D>();

        if (auto n = node["DefaultState"]) ctrl->defaultState = n.as<std::string>();

        // States
        if (YAML::Node statesNode = node["States"]; statesNode && statesNode.IsSequence())
        {
            for (const auto &sn : statesNode)
            {
                AnimState2D &s = ctrl->states.emplace_back();
                if (auto n = sn["Name"])       s.name       = n.as<std::string>();
                if (auto n = sn["AnimHandle"]) s.SetAnimationHandle(AssetHandle(n.as<uint64_t>()));
                if (auto n = sn["EditorPos"]; n && n.IsSequence() && n.size() == 2)
                    s.editorPos = { n[0].as<float>(), n[1].as<float>() };
            }
        }

        // Params
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
                    case AnimParam::Type::Float:  p.floatVal = n.as<float>();       break;
                    case AnimParam::Type::Int:    p.intVal   = n.as<int>();         break;
                    case AnimParam::Type::Bool:   p.boolVal  = n.as<bool>();        break;
                    case AnimParam::Type::String: p.strVal   = n.as<std::string>(); break;
                    default: break;
                    }
                }
                ctrl->params.push_back(p);
            }
        }

        // Transitions
        if (YAML::Node transNode = node["Transitions"]; transNode && transNode.IsSequence())
        {
            for (const auto &tn : transNode)
            {
                AnimTransition tr;
                if (auto n = tn["From"])        tr.fromState    = n.as<std::string>();
                if (auto n = tn["To"])          tr.toState      = n.as<std::string>();
                if (auto n = tn["HasExitTime"]) tr.hasExitTime  = n.as<bool>();
                if (auto n = tn["ExitTime"])    tr.exitTime     = n.as<float>();

                if (YAML::Node condsNode = tn["Conditions"]; condsNode && condsNode.IsSequence())
                {
                    for (const auto &cn : condsNode)
                    {
                        AnimCondition cond;
                        if (auto n = cn["Param"]) cond.paramName = n.as<std::string>();
                        if (auto n = cn["Op"])    cond.op        = anim_utils::StrToOp(n.as<std::string>());

                        // determine param type from the controller params list
                        const AnimParam *param = ctrl->GetParam(cond.paramName);
                        if (auto n = cn["Threshold"])
                        {
                            if (param)
                            {
                                switch (param->type)
                                {
                                case AnimParam::Type::Float:  cond.floatThreshold = n.as<float>();       break;
                                case AnimParam::Type::Int:    cond.intThreshold   = n.as<int>();         break;
                                case AnimParam::Type::Bool:   cond.boolThreshold  = n.as<bool>();        break;
                                case AnimParam::Type::String: cond.strThreshold   = n.as<std::string>(); break;
                                default: break;
                                }
                            }
                            else
                            {
                                // fallback: try float
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

    Ref<AnimatorController2D> AnimatorController2D::Create()
    {
        auto ctrl = CreateRef<AnimatorController2D>();
        ctrl->SetReadyFlag(true);
        return ctrl;
    }
}
