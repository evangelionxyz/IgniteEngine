// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_ENGINE_API_H
#define IGN_ENGINE_API_H

#include "ignite/core/base.hpp"
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

struct IgniteAppConfig
{
    void *nativeWindowHandle;   // HWND on Windows (e.g. Avalonia window or child control)
    uint32_t width;
    uint32_t height;
    bool offscreen;
    int graphicsApi;            // 0 = VULKAN, 1 = D3D12
    bool enableDebug;
};

IGN_API bool Ignite_Init(const IgniteAppConfig *config);
IGN_API void Ignite_Tick(float deltaTime);
IGN_API void Ignite_Resize(uint32_t width, uint32_t height);
IGN_API void Ignite_Shutdown();
IGN_API void *Ignite_GetNativeWindow();
IGN_API void *Ignite_GetSharedTextureHandle();
IGN_API bool Ignite_ReadbackViewportPixels(void *outBuffer, uint32_t bufferSize);

typedef void (*IgniteLogCallback)(int level, const char *message);
IGN_API void Ignite_SetLogCallback(IgniteLogCallback callback);

// ======================================
// Project API
// ======================================
IGN_API bool Ignite_Project_New(const char *name, const char *parentDirectory);
IGN_API bool Ignite_Project_Open(const char *filepath);
IGN_API bool Ignite_Project_Save();
IGN_API void Ignite_Project_Close();
IGN_API bool Ignite_Project_IsOpen();
IGN_API const char *Ignite_Project_GetName();
IGN_API const char *Ignite_Project_GetDirectory();
IGN_API const char *Ignite_Project_GetFilePath();
IGN_API const char *Ignite_Project_GetAssetDirectory();

// ======================================
// Asset Manager API
// ======================================
IGN_API uint32_t Ignite_AssetManager_GetAssetCount();
IGN_API void Ignite_AssetManager_Refresh();
IGN_API void Ignite_AssetManager_SyncFromRust();

// ======================================
// Editor Camera API
// ======================================
// Push the editor camera matrices from C# into the native engine each tick.
// The matrices are column-major float[16] (glm/HLSL convention).
IGN_API void Ignite_Camera_SetNavigationMode(int mode); // 0 = Orbit, 1 = Fly, 2 = Mode2D
IGN_API int  Ignite_Camera_GetNavigationMode();


#ifdef __cplusplus
}
#endif

#endif // IGN_ENGINE_API_H
