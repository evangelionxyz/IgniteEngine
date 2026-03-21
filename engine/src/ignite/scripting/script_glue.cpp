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

#include "script_glue.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/scene/scene_manager.hpp"
#include "ignite/scripting/script_engine.hpp"

#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace ignite
{
    namespace
    {
        static Scene *GetSceneContext()
        {
            if (auto *engine = ScriptEngine::GetInstance())
            {
                return engine->GetSceneContext();
            }

            return nullptr;
        }

        static Entity GetEntityByID(uint64_t entityID)
        {
            Scene *scene = GetSceneContext();
            if (!scene)
            {
                return {};
            }

            Entity entity = SceneManager::GetEntity(scene, UUID(entityID));
            if (entity.IsValid())
            {
                return entity;
            }

            auto view = scene->registry->view<IDComponent>();
            for (entt::entity e : view)
            {
                const IDComponent &id = view.get<IDComponent>(e);
                if (static_cast<uint64_t>(id.uuid) == entityID)
                {
                    return Entity { e, scene };
                }
            }

            return {};
        }

        static ScriptVec3 ToScriptVec3(const glm::vec3 &value)
        {
            return { value.x, value.y, value.z };
        }

        static glm::vec3 ToGlmVec3(ScriptVec3 value)
        {
            return { value.x, value.y, value.z };
        }

        static ScriptQuat ToScriptQuat(const glm::quat &value)
        {
            return { value.x, value.y, value.z, value.w };
        }

        static glm::quat ToGlmQuat(ScriptQuat value)
        {
            return { value.w, value.x, value.y, value.z };
        }

        enum class ManagedComponentType
        {
            Unknown,
            Transform,
            Script,
        };

        static ManagedComponentType ResolveManagedComponentType(const char *componentTypeName)
        {
            if (!componentTypeName)
            {
                return ManagedComponentType::Unknown;
            }

            std::string typeName(componentTypeName);

            if (const size_t comma = typeName.find(','); comma != std::string::npos)
            {
                typeName = typeName.substr(0, comma);
            }

            typeName.erase(typeName.begin(), std::find_if(typeName.begin(), typeName.end(), [](unsigned char ch) { return !std::isspace(ch); }));
            typeName.erase(std::find_if(typeName.rbegin(), typeName.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), typeName.end());

            const size_t plus = typeName.find_last_of('+');
            if (plus != std::string::npos)
            {
                typeName = typeName.substr(plus + 1);
            }

            const size_t dot = typeName.find_last_of('.');
            if (dot != std::string::npos)
            {
                typeName = typeName.substr(dot + 1);
            }

            if (const size_t genericMarker = typeName.find('`'); genericMarker != std::string::npos)
            {
                typeName = typeName.substr(0, genericMarker);
            }

            if (typeName == "Transform" || typeName == "TransformComponent")
            {
                return ManagedComponentType::Transform;
            }

            if (typeName == "ScriptComponent" || typeName == "Script")
            {
                return ManagedComponentType::Script;
            }

            return ManagedComponentType::Unknown;
        }

        static void Debug_Log(const char *message)
        {
            LOG_INFO("[C#] {}", message ? message : "");
        }

        static bool Entity_HasComponent(uint64_t entityID, const char *componentTypeName)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid())
            {
                return false;
            }

            switch (ResolveManagedComponentType(componentTypeName))
            {
            case ManagedComponentType::Transform:
                return entity.HasComponent<TransformComponent>();
            case ManagedComponentType::Script:
                return entity.HasComponent<ScriptComponent>();
            default:
                return false;
            }
        }

        static void Entity_AddComponent(uint64_t entityID, const char *componentTypeName)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid())
            {
                return;
            }

            switch (ResolveManagedComponentType(componentTypeName))
            {
            case ManagedComponentType::Transform:
                entity.AddOrReplaceComponent<TransformComponent>();
                break;
            case ManagedComponentType::Script:
                entity.AddOrReplaceComponent<ScriptComponent>();
                break;
            default:
                break;
            }
        }

        static uint64_t Entity_FindEntityByName(const char *name)
        {
            Scene *scene = GetSceneContext();
            if (!scene || !name)
            {
                return 0;
            }

            Entity entity = SceneManager::GetEntity(scene, std::string(name));
            return entity.IsValid() ? static_cast<uint64_t>(entity.GetUUID()) : 0;
        }

        static uint64_t Entity_Instantiate(uint64_t entityID, ScriptVec3 value)
        {
            Scene *scene = GetSceneContext();
            if (!scene)
            {
                return 0;
            }

            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid())
            {
                return 0;
            }

            Entity copyEntity = SceneManager::DuplicateEntity(scene, entity);
            if (!copyEntity.IsValid())
            {
                return 0;
            }

            copyEntity.GetComponent<TransformComponent>().translation = ToGlmVec3(value);
            return static_cast<uint64_t>(copyEntity.GetUUID());
        }

        static void Entity_Destroy(uint64_t entityID)
        {
            Scene *scene = GetSceneContext();
            if (!scene)
            {
                return;
            }

            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid())
            {
                return;
            }

            SceneManager::DestroyEntity(scene, entity);
        }

        static void Entity_SetVisibility(uint64_t entityID, bool value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TransformComponent>())
            {
                return;
            }

            entity.GetComponent<TransformComponent>().visible = value;
        }

        static void Entity_GetVisibility(uint64_t entityID, bool *result)
        {
            if (!result)
            {
                return;
            }

            *result = false;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TransformComponent>())
            {
                return;
            }

            *result = entity.GetComponent<TransformComponent>().visible;
        }

        static void TransformComponent_GetForward(uint64_t entityID, ScriptVec3 *result)
        {
            if (!result)
            {
                return;
            }

            *result = {};
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TransformComponent>())
            {
                return;
            }

            const auto &transform = entity.GetComponent<TransformComponent>();
            *result = ToScriptVec3(transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f));
        }

        static void TransformComponent_SetForward(uint64_t entityID, ScriptVec3 value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TransformComponent>())
            {
                return;
            }

            glm::vec3 forward = glm::normalize(ToGlmVec3(value));
            if (glm::length2(forward) <= 0.0f)
            {
                return;
            }

            auto &transform = entity.GetComponent<TransformComponent>();
            const glm::quat rotation = glm::quatLookAtRH(forward, glm::vec3(0.0f, 1.0f, 0.0f));
            transform.localRotation = rotation;
            transform.rotation = rotation;
            transform.dirty = true;
        }

        static void TransformComponent_GetRight(uint64_t entityID, ScriptVec3 *result)
        {
            if (!result)
            {
                return;
            }

            *result = {};
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TransformComponent>())
            {
                return;
            }

            const auto &transform = entity.GetComponent<TransformComponent>();
            *result = ToScriptVec3(transform.rotation * glm::vec3(1.0f, 0.0f, 0.0f));
        }

        static void TransformComponent_SetRight(uint64_t entityID, ScriptVec3 value)
        {
            glm::vec3 right = glm::normalize(ToGlmVec3(value));
            if (glm::length2(right) <= 0.0f)
            {
                return;
            }

            glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
            glm::vec3 forward = glm::normalize(glm::cross(worldUp, right));
            if (glm::length2(forward) <= 0.0f)
            {
                return;
            }

            TransformComponent_SetForward(entityID, ToScriptVec3(forward));
        }

        static void TransformComponent_GetUp(uint64_t entityID, ScriptVec3 *result)
        {
            if (!result)
            {
                return;
            }

            *result = {};
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TransformComponent>())
            {
                return;
            }

            const auto &transform = entity.GetComponent<TransformComponent>();
            *result = ToScriptVec3(transform.rotation * glm::vec3(0.0f, 1.0f, 0.0f));
        }

        static void TransformComponent_SetUp(uint64_t entityID, ScriptVec3 value)
        {
            glm::vec3 up = glm::normalize(ToGlmVec3(value));
            if (glm::length2(up) <= 0.0f)
            {
                return;
            }

            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TransformComponent>())
            {
                return;
            }

            auto &transform = entity.GetComponent<TransformComponent>();
            glm::vec3 right = glm::normalize(transform.rotation * glm::vec3(1.0f, 0.0f, 0.0f));
            glm::vec3 forward = glm::normalize(glm::cross(up, right));
            if (glm::length2(forward) <= 0.0f)
            {
                return;
            }

            const glm::quat rotation = glm::quatLookAtRH(forward, up);
            transform.localRotation = rotation;
            transform.rotation = rotation;
            transform.dirty = true;
        }

        static void TransformComponent_GetTranslation(uint64_t entityID, ScriptVec3 *result)
        {
            if (!result)
            {
                return;
            }

            *result = {};
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TransformComponent>())
            {
                return;
            }

            *result = ToScriptVec3(entity.GetComponent<TransformComponent>().translation);
        }

        static void TransformComponent_SetTranslation(uint64_t entityID, ScriptVec3 value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TransformComponent>())
            {
                return;
            }

            auto &transform = entity.GetComponent<TransformComponent>();
            const glm::vec3 translation = ToGlmVec3(value);
            transform.localTranslation = translation;
            transform.translation = translation;
            transform.dirty = true;
        }

        static void TransformComponent_GetRotation(uint64_t entityID, ScriptQuat *result)
        {
            if (!result)
            {
                return;
            }

            *result = {};
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TransformComponent>())
            {
                return;
            }

            *result = ToScriptQuat(entity.GetComponent<TransformComponent>().rotation);
        }

        static void TransformComponent_SetRotation(uint64_t entityID, ScriptQuat value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TransformComponent>())
            {
                return;
            }

            auto &transform = entity.GetComponent<TransformComponent>();
            const glm::quat rotation = ToGlmQuat(value);
            transform.localRotation = rotation;
            transform.rotation = rotation;
            transform.dirty = true;
        }

        static void TransformComponent_GetEulerAngles(uint64_t entityID, ScriptVec3 *result)
        {
            if (!result)
            {
                return;
            }

            *result = {};
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TransformComponent>())
            {
                return;
            }

            *result = ToScriptVec3(glm::eulerAngles(entity.GetComponent<TransformComponent>().rotation));
        }

        static void TransformComponent_SetEulerAngles(uint64_t entityID, ScriptVec3 value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TransformComponent>())
            {
                return;
            }

            auto &transform = entity.GetComponent<TransformComponent>();
            const glm::quat rotation = glm::quat(ToGlmVec3(value));
            transform.localRotation = rotation;
            transform.rotation = rotation;
            transform.dirty = true;
        }

        static void TransformComponent_GetScale(uint64_t entityID, ScriptVec3 *result)
        {
            if (!result)
            {
                return;
            }

            *result = {};
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TransformComponent>())
            {
                return;
            }

            *result = ToScriptVec3(entity.GetComponent<TransformComponent>().scale);
        }

        static void TransformComponent_SetScale(uint64_t entityID, ScriptVec3 value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TransformComponent>())
            {
                return;
            }

            auto &transform = entity.GetComponent<TransformComponent>();
            const glm::vec3 scale = ToGlmVec3(value);
            transform.localScale = scale;
            transform.scale = scale;
            transform.dirty = true;
        }

        static const ScriptGlueAPI s_API =
        {
            &Debug_Log,
            &Entity_HasComponent,
            &Entity_AddComponent,
            &Entity_FindEntityByName,
            &Entity_Instantiate,
            &Entity_Destroy,
            &Entity_SetVisibility,
            &Entity_GetVisibility,
            &TransformComponent_GetForward,
            &TransformComponent_SetForward,
            &TransformComponent_GetRight,
            &TransformComponent_SetRight,
            &TransformComponent_GetUp,
            &TransformComponent_SetUp,
            &TransformComponent_GetTranslation,
            &TransformComponent_SetTranslation,
            &TransformComponent_GetRotation,
            &TransformComponent_SetRotation,
            &TransformComponent_GetEulerAngles,
            &TransformComponent_SetEulerAngles,
            &TransformComponent_GetScale,
            &TransformComponent_SetScale,
        };
    }

    const ScriptGlueAPI *ScriptGlue::GetAPI()
    {
        return &s_API;
    }

    void ScriptGlue::RegisterComponents()
    {
        LOG_INFO("[ScriptGlue] Component bridge initialized (HostFXR)");
    }

    void ScriptGlue::RegisterFunctions()
    {
        LOG_INFO("[ScriptGlue] Function bridge initialized (HostFXR)");
    }
}
