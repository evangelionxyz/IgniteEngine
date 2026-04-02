// Copyright (c) 2026 Evangelion Manuhutu

#ifndef ANIMATOR_CONTROLLER_2D_HPP
#define ANIMATOR_CONTROLLER_2D_HPP

#include "ignite/asset/asset.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <filesystem>

namespace ignite
{
    // -----------------------------------------------------------------------
    // Parameter: named value used in transition conditions
    // -----------------------------------------------------------------------
    struct AnimParam2D
    {
        enum class Type { Float, Bool, Int, String } type = Type::Float;
        std::string name;
        float       floatVal  = 0.0f;
        int         intVal    = 0;
        bool        boolVal   = false;
        std::string strVal;
    };

    // -----------------------------------------------------------------------
    // Transition condition: param op threshold
    // -----------------------------------------------------------------------
    struct AnimCondition2D
    {
        enum class Op { Equals, NotEquals, Greater, Less, GreaterEq, LessEq } op = Op::Equals;

        std::string paramName;

        // threshold stored per type
        float       floatThreshold  = 0.0f;
        int         intThreshold    = 0;
        bool        boolThreshold   = false;
        std::string strThreshold;
    };

    // -----------------------------------------------------------------------
    // Transition: from one state to another when all conditions pass
    // -----------------------------------------------------------------------
    struct AnimTransition2D
    {
        std::string fromState;   // empty string = "Any State"
        std::string toState;
        std::vector<AnimCondition2D> conditions;

        bool  hasExitTime = false;
        float exitTime    = 1.0f; // normalized [0..1]
    };

    // -----------------------------------------------------------------------
    // State: a named animation clip slot + editor position
    // -----------------------------------------------------------------------
    struct AnimState2D
    {
        std::string name;
        AssetHandle animHandle = AssetHandle(0);
        glm::vec2   editorPos  = glm::vec2(100.0f, 100.0f);
    };

    // -----------------------------------------------------------------------
    // AnimatorController2D: the state-machine asset (.ac2d)
    // -----------------------------------------------------------------------
    class AnimatorController2D : public Asset
    {
    public:
        std::string defaultState;
        std::vector<AnimState2D>      states;
        std::vector<AnimParam2D>      params;
        std::vector<AnimTransition2D> transitions;

        // ----- Runtime param setters -----
        void SetParamFloat (const std::string &name, float v);
        void SetParamInt   (const std::string &name, int v);
        void SetParamBool  (const std::string &name, bool v);
        void SetParamString(const std::string &name, const std::string &v);

        AnimParam2D       *GetParam(const std::string &name);
        const AnimParam2D *GetParam(const std::string &name) const;

        // Returns new state name if a transition fires, else empty string.
        std::string EvaluateTransitions(const std::string &currentState, float normalizedTime) const;

        // Convenience accessors
        AnimState2D       *FindState(const std::string &name);
        const AnimState2D *FindState(const std::string &name) const;

        virtual bool Serialize(const std::filesystem::path &filepath) override;
        static Ref<AnimatorController2D> Deserialize(const std::filesystem::path &filepath);
        static Ref<AnimatorController2D> Create();

        virtual AssetType GetAssetType() override { return AssetType::AnimatorController2D; }
    };
}

#endif // ANIMATOR_CONTROLLER_2D_HPP
