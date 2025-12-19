/* MIT License
* 
* Copyright (c) 2025 Evangelion Manuhutu | IGNITE STUDIO
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

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

    int ScriptClass::BindInstanceMethod(const std::string &instanceGuid, const std::string &methodName, ScriptMethodSignature signature)
    {
        if (!m_ScriptHost)
        {
            return 0;
        }

        return m_ScriptHost->BindInstanceMethod(instanceGuid, methodName, signature);
    }

    int ScriptClass::BindStaticMethod(const std::string &methodName, ScriptMethodSignature signature)
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
