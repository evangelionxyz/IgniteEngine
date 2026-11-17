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
#include "ignite/net/network_manager.hpp"
#include "ignite/net/net_replication.hpp"

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
                    if (auto *app = Application::GetInstance())
                    {
                        if (auto *window = app->GetWindow())
                        {
                            glm::ivec2 winPos = window->GetPosition();
                            viewportPosition -= glm::vec2(winPos.x, winPos.y);
                        }
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

        // =========================================================================
        // Networking (NetworkManager)
        // =========================================================================
        static bool Net_IsConnected()
        {
            const auto *nm = NetworkManager::GetInstance();
            return nm && nm->IsConnected();
        }

        static bool Net_IsServerRunning()
        {
            const auto *nm = NetworkManager::GetInstance();
            return nm && nm->IsServerRunning();
        }

        static bool Net_IsConnecting()
        {
            const auto *nm = NetworkManager::GetInstance();
            return nm && nm->IsConnecting();
        }

        static uint32_t Net_GetClientId()
        {
            const auto *nm = NetworkManager::GetInstance();
            return nm ? nm->GetClientId() : 0;
        }

        static bool Net_StartServer(uint16_t port, uint16_t tickRate, uint16_t snapshotRate)
        {
            auto *nm = NetworkManager::GetInstance();
            if (!nm)
                return false;
            return nm->StartServer(port, tickRate, snapshotRate);
        }

        static bool Net_StartServerDefault()
        {
            auto *nm = NetworkManager::GetInstance();
            return nm && nm->StartServer();
        }

        static void Net_StopServer()
        {
            if (auto *nm = NetworkManager::GetInstance())
                nm->StopServer();
        }

        static bool Net_Connect(const char *address, uint16_t port, const char *playerName)
        {
            auto *nm = NetworkManager::GetInstance();
            if (!nm)
                return false;
            return nm->Connect(address ? address : "127.0.0.1", port, playerName ? playerName : "Player");
        }

        static void Net_Disconnect()
        {
            if (auto *nm = NetworkManager::GetInstance())
                nm->Disconnect();
        }

        static uint64_t Net_SpawnEntity(const char *templateOrPrefab, const glm::vec3 *position, const glm::quat *rotation, uint32_t ownerClient)
        {
            auto *nm = NetworkManager::GetInstance();
            if (!nm || !nm->GetServerReplication())
                return 0;

            const glm::vec3 pos = position ? *position : glm::vec3(0.0f);
            const glm::quat rot = rotation ? *rotation : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            Entity spawned = nm->GetServerReplication()->SpawnEntity(templateOrPrefab ? templateOrPrefab : "", pos, rot, ownerClient);
            return spawned.IsValid() ? static_cast<uint64_t>(spawned.GetUUID()) : 0;
        }

        static void Net_DespawnEntity(uint64_t netId)
        {
            auto *nm = NetworkManager::GetInstance();
            if (nm && nm->GetServerReplication())
                nm->GetServerReplication()->DespawnEntity(netId);
        }

        static void Net_SendInput(const glm::vec2 *move, uint16_t buttons, float yaw)
        {
            if (auto *nm = NetworkManager::GetInstance())
                nm->SendInput(move ? *move : glm::vec2(0.0f), buttons, yaw);
        }

        static void Net_SendChat(const char *text)
        {
            if (auto *nm = NetworkManager::GetInstance())
                nm->SendChat(text ? text : "");
        }

        static void Net_KickClient(uint32_t clientId, const char *reason)
        {
            if (auto *nm = NetworkManager::GetInstance())
                nm->KickClient(clientId, reason ? reason : "");
        }

        static void Net_GetStats(int32_t *outRttMs, float *outPacketLoss, float *outInKBps, float *outOutKBps, uint32_t *outClientCount)
        {
            NetStats stats{};
            if (const auto *nm = NetworkManager::GetInstance())
                stats = nm->GetStats();

            if (outRttMs) *outRttMs = stats.rttMs;
            if (outPacketLoss) *outPacketLoss = stats.packetLossPercent;
            if (outInKBps) *outInKBps = stats.inKBytesPerSec;
            if (outOutKBps) *outOutKBps = stats.outKBytesPerSec;
            if (outClientCount) *outClientCount = stats.clientCount;
        }

        static uint32_t Net_GetServerTick()
        {
            const auto *nm = NetworkManager::GetInstance();
            return nm ? nm->GetServerTick() : 0;
        }

        static uint32_t Net_GetClientServerTick()
        {
            const auto *nm = NetworkManager::GetInstance();
            return nm ? nm->GetClientServerTick() : 0;
        }

        static uint32_t Net_GetClientCount()
        {
            const auto *nm = NetworkManager::GetInstance();
            return nm ? static_cast<uint32_t>(nm->GetClientCount()) : 0;
        }

        static std::string s_NetClientNameBuffer;
        static bool Net_GetConnectedClientAt(uint32_t index, uint32_t *outClientId, const char **outName)
        {
            const auto *nm = NetworkManager::GetInstance();
            if (!nm)
                return false;

            const auto ids = nm->GetConnectedClientIds();
            const auto names = nm->GetConnectedClientNames();
            if (index >= ids.size())
                return false;

            if (outClientId) *outClientId = ids[index];
            s_NetClientNameBuffer = index < names.size() ? names[index] : "";
            if (outName) *outName = s_NetClientNameBuffer.c_str();
            return true;
        }

        static uint32_t Net_GetChatCount()
        {
            const auto *nm = NetworkManager::GetInstance();
            return nm ? static_cast<uint32_t>(nm->GetChatHistory().size()) : 0;
        }

        static std::string s_NetChatSenderBuffer;
        static std::string s_NetChatTextBuffer;
        static bool Net_GetChatAt(uint32_t index, uint32_t *outSenderId, const char **outSenderName, const char **outText)
        {
            const auto *nm = NetworkManager::GetInstance();
            if (!nm)
                return false;

            const auto &history = nm->GetChatHistory();
            if (index >= history.size())
                return false;

            const auto &entry = history[index];
            if (outSenderId) *outSenderId = entry.senderId;
            s_NetChatSenderBuffer = entry.senderName;
            s_NetChatTextBuffer = entry.text;
            if (outSenderName) *outSenderName = s_NetChatSenderBuffer.c_str();
            if (outText) *outText = s_NetChatTextBuffer.c_str();
            return true;
        }

        static void Net_SetSimulationSettings(int32_t lagMs, float lossPercent)
        {
            if (auto *nm = NetworkManager::GetInstance())
                nm->SetSimulationSettings(lagMs, lossPercent);
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

            // Networking
            &Net_IsConnected,
            &Net_IsServerRunning,
            &Net_IsConnecting,
            &Net_GetClientId,
            &Net_StartServer,
            &Net_StartServerDefault,
            &Net_StopServer,
            &Net_Connect,
            &Net_Disconnect,
            &Net_SpawnEntity,
            &Net_DespawnEntity,
            &Net_SendInput,
            &Net_SendChat,
            &Net_KickClient,
            &Net_GetStats,
            &Net_GetServerTick,
            &Net_GetClientServerTick,
            &Net_GetClientCount,
            &Net_GetConnectedClientAt,
            &Net_GetChatCount,
            &Net_GetChatAt,
            &Net_SetSimulationSettings,
        };
    }

    const CoreScriptGlueAPI *CoreScriptGlue::GetAPI()
    {
        return &s_CoreScriptGlueAPI;
    }
}
