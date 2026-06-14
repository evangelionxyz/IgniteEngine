// Copyright (c) 2026 Evangelion Manuhutu

#include "script_host.hpp"
#include "ignite/core/logger.hpp"
#include "glue/component_script_glue.hpp"
#include "glue/core_script_glue.hpp"

#include "MochiSharp/MochiManagedFunctions.hpp"
#include "MochiSharp/String.hpp"

#include "ignite/scripting/script_engine.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/asset/asset_manager.hpp"
#include "ignite/scripting/script_instances/script_instance.hpp"

#include <algorithm>
#include <array>
#include <cstring>

namespace ignite
{
    namespace
    {
        static void ManagedLogCallback(std::string_view message, mochi::MessageLevel level)
        {
            switch (level)
            {
                case mochi::MessageLevel::Trace:   LOG_TRACE("[MochiSharp] {}", message); break;
                case mochi::MessageLevel::Info:    LOG_INFO("[MochiSharp] {}", message); break;
                case mochi::MessageLevel::Warning: LOG_WARN("[MochiSharp] {}", message); break;
                case mochi::MessageLevel::Error:   LOG_ERROR("[MochiSharp] {}", message); break;
                default:                           LOG_INFO("[MochiSharp] {}", message); break;
            }
        }

        static void ManagedExceptionCallback(std::string_view message)
        {
            LOG_ERROR("[MochiSharp] {}", message);
        }

        static std::string InvokeStaticStringMethod(mochi::Type &type, std::string_view methodName, std::string_view arg0, std::string_view arg1)
        {
            auto managedArg0 = mochi::String::New(arg0);
            auto managedArg1 = mochi::String::New(arg1);
            mochi::String managedResult = type.InvokeStaticMethod<mochi::String>(methodName, managedArg0, managedArg1);

            std::string result;
            if (managedResult.Data())
            {
                result = static_cast<std::string>(managedResult);
            }

            mochi::String::Free(managedResult);
            mochi::String::Free(managedArg0);
            mochi::String::Free(managedArg1);
            return result;
        }
    }

    ScriptHost::ScriptHost() = default;

    ScriptHost::~ScriptHost()
    {
        m_MethodBindings.clear();
        m_InstanceMap.clear();
        m_TypeMap.clear();
        m_LoadContext.reset();

        if (m_Initialized)
        {
            m_Host.Shutdown();
        }

        m_Initialized = false;
    }

    bool ScriptHost::Init(const ignite::Path &configPath)
    {
        if (m_Initialized)
        {
            LOG_WARN("[Script Host] Already initialized");
            return true;
        }

        if (!ignite::Path::exists(configPath))
        {
            LOG_ERROR("[Script Host] MochiSharp runtime config not found: {}", configPath.generic_string());
            return false;
        }

        m_BaseDir = configPath.parent_path();

        mochi::HostSettings hostSettings;
        hostSettings.MochiSharpDirectory = m_BaseDir.string();
        hostSettings.MessageCallback = ManagedLogCallback;
        hostSettings.ExceptionCallback = ManagedExceptionCallback;

        const auto status = m_Host.Initialize(std::move(hostSettings));
        if (status != mochi::MochiSharpInitStatus::Success)
        {
            LOG_ERROR("[Script Host] Failed to initialize MochiSharp host (status={})", static_cast<int>(status));
            return false;
        }

        m_LoadContext = CreateScope<mochi::AssemblyLoadContext>(m_Host.CreateAssemblyLoadContext("Ignite.Scripting", m_BaseDir.string()));

        m_Initialized = true;
        LOG_INFO("[Script Host] Initialized with MochiSharp directory: {}", m_BaseDir.generic_string());
        return true;
    }

    mochi::ManagedAssembly *ScriptHost::LoadAssemblyInternal(const ignite::Path &assemblyPath, mochi::ManagedAssembly *&targetSlot)
    {
        if (!m_LoadContext)
        {
            return nullptr;
        }

        auto &assembly = m_LoadContext->LoadAssembly(assemblyPath.string());
        if (assembly.GetLoadStatus() != mochi::AssemblyLoadStatus::Success)
        {
            return nullptr;
        }

        targetSlot = &assembly;

        for (auto &type : assembly.GetLocalTypes())
        {
            const std::string fullName = static_cast<std::string>(type.GetFullName());
            if (!fullName.empty())
            {
                m_TypeMap[fullName] = const_cast<mochi::Type *>(&type);
            }
        }

        return &assembly;
    }

    bool ScriptHost::LoadAssembly(const ignite::Path &assemblyPath)
    {
        if (!m_Initialized)
        {
            LOG_ERROR("[Script Host] Cannot load assembly - host not initialized");
            return false;
        }

        const bool isCoreAssembly = assemblyPath.filename() == "Ignite.ScriptEngine.dll";
        mochi::ManagedAssembly *loadedAssembly = LoadAssemblyInternal(assemblyPath, isCoreAssembly ? m_CoreAssembly : m_AppAssembly);
        return loadedAssembly;
    }

    bool ScriptHost::ResetLoadContext()
    {
        if (!m_Initialized)
        {
            LOG_ERROR("[Script Host] Cannot reset load context - host not initialized");
            return false;
        }

        for (auto &entry : m_InstanceMap)
        {
            entry.second.Destroy();
        }

        m_InstanceMap.clear();
        m_MethodBindings.clear();
        m_TypeMap.clear();
        m_ReferenceTypeNames.clear();
        m_SerializeFieldAttributeTypeName.clear();
        m_NextMethodId = 1;
        m_CoreAssembly = nullptr;
        m_AppAssembly = nullptr;

        m_LoadContext.reset();
        const std::string alcName = "Ignite.Scripting." + std::to_string(++m_ReloadCounter);
        m_LoadContext = CreateScope<mochi::AssemblyLoadContext>(m_Host.CreateAssemblyLoadContext(alcName, m_BaseDir.string()));
        if (!m_LoadContext)
        {
            LOG_ERROR("[Script Host] Failed to create assembly load context for reload");
            return false;
        }

        return true;
    }

    void ScriptHost::RegisterSignatures()
    {
        if (!m_Initialized)
        {
            LOG_WARN("[Script Host] RegisterSignatures called before initialization");
            return;
        }

        LOG_INFO("[Script Host] Using MochiSharp HostInstance invocation signatures");
    }

    bool ScriptHost::InitializeCoreInternalCalls()
    {
        if (!m_Initialized)
        {
            LOG_ERROR("[Script Host] Cannot initialize CORE internal calls - host not initialized");
            return false;
        }

        const auto *api = CoreScriptGlue::GetAPI();
        const uint64_t apiPtr = reinterpret_cast<uint64_t>(api);

        const int methodId = BindStaticMethod("Ignite.Core.CoreInternalCalls", "Initialize");
        if (methodId == 0)
        {
            LOG_ERROR("[Script Host] Failed to bind Ignite.Core.CoreInternalCalls.Initialize");
            return false;
        }

        std::array<void *, 1> args = { const_cast<uint64_t *>(&apiPtr) };
        if (!Invoke(methodId, args.data(), static_cast<int>(args.size()), nullptr))
        {
            LOG_ERROR("[Script Host] Failed to invoke Ignite.Core.CoreInternalCalls.Initialize");
            return false;
        }

        LOG_INFO("[Script Host] CORE internal calls bridge initialized");
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
        const uint64_t apiPtr = reinterpret_cast<uint64_t>(api);

        const int methodId = BindStaticMethod("Ignite.Core.Component.ComponentInternalCalls", "Initialize");
        if (methodId == 0)
        {
            LOG_ERROR("[Script Host] Failed to bind Ignite.Core.Component.ComponentInternalCalls.Initialize");
            return false;
        }

        std::array<void *, 1> args = { const_cast<uint64_t *>(&apiPtr) };
        if (!Invoke(methodId, args.data(), static_cast<int>(args.size()), nullptr))
        {
            LOG_ERROR("[Script Host] Failed to invoke Ignite.Core.Component.ComponentInternalCalls.Initialize");
            return false;
        }

        LOG_INFO("[Script Host] COMPONENT internal calls bridge initialized");
        return true;
    }

    mochi::Type *ScriptHost::FindType(const std::string &typeName) const
    {
        const auto it = m_TypeMap.find(typeName);
        return it != m_TypeMap.end() ? it->second : nullptr;
    }

    std::optional<ScriptHost::MethodBinding> ScriptHost::CreateMethodBinding(MethodBinding::Kind kind, uint64_t instanceId, mochi::Type *type, const std::string &methodName) const
    {
        if (!type)
        {
            return std::nullopt;
        }

        std::optional<MethodBinding> result;
        for (auto &method : type->GetMethods())
        {
            if (static_cast<std::string>(method.GetName()) != methodName)
            {
                continue;
            }

            if (result.has_value())
            {
                LOG_ERROR("[Script Host] Failed to bind method '{}.{}': multiple overloads match by name", 
                    static_cast<std::string>(type->GetFullName()), methodName);
                return std::nullopt;
            }

            MethodBinding binding;
            binding.kind = kind;
            binding.instanceId = instanceId;
            binding.type = type;
            binding.methodName = methodName;

            for (const auto *parameterType : method.GetParameterTypes())
            {
                binding.parameterTypes.push_back(parameterType ? parameterType->GetManagedType() : mochi::ManagedType::Unknown);
            }

            result = std::move(binding);
        }

        if (!result.has_value())
        {
            LOG_ERROR("[Script Host] Failed to find method '{}.{}'", static_cast<std::string>(type->GetFullName()), methodName);
        }

        return result;
    }

    bool ScriptHost::CreateInstance(uint64_t instanceId, const std::string &typeName)
    {
        if (!m_Initialized)
        {
            LOG_ERROR("[Script Host] Cannot create instance - host not initialized");
            return false;
        }

        if (m_InstanceMap.contains(instanceId))
        {
            return true;
        }

        mochi::Type *type = FindType(typeName);
        if (!type)
        {
            LOG_ERROR("[Script Host] Failed to resolve type '{}'", typeName);
            return false;
        }

        auto instance = type->CreateInstance();
        if (!instance.IsValid())
        {
            LOG_ERROR("[Script Host] Failed to create managed instance {} of type {}", instanceId, typeName);
            return false;
        }

        auto [insertedIt, inserted] = m_InstanceMap.emplace(instanceId, std::move(instance));
        // Ensure the managed object knows its own ID (same as EnsureReferenceInstance does).
        // Without this, C# code that checks 'obj == null' or reads 'obj.ID' sees zero
        // and treats the instance as null.
        insertedIt->second.InvokeMethod("SetID", instanceId);
        LOG_TRACE("[Script Host] Created instance {} of type {}", instanceId, typeName);
        return true;
    }

    void ScriptHost::DestroyInstance(uint64_t instanceId)
    {
        auto it = m_InstanceMap.find(instanceId);
        if (it == m_InstanceMap.end())
        {
            return;
        }

        it->second.Destroy();
        m_InstanceMap.erase(it);
        LOG_TRACE("[Script Host] Destroyed instance {}", instanceId);
    }

    mochi::Type *ScriptHost::FindFieldType(mochi::ManagedObject &instance, const std::string &fieldName)
    {
        const auto &type = instance.GetType();
        for (auto field : type.GetFields())
        {
            if (static_cast<std::string>(field.GetName()) == fieldName)
            {
                return &field.GetType();
            }
        }

        return nullptr;
    }

    bool ScriptHost::IsReferenceType(const mochi::Type &type) const
    {
        const std::string fullName = static_cast<std::string>(type.GetFullName());
        if (m_ReferenceTypeNames.contains(fullName))
        {
            return true;
        }

        for (const auto &referenceTypeName : m_ReferenceTypeNames)
        {
            mochi::Type *referenceType = FindType(referenceTypeName);
            if (referenceType && type.IsAssignableTo(*referenceType))
            {
                return true;
            }
        }

        return false;
    }

    mochi::ManagedObject *ScriptHost::EnsureReferenceInstance(uint64_t instanceId, const mochi::Type &type)
    {
        if (instanceId == 0)
        {
            return nullptr;
        }

        if (auto it = m_InstanceMap.find(instanceId); it != m_InstanceMap.end())
        {
            return &it->second;
        }

        auto reference = type.CreateInstance();
        if (!reference.IsValid())
        {
            return nullptr;
        }

        reference.InvokeMethod("SetID", instanceId);
        auto [it, inserted] = m_InstanceMap.emplace(instanceId, std::move(reference));
        if (inserted)
        {
            mochi::Type *soBaseType = FindType("Ignite.ScriptableObject");
            if (soBaseType && type.IsAssignableTo(*soBaseType))
            {
                ScriptEngine *se = ScriptEngine::GetInstance();
                Scene *scene = se ? se->GetSceneContext() : nullptr;
                AssetManager *am = scene ? scene->GetAssetManager() : nullptr;
                if (am && am->IsAssetHandleValid(AssetHandle(instanceId)))
                {
                    auto so = am->GetAssetImmediate<ScriptableObject>(AssetHandle(instanceId));
                    if (so)
                    {
                        ScriptInstance::PopulateSOFields(this, instanceId, *so);
                    }
                }
            }
            return &it->second;
        }
        return nullptr;
    }

    std::string ScriptHost::GetInstanceFields(uint64_t instanceId)
    {
        auto it = m_InstanceMap.find(instanceId);
        if (it == m_InstanceMap.end())
        {
            return {};
        }

        return GetTypeFields(static_cast<std::string>(it->second.GetType().GetFullName()));
    }

    mochi::Type *ScriptHost::GetReflectionBridgeType() const
    {
        return FindType("Ignite.Core.ScriptReflectionBridge");
    }

    std::string ScriptHost::GetTypeFields(const std::string &typeName)
    {
        mochi::Type *bridgeType = GetReflectionBridgeType();
        if (!bridgeType)
        {
            LOG_ERROR("[Script Host] ScriptReflectionBridge type was not found");
            return {};
        }

        return InvokeStaticStringMethod(*bridgeType, "GetTypeFields", typeName, m_SerializeFieldAttributeTypeName);
    }

    bool ScriptHost::ConfigureSerialization(const std::string &serializeFieldAttributeTypeName, const std::string &typeName)
    {
        if (!serializeFieldAttributeTypeName.empty())
        {
            m_SerializeFieldAttributeTypeName = serializeFieldAttributeTypeName;
        }

        if (!typeName.empty())
        {
            m_ReferenceTypeNames.insert(typeName);
        }

        return true;
    }

    bool ScriptHost::GetInstanceFieldValue(uint64_t instanceId, const std::string &fieldName, void *buffer, int bufferSize)
    {
        if (!buffer || bufferSize <= 0)
        {
            return false;
        }

        auto instanceIt = m_InstanceMap.find(instanceId);
        if (instanceIt == m_InstanceMap.end())
        {
            return false;
        }

        auto *fieldType = FindFieldType(instanceIt->second, fieldName);
        if (!fieldType)
        {
            return false;
        }

        const auto managedType = fieldType->GetManagedType();
        if (managedType == mochi::ManagedType::String)
        {
            const std::string value = instanceIt->second.GetFieldValue<std::string>(fieldName);
            const size_t copyLength = std::min(value.size(), static_cast<size_t>(bufferSize - 1));
            std::memcpy(buffer, value.data(), copyLength);
            static_cast<char *>(buffer)[copyLength] = '\0';
            return true;
        }

        if (managedType == mochi::ManagedType::Bool)
        {
            const bool value = instanceIt->second.GetFieldValue<bool>(fieldName);
            *static_cast<bool *>(buffer) = value;
            return true;
        }

        if (IsReferenceType(*fieldType))
        {
            void *handle = nullptr;
            instanceIt->second.GetFieldValueRaw(fieldName, &handle);
            if (handle == nullptr)
            {
                *static_cast<uint64_t *>(buffer) = 0;
            }
            else
            {
                mochi::ManagedObject refObj;
                refObj.m_Handle = mochi::s_ManagedFunctions.CopyObjectFptr(handle);
                refObj.m_Type = fieldType;
                *static_cast<uint64_t *>(buffer) = refObj.GetPropertyValue<uint64_t>("ID");
            }
            return true;
        }

        instanceIt->second.GetFieldValueRaw(fieldName, buffer);
        return true;
    }

    bool ScriptHost::SetInstanceFieldValue(uint64_t instanceId, const std::string &fieldName, const void *buffer, int bufferSize)
    {
        if (!buffer || bufferSize <= 0)
        {
            return false;
        }

        auto instanceIt = m_InstanceMap.find(instanceId);
        if (instanceIt == m_InstanceMap.end())
        {
            return false;
        }

        auto *fieldType = FindFieldType(instanceIt->second, fieldName);
        if (!fieldType)
        {
            return false;
        }

        const auto managedType = fieldType->GetManagedType();
        if (managedType == mochi::ManagedType::String)
        {
            instanceIt->second.SetFieldValue(fieldName, std::string(static_cast<const char *>(buffer), bufferSize));
            return true;
        }

        if (managedType == mochi::ManagedType::Bool)
        {
            instanceIt->second.SetFieldValue(fieldName, *static_cast<const bool *>(buffer));
            return true;
        }

        if (IsReferenceType(*fieldType))
        {
            const uint64_t referenceId = *static_cast<const uint64_t *>(buffer);
            if (referenceId == 0)
            {
                void *nullHandle = nullptr;
                instanceIt->second.SetFieldValueRaw(fieldName, &nullHandle);
                return true;
            }

            mochi::ManagedObject *reference = EnsureReferenceInstance(referenceId, *fieldType);
            if (!reference)
            {
                return false;
            }

            void *handle = reference->m_Handle;
            instanceIt->second.SetFieldValueRaw(fieldName, &handle);
            return true;
        }

        instanceIt->second.SetFieldValueRaw(fieldName, const_cast<void *>(buffer));
        return true;
    }

    int ScriptHost::BindInstanceMethod(uint64_t instanceId, const std::string &methodName)
    {
        auto instanceIt = m_InstanceMap.find(instanceId);
        if (instanceIt == m_InstanceMap.end())
        {
            LOG_WARN("[Script Host] Cannot bind method '{}': instance {} not found", methodName, instanceId);
            return 0;
        }

        auto binding = CreateMethodBinding(MethodBinding::Kind::Instance, instanceId, const_cast<mochi::Type *>(&instanceIt->second.GetType()), methodName);
        if (!binding.has_value())
        {
            return 0;
        }

        const int methodId = m_NextMethodId++;
        m_MethodBindings.emplace(methodId, std::move(*binding));
        return methodId;
    }

    int ScriptHost::BindStaticMethod(const std::string &typeName, const std::string &methodName)
    {
        mochi::Type *type = FindType(typeName);
        if (!type)
        {
            LOG_ERROR("[Script Host] Failed to bind static method {}.{}: type not found", typeName, methodName);
            return 0;
        }

        auto binding = CreateMethodBinding(MethodBinding::Kind::Static, 0, type, methodName);
        if (!binding.has_value())
        {
            return 0;
        }

        const int methodId = m_NextMethodId++;
        m_MethodBindings.emplace(methodId, std::move(*binding));
        return methodId;
    }

    bool ScriptHost::Invoke(int methodId, const void *argsPtr, int argCount, void *returnPtr)
    {
        const auto bindingIt = m_MethodBindings.find(methodId);
        if (bindingIt == m_MethodBindings.end())
        {
            LOG_ERROR("[Script Host] Invalid method ID {}", methodId);
            return false;
        }

        const auto &parameterTypes = bindingIt->second.parameterTypes;
        if (argCount != static_cast<int>(parameterTypes.size()))
        {
            LOG_ERROR("[Script Host] Invoke argument mismatch for method {} (expected {}, got {})", methodId, parameterTypes.size(), argCount);
            return false;
        }

        auto parameters = argCount > 0 ? (const void **)(const_cast<void *>(argsPtr)) : nullptr;
        auto methodName = mochi::String::New(bindingIt->second.methodName);

        switch (bindingIt->second.kind)
        {
            case MethodBinding::Kind::Instance:
            {
                auto instanceIt = m_InstanceMap.find(bindingIt->second.instanceId);
                if (instanceIt == m_InstanceMap.end())
                {
                    mochi::String::Free(methodName);
                    return false;
                }

                if (returnPtr)
                {
                    mochi::s_ManagedFunctions.InvokeMethodRetFptr(instanceIt->second.m_Handle, methodName, parameters,
                        parameterTypes.data(), argCount, returnPtr);
                }
                else
                {
                    mochi::s_ManagedFunctions.InvokeMethodFptr(instanceIt->second.m_Handle, methodName, parameters,
                        parameterTypes.data(), argCount);
                }
                break;
            }
            case MethodBinding::Kind::Static:
            {
                if (!bindingIt->second.type)
                {
                    mochi::String::Free(methodName);
                    return false;
                }

                if (returnPtr)
                {
                    mochi::s_ManagedFunctions.InvokeStaticMethodRetFptr(
                        bindingIt->second.type->GetTypeId(),
                        methodName,
                        parameters,
                        parameterTypes.data(),
                        argCount,
                        returnPtr);
                }
                else
                {
                    mochi::s_ManagedFunctions.InvokeStaticMethodFptr(
                        bindingIt->second.type->GetTypeId(),
                        methodName,
                        parameters,
                        parameterTypes.data(),
                        argCount);
                }
                break;
            }
        }

        mochi::String::Free(methodName);
        return true;
    }

    std::string ScriptHost::GetDerivedTypes(const ignite::Path &assemblyPath, const std::string &baseType)
    {
        mochi::Type *bridgeType = GetReflectionBridgeType();
        if (!bridgeType)
        {
            return {};
        }

        return InvokeStaticStringMethod(*bridgeType, "GetDerivedTypes", assemblyPath.stem().string(), baseType);
    }

    std::string ScriptHost::GetCreateAssetMenuData(const ignite::Path &assemblyPath, const std::string &baseType)
    {
        mochi::Type *bridgeType = GetReflectionBridgeType();
        if (!bridgeType)
        {
            return {};
        }

        return InvokeStaticStringMethod(*bridgeType, "GetCreateAssetMenuData", assemblyPath.stem().string(), baseType);
    }
}
