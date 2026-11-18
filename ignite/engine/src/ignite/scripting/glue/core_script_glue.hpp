// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_CORE_SCRIPT_GLUE_HPP
#define IGN_CORE_SCRIPT_GLUE_HPP

#include <cstdint>
#include <glm/glm.hpp>

namespace ignite
{
    struct CoreScriptGlueAPI
    {
        void (*Debug_Log)(const char *message, uint8_t logLevel);

        bool (*Input_IsKeyPressed)(uint32_t keyCode);
        bool (*Input_IsModifierPressed)(uint16_t modCode);
        bool (*Input_IsMouseButtonPressed)(uint8_t button);
        void (*Input_GetMousePosition)(glm::vec2 *result);
        void (*Input_GetMouseDelta)(glm::vec2 *result);
        void (*Input_SetMouseToCenter)();
        void (*Input_SetCursorMode)(int32_t mode);
        int32_t (*Input_GetCursorMode)();
        bool (*Input_IsMouseOverUI)();
        bool (*Input_IsActionPressed)(const char *actionName);

        bool (*AssetManager_IsAssetHandleValid)(uint64_t handle);
        bool (*AssetManager_IsAssetLoaded)(uint64_t handle);

        uint64_t (*AssetManager_LoadAssetAsyncFromFile)(const char *filename);
        uint64_t (*AssetManager_LoadAssetImmediateFromFile)(const char *filename);

        void (*AssetManager_LoadAssetAsync)(uint64_t handle);
        void (*AssetManager_LoadAssetImmediate)(uint64_t handle);

        // ScriptableObject runtime field access
        float       (*ScriptableObject_GetFieldValueFloat)(uint64_t handle, const char *fieldName);
        int32_t     (*ScriptableObject_GetFieldValueInt)(uint64_t handle, const char *fieldName);
        bool        (*ScriptableObject_GetFieldValueBool)(uint64_t handle, const char *fieldName);
        const char *(*ScriptableObject_GetFieldValueString)(uint64_t handle, const char *fieldName);
        const char *(*ScriptableObject_GetClassName)(uint64_t handle);

        void (*Scene_Load)(uint64_t sceneAssetHandle);

        // Networking (NetworkManager)
        bool (*Net_IsConnected)();
        bool (*Net_IsServerRunning)();
        bool (*Net_IsConnecting)();
        uint32_t (*Net_GetClientId)();
        bool (*Net_StartServer)(uint16_t port, uint16_t tickRate, uint16_t snapshotRate);
        bool (*Net_StartServerDefault)();
        void (*Net_StopServer)();
        bool (*Net_Connect)(const char *address, uint16_t port, const char *playerName);
        void (*Net_Disconnect)();
        uint64_t (*Net_SpawnEntity)(const char *templateOrPrefab, const glm::vec3 *position, const glm::quat *rotation, uint32_t ownerClient);
        void (*Net_DespawnEntity)(uint64_t netId);
        void (*Net_SendInput)(const glm::vec2 *move, uint16_t buttons, float yaw);
        void (*Net_SendChat)(const char *text);
        void (*Net_KickClient)(uint32_t clientId, const char *reason);
        void (*Net_GetStats)(int32_t *outRttMs, float *outPacketLoss, float *outInKBps, float *outOutKBps, uint32_t *outClientCount);
        uint32_t (*Net_GetServerTick)();
        uint32_t (*Net_GetClientServerTick)();
        uint32_t (*Net_GetClientCount)();
        bool (*Net_GetConnectedClientAt)(uint32_t index, uint32_t *outClientId, const char **outName);
        uint32_t (*Net_GetChatCount)();
        bool (*Net_GetChatAt)(uint32_t index, uint32_t *outSenderId, const char **outSenderName, const char **outText);
        void (*Net_SetSimulationSettings)(int32_t lagMs, float lossPercent);

        uint64_t (*AssetManager_GetAssetHandleByPath)(const char *filepath);
        void (*Scene_LoadByPath)(const char *scenePath);
        void (*Scene_Stop)();
    };

    class CoreScriptGlue
    {
    public:
        static const CoreScriptGlueAPI *GetAPI();
    };
}

#endif