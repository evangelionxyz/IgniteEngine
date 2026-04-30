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

    using ScriptInstanceID = uint64_t;

    class ScriptInstance
    {
    public:
        ScriptInstance(Ref<ScriptClass> scriptClass, ScriptInstanceID instanceID);

        void InvokeOnCreate();
        void InvokeOnDestroy();
        void InvokeOnUpdate(float time);

        const Ref<ScriptClass> &GetScriptClass() const { return m_ScriptClass; }
        ScriptInstanceID GetInstanceID() const { return m_InstanceId; }

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

        template<>
        inline std::string GetFieldValue<std::string>(const std::string &fieldName)
        {
            if (!m_ScriptHost || fieldName.empty() || s_FieldValueBuffer == nullptr)
            {
                return {};
            }

            const bool success = m_ScriptHost->GetInstanceFieldValue(m_InstanceId, fieldName, s_FieldValueBuffer, sizeof(s_FieldValueBuffer));
            if (!success)
            {
                return {};
            }

            m_ScriptClass->GetInstanceFieldsById(m_InstanceId)->at(fieldName).SetValue<std::string>(std::string(s_FieldValueBuffer));
            return std::string(s_FieldValueBuffer);
        }

        template<>
        inline bool SetFieldValue<std::string>(const std::string &fieldName, const std::string &value)
        {
            if (!m_ScriptHost || fieldName.empty())
            {
                return false;
            }

            memset(s_FieldValueBuffer, 0, sizeof(s_FieldValueBuffer));
            size_t copyLen = std::min(value.size(), sizeof(s_FieldValueBuffer) - 1);
            if (copyLen > 0)
                memcpy(s_FieldValueBuffer, value.data(), copyLen);
            s_FieldValueBuffer[copyLen] = '\0';

            const bool success = m_ScriptHost->SetInstanceFieldValue(m_InstanceId, fieldName, s_FieldValueBuffer, (int)copyLen);
            if (success)
            {
                m_ScriptClass->GetInstanceFieldsById(m_InstanceId)->at(fieldName).SetValue<std::string>(std::string(s_FieldValueBuffer));
            }
            return success;
        }

    private:
        Ref<ScriptClass> m_ScriptClass;

        ScriptHost *m_ScriptHost = nullptr;

        ScriptInstanceID m_InstanceId = 0;
        int m_OnCreateMethodId = 0;
        int m_OnDestroyMethodId = 0;
        int m_OnUpdateMethodId = 0;

        inline static char s_FieldValueBuffer[24];
        friend class ScriptEngine;
    };
}

#endif