// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_SCRIPT_INSTANCE_HPP
#define IGN_SCRIPT_INSTANCE_HPP

#include "ignite/scripting/script_field.hpp"
#include "ignite/scripting/script_class.hpp"
#include "ignite/scripting/script_host.hpp"

#include "ignite/core/base.hpp"
#include "ignite/core/types.hpp"
#include "ignite/core/logger.hpp"

namespace ignite
{
    class Entity;
    class ScriptClass;
    class ScriptableObject;

    using ScriptInstanceID = uint64_t;

    class IGN_API ScriptInstance
    {
    public:
        ScriptInstance(Ref<ScriptClass> scriptClass, ScriptInstanceID instanceId);

        static void PopulateSOFields(ScriptHost *host, uint64_t instanceId, const ScriptableObject &so);

        void InvokeOnCreate();
        void InvokeOnDestroy();
        void InvokeOnUpdate(float time);
        void InvokeOnFixedUpdate();
        void InvokeOnHotReload();

        // Collision lifecycle callbacks
        void InvokeOnCollisionEnter(uint64_t otherEntityId);
        void InvokeOnCollisionStay(uint64_t otherEntityId);
        void InvokeOnCollisionExit(uint64_t otherEntityId);

        void InvokeOnBodyActivated();
        void InvokeOnBodyDeactivated();

        const Ref<ScriptClass> &GetScriptClass() const { return m_ScriptClass; }
        ScriptInstanceID GetInstanceId() const { return m_InstanceId; }

        template<typename T>
        T GetFieldValue(const std::string &fieldName)
        {
            static_assert(sizeof(T) <= 64, "Type too large!");

            if (!m_ScriptHost || fieldName.empty() || s_FieldValueBuffer == nullptr)
            {
                return T();
            }

            const bool success = m_ScriptHost->GetInstanceFieldValue(m_InstanceId, fieldName, s_FieldValueBuffer, sizeof(s_FieldValueBuffer));
            LOG_ASSERT(success, "Invalid field data");
            if (!success)
                return T();

            m_ScriptClass->GetInstanceFieldsById(m_InstanceId)->at(fieldName).SetValue(*(T *)s_FieldValueBuffer);
            return *(T *)s_FieldValueBuffer;
        }

        template<typename T>
        bool SetFieldValue(const std::string &fieldName, const T &value)
        {
            static_assert(sizeof(T) <= 64, "Type too large!");

            if (!m_ScriptHost || fieldName.empty())
            {
                return false;
            }

            const bool success = m_ScriptHost->SetInstanceFieldValue(m_InstanceId, fieldName, &value, (int)sizeof(T));
            LOG_ASSERT(success, "Invalid field data");
            if (success)
                m_ScriptClass->GetInstanceFieldsById(m_InstanceId)->at(fieldName).SetValue(value);
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
            LOG_ASSERT(success, "Invalid field data");
            if (!success)
                return {};

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
            const size_t copyLen = std::min(value.size(), sizeof(s_FieldValueBuffer) - 1);
            if (copyLen > 0)
                memcpy(s_FieldValueBuffer, value.data(), copyLen);
            s_FieldValueBuffer[copyLen] = '\0';

            const bool success = m_ScriptHost->SetInstanceFieldValue(m_InstanceId, fieldName, s_FieldValueBuffer, (int)copyLen);
            LOG_ASSERT(success, "Invalid field data");
            if (success)
                m_ScriptClass->GetInstanceFieldsById(m_InstanceId)->at(fieldName).SetValue<std::string>(std::string(s_FieldValueBuffer));
            return success;
        }

    protected:
        Ref<ScriptClass> m_ScriptClass;

        ScriptHost *m_ScriptHost = nullptr;

        ScriptInstanceID m_InstanceId = 0;

        int m_OnCreateMethodId = 0;
        int m_OnDestroyMethodId = 0;
        int m_OnUpdateMethodId = 0;
        int m_OnFixedUpdateMethodId = 0;
        int m_OnHotReloadMethodId = 0;

        int m_OnCollisionEnterMethodId = 0;
        int m_OnCollisionStayMethodId  = 0;
        int m_OnCollisionExitMethodId  = 0;

        int m_OnBodyActivatedMethodId = 0;
        int m_OnBodyDeactivatedMethodId = 0;

        inline static char s_FieldValueBuffer[64];
        friend class ScriptEngine;
    };
}

#endif
