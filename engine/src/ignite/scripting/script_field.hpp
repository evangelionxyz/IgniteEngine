// Copyright (c) 2026 Evangelion Manuhutu

#ifndef SCRIPT_FIELD_HPP
#define SCRIPT_FIELD_HPP

#include <string>
#include <vector>

namespace ignite
{
    enum class ScriptFieldType
    {
        Invalid = 0,
        Entity,
        Bool, 
        Char,
        String,
        Byte, 
        SByte,
        Short, 
        UShort, 
        Int, 
        UInt,
        Long, 
        ULong,
        Float, 
        Double,
        Vector2, 
        Vector3, 
        Vector4,
        Quat,
        Color,
        Enum,
        Asset,   // AssetHandle - uint64_t; covers ScriptableObject, Texture, etc.

        // Generic List<T> variants
        List_Bool,
        List_Char,
        List_String,
        List_Byte,
        List_SByte,
        List_Short,
        List_UShort,
        List_Int,
        List_UInt,
        List_Long,
        List_ULong,
        List_Float,
        List_Double,
        List_Vector2,
        List_Vector3,
        List_Vector4,
        List_Quat,
        List_Color,
        List_Entity,
        List_Asset,
    };

    struct ScriptField
    {
        ScriptFieldType Type = ScriptFieldType::Invalid;
        std::string Name;
        std::string ManagedTypeName;
        std::string ListElementTypeName; // Populated when Type is a List_* variant
        bool IsPublic = false;
        bool HasSerializeFieldAttribute = false;

        bool IsEnum = false;
        std::vector<std::string> EnumNames;
        std::vector<int> EnumValues;

        bool IsList() const
        {
            return Type >= ScriptFieldType::List_Bool && Type <= ScriptFieldType::List_Asset;
        }
    };

    struct ScriptInstanceField
    {
        ScriptField field;

        ScriptInstanceField()
        {
            memset(m_Buffer, 0, sizeof(m_Buffer));
        }

        template<typename T>
        T GetValue() const
        {
            static_assert(sizeof(T) <= 64, "Type too large!");
            return *reinterpret_cast<const T *>(m_Buffer);
        }

        template<typename T>
        void SetValue(T value)
        {
            static_assert(sizeof(T) <= 64, "Type too large!");
            memcpy(m_Buffer, &value, sizeof(T));
        }

        // Specialization for std::string stored in the fixed buffer (truncated to fit)
        template<>
        inline std::string GetValue<std::string>() const
        {
            // Ensure null-terminated
            const size_t maxLen = sizeof(m_Buffer);
            size_t len = 0;
            while (len < maxLen && m_Buffer[len] != '\0') ++len;
            return std::string(m_Buffer, static_cast<size_t>(len));
        }

        template<>
        inline void SetValue<std::string>(std::string value)
        {
            // Truncate to fit into buffer (reserve one byte for null)
            size_t maxCopy = sizeof(m_Buffer) - 1;
            size_t copyLen = std::min(value.size(), maxCopy);
            memset(m_Buffer, 0, sizeof(m_Buffer));
            if (copyLen > 0)
                memcpy(m_Buffer, value.data(), copyLen);
            m_Buffer[copyLen] = '\0';
        }

    private:
        char m_Buffer[64];

        friend class ScriptEngine;
        friend class ScriptInstance;
    };
}

#endif
