// Copyright (c) 2026 Evangelion Manuhutu

#ifndef SCRIPT_INSTANCE_HPP
#define SCRIPT_INSTANCE_HPP

#include "script_field.hpp"
#include "script_class.hpp"
#include "script_host.hpp"

#include "ignite/core/types.hpp"

namespace ignite
{
    class Entity;
    class ScriptClass;

    class ScriptInstance
    {
    public:
        ScriptInstance(Ref<ScriptClass> scriptClass, Entity entity);

        void InvokeOnCreate();
        void InvokeOnDestroy();
        void InvokeOnUpdate(float time);

        const Ref<ScriptClass> &GetScriptClass() const { return m_ScriptClass; }
        uint64_t GetInstanceID() const { return m_InstanceId; }

        template<typename T>
		T GetFieldValue(const std::string &fieldName)
        {
            static_assert(sizeof(T) <= 24, "Type too large!");

			if (!m_ScriptHost || fieldName.empty() || s_FieldValueBuffer == nullptr)
			{
                return T();
			}

            const bool success = m_ScriptHost->GetInstanceFieldValue(m_InstanceId, fieldName, s_FieldValueBuffer, sizeof(s_FieldValueBuffer));
            if (!success)
            {
                return T();
            }

            m_ScriptClass->GetInstanceFieldsById(m_InstanceId)->at(fieldName).SetValue(s_FieldValueBuffer);
            return *(T *)s_FieldValueBuffer;
        }

        template<typename T>
        bool SetFieldValue(const std::string &fieldName, const T &value)
        {
            static_assert(sizeof(T) <= 24, "Type too large!");

			if (!m_ScriptHost || fieldName.empty() || &value == nullptr)
			{
				return false;
			}

			const bool success = m_ScriptHost->SetInstanceFieldValue(m_InstanceId, fieldName, &value, sizeof(s_FieldValueBuffer));
            if (success)
            {
			    m_ScriptClass->GetInstanceFieldsById(m_InstanceId)->at(fieldName).SetValue(value);
            }
            return success;
        }

    private:
        Ref<ScriptClass> m_ScriptClass;

        ScriptHost *m_ScriptHost = nullptr;

        uint64_t m_InstanceId = 0;
        int m_OnCreateMethodId = 0;
        int m_OnDestroyMethodId = 0;
        int m_OnUpdateMethodId = 0;

        inline static char s_FieldValueBuffer[24];
        friend class ScriptEngine;
    };
}

#endif