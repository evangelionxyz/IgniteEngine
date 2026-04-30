// Copyright (c) 2026 Evangelion Manuhutu

#include "script_instance.hpp"
#include "script_engine.hpp"
#include "ignite/scene/entity.hpp"
#include "script_class.hpp"
#include "ignite/core/profiler/profiler.hpp"

namespace ignite
{
    ScriptInstance::ScriptInstance(Ref<ScriptClass> scriptClass, ScriptInstanceID instanceID)
        : m_ScriptClass(scriptClass), m_InstanceId(instanceID)
    {
        IGN_PROFILE_FUNCTION();
        m_ScriptHost = ScriptEngine::GetInstance()->GetScriptHost();
        LOG_ASSERT(m_ScriptHost, "[Script Instance] ScriptHost is null");

        // Create managed instance
        if (!m_ScriptHost->CreateInstance(m_InstanceId, scriptClass->GetFullName()))
        {
            LOG_ERROR("[Script Instance] Failed to create managed instance {}", m_InstanceId);
            return;
        }

        // Bind lifecycle methods
        m_OnCreateMethodId = scriptClass->BindInstanceMethod(m_InstanceId, "OnCreate", ScriptMethodSig::Void);
        m_OnDestroyMethodId = scriptClass->BindInstanceMethod(m_InstanceId, "OnDestroy", ScriptMethodSig::Void);
        m_OnUpdateMethodId = scriptClass->BindInstanceMethod(m_InstanceId, "OnUpdate", ScriptMethodSig::Void_Float);

        // Set Entity ID on managed instance if available
        const int setIdMethod = scriptClass->BindInstanceMethod(m_InstanceId, "SetID", ScriptMethodSig::Void_UInt64);
        if (setIdMethod)
        {
            const uint64_t entityId = static_cast<uint64_t>(m_InstanceId);
            void *args[] = { const_cast<uint64_t *>(&entityId) };
            m_ScriptHost->Invoke(setIdMethod, args, 1, nullptr);
        }

        // Load script fields, based on class field name
        auto &classRegisteredInstanceField = scriptClass->GetInstancesFields();
        auto existingFieldsIt = classRegisteredInstanceField.find(m_InstanceId);
        const bool hasDeserializedValues = existingFieldsIt != classRegisteredInstanceField.end();
        auto &instanceFields = classRegisteredInstanceField[m_InstanceId];

        for (auto &[name, field] : scriptClass->GetFields())
        {
            if (!instanceFields.contains(name))
            {
                ScriptInstanceField defaultField;
                defaultField.field = field;
                instanceFields[name] = defaultField;
            }
            else
            {
                instanceFields[name].field = field;
            }

            ScriptInstanceField &instanceField = instanceFields[name];
            switch (field.Type)
            {
            case ScriptFieldType::String:
            {
                if (hasDeserializedValues) SetFieldValue<std::string>(name, instanceField.GetValue<std::string>());
                else instanceField.SetValue(GetFieldValue<std::string>(name));
                break;
            }
            case ScriptFieldType::Float:
            {
                if (hasDeserializedValues) SetFieldValue<float>(name, instanceField.GetValue<float>());
                else instanceField.SetValue(GetFieldValue<float>(name));
                break;
            }
            case ScriptFieldType::Double:
            {
                if (hasDeserializedValues) SetFieldValue<double>(name, instanceField.GetValue<double>());
                else instanceField.SetValue(GetFieldValue<double>(name));
                break;
            }
            case ScriptFieldType::Bool:
            {
                if (hasDeserializedValues) SetFieldValue<bool>(name, instanceField.GetValue<bool>());
                else instanceField.SetValue(GetFieldValue<bool>(name));
                break;
            }
            case ScriptFieldType::Char:
            {
                if (hasDeserializedValues) SetFieldValue<char>(name, instanceField.GetValue<char>());
                else instanceField.SetValue(GetFieldValue<char>(name));
                break;
            }
            case ScriptFieldType::Byte:
            {
                if (hasDeserializedValues) SetFieldValue<int8_t>(name, instanceField.GetValue<int8_t>());
                else instanceField.SetValue(GetFieldValue<int8_t>(name));
                break;
            }
            case ScriptFieldType::Short:
            {
                if (hasDeserializedValues) SetFieldValue<int16_t>(name, instanceField.GetValue<int16_t>());
                else instanceField.SetValue(GetFieldValue<int16_t>(name));
                break;
            }
            case ScriptFieldType::Int:
            {
                if (hasDeserializedValues) SetFieldValue<int>(name, instanceField.GetValue<int>());
                else instanceField.SetValue(GetFieldValue<int>(name));
                break;
            }
            case ScriptFieldType::Long:
            {
                if (hasDeserializedValues) SetFieldValue<int64_t>(name, instanceField.GetValue<int64_t>());
                else instanceField.SetValue(GetFieldValue<int64_t>(name));
                break;
            }
            case ScriptFieldType::UByte:
            {
                if (hasDeserializedValues) SetFieldValue<uint8_t>(name, instanceField.GetValue<uint8_t>());
                else instanceField.SetValue(GetFieldValue<uint8_t>(name));
                break;
            }
            case ScriptFieldType::UShort:
            {
                if (hasDeserializedValues) SetFieldValue<uint16_t>(name, instanceField.GetValue<uint16_t>());
                else instanceField.SetValue(GetFieldValue<uint16_t>(name));
                break;
            }
            case ScriptFieldType::UInt:
            {
                if (hasDeserializedValues) SetFieldValue<uint32_t>(name, instanceField.GetValue<uint32_t>());
                else instanceField.SetValue(GetFieldValue<uint32_t>(name));
                break;
            }
            case ScriptFieldType::ULong:
            {
                if (hasDeserializedValues) SetFieldValue<uint64_t>(name, instanceField.GetValue<uint64_t>());
                else instanceField.SetValue(GetFieldValue<uint64_t>(name));
                break;
            }
            case ScriptFieldType::Vector2:
            {
                if (hasDeserializedValues) SetFieldValue<glm::vec2>(name, instanceField.GetValue<glm::vec2>());
                else instanceField.SetValue(GetFieldValue<glm::vec2>(name));
                break;
            }
            case ScriptFieldType::Vector3:
            {
                if (hasDeserializedValues) SetFieldValue<glm::vec3>(name, instanceField.GetValue<glm::vec3>());
                else instanceField.SetValue(GetFieldValue<glm::vec3>(name));
                break;
            }
            case ScriptFieldType::Vector4:
            {
                if (hasDeserializedValues) SetFieldValue<glm::vec4>(name, instanceField.GetValue<glm::vec4>());
                else instanceField.SetValue(GetFieldValue<glm::vec4>(name));
                break;
            }
            case ScriptFieldType::Entity:
            {
                if (hasDeserializedValues) SetFieldValue<uint64_t>(name, instanceField.GetValue<uint64_t>());
                else instanceField.SetValue(GetFieldValue<uint64_t>(name));
                break;
            }
            default: break;
            }
        }
    }

    void ScriptInstance::InvokeOnCreate()
    {
        IGN_PROFILE_FUNCTION();
        if (m_OnCreateMethodId)
        {
            if (!m_ScriptHost->Invoke(m_OnCreateMethodId, nullptr, 0, nullptr))
            {
                LOG_ERROR("[Script Instance] OnCreate invocation failed (instanceId={}, type={})", m_InstanceId, m_ScriptClass->GetFullName());
                m_OnCreateMethodId = 0;
            }
        }
    }

	void ScriptInstance::InvokeOnDestroy()
	{
        IGN_PROFILE_FUNCTION();
        if (m_OnDestroyMethodId)
        {
            if (!m_ScriptHost->Invoke(m_OnDestroyMethodId, nullptr, 0, nullptr))
            {
                LOG_ERROR("[Script Instance] OnDestroy invocation failed (instanceId={}, type={})", m_InstanceId, m_ScriptClass->GetFullName());
                m_OnDestroyMethodId = 0;
            }
        }
	}

	void ScriptInstance::InvokeOnUpdate(float time)
    {
        IGN_PROFILE_FUNCTION();
        if (m_OnUpdateMethodId)
        {
            void *args[] = { &time };
            if (!m_ScriptHost->Invoke(m_OnUpdateMethodId, args, 1, nullptr))
            {
                LOG_ERROR("[Script Instance] OnUpdate invocation failed (instanceId={}, type={})", m_InstanceId, m_ScriptClass->GetFullName());
                m_OnUpdateMethodId = 0;
            }
        }
    }
}
