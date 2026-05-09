// Copyright (c) 2026 Evangelion Manuhutu

#ifndef CORE_SCRIPT_GLUE_HPP
#define CORE_SCRIPT_GLUE_HPP

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
        void (*Input_SetMouseToCenter)();
        void (*Input_SetCursorMode)(int32_t mode);

        bool (*AssetManager_IsAssetHandleValid)(uint64_t handle);
        bool (*AssetManager_IsAssetLoaded)(uint64_t handle);

        uint64_t (*AssetManager_LoadAssetAsyncFromFile)(const char *filename);
        uint64_t (*AssetManager_LoadAssetImmediateFromFile)(const char *filename);

        void (*AssetManager_LoadAssetAsync)(uint64_t handle);
        void (*AssetManager_LoadAssetImmediate)(uint64_t handle);
    };

    class CoreScriptGlue
    {
    public:
        static const CoreScriptGlueAPI *GetAPI();
    };
}

#endif