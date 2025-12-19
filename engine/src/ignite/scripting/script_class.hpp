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

#pragma once

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
        int BindInstanceMethod(const std::string &instanceGuid, const std::string &methodName, ScriptMethodSignature signature);
        int BindStaticMethod(const std::string &methodName, ScriptMethodSignature signature);

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
