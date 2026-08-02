// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_SCRIPT_HOST_HPP
#define IGN_SCRIPT_HOST_HPP

#include "MochiSharp/HostInstance.hpp"
#include "MochiSharp/ManagedObject.hpp"
#include "MochiSharp/Assembly.hpp"

#include "ignite/core/base.hpp"
#include "ignite/core/types.hpp"
#include "ignite/core/path.hpp"

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace ignite
{
    class IGN_API ScriptHost
    {
    public:
        ScriptHost();
        ~ScriptHost();

        // Initialize the .NET runtime with the specified config
        bool Init(const ignite::Path &configPath);

        // Load a .NET assembly (core or app)
        bool LoadAssembly(const ignite::Path &assemblyPath);
        bool ResetLoadContext();

        // Register method signatures for script methods
        void RegisterSignatures();

        // Initialize C# InternalCalls bridge with native callbacks
        bool InitializeCoreInternalCalls();
        bool InitializeComponentInternalCalls();

        // Create a script instance
        bool CreateInstance(uint64_t instanceId, const std::string &typeName);
        void DestroyInstance(uint64_t instanceId);

        std::string GetInstanceFields(uint64_t instanceId);
        std::string GetTypeFields(const std::string &typeName);
        bool ConfigureSerialization(const std::string &serializeFieldAttributeTypeName, const std::string &typeName);
        bool GetInstanceFieldValue(uint64_t instanceId, const std::string &fieldName, void *buffer, int bufferSize);
        bool SetInstanceFieldValue(uint64_t instanceId, const std::string &fieldName, const void *buffer, int bufferSize);

        // List<Entity> hot-reload helpers: capture IDs before ALC unload, restore after reload
        std::string GetEntityListFieldIds(uint64_t instanceId, const std::string &fieldName);
        bool SetEntityListField(uint64_t instanceId, const std::string &fieldName, const std::string &pipeSeparatedIds);

        // Bind an instance method and return a method handle
        int BindInstanceMethod(uint64_t instanceId, const std::string &methodName);

        // Bind a static method and return a method handle
        int BindStaticMethod(const std::string &typeName, const std::string &methodName);

        // Invoke a method with arguments
        bool Invoke(int methodId, const void *argsPtr, int argCount, void *returnPtr);

        // Get all non-abstract classes derived from baseType in an assembly
        std::string GetDerivedTypes(const ignite::Path &assemblyPath, const std::string &baseType);
        std::string GetCreateAssetMenuData(const ignite::Path &assemblyPath, const std::string &baseType);
        std::string GetFieldUIAttribute(const std::string &classFullName, const std::string &attributeTypeName);

        // Check if initialized
        bool IsInitialized() const { return m_Initialized; }

        // Get the underlying MochiSharp host
        mochi::HostInstance *GetHost() { return &m_Host; }

    private:
        struct MethodBinding
        {
            enum class Kind
            {
                Instance,
                Static,
            };

            Kind kind = Kind::Instance;
            uint64_t instanceId = 0;
            mochi::Type *type = nullptr;
            std::string methodName;
            std::vector<mochi::ManagedType> parameterTypes;
        };

        mochi::HostInstance m_Host;
        Scope<mochi::AssemblyLoadContext> m_LoadContext;
        mochi::ManagedAssembly *m_CoreAssembly = nullptr;
        mochi::ManagedAssembly *m_AppAssembly = nullptr;
        ignite::Path m_BaseDir;
        std::unordered_map<uint64_t, mochi::ManagedObject> m_InstanceMap;
        std::unordered_map<std::string, mochi::Type *> m_TypeMap;
        std::unordered_map<int, MethodBinding> m_MethodBindings;
        std::unordered_set<std::string> m_ReferenceTypeNames;
        std::string m_SerializeFieldAttributeTypeName;
        bool m_Initialized = false;
        int m_NextMethodId = 1;
        int m_ReloadCounter = 0;

    private:
        mochi::ManagedAssembly *LoadAssemblyInternal(const ignite::Path &assemblyPath, mochi::ManagedAssembly *&targetSlot);
        mochi::Type *FindType(const std::string &typeName) const;
        std::optional<MethodBinding> CreateMethodBinding(MethodBinding::Kind kind, uint64_t instanceId, mochi::Type *type, const std::string &methodName) const;
        mochi::Type *FindFieldType(mochi::ManagedObject &instance, const std::string &fieldName);
        bool IsReferenceType(const mochi::Type &type) const;
        mochi::ManagedObject *EnsureReferenceInstance(uint64_t instanceId, const mochi::Type &type);
        mochi::Type *GetReflectionBridgeType() const;
    };
}

#endif
