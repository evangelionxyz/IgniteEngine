// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "script_host.hpp"
#include "ignite/core/logger.hpp"
#include "glue/component_script_glue.hpp"
#include "glue/core_script_glue.hpp"

namespace ignite
{
    ScriptHost::ScriptHost()
    {
        m_Host = std::make_unique<MochiSharp::DotNetHost>();
    }

    ScriptHost::~ScriptHost()
    {
        m_Host.reset();
        m_Initialized = false;
    }

    static void ManagedLogCallback(const char* message)
    {
        std::string msg(message);
        if (msg.find("failed") != std::string::npos || msg.find("Exception") != std::string::npos)
        {
            LOG_ERROR("[MochiSharp] {}", msg);
        }
        else
        {
            LOG_INFO("[MochiSharp] {}", msg);
        }
    }

    bool ScriptHost::Init(const ignite::Path &configPath)
    {
        if (m_Initialized)
        {
            LOG_WARN("[Script Host] Already initialized");
            return true;
        }

        std::wstring wConfigPath = configPath.wstring();
        if (!m_Host->Init(wConfigPath, ManagedLogCallback))
        {
            LOG_ERROR("[Script Host] Failed to initialize HostFXR with config: {}", configPath.generic_string());
            return false;
        }

        m_BaseDir = configPath.parent_path();
        m_Initialized = true;

        LOG_INFO("[Script Host] Initialized with config: {}", configPath.generic_string());
        return true;
    }

    bool ScriptHost::LoadAssembly(const ignite::Path &assemblyPath)
    {
        if (!m_Initialized)
        {
            LOG_ERROR("[Script Host] Cannot load assembly - host not initialized");
            return false;
        }

        std::string path = assemblyPath.string();
        if (!m_Host->LoadAssembly(path.c_str()))
        {
            LOG_ERROR("[Script Host] Failed to load assembly: {}", assemblyPath.generic_string());
            return false;
        }

        LOG_INFO("[Script Host] Loaded assembly: {}", assemblyPath.generic_string());
        return true;
    }

    void ScriptHost::RegisterSignatures()
    {
        // Void signatures
        m_Host->RegisterSignature(static_cast<int>(ScriptMethodSig::Void), "System.Void", nullptr, 0);

        // Void with float parameter
        {
            const char *params[] = { "System.Single" };
            m_Host->RegisterSignature(static_cast<int>(ScriptMethodSig::Void_Float), "System.Void", params, 1);
        }

        // Void with UInt64 parameter
        {
            const char *params[] = { "System.UInt64" };
            m_Host->RegisterSignature(static_cast<int>(ScriptMethodSig::Void_UInt64), "System.Void", params, 1);
        }

        // Bool with Type parameter
        {
            const char *params[] = { "System.Type" };
            m_Host->RegisterSignature(static_cast<int>(ScriptMethodSig::Bool_Type), "System.Boolean", params, 1);
        }

        // Void with UInt64 and Type parameters
        {
            const char *params[] = { "System.UInt64", "System.Type" };
            m_Host->RegisterSignature(static_cast<int>(ScriptMethodSig::Void_UInt64_Type), "System.Void", params, 2);
        }

        // UInt64 with String parameter
        {
            const char *params[] = { "System.String" };
            m_Host->RegisterSignature(static_cast<int>(ScriptMethodSig::UInt64_String), "System.UInt64", params, 1);
        }

        // Void with UInt64 and Bool parameters
        {
            const char *params[] = { "System.UInt64", "System.Boolean" };
            m_Host->RegisterSignature(static_cast<int>(ScriptMethodSig::Void_UInt64_Bool), "System.Void", params, 2);
        }

        // Void with UInt64 and out Bool
        {
            const char *params[] = { "System.UInt64", "System.Boolean&" };
            m_Host->RegisterSignature(static_cast<int>(ScriptMethodSig::Void_UInt64_OutBool), "System.Void", params, 2);
        }

        // ===================================
        // VECTOR 2
        // UInt64 with UInt64 and Vector2 parameters
        {
            const char *params[] = { "System.UInt64", "Ignite.Mathf+Vector2, Ignite" };
            m_Host->RegisterSignature(static_cast<int>(ScriptMethodSig::UInt64_UInt64_Vec2), "System.UInt64", params, 2);
        }

        // Void with UInt64 and out Vector2
        {
            const char *params[] = { "System.UInt64", "Ignite.Mathf+Vector2&, Ignite" };
            m_Host->RegisterSignature(static_cast<int>(ScriptMethodSig::Void_UInt64_OutVec2), "System.Void", params, 2);
        }

        // Void with UInt64 and Vector2
        {
            const char *params[] = { "System.UInt64", "Ignite.Mathf+Vector2, Ignite" };
            m_Host->RegisterSignature(static_cast<int>(ScriptMethodSig::Void_UInt64_Vec2), "System.Void", params, 2);
        }


        // ===================================
        // VECTOR 3
        // UInt64 with UInt64 and Vector3 parameters
        {
            const char *params[] = { "System.UInt64", "Ignite.Mathf+Vector3, Ignite" };
            m_Host->RegisterSignature(static_cast<int>(ScriptMethodSig::UInt64_UInt64_Vec3), "System.UInt64", params, 2);
        }

        // Void with UInt64 and out Vector3
        {
            const char *params[] = { "System.UInt64", "Ignite.Mathf+Vector3&, Ignite" };
            m_Host->RegisterSignature(static_cast<int>(ScriptMethodSig::Void_UInt64_OutVec3), "System.Void", params, 2);
        }

        // Void with UInt64 and Vector3
        {
            const char *params[] = { "System.UInt64", "Ignite.Mathf+Vector3, Ignite" };
            m_Host->RegisterSignature(static_cast<int>(ScriptMethodSig::Void_UInt64_Vec3), "System.Void", params, 2);
        }

        // ===================================
        // VECTOR 4
        // UInt64 with UInt64 and Vector4 parameters
        {
            const char *params[] = { "System.UInt64", "Ignite.Mathf+Vector4, Ignite" };
            m_Host->RegisterSignature(static_cast<int>(ScriptMethodSig::UInt64_UInt64_Vec4), "System.UInt64", params, 2);
        }

        // Void with UInt64 and out Vector4
        {
            const char *params[] = { "System.UInt64", "Ignite.Mathf+Vector4&, Ignite" };
            m_Host->RegisterSignature(static_cast<int>(ScriptMethodSig::Void_UInt64_OutVec4), "System.Void", params, 2);
        }

        // Void with UInt64 and Vector4
        {
            const char *params[] = { "System.UInt64", "Ignite.Mathf+Vector4, Ignite" };
            m_Host->RegisterSignature(static_cast<int>(ScriptMethodSig::Void_UInt64_Vec4), "System.Void", params, 2);
        }

        // ===================================
        // Quaternion
        // Void with UInt64 and out Quaternion
        {
            const char *params[] = { "System.UInt64", "Ignite.Mathf+Quaternion&, Ignite" };
            m_Host->RegisterSignature(static_cast<int>(ScriptMethodSig::Void_UInt64_OutQuat), "System.Void", params, 2);
        }

        // Void with UInt64 and out Quaternion
        {
            const char *params[] = { "System.UInt64", "Ignite.Mathf+Quaternion&, Ignite" };
            m_Host->RegisterSignature(static_cast<int>(ScriptMethodSig::Void_UInt64_OutQuat), "System.Void", params, 2);
        }

        // Void with UInt64 and Quaternion
        {
            const char *params[] = { "System.UInt64", "Ignite.Mathf+Quaternion, Ignite" };
            m_Host->RegisterSignature(static_cast<int>(ScriptMethodSig::Void_UInt64_Quat), "System.Void", params, 2);
        }

        // Object with UInt64
        {
            const char *params[] = { "System.UInt64" };
            m_Host->RegisterSignature(static_cast<int>(ScriptMethodSig::Object_UInt64), "System.Object", params, 1);
        }

        LOG_INFO("[Script Host] Registered method signatures");
    }

    bool ScriptHost::InitializeCoreInternalCalls()
    {
        if (!m_Initialized)
        {
            LOG_ERROR("[Script Host] Cannot initialize CORE internal calls - host not initialized");
            return false;
        }

        const auto *api = CoreScriptGlue::GetAPI();
        const auto apiPtr = reinterpret_cast<uint64_t>(api);

        const int methodId = m_Host->BindStaticMethod("Ignite.Core.CoreInternalCalls", "Initialize", static_cast<int>(ScriptMethodSig::Void_UInt64));
        if (methodId == 0)
        {
            LOG_ERROR("[Script Host] Failed to bind Ignite.Core.CoreInternalCalls.Initialize");
            return false;
        }

        std::array<void *, 1> args = { const_cast<uint64_t *>(&apiPtr) };
        if (!m_Host->Invoke(methodId, args.data(), (int)args.size(), nullptr))
        {
            LOG_ERROR("[Script Host] Failed to invoke Ignite.Core.CoreInternalCalls.Initialize");
            return false;
        }

        LOG_INFO("[Script Host] CORE Internal calls bridge initialized");
        return true;
    }

    bool ScriptHost::InitializeComponentInternalCalls()
    {
        if (!m_Initialized)
        {
            LOG_ERROR("[Script Host] Cannot initialize COMPONENT internal calls - host not initialized");
            return false;
        }

        const auto *api = ComponentScriptGlue::GetAPI();
        const auto apiPtr = reinterpret_cast<uint64_t>(api);

        const int methodId = m_Host->BindStaticMethod("Ignite.Core.Component.ComponentInternalCalls", "Initialize", static_cast<int>(ScriptMethodSig::Void_UInt64));
        if (methodId == 0)
        {
            LOG_ERROR("[Script Host] Failed to bind Ignite.Core.Component.ComponentInternalCalls.Initialize");
            return false;
        }

        std::array<void *, 1> args = { const_cast<uint64_t *>(&apiPtr) };
        if (!m_Host->Invoke(methodId, args.data(), (int)args.size(), nullptr))
        {
            LOG_ERROR("[Script Host] Failed to invoke Ignite.Core.Component.ComponentInternalCalls.Initialize");
            return false;
        }

        LOG_INFO("[Script Host] COMPONENT Internal calls bridge initialized");
        return true;
    }

    bool ScriptHost::CreateInstance(uint64_t instanceId, const std::string &typeName)
    {
        if (!m_Initialized)
        {
            LOG_ERROR("[Script Host] Cannot create instance - host not initialized");
            return false;
        }

        if (!m_Host->CreateInstance(typeName.c_str(), instanceId))
        {
            LOG_ERROR("[Script Host] Failed to create instance {} of type {}", instanceId, typeName);
            return false;
        }

        LOG_TRACE("[Script Host] Created instance {} of type {}", instanceId, typeName);
        return true;
    }

    void ScriptHost::DestroyInstance(uint64_t instanceId)
    {
        if (!m_Initialized)
        {
            return;
        }

        m_Host->DestroyInstance(instanceId);
        LOG_TRACE("[Script Host] Destroyed instance {}", instanceId);
    }

	std::string ScriptHost::GetInstanceFields(uint64_t instanceId)
	{
        if (!m_Initialized)
        {
            LOG_ERROR("[Script Host] Cannot get instance fields - host not initialized");
            return {};
        }

        return m_Host->GetInstanceFields(instanceId);
	}

    std::string ScriptHost::GetTypeFields(const std::string &typeName)
    {
        if (!m_Initialized)
        {
            LOG_ERROR("[Script Host] Cannot get type fields - host not initialized");
            return {};
        }

        return m_Host->GetTypeFields(typeName.c_str());
    }

    bool ScriptHost::ConfigureSerialization(const std::string &serializeFieldAttributeTypeName, const std::string &typeName)
    {
        if (!m_Initialized)
        {
            LOG_ERROR("[Script Host] Cannot configure serialization - host not initialized");
            return false;
        }

        return m_Host->ConfigureSerialization(serializeFieldAttributeTypeName.c_str(), typeName.c_str());
    }

    bool ScriptHost::GetInstanceFieldValue(uint64_t instanceId, const std::string &fieldName, void *buffer, int bufferSize)
    {
        if (!m_Initialized)
        {
            LOG_ERROR("[Script Host] Cannot get field value - host not initialized");
            return false;
        }

        return m_Host->GetInstanceFieldValue(instanceId, fieldName.c_str(), buffer, bufferSize);
    }

    bool ScriptHost::SetInstanceFieldValue(uint64_t instanceId, const std::string &fieldName, const void *buffer, int bufferSize)
    {
        if (!m_Initialized)
        {
            LOG_ERROR("[Script Host] Cannot set field value - host not initialized");
            return false;
        }

        return m_Host->SetInstanceFieldValue(instanceId, fieldName.c_str(), buffer, bufferSize);
    }

	int ScriptHost::BindInstanceMethod(uint64_t instanceId, const std::string &methodName, ScriptMethodSig signature)
    {
        if (!m_Initialized)
        {
            LOG_ERROR("[Script Host] Cannot bind method - host not initialized");
            return 0;
        }

        int methodId = m_Host->BindInstanceMethod(instanceId, methodName.c_str(), static_cast<int>(signature));
        if (methodId == 0)
        {
            LOG_WARN("[Script Host] Failed to bind instance method {}.{}", instanceId, methodName);
        }

        return methodId;
    }

    int ScriptHost::BindStaticMethod(const std::string &typeName, const std::string &methodName, ScriptMethodSig signature)
    {
        if (!m_Initialized)
        {
            LOG_ERROR("[Script Host] Cannot bind static method - host not initialized");
            return 0;
        }

        int methodId = m_Host->BindStaticMethod(typeName.c_str(), methodName.c_str(), static_cast<int>(signature));
        if (methodId == 0)
        {
            LOG_ERROR("[Script Host] Failed to bind static method {}.{}", typeName, methodName);
        }

        return methodId;
    }

    bool ScriptHost::Invoke(int methodId, const void *argsPtr, int argCount, void *returnPtr)
    {
        if (!m_Initialized)
        {
            LOG_ERROR("[Script Host] Cannot invoke - host not initialized");
            return false;
        }

        if (methodId == 0)
        {
            LOG_ERROR("[Script Host] Invalid method ID");
            return false;
        }

        const bool success = m_Host->Invoke(methodId, argsPtr, argCount, returnPtr);
        if (!success)
        {
            LOG_ERROR("[Script Host] Invoke failed (methodId={}, argCount={})", methodId, argCount);
        }

        return success;
    }

    std::string ScriptHost::GetDerivedTypes(const ignite::Path &assemblyPath, const std::string &baseType)
    {
        if (!m_Initialized)
        {
            LOG_ERROR("[Script Host] Cannot get derived types - host not initialized");
            return {};
        }

        std::string path = assemblyPath.string();
        return m_Host->GetDerivedTypes(path.c_str(), baseType.c_str());
    }

    std::string ScriptHost::GetCreateAssetMenuData(const ignite::Path &assemblyPath, const std::string &baseType)
    {
        if (!m_Initialized)
        {
            LOG_ERROR("[Script Host] Cannot get CreateAssetMenu data - host not initialized");
            return {};
        }

        std::string path = assemblyPath.string();
        return m_Host->GetCreateAssetMenuData(path.c_str(), baseType.c_str());
    }
}
