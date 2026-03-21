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

#include "script_instance.hpp"
#include "script_engine.hpp"
#include "ignite/scene/entity.hpp"
#include "script_class.hpp"

namespace ignite
{
    ScriptInstance::ScriptInstance(Ref<ScriptClass> scriptClass, Entity entity)
        : m_ScriptClass(scriptClass)
    {
        m_ScriptHost = ScriptEngine::GetInstance()->GetScriptHost();
        LOG_ASSERT(m_ScriptHost, "[Script Instance] ScriptHost is null");

        // Use entity UUID as GUID key for the managed instance
        m_InstanceGuid = std::to_string(static_cast<uint64_t>(entity.GetUUID()));

        // Create managed instance
        if (!m_ScriptHost->CreateInstance(m_InstanceGuid, scriptClass->GetFullName()))
        {
            LOG_ERROR("[Script Instance] Failed to create managed instance {}", m_InstanceGuid);
            return;
        }

        // Bind lifecycle methods
        m_OnCreateMethodId = scriptClass->BindInstanceMethod(m_InstanceGuid, "OnCreate", ScriptMethodSignature::Void);
        m_OnUpdateMethodId = scriptClass->BindInstanceMethod(m_InstanceGuid, "OnUpdate", ScriptMethodSignature::Void_Float);

        // Set Entity ID on managed instance if available
        const int setIdMethod = scriptClass->BindInstanceMethod(m_InstanceGuid, "SetID", ScriptMethodSignature::Void_UInt64);
        if (setIdMethod)
        {
            const uint64_t entityId = static_cast<uint64_t>(entity.GetUUID());
            void *args[] = { const_cast<uint64_t *>(&entityId) };
            m_ScriptHost->Invoke(setIdMethod, args, 1, nullptr);
        }
    }

    ScriptInstance::~ScriptInstance()
    {
        if (m_ScriptHost && !m_InstanceGuid.empty())
        {
            m_ScriptHost->DestroyInstance(m_InstanceGuid);
        }
    }

    void ScriptInstance::InvokeOnCreate()
    {
        if (m_OnCreateMethodId)
        {
            m_ScriptHost->Invoke(m_OnCreateMethodId, nullptr, 0, nullptr);
        }
    }

    void ScriptInstance::InvokeOnUpdate(float time)
    {
        if (m_OnUpdateMethodId)
        {
            void *args[] = { &time };
            m_ScriptHost->Invoke(m_OnUpdateMethodId, args, 1, nullptr);
        }
    }

    bool ScriptInstance::GetFieldValueInternal(const std::string &name, void *buffer)
    {
        (void)name;
        (void)buffer;
        LOG_WARN("[Script Instance] GetFieldValueInternal not implemented for HostFXR yet");
        return false;
    }

    bool ScriptInstance::SetFieldValueInternal(const std::string &name, const void *value)
    {
        (void)name;
        (void)value;
        LOG_WARN("[Script Instance] SetFieldValueInternal not implemented for HostFXR yet");
        return false;
    }
}
