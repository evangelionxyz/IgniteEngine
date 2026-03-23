// Copyright (c) 2026 Evangelion Manuhutu

#include "script_class.hpp"
#include "script_engine.hpp"

namespace ignite
{
    ScriptClass::ScriptClass(const std::string &classNamespace, const std::string &className, bool core)
        : m_ClassNamespace(classNamespace), m_ClassName(className), m_IsCore(core)
    {
        m_FullName = m_ClassName;
        if (!m_ClassNamespace.empty())
        {
            m_FullName = m_ClassNamespace + "." + m_ClassName;
        }

        m_ScriptHost = ScriptEngine::GetInstance()->GetScriptHost();
    }

    ScriptClass::ScriptClass(const std::string &classNamespace, const std::string &className, const std::string &assemblyName)
        : m_ClassNamespace(classNamespace), m_ClassName(className), m_AssemblyName(assemblyName), m_IsCore(false)
    {
        m_FullName = m_ClassName;
        if (!m_ClassNamespace.empty())
        {
            m_FullName = m_ClassNamespace + "." + m_ClassName;
        }

        m_ScriptHost = ScriptEngine::GetInstance()->GetScriptHost();
    }

    int ScriptClass::BindInstanceMethod(uint64_t instanceId, const std::string &methodName, ScriptMethodSig signature)
    {
        if (!m_ScriptHost)
        {
            return 0;
        }

        return m_ScriptHost->BindInstanceMethod(instanceId, methodName, signature);
    }

    int ScriptClass::BindStaticMethod(const std::string &methodName, ScriptMethodSig signature)
    {
        if (!m_ScriptHost)
        {
            return 0;
        }

        return m_ScriptHost->BindStaticMethod(m_FullName, methodName, signature);
    }

    void ScriptClass::InsertField(const std::string &fieldName, const ScriptField &field)
    {
        m_Fields[fieldName] = field;
    }

}
