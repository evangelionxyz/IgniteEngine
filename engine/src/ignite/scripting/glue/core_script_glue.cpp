// Copyright (c) 2026 Evangelion Manuhutu

#include "core_script_glue.hpp"
#include "ignite/core/input/input.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/scripting/script_engine.hpp"
#include "ignite/project/project.hpp"

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
            glm::vec2 absPos = glm::vec2(mousePos.x, mousePos.y);
            Scene *scene = GetSceneContext();
            if (scene)
            {
                Entity cameraEntity = scene->GetPrimaryCamera();
                if (cameraEntity.IsValid() && cameraEntity.HasComponent<CameraComponent>())
                {
                    const auto &cc = cameraEntity.GetComponent<CameraComponent>();
                    absPos -= cc.camera.viewportPosition;
                }
            }

            *result = absPos;
        }

        static void Input_SetMouseToCenter()
        {
            Input::SetMouseToCenter();
        }

        static void Input_SetCursorMode(int32_t mode)
        {
            Input::SetCursorMode(static_cast<CursorMode>(mode));
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

        static const CoreScriptGlueAPI s_CoreScriptGlueAPI =
        {
            &Debug_Log,

            &Input_IsKeyPressed,
            &Input_IsModifierPressed,
            &Input_IsMouseButtonPressed,
            &Input_GetMousePosition,
            &Input_SetMouseToCenter,
            &Input_SetCursorMode,

            &AssetManager_IsAssetHandleValid,
            &AssetManager_IsAssetLoaded,

            &AssetManager_LoadAssetAsyncFromFile,
            &AssetManager_LoadAssetImmediateFromFile,

            &AssetManager_LoadAssetAsync,
            &AssetManager_LoadAssetImmediate,
        };
    }

    const CoreScriptGlueAPI *CoreScriptGlue::GetAPI()
    {
        return &s_CoreScriptGlueAPI;
    }
}
