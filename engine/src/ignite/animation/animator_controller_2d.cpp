// Copyright (c) 2026 Evangelion Manuhutu

#include "animator_controller_2d.hpp"
#include "ignite/core/logger.hpp"

#pragma warning(push)
#pragma warning(disable : 4275 4251)
#include <yaml-cpp/yaml.h>
#pragma warning(pop)

#include <fstream>
#include <algorithm>

namespace ignite
{
    // -----------------------------------------------------------------------
    // String helpers
    // -----------------------------------------------------------------------
    namespace
    {
        static const char *ParamTypeToStr(AnimParam2D::Type t)
        {
            switch (t)
            {
            case AnimParam2D::Type::Float:  return "Float";
            case AnimParam2D::Type::Int:    return "Int";
            case AnimParam2D::Type::Bool:   return "Bool";
            case AnimParam2D::Type::String: return "String";
            default:                        return "Float";
            }
        }

        static AnimParam2D::Type StrToParamType(const std::string &s)
        {
            if (s == "Int")    return AnimParam2D::Type::Int;
            if (s == "Bool")   return AnimParam2D::Type::Bool;
            if (s == "String") return AnimParam2D::Type::String;
            return AnimParam2D::Type::Float;
        }

        static const char *OpToStr(AnimCondition2D::Op op)
        {
            switch (op)
            {
            case AnimCondition2D::Op::Equals:    return "Equals";
            case AnimCondition2D::Op::NotEquals: return "NotEquals";
            case AnimCondition2D::Op::Greater:   return "Greater";
            case AnimCondition2D::Op::Less:      return "Less";
            case AnimCondition2D::Op::GreaterEq: return "GreaterEq";
            case AnimCondition2D::Op::LessEq:    return "LessEq";
            default:                             return "Equals";
            }
        }

        static AnimCondition2D::Op StrToOp(const std::string &s)
        {
            if (s == "NotEquals") return AnimCondition2D::Op::NotEquals;
            if (s == "Greater")   return AnimCondition2D::Op::Greater;
            if (s == "Less")      return AnimCondition2D::Op::Less;
            if (s == "GreaterEq") return AnimCondition2D::Op::GreaterEq;
            if (s == "LessEq")   return AnimCondition2D::Op::LessEq;
            return AnimCondition2D::Op::Equals;
        }

        static bool EvalCondition(const AnimCondition2D &cond, const AnimParam2D *param)
        {
            if (!param)
                return false;

            const auto op = cond.op;

            switch (param->type)
            {
            case AnimParam2D::Type::Float:
            {
                const float v = param->floatVal;
                const float t = cond.floatThreshold;
                switch (op)
                {
                case AnimCondition2D::Op::Equals:    return v == t;
                case AnimCondition2D::Op::NotEquals: return v != t;
                case AnimCondition2D::Op::Greater:   return v >  t;
                case AnimCondition2D::Op::Less:      return v <  t;
                case AnimCondition2D::Op::GreaterEq: return v >= t;
                case AnimCondition2D::Op::LessEq:    return v <= t;
                default: return false;
                }
            }
            case AnimParam2D::Type::Int:
            {
                const int v = param->intVal;
                const int t = cond.intThreshold;
                switch (op)
                {
                case AnimCondition2D::Op::Equals:    return v == t;
                case AnimCondition2D::Op::NotEquals: return v != t;
                case AnimCondition2D::Op::Greater:   return v >  t;
                case AnimCondition2D::Op::Less:      return v <  t;
                case AnimCondition2D::Op::GreaterEq: return v >= t;
                case AnimCondition2D::Op::LessEq:    return v <= t;
                default: return false;
                }
            }
            case AnimParam2D::Type::Bool:
                return (op == AnimCondition2D::Op::Equals)    ? (param->boolVal == cond.boolThreshold)
                     : (op == AnimCondition2D::Op::NotEquals) ? (param->boolVal != cond.boolThreshold)
                     : false;

            case AnimParam2D::Type::String:
                return (op == AnimCondition2D::Op::Equals)    ? (param->strVal == cond.strThreshold)
                     : (op == AnimCondition2D::Op::NotEquals) ? (param->strVal != cond.strThreshold)
                     : false;

            default:
                return false;
            }
        }
    }

    // -----------------------------------------------------------------------
    // Param setters
    // -----------------------------------------------------------------------
    void AnimatorController2D::SetParamFloat(const std::string &name, float v)
    {
        if (auto *p = GetParam(name)) p->floatVal = v;
    }

    void AnimatorController2D::SetParamInt(const std::string &name, int v)
    {
        if (auto *p = GetParam(name)) p->intVal = v;
    }

    void AnimatorController2D::SetParamBool(const std::string &name, bool v)
    {
        if (auto *p = GetParam(name)) p->boolVal = v;
    }

    void AnimatorController2D::SetParamString(const std::string &name, const std::string &v)
    {
        if (auto *p = GetParam(name)) p->strVal = v;
    }

    AnimParam2D *AnimatorController2D::GetParam(const std::string &name)
    {
        auto it = std::find_if(params.begin(), params.end(), [&name](const AnimParam2D &p) { return p.name == name; });
        return it != params.end() ? &(*it) : nullptr;
    }

    const AnimParam2D *AnimatorController2D::GetParam(const std::string &name) const
    {
        auto it = std::find_if(params.begin(), params.end(), [&name](const AnimParam2D &p) { return p.name == name; });
        return it != params.end() ? &(*it) : nullptr;
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
    // EvaluateTransitions
    // -----------------------------------------------------------------------
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
                const AnimParam2D *param = GetParam(cond.paramName);
                if (!EvalCondition(cond, param))
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

    // -----------------------------------------------------------------------
    // Serialize
    // -----------------------------------------------------------------------
    bool AnimatorController2D::Serialize(const std::filesystem::path &filepath)
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
            out << YAML::Key << "AnimHandle" << YAML::Value << static_cast<uint64_t>(s.animHandle);
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
            out << YAML::Key << "Type" << YAML::Value << ParamTypeToStr(p.type);
            switch (p.type)
            {
            case AnimParam2D::Type::Float:  out << YAML::Key << "Value" << YAML::Value << p.floatVal; break;
            case AnimParam2D::Type::Int:    out << YAML::Key << "Value" << YAML::Value << p.intVal;   break;
            case AnimParam2D::Type::Bool:   out << YAML::Key << "Value" << YAML::Value << p.boolVal;  break;
            case AnimParam2D::Type::String: out << YAML::Key << "Value" << YAML::Value << p.strVal;   break;
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
                out << YAML::Key << "Op"    << YAML::Value << OpToStr(cond.op);

                // Find param type for threshold serialization
                const AnimParam2D *param = GetParam(cond.paramName);
                if (param)
                {
                    switch (param->type)
                    {
                    case AnimParam2D::Type::Float:  out << YAML::Key << "Threshold" << YAML::Value << cond.floatThreshold; break;
                    case AnimParam2D::Type::Int:    out << YAML::Key << "Threshold" << YAML::Value << cond.intThreshold;   break;
                    case AnimParam2D::Type::Bool:   out << YAML::Key << "Threshold" << YAML::Value << cond.boolThreshold;  break;
                    case AnimParam2D::Type::String: out << YAML::Key << "Threshold" << YAML::Value << cond.strThreshold;   break;
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
    Ref<AnimatorController2D> AnimatorController2D::Deserialize(const std::filesystem::path &filepath)
    {
        if (!std::filesystem::exists(filepath))
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
                AnimState2D s;
                if (auto n = sn["Name"])       s.name       = n.as<std::string>();
                if (auto n = sn["AnimHandle"]) s.animHandle = AssetHandle(n.as<uint64_t>());
                if (auto n = sn["EditorPos"]; n && n.IsSequence() && n.size() == 2)
                    s.editorPos = { n[0].as<float>(), n[1].as<float>() };
                ctrl->states.push_back(s);
            }
        }

        // Params
        if (YAML::Node paramsNode = node["Params"]; paramsNode && paramsNode.IsSequence())
        {
            for (const auto &pn : paramsNode)
            {
                AnimParam2D p;
                if (auto n = pn["Name"]) p.name = n.as<std::string>();
                if (auto n = pn["Type"]) p.type = StrToParamType(n.as<std::string>());
                if (auto n = pn["Value"])
                {
                    switch (p.type)
                    {
                    case AnimParam2D::Type::Float:  p.floatVal = n.as<float>();       break;
                    case AnimParam2D::Type::Int:    p.intVal   = n.as<int>();         break;
                    case AnimParam2D::Type::Bool:   p.boolVal  = n.as<bool>();        break;
                    case AnimParam2D::Type::String: p.strVal   = n.as<std::string>(); break;
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
                AnimTransition2D tr;
                if (auto n = tn["From"])        tr.fromState    = n.as<std::string>();
                if (auto n = tn["To"])          tr.toState      = n.as<std::string>();
                if (auto n = tn["HasExitTime"]) tr.hasExitTime  = n.as<bool>();
                if (auto n = tn["ExitTime"])    tr.exitTime     = n.as<float>();

                if (YAML::Node condsNode = tn["Conditions"]; condsNode && condsNode.IsSequence())
                {
                    for (const auto &cn : condsNode)
                    {
                        AnimCondition2D cond;
                        if (auto n = cn["Param"]) cond.paramName = n.as<std::string>();
                        if (auto n = cn["Op"])    cond.op        = StrToOp(n.as<std::string>());

                        // determine param type from the controller params list
                        const AnimParam2D *param = ctrl->GetParam(cond.paramName);
                        if (auto n = cn["Threshold"])
                        {
                            if (param)
                            {
                                switch (param->type)
                                {
                                case AnimParam2D::Type::Float:  cond.floatThreshold = n.as<float>();       break;
                                case AnimParam2D::Type::Int:    cond.intThreshold   = n.as<int>();         break;
                                case AnimParam2D::Type::Bool:   cond.boolThreshold  = n.as<bool>();        break;
                                case AnimParam2D::Type::String: cond.strThreshold   = n.as<std::string>(); break;
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
