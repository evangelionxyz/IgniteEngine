// Copyright (c) 2026 Evangelion Manuhutu

#ifndef SCRIPT_FIELD_HPP
#define SCRIPT_FIELD_HPP

#include <string>

namespace ignite
{
    enum class ScriptFieldType
    {
        Invalid = 0,
        Entity,
        Float, 
        Double,
        Bool, 
        Char,
        Byte, 
        Short, 
        Int, 
        Long,
        UByte, 
        UShort, 
        UInt, 
        ULong,
        Vector2, 
        Vector3, 
        Vector4
    };

	struct ScriptField
	{
		ScriptFieldType Type = ScriptFieldType::Invalid;
		std::string Name;
		std::string ManagedTypeName;
		bool IsPublic = false;
		bool HasSerializeFieldAttribute = false;
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
			static_assert(sizeof(T) <= 16, "Type too large!");
			return *reinterpret_cast<const T *>(m_Buffer);
		}

		template<typename T>
		void SetValue(T value)
		{
			static_assert(sizeof(T) <= 16, "Type too large!");
			memcpy(m_Buffer, &value, sizeof(T));
		}

	private:
		char m_Buffer[16];

		friend class ScriptEngine;
		friend class ScriptInstance;
	};
}

#endif
