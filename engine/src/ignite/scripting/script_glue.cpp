// Copyright (c) 2026 Evangelion Manuhutu

#include "script_glue.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/scene/component_group.hpp"
#include "ignite/scene/scene_manager.hpp"
#include "ignite/scripting/script_engine.hpp"

#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <cctype>
#include <functional>
#include <string>
#include <string_view>
#include <typeinfo>
#include <unordered_map>

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

        static std::unordered_map<std::string, std::function<bool(Entity)>> s_EntityHasComponentFuncs;
        static std::unordered_map<std::string, std::function<void(Entity)>> s_EntityAddComponentFuncs;

        static std::string TrimString(std::string value)
        {
            value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char ch) { return !std::isspace(ch); }));
            value.erase(std::find_if(value.rbegin(), value.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), value.end());
            return value;
        }

        static std::string NormalizeManagedTypeName(const char *componentTypeName)
        {
            if (!componentTypeName)
            {
                return {};
            }

            std::string typeName(componentTypeName);

            if (const size_t comma = typeName.find(','); comma != std::string::npos)
            {
                typeName = typeName.substr(0, comma);
            }

            typeName = TrimString(typeName);

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

            return typeName;
        }

        static std::string GetNativeComponentName(std::string typeName)
        {
            const size_t separator = typeName.find_last_of(':');
            if (separator != std::string::npos)
            {
                typeName = typeName.substr(separator + 1);
            }

            typeName = TrimString(typeName);

            constexpr std::string_view classPrefix = "class ";
            if (typeName.rfind(classPrefix, 0) == 0)
            {
                typeName = typeName.substr(classPrefix.size());
            }

            constexpr std::string_view structPrefix = "struct ";
            if (typeName.rfind(structPrefix, 0) == 0)
            {
                typeName = typeName.substr(structPrefix.size());
            }

            return TrimString(typeName);
        }

        static std::string GetManagedComponentName(std::string nativeComponentName)
        {
            constexpr std::string_view suffix = "Component";
            if (nativeComponentName.size() > suffix.size() && nativeComponentName.ends_with(suffix))
            {
                nativeComponentName = nativeComponentName.substr(0, nativeComponentName.size() - suffix.size());
            }

            return nativeComponentName;
        }

        template<typename... Component>
        static void RegisterComponent()
        {
            (([]()
            {
                std::string nativeTypeName = GetNativeComponentName(typeid(Component).name());
                std::string managedTypeName = GetManagedComponentName(nativeTypeName);

                const auto hasComponentFunc = [](Entity entity) { return entity.HasComponent<Component>(); };
                const auto addComponentFunc = [](Entity entity) { entity.AddOrReplaceComponent<Component>(); };

                s_EntityHasComponentFuncs[managedTypeName] = hasComponentFunc;
                s_EntityAddComponentFuncs[managedTypeName] = addComponentFunc;

                s_EntityHasComponentFuncs[nativeTypeName] = hasComponentFunc;
                s_EntityAddComponentFuncs[nativeTypeName] = addComponentFunc;
            }()), ...);
        }

        template<typename... Component>
        static void RegisterComponent(ComponentGroup<Component...>)
        {
            RegisterComponent<Component...>();
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

            const std::string typeName = NormalizeManagedTypeName(componentTypeName);
            if (typeName.empty())
            {
                return false;
            }

            const auto hasComponentIt = s_EntityHasComponentFuncs.find(typeName);
            if (hasComponentIt == s_EntityHasComponentFuncs.end())
            {
                return false;
            }

            return hasComponentIt->second(entity);
        }

        static void Entity_AddComponent(uint64_t entityID, const char *componentTypeName)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid())
            {
                return;
            }

            const std::string typeName = NormalizeManagedTypeName(componentTypeName);
            if (typeName.empty())
            {
                return;
            }

            const auto addComponentIt = s_EntityAddComponentFuncs.find(typeName);
            if (addComponentIt == s_EntityAddComponentFuncs.end())
            {
                return;
            }

            addComponentIt->second(entity);
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

		static uint64_t Entity_Instantiate(uint64_t entityID, glm::vec3 value)
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

            copyEntity.GetComponent<TransformComponent>().translation = value;
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

        static void TransformComponent_GetForward(uint64_t entityID, glm::vec3 *result)
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
            *result = glm::vec3(transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f));
        }

        static void TransformComponent_SetForward(uint64_t entityID, glm::vec3 value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TransformComponent>())
            {
                return;
            }

            glm::vec3 forward = glm::normalize(value);
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

        static void TransformComponent_GetRight(uint64_t entityID, glm::vec3 *result)
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
            *result = glm::vec3(transform.rotation * glm::vec3(1.0f, 0.0f, 0.0f));
        }

        static void TransformComponent_SetRight(uint64_t entityID, glm::vec3 value)
        {
            glm::vec3 right = glm::normalize(value);
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

            TransformComponent_SetForward(entityID, glm::vec3(forward));
        }

        static void TransformComponent_GetUp(uint64_t entityID, glm::vec3 *result)
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
            *result = glm::vec3(transform.rotation * glm::vec3(0.0f, 1.0f, 0.0f));
        }

        static void TransformComponent_SetUp(uint64_t entityID, glm::vec3 value)
        {
            glm::vec3 up = glm::normalize(value);
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

        static void TransformComponent_GetTranslation(uint64_t entityID, glm::vec3 *result)
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

            *result = glm::vec3(entity.GetComponent<TransformComponent>().translation);
        }

        static void TransformComponent_SetTranslation(uint64_t entityID, glm::vec3 value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TransformComponent>())
            {
                return;
            }

            auto &transform = entity.GetComponent<TransformComponent>();
            transform.localTranslation = value;
            transform.translation = value;
            transform.dirty = true;
        }

        static void TransformComponent_GetRotation(uint64_t entityID, glm::quat *result)
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

            *result = glm::quat(entity.GetComponent<TransformComponent>().rotation);
        }

        static void TransformComponent_SetRotation(uint64_t entityID, glm::quat value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TransformComponent>())
            {
                return;
            }

            auto &transform = entity.GetComponent<TransformComponent>();
            transform.localRotation = value;
            transform.rotation = value;
            transform.dirty = true;
        }

        static void TransformComponent_GetEulerAngles(uint64_t entityID, glm::vec3 *result)
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

            *result = glm::vec3(glm::eulerAngles(entity.GetComponent<TransformComponent>().rotation));
        }

        static void TransformComponent_SetEulerAngles(uint64_t entityID, glm::vec3 value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TransformComponent>())
            {
                return;
            }

            auto &transform = entity.GetComponent<TransformComponent>();
            const glm::quat rotation = glm::quat(value);
            transform.localRotation = rotation;
            transform.rotation = rotation;
            transform.dirty = true;
        }

        static void TransformComponent_GetScale(uint64_t entityID, glm::vec3 *result)
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

            *result = glm::vec3(entity.GetComponent<TransformComponent>().scale);
        }

        static void TransformComponent_SetScale(uint64_t entityID, glm::vec3 value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TransformComponent>())
            {
                return;
            }

            auto &transform = entity.GetComponent<TransformComponent>();
            const glm::vec3 scale = value;
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
        s_EntityHasComponentFuncs.clear();
        s_EntityAddComponentFuncs.clear();
        RegisterComponent(AllComponents {});

        LOG_INFO("[ScriptGlue] Component bridge initialized (HostFXR)");
    }

    void ScriptGlue::RegisterFunctions()
    {
        LOG_INFO("[ScriptGlue] Function bridge initialized (HostFXR)");
    }
}
