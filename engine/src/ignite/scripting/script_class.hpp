// Copyright (c) 2026 Evangelion Manuhutu

#ifndef SCRIPT_CLASS_HPP
#define SCRIPT_CLASS_HPP

#include "script_field.hpp"
#include "script_host.hpp"

#include <string>
#include <unordered_map>

namespace ignite
{
    class ScriptInstance;
    class ScriptHost;

    class ScriptClass
    {
    public:
        ScriptClass() = default;
        ScriptClass(const std::string &classNamespace, const std::string &className, bool core = false);
        ScriptClass(const std::string &classNamespace, const std::string &className, const std::string &assemblyName);

        // Bind methods (instance/static) using HostFXR
        int BindInstanceMethod(uint64_t instanceId, const std::string &methodName, ScriptMethodSig signature);
        int BindStaticMethod(const std::string &methodName, ScriptMethodSig signature);

        const std::string &GetNamespace() const { return m_ClassNamespace; }
        const std::string &GetClassName() const { return m_ClassName; }
        const std::string &GetFullName() const { return m_FullName; }
        const std::string &GetAssemblyName() const { return m_AssemblyName; }
        bool IsCore() const { return m_IsCore; }

        void InsertField(const std::string &fieldName, const ScriptField &field);
        std::unordered_map<std::string, ScriptField> GetFields() const { return m_Fields; }

    private:
        std::string m_ClassName;
        std::string m_ClassNamespace;
        std::string m_FullName;
        std::string m_AssemblyName;
        bool m_IsCore = false;
        ScriptHost *m_ScriptHost = nullptr;
        std::unordered_map<std::string, ScriptField> m_Fields;
    };
}

#endif