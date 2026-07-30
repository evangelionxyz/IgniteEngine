// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_SCRIPT_FIELD_HPP
#define IGN_SCRIPT_FIELD_HPP

#include <string>
#include <vector>
#include <cstring>

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

    static std::string ScriptFieldTypeToString(ScriptFieldType type)
    {
        switch (type)
        {
            default:
			case ScriptFieldType::Invalid: return "Invalid";
            case ScriptFieldType::Entity: return "Entity";
            case ScriptFieldType::Bool: return "Bool";
            case ScriptFieldType::Char: return "Char";
            case ScriptFieldType::String: return "String";
            case ScriptFieldType::Byte: return "Byte";
            case ScriptFieldType::SByte: return "SByte";
            case ScriptFieldType::Short: return "Short";
            case ScriptFieldType::UShort: return "UShort";
            case ScriptFieldType::Int: return "Int";
            case ScriptFieldType::UInt: return "UInt";
            case ScriptFieldType::Long: return "Long";
            case ScriptFieldType::ULong: return "ULong";
            case ScriptFieldType::Float: return "Float";
            case ScriptFieldType::Double: return "Double";
            case ScriptFieldType::Vector2: return "Vector2";
            case ScriptFieldType::Vector3: return "Vector3";
            case ScriptFieldType::Vector4: return "Vector4";
            case ScriptFieldType::Quat: return "Quat";
            case ScriptFieldType::Color: return "Color";
            case ScriptFieldType::Enum: return "Enum";
            case ScriptFieldType::Asset: return "Asset";
            case ScriptFieldType::List_Bool: return "List_Bool";
            case ScriptFieldType::List_Char: return "List_Char";
            case ScriptFieldType::List_String: return "List_String";
            case ScriptFieldType::List_Byte: return "List_Byte";
            case ScriptFieldType::List_SByte: return "List_SByte";
            case ScriptFieldType::List_Short: return "List_Short";
            case ScriptFieldType::List_UShort: return "List_UShort";
            case ScriptFieldType::List_Int: return "List_Int";
            case ScriptFieldType::List_UInt: return "List_UInt";
            case ScriptFieldType::List_Long: return "List_Long";
            case ScriptFieldType::List_ULong: return "List_ULong";
            case ScriptFieldType::List_Float: return "List_Float";
            case ScriptFieldType::List_Double: return "List_Double";
            case ScriptFieldType::List_Vector2: return "List_Vector2";
            case ScriptFieldType::List_Vector3: return "List_Vector3";
            case ScriptFieldType::List_Vector4: return "List_Vector4";
            case ScriptFieldType::List_Quat: return "List_Quat";
            case ScriptFieldType::List_Color: return "List_Color";
            case ScriptFieldType::List_Entity: return "List_Entity";
            case ScriptFieldType::List_Asset: return "List_Asset";
        }
    }

	static ScriptFieldType ScriptFieldTypeFromString(std::string &typeStr)
	{
        if (typeStr == "Entity") return ScriptFieldType::Entity;
        if (typeStr == "Bool") return ScriptFieldType::Bool;
        if (typeStr == "Char") return ScriptFieldType::Char;
        if (typeStr == "String") return ScriptFieldType::String;
        if (typeStr == "Byte") return ScriptFieldType::Byte;
        if (typeStr == "SByte") return ScriptFieldType::SByte;
        if (typeStr == "Short") return ScriptFieldType::Short;
        if (typeStr == "UShort") return ScriptFieldType::UShort;
        if (typeStr == "Int") return ScriptFieldType::Int;
        if (typeStr == "UInt") return ScriptFieldType::UInt;
        if (typeStr == "Long") return ScriptFieldType::Long;
        if (typeStr == "ULong") return ScriptFieldType::ULong;
        if (typeStr == "Float") return ScriptFieldType::Float;
        if (typeStr == "Double") return ScriptFieldType::Double;
        if (typeStr == "Vector2") return ScriptFieldType::Vector2;
        if (typeStr == "Vector3") return ScriptFieldType::Vector3;
        if (typeStr == "Vector4") return ScriptFieldType::Vector4;
        if (typeStr == "Quat") return ScriptFieldType::Quat;
        if (typeStr == "Color") return ScriptFieldType::Color;
        if (typeStr == "Enum") return ScriptFieldType::Enum;
        if (typeStr == "Asset") return ScriptFieldType::Asset;
        if (typeStr == "List_Bool") return ScriptFieldType::List_Bool;
        if (typeStr == "List_Char") return ScriptFieldType::List_Char;
        if (typeStr == "List_String") return ScriptFieldType::List_String;
        if (typeStr == "List_Byte") return ScriptFieldType::List_Byte;
        if (typeStr == "List_SByte") return ScriptFieldType::List_SByte;
        if (typeStr == "List_Short") return ScriptFieldType::List_Short;
        if (typeStr == "List_UShort") return ScriptFieldType::List_UShort;
        if (typeStr == "List_Int") return ScriptFieldType::List_Int;
        if (typeStr == "List_UInt") return ScriptFieldType::List_UInt;
        if (typeStr == "List_Long") return ScriptFieldType::List_Long;
        if (typeStr == "List_ULong") return ScriptFieldType::List_ULong;
        if (typeStr == "List_Float") return ScriptFieldType::List_Float;
        if (typeStr == "List_Double") return ScriptFieldType::List_Double;
        if (typeStr == "List_Vector2") return ScriptFieldType::List_Vector2;
        if (typeStr == "List_Vector3") return ScriptFieldType::List_Vector3;
        if (typeStr == "List_Vector4") return ScriptFieldType::List_Vector4;
        if (typeStr == "List_Quat") return ScriptFieldType::List_Quat;
        if (typeStr == "List_Color") return ScriptFieldType::List_Color;
        if (typeStr == "List_Entity") return ScriptFieldType::List_Entity;
        if (typeStr == "List_Asset") return ScriptFieldType::List_Asset;
        return ScriptFieldType::Invalid;
	}

    struct ScriptField
    {
        std::string Name;
        std::string ManagedTypeName;
        std::string ListElementTypeName; // Populated when Type is a List_* variant
		std::vector<std::string> EnumNames;
        std::vector<int> EnumValues;

		ScriptFieldType Type = ScriptFieldType::Invalid;
		bool IsPublic = false;
		bool HasSerializeFieldAttribute = false;
		bool IsEnum = false;

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

        void SetValueRaw(const void *buffer, size_t size)
        {
            memset(m_Buffer, 0, sizeof(m_Buffer));
            if (buffer && size > 0)
                memcpy(m_Buffer, buffer, std::min(size, sizeof(m_Buffer)));
        }

        const void *GetBufferRaw() const { return m_Buffer; }
        void *GetBufferRaw() { return m_Buffer; }

    private:
        char m_Buffer[64];

        friend class ScriptEngine;
        friend class ScriptInstance;
        friend class ScriptableObject;
    };
}

#endif
