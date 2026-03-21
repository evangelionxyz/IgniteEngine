// Copyright (c) 2026 Evangelion Manuhutu

#include "script_glue.hpp"
#include "ignite/core/input/input.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/scene/component_group.hpp"
#include "ignite/scene/scene_manager.hpp"
#include "ignite/scripting/script_engine.hpp"

#include "box2d/box2d.h"

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

        static bool Input_IsKeyPressed(uint32_t keyCode)
        {
            return Input::IsKeyPressed(static_cast<KeyCode>(keyCode));
        }

        static bool Input_IsModifierPressed(uint16_t modCode)
        {
            return Input::IsModifierPressed(static_cast<KeyModCode>(modCode));
        }

        static bool Input_IsMouseButtonPressed(uint8_t button)
        {
            return Input::IsMouseButtonPressed(static_cast<MouseCode>(button));
        }

        static void Input_GetMousePosition(glm::vec2 *result)
        {
            if (!result)
            {
                return;
            }

            const glm::ivec2 mousePos = Input::GetMousePosition();
            *result = glm::vec2(mousePos.x, mousePos.y);
        }

        static void Input_SetMouseToCenter()
        {
            Input::SetMouseToCenter();
        }

        static void Input_SetCursorMode(int32_t mode)
        {
            Input::SetCursorMode(static_cast<CursorMode>(mode));
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
            transform.localScale = value;
            transform.scale = value;
            transform.dirty = true;
        }

		static void Sprite2DComponent_SetColor(uint64_t entityID, glm::vec4 value)
		{
			Entity entity = GetEntityByID(entityID);
			if (!entity.IsValid() || !entity.HasComponent<Sprite2DComponent>())
			{
				return;
			}

			auto &comp = entity.GetComponent<Sprite2DComponent>();
			comp.color = value;
		}

		static void Sprite2DComponent_GetColor(uint64_t entityID, glm::vec4 *result)
		{
			Entity entity = GetEntityByID(entityID);
			if (!entity.IsValid() || !entity.HasComponent<Sprite2DComponent>())
			{
				return;
			}

			auto &comp = entity.GetComponent<Sprite2DComponent>();
			*result = comp.color;
		}

		static void Sprite2DComponent_SetTilingFactor(uint64_t entityID, glm::vec2 value)
		{
			Entity entity = GetEntityByID(entityID);
			if (!entity.IsValid() || !entity.HasComponent<Sprite2DComponent>())
			{
				return;
			}

			auto &comp = entity.GetComponent<Sprite2DComponent>();
			comp.tilingFactor = value;
		}

		static void Sprite2DComponent_GetTilingFactor(uint64_t entityID, glm::vec2 *result)
		{
            if (!result)
            {
                return;
            }

			Entity entity = GetEntityByID(entityID);
			if (!entity.IsValid() || !entity.HasComponent<Sprite2DComponent>())
			{
				return;
			}

			auto &comp = entity.GetComponent<Sprite2DComponent>();
			*result = comp.tilingFactor;
		}

        static void Rigidbody2DComponent_GetType(uint64_t entityID, int32_t *result)
        {
            if (!result)
            {
                return;
            }

            *result = static_cast<int32_t>(Body2DType_Static);
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
            {
                return;
            }

            *result = static_cast<int32_t>(entity.GetComponent<Rigidbody2DComponent>().type);
        }

        static void Rigidbody2DComponent_SetType(uint64_t entityID, int32_t value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
            {
                return;
            }

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            rb.type = static_cast<Body2DType>(value);
            if (b2Body_IsValid(rb.bodyId))
            {
                b2Body_SetType(rb.bodyId, GetB2BodyType(rb.type));
            }
        }

        static void Rigidbody2DComponent_GetLinearVelocity(uint64_t entityID, glm::vec2 *result)
        {
            if (!result)
            {
                return;
            }

            *result = {};
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
            {
                return;
            }

            *result = entity.GetComponent<Rigidbody2DComponent>().linearVelocity;
        }

        static void Rigidbody2DComponent_SetLinearVelocity(uint64_t entityID, glm::vec2 value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
            {
                return;
            }

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            rb.linearVelocity = value;
            if (b2Body_IsValid(rb.bodyId))
            {
                b2Body_SetLinearVelocity(rb.bodyId, { value.x, value.y });
            }
        }

        static void Rigidbody2DComponent_GetAngularVelocity(uint64_t entityID, float *result)
        {
            if (!result)
            {
                return;
            }

            *result = 0.0f;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
            {
                return;
            }

            *result = entity.GetComponent<Rigidbody2DComponent>().angularVelocity;
        }

        static void Rigidbody2DComponent_SetAngularVelocity(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
            {
                return;
            }

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            rb.angularVelocity = value;
            if (b2Body_IsValid(rb.bodyId))
            {
                b2Body_SetAngularVelocity(rb.bodyId, value);
            }
        }

        static void Rigidbody2DComponent_GetGravityScale(uint64_t entityID, float *result)
        {
            if (!result)
            {
                return;
            }

            *result = 0.0f;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
            {
                return;
            }

            *result = entity.GetComponent<Rigidbody2DComponent>().gravityScale;
        }

        static void Rigidbody2DComponent_SetGravityScale(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
            {
                return;
            }

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            rb.gravityScale = value;
            if (b2Body_IsValid(rb.bodyId))
            {
                b2Body_SetGravityScale(rb.bodyId, value);
            }
        }

        static void Rigidbody2DComponent_GetLinearDamping(uint64_t entityID, float *result)
        {
            if (!result)
            {
                return;
            }

            *result = 0.0f;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
            {
                return;
            }

            *result = entity.GetComponent<Rigidbody2DComponent>().linearDamping;
        }

        static void Rigidbody2DComponent_SetLinearDamping(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
            {
                return;
            }

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            rb.linearDamping = value;
            if (b2Body_IsValid(rb.bodyId))
            {
                b2Body_SetLinearDamping(rb.bodyId, value);
            }
        }

        static void Rigidbody2DComponent_GetAngularDamping(uint64_t entityID, float *result)
        {
            if (!result)
            {
                return;
            }

            *result = 0.0f;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
            {
                return;
            }

            *result = entity.GetComponent<Rigidbody2DComponent>().angularDamping;
        }

        static void Rigidbody2DComponent_SetAngularDamping(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
            {
                return;
            }

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            rb.angularDamping = value;
            if (b2Body_IsValid(rb.bodyId))
            {
                b2Body_SetAngularDamping(rb.bodyId, value);
            }
        }

        static void Rigidbody2DComponent_GetIsAwake(uint64_t entityID, bool *result)
        {
            if (!result)
            {
                return;
            }

            *result = false;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
            {
                return;
            }

            *result = entity.GetComponent<Rigidbody2DComponent>().isAwake;
        }

        static void Rigidbody2DComponent_SetIsAwake(uint64_t entityID, bool value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
            {
                return;
            }

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            rb.isAwake = value;
            if (b2Body_IsValid(rb.bodyId))
            {
                b2Body_SetAwake(rb.bodyId, value);
            }
        }

        static void Rigidbody2DComponent_GetIsEnabled(uint64_t entityID, bool *result)
        {
            if (!result)
            {
                return;
            }

            *result = false;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
            {
                return;
            }

            *result = entity.GetComponent<Rigidbody2DComponent>().isEnabled;
        }

        static void Rigidbody2DComponent_SetIsEnabled(uint64_t entityID, bool value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
            {
                return;
            }

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            rb.isEnabled = value;
            if (b2Body_IsValid(rb.bodyId))
            {
                if (value)
                {
                    b2Body_Enable(rb.bodyId);
                }
                else
                {
                    b2Body_Disable(rb.bodyId);
                }
            }
        }

        static void Rigidbody2DComponent_GetIsEnableSleep(uint64_t entityID, bool *result)
        {
            if (!result)
            {
                return;
            }

            *result = false;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
            {
                return;
            }

            *result = entity.GetComponent<Rigidbody2DComponent>().isEnableSleep;
        }

        static void Rigidbody2DComponent_SetIsEnableSleep(uint64_t entityID, bool value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
            {
                return;
            }

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            rb.isEnableSleep = value;
            if (b2Body_IsValid(rb.bodyId))
            {
                b2Body_EnableSleep(rb.bodyId, value);
            }
        }

        static void Rigidbody2DComponent_ApplyForce(uint64_t entityID, glm::vec2 force, glm::vec2 point, bool wake)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
            {
                return;
            }

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            if (!b2Body_IsValid(rb.bodyId))
            {
                return;
            }

            b2Body_ApplyForce(rb.bodyId, { force.x, force.y }, { point.x, point.y }, wake);
        }

        static void Rigidbody2DComponent_ApplyForceToCenter(uint64_t entityID, glm::vec2 force, bool wake)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
            {
                return;
            }

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            if (!b2Body_IsValid(rb.bodyId))
            {
                return;
            }

            b2Body_ApplyForceToCenter(rb.bodyId, { force.x, force.y }, wake);
        }

        static void Rigidbody2DComponent_ApplyLinearImpulse(uint64_t entityID, glm::vec2 impulse, glm::vec2 point, bool wake)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
            {
                return;
            }

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            if (!b2Body_IsValid(rb.bodyId))
            {
                return;
            }

            b2Body_ApplyLinearImpulse(rb.bodyId, { impulse.x, impulse.y }, { point.x, point.y }, wake);
        }

        static void Rigidbody2DComponent_ApplyLinearImpulseToCenter(uint64_t entityID, glm::vec2 impulse, bool wake)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
            {
                return;
            }

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            if (!b2Body_IsValid(rb.bodyId))
            {
                return;
            }

            b2Body_ApplyLinearImpulseToCenter(rb.bodyId, { impulse.x, impulse.y }, wake);
        }

        static void Rigidbody2DComponent_ApplyAngularImpulse(uint64_t entityID, float impulse, bool wake)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
            {
                return;
            }

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            if (!b2Body_IsValid(rb.bodyId))
            {
                return;
            }

            b2Body_ApplyAngularImpulse(rb.bodyId, impulse, wake);
        }

        static void Rigidbody2DComponent_ApplyTorque(uint64_t entityID, float torque, bool wake)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
            {
                return;
            }

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            if (!b2Body_IsValid(rb.bodyId))
            {
                return;
            }

            b2Body_ApplyTorque(rb.bodyId, torque, wake);
        }

        static void Rigidbody2DComponent_GetMass(uint64_t entityID, float *result)
        {
            if (!result)
            {
                return;
            }

            *result = 0.0f;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
            {
                return;
            }

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            if (!b2Body_IsValid(rb.bodyId))
            {
                return;
            }

            *result = b2Body_GetMass(rb.bodyId);
        }

        static void Rigidbody2DComponent_GetIsBullet(uint64_t entityID, bool *result)
        {
            if (!result)
            {
                return;
            }

            *result = false;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
            {
                return;
            }

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            if (!b2Body_IsValid(rb.bodyId))
            {
                return;
            }

            *result = b2Body_IsBullet(rb.bodyId);
        }

        static void Rigidbody2DComponent_SetIsBullet(uint64_t entityID, bool value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
            {
                return;
            }

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            if (!b2Body_IsValid(rb.bodyId))
            {
                return;
            }

            b2Body_SetBullet(rb.bodyId, value);
        }

        static void BoxCollider2DComponent_GetSize(uint64_t entityID, glm::vec2 *result)
        {
            if (!result)
            {
                return;
            }

            *result = {};
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<BoxCollider2DComponent>())
            {
                return;
            }

            *result = entity.GetComponent<BoxCollider2DComponent>().size;
        }

        static void BoxCollider2DComponent_SetSize(uint64_t entityID, glm::vec2 value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<BoxCollider2DComponent>())
            {
                return;
            }

            entity.GetComponent<BoxCollider2DComponent>().size = value;
        }

        static void BoxCollider2DComponent_GetOffset(uint64_t entityID, glm::vec2 *result)
        {
            if (!result)
            {
                return;
            }

            *result = {};
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<BoxCollider2DComponent>())
            {
                return;
            }

            *result = entity.GetComponent<BoxCollider2DComponent>().offset;
        }

        static void BoxCollider2DComponent_SetOffset(uint64_t entityID, glm::vec2 value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<BoxCollider2DComponent>())
            {
                return;
            }

            entity.GetComponent<BoxCollider2DComponent>().offset = value;
        }

        static void BoxCollider2DComponent_GetRestitution(uint64_t entityID, float *result)
        {
            if (!result)
            {
                return;
            }

            *result = 0.0f;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<BoxCollider2DComponent>())
            {
                return;
            }

            *result = entity.GetComponent<BoxCollider2DComponent>().restitution;
        }

        static void BoxCollider2DComponent_SetRestitution(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<BoxCollider2DComponent>())
            {
                return;
            }

            entity.GetComponent<BoxCollider2DComponent>().restitution = value;
        }

        static void BoxCollider2DComponent_GetFriction(uint64_t entityID, float *result)
        {
            if (!result)
            {
                return;
            }

            *result = 0.0f;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<BoxCollider2DComponent>())
            {
                return;
            }

            *result = entity.GetComponent<BoxCollider2DComponent>().friction;
        }

        static void BoxCollider2DComponent_SetFriction(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<BoxCollider2DComponent>())
            {
                return;
            }

            entity.GetComponent<BoxCollider2DComponent>().friction = value;
        }

        static void BoxCollider2DComponent_GetDensity(uint64_t entityID, float *result)
        {
            if (!result)
            {
                return;
            }

            *result = 0.0f;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<BoxCollider2DComponent>())
            {
                return;
            }

            *result = entity.GetComponent<BoxCollider2DComponent>().density;
        }

        static void BoxCollider2DComponent_SetDensity(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<BoxCollider2DComponent>())
            {
                return;
            }

            entity.GetComponent<BoxCollider2DComponent>().density = value;
        }

        static void BoxCollider2DComponent_GetIsSensor(uint64_t entityID, bool *result)
        {
            if (!result)
            {
                return;
            }

            *result = false;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<BoxCollider2DComponent>())
            {
                return;
            }

            *result = entity.GetComponent<BoxCollider2DComponent>().isSensor;
        }

        static void BoxCollider2DComponent_SetIsSensor(uint64_t entityID, bool value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<BoxCollider2DComponent>())
            {
                return;
            }

            entity.GetComponent<BoxCollider2DComponent>().isSensor = value;
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

            &Input_IsKeyPressed,
            &Input_IsModifierPressed,
            &Input_IsMouseButtonPressed,
            &Input_GetMousePosition,
            &Input_SetMouseToCenter,
            &Input_SetCursorMode,
            
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

            &Sprite2DComponent_SetColor,
            &Sprite2DComponent_GetColor,
            &Sprite2DComponent_SetTilingFactor,
            &Sprite2DComponent_GetTilingFactor,

            &Rigidbody2DComponent_GetType,
            &Rigidbody2DComponent_SetType,
            &Rigidbody2DComponent_GetLinearVelocity,
            &Rigidbody2DComponent_SetLinearVelocity,
            &Rigidbody2DComponent_GetAngularVelocity,
            &Rigidbody2DComponent_SetAngularVelocity,
            &Rigidbody2DComponent_GetGravityScale,
            &Rigidbody2DComponent_SetGravityScale,
            &Rigidbody2DComponent_GetLinearDamping,
            &Rigidbody2DComponent_SetLinearDamping,
            &Rigidbody2DComponent_GetAngularDamping,
            &Rigidbody2DComponent_SetAngularDamping,
            &Rigidbody2DComponent_GetIsAwake,
            &Rigidbody2DComponent_SetIsAwake,
            &Rigidbody2DComponent_GetIsEnabled,
            &Rigidbody2DComponent_SetIsEnabled,
            &Rigidbody2DComponent_GetIsEnableSleep,
            &Rigidbody2DComponent_SetIsEnableSleep,
            &Rigidbody2DComponent_ApplyForce,
            &Rigidbody2DComponent_ApplyForceToCenter,
            &Rigidbody2DComponent_ApplyLinearImpulse,
            &Rigidbody2DComponent_ApplyLinearImpulseToCenter,
            &Rigidbody2DComponent_ApplyAngularImpulse,
            &Rigidbody2DComponent_ApplyTorque,
            &Rigidbody2DComponent_GetMass,
            &Rigidbody2DComponent_GetIsBullet,
            &Rigidbody2DComponent_SetIsBullet,

            &BoxCollider2DComponent_GetSize,
            &BoxCollider2DComponent_SetSize,
            &BoxCollider2DComponent_GetOffset,
            &BoxCollider2DComponent_SetOffset,
            &BoxCollider2DComponent_GetRestitution,
            &BoxCollider2DComponent_SetRestitution,
            &BoxCollider2DComponent_GetFriction,
            &BoxCollider2DComponent_SetFriction,
            &BoxCollider2DComponent_GetDensity,
            &BoxCollider2DComponent_SetDensity,
            &BoxCollider2DComponent_GetIsSensor,
            &BoxCollider2DComponent_SetIsSensor,
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
