// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_SCRIPT_CLASS_HPP
#define IGN_SCRIPT_CLASS_HPP

#include "script_field.hpp"
#include "script_host.hpp"

#include "ignite/core/base.hpp"
#include <string>
#include <unordered_map>

namespace ignite
{
    struct ScriptField;
    class ScriptHost;

    class IGN_API ScriptClass
    {
    public:
        ScriptClass() = default;
        ScriptClass(const std::string &classNamespace, const std::string &className, const std::string &assemblyName);

        // Bind methods (instance/static) using HostFXR
        int BindInstanceMethod(uint64_t instanceId, const std::string &methodName) const;
        int BindStaticMethod(const std::string &methodName) const;

        const std::string &GetNamespace() const { return m_ClassNamespace; }
        const std::string &GetClassName() const { return m_ClassName; }
        const std::string &GetFullName() const { return m_FullName; }
        const std::string &GetAssemblyName() const { return m_AssemblyName; }

        // Class fields
        void InsertField(const std::string &fieldName, const ScriptField &field);
        std::unordered_map<std::string, ScriptField> &GetFields() { return m_Fields; }
        const std::vector<std::string>& GetOrderedFieldNames() const { return m_OrderedFieldNames; }

        std::unordered_map<std::string, ScriptInstanceField> &GetDefaultFields() { return m_DefaultFields; }
        const std::unordered_map<std::string, ScriptInstanceField> &GetDefaultFields() const { return m_DefaultFields; }

        // Script instance fields
        void InsertInstanceFields(uint64_t instanceId, const std::unordered_map<std::string, ScriptInstanceField> &instanceField);
        std::unordered_map<std::string, ScriptInstanceField> *GetInstanceFieldsById(uint64_t instanceId);
		std::unordered_map<uint64_t, std::unordered_map<std::string, ScriptInstanceField>> &GetInstancesFields() { return m_InstancesFields; }

    private:
        std::string m_ClassName;
        std::string m_ClassNamespace;
        std::string m_FullName;
        std::string m_AssemblyName;
        ScriptHost *m_ScriptHost = nullptr;

        // Class fields
        std::unordered_map<std::string, ScriptField> m_Fields;
        std::unordered_map<std::string, ScriptInstanceField> m_DefaultFields;
        std::vector<std::string> m_OrderedFieldNames;

        // Script instance fields
        std::unordered_map<uint64_t, std::unordered_map<std::string, ScriptInstanceField>> m_InstancesFields;
    };
}

#endif
