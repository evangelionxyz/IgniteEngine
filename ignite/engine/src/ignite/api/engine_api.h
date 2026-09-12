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

#ifdef __cplusplus
}
#endif

#endif // IGN_ENGINE_API_H
