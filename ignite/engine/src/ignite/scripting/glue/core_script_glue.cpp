// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "core_script_glue.hpp"
#include "ignite/scene/scene_manager.hpp"
#include "ignite/core/input/input_system.hpp"
#include "ignite/graphics/ui/game_ui_system.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/scripting/script_engine.hpp"
#include "ignite/scripting/scriptable_object.hpp"
#include "ignite/project/project.hpp"
#include "ignite/globals/globals.hpp"

#include "ignite/core/application.hpp"
#include "ignite/graphics/window.hpp"

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

        static void Debug_Log(const char *message, uint8_t logLevel)
        {
            const std::string messageStr = std::format("[C# Script] {}", message ? message : "");
            switch (logLevel)
            {
                case spdlog::level::trace: LOG_TRACE("{}", messageStr); break;
                case spdlog::level::debug: LOG_DEBUG("{}", messageStr); break;
                case spdlog::level::info: LOG_INFO("{}", messageStr); break;
                case spdlog::level::warn: LOG_WARN("{}", messageStr); break;
                case spdlog::level::err: LOG_ERROR("{}", messageStr); break;
            }
        }

        static bool Input_IsKeyPressed(uint32_t keyCode)
        {
            return InputSystem::IsKeyPressed(static_cast<KeyCode>(keyCode));
        }

        static bool Input_IsModifierPressed(uint16_t modCode)
        {
            return InputSystem::IsModifierPressed(static_cast<KeyModCode>(modCode));
        }

        static bool Input_IsMouseButtonPressed(uint8_t button)
        {
            return InputSystem::IsMouseButtonPressed(static_cast<MouseCode>(button));
        }

        static void Input_GetMousePosition(glm::vec2 *result)
        {
            if (!result)
            {
                return;
            }

            if (InputSystem::GetActiveSystem()->GetCursorMode() == CursorMode::Disabled)
            {
                const glm::ivec2 mousePos = InputSystem::GetMousePosition();
                *result = glm::vec2(mousePos.x, mousePos.y);
                return;
            }

            if (InputSystem::IsGameplayMousePositionEnabled())
            {
                *result = InputSystem::GetGameplayMousePosition();
                return;
            }

            const glm::ivec2 mousePos = InputSystem::GetMousePosition();
            glm::vec2 absPos = glm::vec2(mousePos.x, mousePos.y);

            Scene *scene = GetSceneContext();
            if (scene)
            {
                if (Entity cameraEntity = scene->GetPrimaryCamera())
                {
                    const auto &cc = cameraEntity.GetComponent<CameraComponent>();
                    glm::vec2 viewportPosition = globals::GEditor::GameViewport.min;
                    if (auto *window = Application::GetInstance()->GetWindow())
                    {
                        glm::ivec2 winPos = window->GetPosition();
                        viewportPosition -= glm::vec2(winPos.x, winPos.y);
                    }
                    absPos -= viewportPosition;
                }
            }

            *result = absPos;
        }

        static void Input_GetMouseDelta(glm::vec2 *result)
        {
            if (!result)
                return;
            *result = InputSystem::GetMouseDelta();
        }

        static void Input_SetMouseToCenter()
        {
            InputSystem::SetMouseToCenter();
        }

        static void Input_SetCursorMode(int32_t mode)
        {
            InputSystem::SetCursorMode(static_cast<CursorMode>(mode));
        }

        static int32_t Input_GetCursorMode()
        {
            return static_cast<int32_t>(InputSystem::GetActiveSystem()->GetCursorMode());
        }

        static bool Input_IsMouseOverUI()
        {
            glm::vec2 mousePos(0.0f);
            Input_GetMousePosition(&mousePos);
            return GameUISystem::IsMouseOverUI(mousePos.x, mousePos.y);
        }

        static bool Input_IsActionPressed(const char *actionName)
        {
            return InputSystem::IsActionPressed(actionName ? actionName : "");
        }

        static bool AssetManager_IsAssetHandleValid(uint64_t handle)
        {
            if (Scene *scene = GetSceneContext())
            {
                if (AssetManager *assetManager = scene->GetAssetManager())
                {
                    return assetManager->IsAssetHandleValid(AssetHandle(handle));
                }
            }
            return false;
        }

        static bool AssetManager_IsAssetLoaded(uint64_t handle)
        {
            if (Scene *scene = GetSceneContext())
            {
                if (AssetManager *assetManager = scene->GetAssetManager())
                {
                    return assetManager->IsAssetLoaded(AssetHandle(handle));
                }
            }
            return false;
        }

        static uint64_t AssetManager_LoadAssetAsyncFromFile(const char *filename)
        {
            if (Scene *scene = GetSceneContext())
            {
                if (AssetManager *assetManager = scene->GetAssetManager())
                {
                    return assetManager->ImportAsset(filename);
                }
            }
            return 0;
        }

        static uint64_t AssetManager_LoadAssetImmediateFromFile(const char *filename)
        {
            if (Scene *scene = GetSceneContext())
            {
                if (AssetManager *assetManager = scene->GetAssetManager())
                {
                    return assetManager->ImportAssetImmedate(filename);
                }
            }
            return 0;
        }

        static void AssetManager_LoadAssetAsync(uint64_t handle)
        {
            if (Scene *scene = GetSceneContext())
            {
                if (AssetManager *assetManager = scene->GetAssetManager())
                {
                    assetManager->GetAsset<Asset>(AssetHandle(handle));
                }
            }
        }

        static void AssetManager_LoadAssetImmediate(uint64_t handle)
        {
            if (Scene *scene = GetSceneContext())
            {
                if (AssetManager *assetManager = scene->GetAssetManager())
                {
                    assetManager->GetAssetImmediate<Asset>(AssetHandle(handle));
                }
            }
        }

        // ScriptableObject runtime field access
        static Ref<ScriptableObject> GetSO(uint64_t handle)
        {
            if (Scene *scene = GetSceneContext())
            {
                if (AssetManager *am = scene->GetAssetManager())
                    return am->GetAssetImmediate<ScriptableObject>(AssetHandle(handle));
            }
            return nullptr;
        }

        static float ScriptableObject_GetFieldValueFloat(uint64_t handle, const char *fieldName)
        {
            if (auto so = GetSO(handle))
            {
                auto &fields = so->GetFields();
                if (auto it = fields.find(fieldName); it != fields.end())
                    return it->second.GetValue<float>();
            }
            return 0.0f;
        }

        static int32_t ScriptableObject_GetFieldValueInt(uint64_t handle, const char *fieldName)
        {
            if (auto so = GetSO(handle))
            {
                auto &fields = so->GetFields();
                if (auto it = fields.find(fieldName); it != fields.end())
                    return it->second.GetValue<int32_t>();
            }
            return 0;
        }

        static bool ScriptableObject_GetFieldValueBool(uint64_t handle, const char *fieldName)
        {
            if (auto so = GetSO(handle))
            {
                auto &fields = so->GetFields();
                if (auto it = fields.find(fieldName); it != fields.end())
                    return it->second.GetValue<bool>();
            }
            return false;
        }

        // String storage for the return value (per-call static buffer - single-threaded only)
        static std::string s_StringReturnBuffer;

        static const char *ScriptableObject_GetFieldValueString(uint64_t handle, const char *fieldName)
        {
            if (auto so = GetSO(handle))
            {
                auto &fields = so->GetFields();
                if (auto it = fields.find(fieldName); it != fields.end())
                {
                    s_StringReturnBuffer = it->second.GetValue<std::string>();
                    return s_StringReturnBuffer.c_str();
                }
            }
            return "";
        }

        static std::string s_ClassNameBuffer;

        static const char *ScriptableObject_GetClassName(uint64_t handle)
        {
            if (auto so = GetSO(handle))
            {
                s_ClassNameBuffer = so->GetClassName();
                return s_ClassNameBuffer.c_str();
            }
            return "";
        }

        static void Scene_Load(uint64_t sceneAssetHandle)
        {
            SceneManager::Transition(AssetHandle(sceneAssetHandle));
        }

        static const CoreScriptGlueAPI s_CoreScriptGlueAPI =
        {
            &Debug_Log,

            &Input_IsKeyPressed,
            &Input_IsModifierPressed,
            &Input_IsMouseButtonPressed,
            &Input_GetMousePosition,
            &Input_GetMouseDelta,
            &Input_SetMouseToCenter,
            &Input_SetCursorMode,
            &Input_GetCursorMode,
            &Input_IsMouseOverUI,
            &Input_IsActionPressed,


            &AssetManager_IsAssetHandleValid,
            &AssetManager_IsAssetLoaded,

            &AssetManager_LoadAssetAsyncFromFile,
            &AssetManager_LoadAssetImmediateFromFile,

            &AssetManager_LoadAssetAsync,
            &AssetManager_LoadAssetImmediate,

            // ScriptableObject
            &ScriptableObject_GetFieldValueFloat,
            &ScriptableObject_GetFieldValueInt,
            &ScriptableObject_GetFieldValueBool,
            &ScriptableObject_GetFieldValueString,
            &ScriptableObject_GetClassName,

            &Scene_Load,
        };
    }

    const CoreScriptGlueAPI *CoreScriptGlue::GetAPI()
    {
        return &s_CoreScriptGlueAPI;
    }
}
