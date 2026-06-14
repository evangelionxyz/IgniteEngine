// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_ANIMATOR_HPP
#define IGN_ANIMATOR_HPP

#include "ignite/core/base.hpp"

#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace ignite
{
    struct TRS
    {
        glm::vec3 translation = glm::vec3(0.0f);
        glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        glm::vec3 scale = glm::vec3(1.0f);
    };

    struct AnimParam
    {
        std::string name;
        std::string strVal;
        float       floatVal = 0.0f;
        int         intVal = 0;
        bool        boolVal = false;

        enum class Type { Float, Bool, Int, String } type = Type::Float;
    };

    struct AnimCondition
    {
        std::string paramName;
        std::string strThreshold;

        // threshold stored per type
        float       floatThreshold = 0.0f;
        int         intThreshold = 0;
        bool        boolThreshold = false;
        enum class Op { Equals, NotEquals, Greater, Less, GreaterEq, LessEq } op = Op::Equals;
    };

    struct AnimTransition
    {
        std::string fromState;   // empty string = "Any State"
        std::string toState;
        std::vector<AnimCondition> conditions;

        bool  hasExitTime = false;
        float exitTime = 1.0f; // normalized [0..1]
    };

    class IGN_API Animator
    {
    public:
        // ----- Runtime param setters -----
        virtual void SetParamFloat(const std::string &name, float v);
        virtual void SetParamInt(const std::string &name, int v);
        virtual void SetParamBool(const std::string &name, bool v);
        virtual void SetParamString(const std::string &name, const std::string &v);

        virtual AnimParam *GetParam(const std::string &name);
        virtual const AnimParam *GetParam(const std::string &name) const;

        std::vector<AnimParam> params;
        std::vector<AnimTransition> transitions;
    };

    // -----------------------------------------------------------------------
    // String helpers
    // -----------------------------------------------------------------------
    namespace anim_utils
    {
        static const char *ParamTypeToStr(AnimParam::Type t)
        {
            switch (t)
            {
                case AnimParam::Type::Float: return "Float";
                case AnimParam::Type::Int: return "Int";
                case AnimParam::Type::Bool: return "Bool";
                case AnimParam::Type::String: return "String";
                default: return "Float";
            }
        }

        static AnimParam::Type StrToParamType(const std::string &s)
        {
            if (s == "Int") return AnimParam::Type::Int;
            if (s == "Bool") return AnimParam::Type::Bool;
            if (s == "String") return AnimParam::Type::String;
            return AnimParam::Type::Float;
        }

        static const char *OpToStr(AnimCondition::Op op)
        {
            switch (op)
            {
                case AnimCondition::Op::Equals: return "Equals";
                case AnimCondition::Op::NotEquals: return "NotEquals";
                case AnimCondition::Op::Greater: return "Greater";
                case AnimCondition::Op::Less: return "Less";
                case AnimCondition::Op::GreaterEq: return "GreaterEq";
                case AnimCondition::Op::LessEq: return "LessEq";
                default: return "Equals";
            }
        }

        static AnimCondition::Op StrToOp(const std::string &s)
        {
            if (s == "NotEquals") return AnimCondition::Op::NotEquals;
            if (s == "Greater") return AnimCondition::Op::Greater;
            if (s == "Less") return AnimCondition::Op::Less;
            if (s == "GreaterEq") return AnimCondition::Op::GreaterEq;
            if (s == "LessEq") return AnimCondition::Op::LessEq;
            return AnimCondition::Op::Equals;
        }

        static bool EvalCondition(const AnimCondition &cond, const AnimParam *param)
        {
            if (!param)
                return false;

            const auto op = cond.op;
            switch (param->type)
            {
                case AnimParam::Type::Float:
                {
                    const float v = param->floatVal;
                    const float t = cond.floatThreshold;
                    switch (op)
                    {
                        case AnimCondition::Op::Equals: return v == t;
                        case AnimCondition::Op::NotEquals: return v != t;
                        case AnimCondition::Op::Greater: return v > t;
                        case AnimCondition::Op::Less: return v < t;
                        case AnimCondition::Op::GreaterEq: return v >= t;
                        case AnimCondition::Op::LessEq: return v <= t;
                        default: return false;
                    }
                }
                case AnimParam::Type::Int:
                {
                    const int v = param->intVal;
                    const int t = cond.intThreshold;
                    switch (op)
                    {
                        case AnimCondition::Op::Equals: return v == t;
                        case AnimCondition::Op::NotEquals: return v != t;
                        case AnimCondition::Op::Greater: return v > t;
                        case AnimCondition::Op::Less: return v < t;
                        case AnimCondition::Op::GreaterEq: return v >= t;
                        case AnimCondition::Op::LessEq: return v <= t;
                        default: return false;
                    }
                }
                case AnimParam::Type::Bool:
                return (op == AnimCondition::Op::Equals) ? (param->boolVal == cond.boolThreshold)
                    : (op == AnimCondition::Op::NotEquals) ? (param->boolVal != cond.boolThreshold)
                    : false;

                case AnimParam::Type::String:
                return (op == AnimCondition::Op::Equals) ? (param->strVal == cond.strThreshold)
                    : (op == AnimCondition::Op::NotEquals) ? (param->strVal != cond.strThreshold)
                    : false;

                default:
                return false;
            }
        }
    }
}

#endif