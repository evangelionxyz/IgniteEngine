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
#include "script_class.hpp"

#include "ignite/core/types.hpp"

extern "C"
{
    typedef struct _MonoMethod MonoMethod;
    typedef struct _MonoObject MonoObject;
    typedef struct _MonoClassField MonoClassField;
}

namespace ignite
{
    class Entity;
    class ScriptClass;

    // script field + data storage
    struct ScriptFieldInstance
    {
        ScriptField Field;
        ScriptFieldInstance()
        {
            memset(m_Buffer, 0, sizeof(m_Buffer));
        }

        template<typename T>
        T GetValue()
        {
            static_assert(sizeof(T) <= 16, "Type too large!");
            return *(T *)m_Buffer;
        }

        template<typename T>
        void SetValue(T value)
        {
            static_assert(sizeof(T) <= 16, "Type too large!");
            memcpy(m_Buffer, &value, sizeof(T));
        }

    private:
        char m_Buffer[16];

        friend class ScriptEngine;
        friend class ScriptInstance;
    };

    class ScriptInstance
    {
    public:
        ScriptInstance(Ref<ScriptClass> scriptClass, Entity entity);

        void InvokeOnCreate();
        void InvokeOnUpdate(float time);

        Ref<ScriptClass> GetScriptClass() { return m_ScriptClass; }
        MonoObject *GetMonoObject() { return m_Instance; }

        template<typename T>
        T GetFieldValue(const std::string &name)
        {
            static_assert(sizeof(T) <= 24, "Type too large!");

            bool success = GetFieldValueInternal(name, s_FieldValueBuffer);
            if (!success)
                return T();

            return *(T *)s_FieldValueBuffer;
        }

        template<typename T>
        void SetFieldValue(const std::string &name, const T &value)
        {
            static_assert(sizeof(T) <= 24, "Type too large!");
            SetFieldValueInternal(name, &value);
        }

    private:
        bool GetFieldValueInternal(const std::string &name, void *buffer);
        bool SetFieldValueInternal(const std::string &name, const void *value);

    private:
        Ref<ScriptClass> m_ScriptClass;

        MonoObject *m_Instance = nullptr;
        MonoMethod *m_OnConstructor = nullptr;
        MonoMethod *m_OnCreateMethod = nullptr;
        MonoMethod *m_OnUpdateMethod = nullptr;

        inline static char s_FieldValueBuffer[24];
        friend class ScriptEngine;
    };
}
