// Copyright (c) 2026 Evangelion Manuhutu

#include "script_host.hpp"
#include "ignite/core/logger.hpp"
#include "script_glue.hpp"

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

    bool ScriptHost::Init(const std::filesystem::path &configPath)
    {
        if (m_Initialized)
        {
            LOG_WARN("[Script Host] Already initialized");
            return true;
        }

        std::wstring wConfigPath = configPath.wstring();
        if (!m_Host->Init(wConfigPath))
        {
            LOG_ERROR("[Script Host] Failed to initialize HostFXR with config: {}", configPath.generic_string());
            return false;
        }

        m_BaseDir = configPath.parent_path();
        m_Initialized = true;

        LOG_INFO("[Script Host] Initialized with config: {}", configPath.generic_string());
        return true;
    }

    bool ScriptHost::LoadAssembly(const std::filesystem::path &assemblyPath)
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
        m_Host->RegisterSignature(static_cast<int>(ScriptMethodSig::Void), 
            "System.Void", nullptr, 0);

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

        // UInt64 with UInt64 and Vector3 parameters
        {
            const char *params[] = { "System.UInt64", "Ignite.Vector3, Ignite" };
            m_Host->RegisterSignature(static_cast<int>(ScriptMethodSig::UInt64_UInt64_Vec3), "System.UInt64", params, 2);
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

        // Void with UInt64 and out Vector3
        {
			const char *params[] = { "System.UInt64", "Ignite.Vector3&, Ignite" };
            m_Host->RegisterSignature(static_cast<int>(ScriptMethodSig::Void_UInt64_OutVec3), "System.Void", params, 2);
        }

        // Void with UInt64 and Vector3
        {
            const char *params[] = { "System.UInt64", "Ignite.Vector3, Ignite" };
            m_Host->RegisterSignature(static_cast<int>(ScriptMethodSig::Void_UInt64_Vec3), "System.Void", params, 2);
        }

        // Void with UInt64 and out Quaternion
        {
            const char *params[] = { "System.UInt64", "Ignite.Quaternion&, Ignite" };
            m_Host->RegisterSignature(static_cast<int>(ScriptMethodSig::Void_UInt64_OutQuat), "System.Void", params, 2);
        }

        // Void with UInt64 and Quaternion
        {
            const char *params[] = { "System.UInt64", "Ignite.Quaternion, Ignite" };
            m_Host->RegisterSignature(static_cast<int>(ScriptMethodSig::Void_UInt64_Quat), "System.Void", params, 2);
        }

        // Object with UInt64
        {
            const char *params[] = { "System.UInt64" };
            m_Host->RegisterSignature(static_cast<int>(ScriptMethodSig::Object_UInt64), "System.Object", params, 1);
        }

        LOG_INFO("[Script Host] Registered method signatures");
    }

    bool ScriptHost::InitializeInternalCalls()
    {
        if (!m_Initialized)
        {
            LOG_ERROR("[Script Host] Cannot initialize internal calls - host not initialized");
            return false;
        }

        const auto *api = ScriptGlue::GetAPI();
        const uint64_t apiPtr = reinterpret_cast<uint64_t>(api);

        const int methodId = m_Host->BindStaticMethod("Ignite.InternalCalls", "Initialize", static_cast<int>(ScriptMethodSig::Void_UInt64));
        if (methodId == 0)
        {
            LOG_ERROR("[Script Host] Failed to bind Ignite.InternalCalls.Initialize");
            return false;
        }

        void *args[] = { const_cast<uint64_t *>(&apiPtr) };
        if (!m_Host->Invoke(methodId, args, 1, nullptr))
        {
            LOG_ERROR("[Script Host] Failed to invoke Ignite.InternalCalls.Initialize");
            return false;
        }

        LOG_INFO("[Script Host] Internal calls bridge initialized");
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

        return m_Host->Invoke(methodId, argsPtr, argCount, returnPtr);
    }

    std::string ScriptHost::GetDerivedTypes(const std::filesystem::path &assemblyPath, const std::string &baseType)
    {
        if (!m_Initialized)
        {
            LOG_ERROR("[Script Host] Cannot get derived types - host not initialized");
            return {};
        }

        std::string path = assemblyPath.string();
        return m_Host->GetDerivedTypes(path.c_str(), baseType.c_str());
    }
}
