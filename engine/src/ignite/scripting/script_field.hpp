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
		String,
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
		char m_Buffer[16];

		friend class ScriptEngine;
		friend class ScriptInstance;
	};
}

#endif
