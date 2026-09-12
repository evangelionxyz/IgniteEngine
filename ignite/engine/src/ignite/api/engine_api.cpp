// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"
#include "engine_api.h"
#include "ignite/core/application.hpp"
#include "ignite/graphics/window.hpp"
#include "ignite/core/device/device_manager.hpp"

namespace ignite
{
    class NativeEmbeddedApp final : public Application
    {
    public:
        explicit NativeEmbeddedApp(const ApplicationCreateInfo &createInfo)
            : Application(createInfo)
        {
        }
    };
}

static ignite::Application *s_EmbeddedApp = nullptr;

extern "C" {

IGN_API bool Ignite_Init(const IgniteAppConfig *config)
{
    if (s_EmbeddedApp)
    {
        return true;
    }

    ignite::Logger::Init();

    ignite::ApplicationCreateInfo createInfo{};
    createInfo.name = "Ignite Viewport";
    createInfo.width = config ? config->width : 1280;
    createInfo.height = config ? config->height : 720;
    createInfo.offscreen = config ? config->offscreen : false;
    createInfo.nativeWindowHandle = config ? config->nativeWindowHandle : nullptr;
    createInfo.useGui = false; // Avalonia manages UI

    if (config)
    {
        createInfo.graphicsApi = (config->graphicsApi == 1) ? nvrhi::GraphicsAPI::D3D12 : nvrhi::GraphicsAPI::VULKAN;
    }
    else
    {
        createInfo.graphicsApi = nvrhi::GraphicsAPI::VULKAN;
    }

    s_EmbeddedApp = new ignite::NativeEmbeddedApp(createInfo);
    return s_EmbeddedApp != nullptr;
}

IGN_API void Ignite_Tick(float deltaTime)
{
    if (s_EmbeddedApp)
    {
        s_EmbeddedApp->Step(deltaTime);
    }
}

IGN_API void Ignite_Resize(uint32_t width, uint32_t height)
{
    if (s_EmbeddedApp)
    {
        s_EmbeddedApp->Resize(width, height);
    }
}

IGN_API void Ignite_Shutdown()
{
    if (s_EmbeddedApp)
    {
        delete s_EmbeddedApp;
        s_EmbeddedApp = nullptr;
        ignite::Logger::Shutdown();
    }
}

IGN_API void *Ignite_GetNativeWindow()
{
    if (s_EmbeddedApp && s_EmbeddedApp->GetWindow())
    {
        return (void*)s_EmbeddedApp->GetWindow()->GetNativeWindow();
    }
    return nullptr;
}

IGN_API void *Ignite_GetSharedTextureHandle()
{
    ignite::DeviceManager *dm = ignite::DeviceManager::GetInstance();
    if (dm)
    {
        return dm->GetSharedBackBufferHandle();
    }
    return nullptr;
}

IGN_API bool Ignite_ReadbackViewportPixels(void *outBuffer, uint32_t bufferSize)
{
    ignite::DeviceManager *dm = ignite::DeviceManager::GetInstance();
    if (dm)
    {
        return dm->ReadbackBackBuffer(outBuffer, bufferSize);
    }
    return false;
}

IGN_API void Ignite_SetLogCallback(IgniteLogCallback callback)
{
    ignite::Logger::SetLogCallback(callback);
}

}
