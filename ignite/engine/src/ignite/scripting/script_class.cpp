// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "script_class.hpp"
#include "script_engine.hpp"

namespace ignite
{
    ScriptClass::ScriptClass(const std::string &classNamespace, const std::string &className, const std::string &assemblyName)
        : m_ClassNamespace(classNamespace), m_ClassName(className), m_AssemblyName(assemblyName)
    {
        m_FullName = m_ClassName;
        if (!m_ClassNamespace.empty())
        {
            m_FullName = m_ClassNamespace + "." + m_ClassName;
        }

        m_ScriptHost = ScriptEngine::GetInstance()->GetScriptHost();
    }

    int ScriptClass::BindInstanceMethod(uint64_t instanceId, const std::string &methodName)
    {
        if (!m_ScriptHost)
        {
            return 0;
        }

        return m_ScriptHost->BindInstanceMethod(instanceId, methodName);
    }

    int ScriptClass::BindStaticMethod(const std::string &methodName)
    {
        if (!m_ScriptHost)
        {
            return 0;
        }

        return m_ScriptHost->BindStaticMethod(m_FullName, methodName);
    }

    void ScriptClass::InsertField(const std::string &fieldName, const ScriptField &field)
    {
        if (m_Fields.find(fieldName) == m_Fields.end())
        {
            m_OrderedFieldNames.push_back(fieldName);
        }
        m_Fields[fieldName] = field;
    }

	void ScriptClass::InsertInstanceFields(uint64_t instanceId, const std::unordered_map<std::string, ScriptInstanceField> &instanceFields)
	{
        m_InstancesFields[instanceId] = instanceFields;
    }

    std::unordered_map<std::string, ScriptInstanceField> *ScriptClass::GetInstanceFieldsById(uint64_t instanceId)
    {
        auto it = m_InstancesFields.find(instanceId);
        if (it != m_InstancesFields.end())
            return &it->second;
        return nullptr;
	}

}
