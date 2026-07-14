// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "script_instance.hpp"
#include "ignite/scripting/script_engine.hpp"
#include "ignite/scripting/scriptable_object.hpp"
#include "ignite/scene/entity.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/scripting/script_class.hpp"
#include "ignite/core/profiler/profiler.hpp"
#include "ignite/asset/asset_manager.hpp"

namespace ignite
{
    // Helper: push every field from a C++ ScriptableObject into a managed instance already registered under (assetHandle) in MochiSharp.
    void ScriptInstance::PopulateSOFields(ScriptHost *host, uint64_t instanceId, const ScriptableObject &so)
    {
        for (const auto &[fieldName, instanceField] : so.GetFields())
        {
            const ScriptField &field = instanceField.field;
            switch (field.Type)
            {
            case ScriptFieldType::Bool:    { auto v = instanceField.GetValue<bool>();      host->SetInstanceFieldValue(instanceId, fieldName, &v, sizeof(v)); break; }
            case ScriptFieldType::Byte:    { auto v = instanceField.GetValue<uint8_t>();   host->SetInstanceFieldValue(instanceId, fieldName, &v, sizeof(v)); break; }
            case ScriptFieldType::SByte:   { auto v = instanceField.GetValue<int8_t>();    host->SetInstanceFieldValue(instanceId, fieldName, &v, sizeof(v)); break; }
            case ScriptFieldType::Short:   { auto v = instanceField.GetValue<int16_t>();   host->SetInstanceFieldValue(instanceId, fieldName, &v, sizeof(v)); break; }
            case ScriptFieldType::UShort:  { auto v = instanceField.GetValue<uint16_t>();  host->SetInstanceFieldValue(instanceId, fieldName, &v, sizeof(v)); break; }
            case ScriptFieldType::Int:     { auto v = instanceField.GetValue<int32_t>();   host->SetInstanceFieldValue(instanceId, fieldName, &v, sizeof(v)); break; }
            case ScriptFieldType::UInt:    { auto v = instanceField.GetValue<uint32_t>();  host->SetInstanceFieldValue(instanceId, fieldName, &v, sizeof(v)); break; }
            case ScriptFieldType::Long:    { auto v = instanceField.GetValue<int64_t>();   host->SetInstanceFieldValue(instanceId, fieldName, &v, sizeof(v)); break; }
            case ScriptFieldType::ULong:   { auto v = instanceField.GetValue<uint64_t>();  host->SetInstanceFieldValue(instanceId, fieldName, &v, sizeof(v)); break; }
            case ScriptFieldType::Float:   { auto v = instanceField.GetValue<float>();     host->SetInstanceFieldValue(instanceId, fieldName, &v, sizeof(v)); break; }
            case ScriptFieldType::Double:  { auto v = instanceField.GetValue<double>();    host->SetInstanceFieldValue(instanceId, fieldName, &v, sizeof(v)); break; }
            case ScriptFieldType::Vector2: { auto v = instanceField.GetValue<glm::vec2>(); host->SetInstanceFieldValue(instanceId, fieldName, &v, sizeof(v)); break; }
            case ScriptFieldType::Vector3: { auto v = instanceField.GetValue<glm::vec3>(); host->SetInstanceFieldValue(instanceId, fieldName, &v, sizeof(v)); break; }
            case ScriptFieldType::Vector4:
            case ScriptFieldType::Color:   { auto v = instanceField.GetValue<glm::vec4>(); host->SetInstanceFieldValue(instanceId, fieldName, &v, sizeof(v)); break; }
            case ScriptFieldType::Quat:    { auto v = instanceField.GetValue<glm::quat>(); host->SetInstanceFieldValue(instanceId, fieldName, &v, sizeof(v)); break; }
            case ScriptFieldType::Enum:    { auto v = instanceField.GetValue<int32_t>();   host->SetInstanceFieldValue(instanceId, fieldName, &v, sizeof(v)); break; }
            case ScriptFieldType::Entity:
            case ScriptFieldType::Asset:
            {
                // Both Entity and Asset reference types are stored as uint64_t IDs.
                // MochiSharp's TryReadFieldValueFromBuffer will reconstruct the managed
                // reference via ctor(ulong) / _instances lookup.
                auto v = instanceField.GetValue<uint64_t>();
                if (v != 0)
                    host->SetInstanceFieldValue(instanceId, fieldName, &v, sizeof(v));
                break;
            }
            case ScriptFieldType::String:
            {
                auto str = instanceField.GetValue<std::string>();
                host->SetInstanceFieldValue(instanceId, fieldName, str.data(), (int)str.size());
                break;
            }
            default: break;
            }
        }
    }

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
        m_OnCreateMethodId = scriptClass->BindInstanceMethod(m_InstanceId, "OnCreate");
        m_OnDestroyMethodId = scriptClass->BindInstanceMethod(m_InstanceId, "OnDestroy");
        m_OnUpdateMethodId = scriptClass->BindInstanceMethod(m_InstanceId, "OnUpdate");

        // Bind internal collision callback
        m_OnCollisionEnterMethodId = scriptClass->BindInstanceMethod(m_InstanceId, "Internal_OnCollisionEnterNative");
        m_OnCollisionStayMethodId = scriptClass->BindInstanceMethod(m_InstanceId, "Internal_OnCollisionStayNative");
        m_OnCollisionExitMethodId = scriptClass->BindInstanceMethod(m_InstanceId, "Internal_OnCollisionExitNative");

		// Bind internal body activation callback
		m_OnBodyActivatedMethodId = scriptClass->BindInstanceMethod(m_InstanceId, "Internal_OnBodyActivatedNative");
		m_OnBodyDeactivatedMethodId = scriptClass->BindInstanceMethod(m_InstanceId, "Internal_OnBodyDeactivatedNative");

        // Set Entity ID on managed instance if available
        const int setIdMethod = scriptClass->BindInstanceMethod(m_InstanceId, "SetID");
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
            case ScriptFieldType::Bool:
            {
                if (hasDeserializedValues) SetFieldValue<bool>(name, instanceField.GetValue<bool>());
                else instanceField.SetValue(GetFieldValue<bool>(name));
                break;
            }
            case ScriptFieldType::Char:
            {
                if (hasDeserializedValues) SetFieldValue<char16_t>(name, instanceField.GetValue<char16_t>());
                else instanceField.SetValue(GetFieldValue<char16_t>(name));
                break;
            }
            case ScriptFieldType::String:
            {
                if (hasDeserializedValues) SetFieldValue<std::string>(name, instanceField.GetValue<std::string>());
                else instanceField.SetValue(GetFieldValue<std::string>(name));
                break;
            }
            case ScriptFieldType::Byte:
            {
                if (hasDeserializedValues) SetFieldValue<uint8_t>(name, instanceField.GetValue<uint8_t>());
                else instanceField.SetValue(GetFieldValue<uint8_t>(name));
                break;
            }
            case ScriptFieldType::SByte:
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
            case ScriptFieldType::UShort:
            {
                if (hasDeserializedValues) SetFieldValue<uint16_t>(name, instanceField.GetValue<uint16_t>());
                else instanceField.SetValue(GetFieldValue<uint16_t>(name));
                break;
            }
            case ScriptFieldType::Int:
            {
                if (hasDeserializedValues) SetFieldValue<int32_t>(name, instanceField.GetValue<int32_t>());
                else instanceField.SetValue(GetFieldValue<int32_t>(name));
                break;
            }
            case ScriptFieldType::UInt:
            {
                if (hasDeserializedValues) SetFieldValue<uint32_t>(name, instanceField.GetValue<uint32_t>());
                else instanceField.SetValue(GetFieldValue<uint32_t>(name));
                break;
            }
            case ScriptFieldType::Long:
            {
                if (hasDeserializedValues) SetFieldValue<int64_t>(name, instanceField.GetValue<int64_t>());
                else instanceField.SetValue(GetFieldValue<int64_t>(name));
                break;
            }
            case ScriptFieldType::ULong:
            {
                if (hasDeserializedValues) SetFieldValue<uint64_t>(name, instanceField.GetValue<uint64_t>());
                else instanceField.SetValue(GetFieldValue<uint64_t>(name));
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
            case ScriptFieldType::Quat:
            {
                if (hasDeserializedValues) SetFieldValue<glm::quat>(name, instanceField.GetValue<glm::quat>());
                else instanceField.SetValue(GetFieldValue<glm::quat>(name));
                break;
            }
            case ScriptFieldType::Color:
            {
                if (hasDeserializedValues) SetFieldValue<glm::vec4>(name, instanceField.GetValue<glm::vec4>());
                else instanceField.SetValue(GetFieldValue<glm::vec4>(name));
                break;
            }
            case ScriptFieldType::Enum:
            {
                if (hasDeserializedValues) SetFieldValue<int>(name, instanceField.GetValue<int>());
                else instanceField.SetValue(GetFieldValue<int>(name));
                break;
            }
            case ScriptFieldType::Asset:
            case ScriptFieldType::Entity:
            {
                if (hasDeserializedValues)
                {
                    uint64_t id = instanceField.GetValue<uint64_t>();
                    if (id != 0)
                    {
                        // For Asset (ScriptableObject) fields: pre-create the managed instance and
                        // populate its fields from the C++ asset so C# can access them directly.
                        if (field.Type == ScriptFieldType::Asset)
                        {
                            Scene *scene = ScriptEngine::GetInstance()->GetSceneContext();
                            AssetManager *am = scene ? scene->GetAssetManager() : nullptr;
                            if (!scene)
                            {
                                LOG_WARN("[Script Instance] ScriptableObject field '{}' could not resolve: Scene context is null (instanceId={})", name, m_InstanceId);
                            }
                            else if (!am)
                            {
                                LOG_WARN("[Script Instance] ScriptableObject field '{}' could not resolve: AssetManager is null (instanceId={})", name, m_InstanceId);
                            }
                            else if (!am->IsAssetHandleValid(AssetHandle(id)))
                            {
                                LOG_WARN("[Script Instance] ScriptableObject field '{}' handle {} is invalid (instanceId={})", name, id, m_InstanceId);
                            }
                            else
                            {
                                auto so = am->GetAssetImmediate<ScriptableObject>(AssetHandle(id));
                                if (so)
                                {
                                    // Create the managed C# instance keyed by asset handle
                                    if (!m_ScriptHost->CreateInstance(id, so->GetClassName()))
                                    {
                                        LOG_ERROR("[Script Instance] Failed to create ScriptableObject managed instance '{}' (handle={}, instanceId={})", so->GetClassName(), id, m_InstanceId);
                                    }
                                    // Populate its serialized fields from the .ixso data
                                    PopulateSOFields(m_ScriptHost, id, *so);
                                }
                                else
                                {
                                    const AssetMetaData &metadata = am->GetMetaData(AssetHandle(id));
                                    LOG_ERROR("[Script Instance] Failed to load ScriptableObject handle {} (file='{}', instanceId={})", id, metadata.filepath.generic_string(), m_InstanceId);
                                }
                            }
                        }
                        SetFieldValue<uint64_t>(name, id);
                    }
                    // If id == 0, leave the field null (default) in the managed instance
                }
                else
                {
                    instanceField.SetValue(GetFieldValue<uint64_t>(name));
                }
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

    void ScriptInstance::InvokeOnCollisionEnter(uint64_t otherEntityID)
    {
        IGN_PROFILE_FUNCTION();
        if (m_OnCollisionEnterMethodId)
        {
            void *args[] = { &otherEntityID };
            if (!m_ScriptHost->Invoke(m_OnCollisionEnterMethodId, args, 1, nullptr))
            {
                LOG_ERROR("[Script Instance] OnCollisionEnter invocation failed (instanceId={}, type={})", m_InstanceId, m_ScriptClass->GetFullName());
                m_OnCollisionEnterMethodId = 0;
            }
        }
    }

    void ScriptInstance::InvokeOnCollisionStay(uint64_t otherEntityID)
    {
        IGN_PROFILE_FUNCTION();
        if (m_OnCollisionStayMethodId)
        {
            void *args[] = { &otherEntityID };
            if (!m_ScriptHost->Invoke(m_OnCollisionStayMethodId, args, 1, nullptr))
            {
                LOG_ERROR("[Script Instance] OnCollisionStay invocation failed (instanceId={}, type={})", m_InstanceId, m_ScriptClass->GetFullName());
                m_OnCollisionStayMethodId = 0;
            }
        }
    }

    void ScriptInstance::InvokeOnCollisionExit(uint64_t otherEntityID)
    {
        IGN_PROFILE_FUNCTION();
        if (m_OnCollisionExitMethodId)
        {
            void *args[] = { &otherEntityID };
            if (!m_ScriptHost->Invoke(m_OnCollisionExitMethodId, args, 1, nullptr))
            {
                LOG_ERROR("[Script Instance] OnCollisionExit invocation failed (instanceId={}, type={})", m_InstanceId, m_ScriptClass->GetFullName());
                m_OnCollisionExitMethodId = 0;
            }
        }
    }

	void ScriptInstance::InvokeOnBodyActivated()
	{
		IGN_PROFILE_FUNCTION();
		if (m_OnBodyActivatedMethodId)
		{
			if (!m_ScriptHost->Invoke(m_OnBodyActivatedMethodId, nullptr, 0, nullptr))
			{
				LOG_ERROR("[Script Instance] OnBodyActivated invocation failed (instanceId={}, type={})", m_InstanceId, m_ScriptClass->GetFullName());
				m_OnBodyActivatedMethodId = 0;
			}
		}
	}

	void ScriptInstance::InvokeOnBodyDeactivated()
	{
		IGN_PROFILE_FUNCTION();
		if (m_OnBodyDeactivatedMethodId)
		{
			if (!m_ScriptHost->Invoke(m_OnBodyDeactivatedMethodId, nullptr, 0, nullptr))
			{
				LOG_ERROR("[Script Instance] OnBodyDeactivated invocation failed (instanceId={}, type={})", m_InstanceId, m_ScriptClass->GetFullName());
				m_OnBodyDeactivatedMethodId = 0;
			}
		}
	}

}
