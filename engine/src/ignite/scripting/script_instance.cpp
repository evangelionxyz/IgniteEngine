// Copyright (c) 2026 Evangelion Manuhutu

#include "script_instance.hpp"
#include "script_engine.hpp"
#include "ignite/scene/entity.hpp"
#include "script_class.hpp"

namespace ignite
{
    ScriptInstance::ScriptInstance(Ref<ScriptClass> scriptClass, Entity entity)
        : m_ScriptClass(scriptClass), m_InstanceId(0)
    {
        m_ScriptHost = ScriptEngine::GetInstance()->GetScriptHost();
        LOG_ASSERT(m_ScriptHost, "[Script Instance] ScriptHost is null");

        // Use entity UUID as GUID key for the managed instance
		m_InstanceId = entity.GetUUID();

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
            const uint64_t entityId = static_cast<uint64_t>(entity.GetUUID());
            void *args[] = { const_cast<uint64_t *>(&entityId) };
            m_ScriptHost->Invoke(setIdMethod, args, 1, nullptr);
        }
    }

    void ScriptInstance::InvokeOnCreate()
    {
        if (m_OnCreateMethodId)
        {
            m_ScriptHost->Invoke(m_OnCreateMethodId, nullptr, 0, nullptr);
        }
    }

	void ScriptInstance::InvokeOnDestroy()
	{
        if (m_OnDestroyMethodId)
        {
            m_ScriptHost->Invoke(m_OnDestroyMethodId, nullptr, 0, nullptr);
        }
	}

	void ScriptInstance::InvokeOnUpdate(float time)
    {
        if (m_OnUpdateMethodId)
        {
            void *args[] = { &time };
            m_ScriptHost->Invoke(m_OnUpdateMethodId, args, 1, nullptr);
        }
    }

    bool ScriptInstance::GetFieldValueInternal(const std::string &name, void *buffer)
    {
        (void)name;
        (void)buffer;
        LOG_WARN("[Script Instance] GetFieldValueInternal not implemented for HostFXR yet");
        return false;
    }

    bool ScriptInstance::SetFieldValueInternal(const std::string &name, const void *value)
    {
        (void)name;
        (void)value;
        LOG_WARN("[Script Instance] SetFieldValueInternal not implemented for HostFXR yet");
        return false;
    }
}
