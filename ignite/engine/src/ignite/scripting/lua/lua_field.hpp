// Copyright (c) 2026 Evangelion Manuhutu
#pragma once

#include "ignite/scripting/script_field.hpp"

#include <glm/glm.hpp>
#include <algorithm>
#include <cfloat>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace ignite
{
    using LuaFieldValue = std::variant<
        std::monostate,
        bool,
        int32_t,
        float,
        std::string,
        glm::vec2,
        glm::vec3,
        glm::vec4,
        uint64_t>;

    struct LuaFieldDesc
    {
        std::string name;
        ScriptFieldType type = ScriptFieldType::Invalid;
        LuaFieldValue defaultValue;

        FieldUIType uiType = FieldUIType::Default;
        float minValue = -FLT_MAX;
        float maxValue = FLT_MAX;
        float speed = 0.05f;
        bool hasMin = false;
        bool hasMax = false;

        std::vector<std::string> enumNames;
        std::string assetTypeFilter;
        std::string tooltip;
        uint32_t order = 0;
    };

    struct LuaSerializedField
    {
        ScriptFieldType type = ScriptFieldType::Invalid;
        LuaFieldValue value;
    };

    inline bool IsLuaFieldValueCompatible(ScriptFieldType type, const LuaFieldValue &value)
    {
        switch (type)
        {
            case ScriptFieldType::Bool:    return std::holds_alternative<bool>(value);
            case ScriptFieldType::Int:     return std::holds_alternative<int32_t>(value);
            case ScriptFieldType::Float:   return std::holds_alternative<float>(value);
            case ScriptFieldType::String:  return std::holds_alternative<std::string>(value);
            case ScriptFieldType::Vector2: return std::holds_alternative<glm::vec2>(value);
            case ScriptFieldType::Vector3: return std::holds_alternative<glm::vec3>(value);
            case ScriptFieldType::Vector4:
            case ScriptFieldType::Color:   return std::holds_alternative<glm::vec4>(value);
            case ScriptFieldType::Entity:
            case ScriptFieldType::Asset:   return std::holds_alternative<uint64_t>(value);
            case ScriptFieldType::Enum:    return std::holds_alternative<std::string>(value) || std::holds_alternative<int32_t>(value);
            default:                       return false;
        }
    }

    inline LuaFieldValue ClampLuaFieldValue(const LuaFieldDesc &desc, const LuaFieldValue &value)
    {
        if (desc.type == ScriptFieldType::Float && std::holds_alternative<float>(value))
        {
            float v = std::get<float>(value);
            if (desc.hasMin) v = std::max(v, desc.minValue);
            if (desc.hasMax) v = std::min(v, desc.maxValue);
            return v;
        }
        if (desc.type == ScriptFieldType::Int && std::holds_alternative<int32_t>(value))
        {
            int32_t v = std::get<int32_t>(value);
            if (desc.hasMin) v = std::max(v, static_cast<int32_t>(desc.minValue));
            if (desc.hasMax) v = std::min(v, static_cast<int32_t>(desc.maxValue));
            return v;
        }
        return value;
    }
}

