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

#include <cstdint>

namespace ignite
{
    struct ScriptVec3
    {
        float x, y, z;
    };

    struct ScriptQuat
    {
        float x, y, z, w;
    };

    struct ScriptGlueAPI
    {
        void (*Debug_Log)(const char *message);

        bool (*Entity_HasComponent)(uint64_t entityID, const char *componentTypeName);
        void (*Entity_AddComponent)(uint64_t entityID, const char *componentTypeName);
        uint64_t (*Entity_FindEntityByName)(const char *name);
        uint64_t (*Entity_Instantiate)(uint64_t entityID, ScriptVec3 value);
        void (*Entity_Destroy)(uint64_t entityID);
        void (*Entity_SetVisibility)(uint64_t entityID, bool value);
        void (*Entity_GetVisibility)(uint64_t entityID, bool *result);

        void (*TransformComponent_GetForward)(uint64_t entityID, ScriptVec3 *result);
        void (*TransformComponent_SetForward)(uint64_t entityID, ScriptVec3 value);
        void (*TransformComponent_GetRight)(uint64_t entityID, ScriptVec3 *result);
        void (*TransformComponent_SetRight)(uint64_t entityID, ScriptVec3 value);
        void (*TransformComponent_GetUp)(uint64_t entityID, ScriptVec3 *result);
        void (*TransformComponent_SetUp)(uint64_t entityID, ScriptVec3 value);
        void (*TransformComponent_GetTranslation)(uint64_t entityID, ScriptVec3 *result);
        void (*TransformComponent_SetTranslation)(uint64_t entityID, ScriptVec3 value);
        void (*TransformComponent_GetRotation)(uint64_t entityID, ScriptQuat *result);
        void (*TransformComponent_SetRotation)(uint64_t entityID, ScriptQuat value);
        void (*TransformComponent_GetEulerAngles)(uint64_t entityID, ScriptVec3 *result);
        void (*TransformComponent_SetEulerAngles)(uint64_t entityID, ScriptVec3 value);
        void (*TransformComponent_GetScale)(uint64_t entityID, ScriptVec3 *result);
        void (*TransformComponent_SetScale)(uint64_t entityID, ScriptVec3 value);
    };

    class ScriptGlue
    {
    public:
        static void RegisterComponents();
        static void RegisterFunctions();
        static const ScriptGlueAPI *GetAPI();
    };
}
