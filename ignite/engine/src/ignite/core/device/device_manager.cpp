// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"
#include "ignite/graphics/window.hpp"

#ifdef PLATFORM_WINDOWS
    #include <ShellScalingApi.h>
    #include <dwmapi.h>
    #pragma comment(lib, "Dwmapi.lib") // Link to DWM API
    #pragma comment(lib, "shcore.lib")
#endif

#include "device_manager.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"

#include <glm/glm.hpp>

namespace ignite
{
    DeviceManager *s_DeviceManagerInstance = nullptr;

    void NRI_CALL DefaultMessageCallback::MessageCallback(nri::Message messageType, const char *file, uint32_t line, const char *message, void *userArg)
    {
        switch (messageType)
        {
            case nri::Message::INFO:
            {
                LOG_INFO("[NRI INFO] {}:{} - {}", file ? file : "", line, message ? message : "");
                break;
            }
            case nri::Message::WARNING:
            {
                LOG_WARN("[NRI WARN] {}:{} - {}", file ? file : "", line, message ? message : "");
                break;
            }
            case nri::Message::ERROR:
            {
                LOG_ERROR("[NRI ERROR] {}:{} - {}", file ? file : "", line, message ? message : "");
                break;
            }
        }
    }

    void NRI_CALL DefaultMessageCallback::AbortExecution(void *userArg)
    {
        LOG_ASSERT(false, "[NRI] Execution aborted due to fatal error");
    }

    nri::CallbackInterface DefaultMessageCallback::GetInterface()
    {
        nri::CallbackInterface cb{};
        cb.MessageCallback = MessageCallback;
        cb.AbortExecution = AbortExecution;
        cb.userArg = nullptr;
        return cb;
    }

    bool DeviceManager::CreateInstance(const InstanceParameters &params)
    {
        if (m_InstanceCreated)
            return true;

        static_cast<InstanceParameters &>(m_DeviceParameters) = params;
        if (!params.headlessDevice)
        {
#ifdef PLATFORM_WINDOWS
            if (params.enablePerMonitorDPI)
            {
                // Enable per-monitor DPI awareness V2 for better DPI handling
                // Use runtime linking to avoid compilation issues on older SDKs
                typedef BOOL(WINAPI *SetProcessDpiAwarenessContextFunc)(DPI_AWARENESS_CONTEXT);
                HMODULE user32 = GetModuleHandleA("user32.dll");
                SetProcessDpiAwarenessContextFunc setProcessDpiAwarenessContext =
                    (SetProcessDpiAwarenessContextFunc)GetProcAddress(user32, "SetProcessDpiAwarenessContext");

                if (setProcessDpiAwarenessContext)
                {
                    // Try to set per-monitor DPI aware V2 (Windows 10 1703+)
                    if (!setProcessDpiAwarenessContext((DPI_AWARENESS_CONTEXT)-4)) // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
                    {
                        // Fallback to V1 if V2 fails
                        SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
                    }
                }
                else
                {
                    // Fallback for older Windows versions
                    SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
                }
            }
            else
            {
                SetProcessDpiAwareness(PROCESS_DPI_UNAWARE);
            }
#endif
        }

        SDL_InitFlags sdlFlags = SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC | SDL_INIT_CAMERA;
        if (!SDL_Init(sdlFlags))
        {
            return false;
        }

        return CreateInstanceInternal();
    }

    bool DeviceManager::IsUpdateDPIScaleFactor()
    {
        if (m_PrevDPIScaleFactorX != m_DPIScaleFactorX || m_PrevDPIScaleFactorY != m_DPIScaleFactorY)
        {
            m_PrevDPIScaleFactorX = m_DPIScaleFactorX;
            m_PrevDPIScaleFactorY = m_DPIScaleFactorY;
            return true;
        }
        return false;
    }

    void DeviceManager::GetDPIScaleInfo(float &x, float &y) const
    {
        x = m_DPIScaleFactorX;
        y = m_DPIScaleFactorY;
    }

    void DeviceManager::ResizeBackbuffer(uint32_t width, uint32_t height)
    {
        m_DeviceParameters.backBufferWidth = width;
        m_DeviceParameters.backBufferHeight = height;
    }

    void DeviceManager::SetDPISacaleFactors(float x, float y)
    {
        m_DPIScaleFactorX = x;
        m_DPIScaleFactorY = y;
    }

    void DeviceManager::CreateBackBuffers()
    {
        // NRI back buffers and RTV/DSV descriptors are created in CreateRenderTargets() / CreateVkSwapChain()
    }

    DeviceManager::DeviceManager()
    {
    }

    void DeviceManager::Destroy()
    {
        DestroyDeviceAndSwapChain();
        m_InstanceCreated = false;
    }

    void DeviceManager::EnableVsync()
    {
        m_DeviceParameters.vsyncEnable = true;
        ResizeSwapChain();
        CreateBackBuffers();
    }

    void DeviceManager::DisableVsync()
    {
        m_DeviceParameters.vsyncEnable = false;
        ResizeSwapChain();
        CreateBackBuffers();
    }

    DeviceManager* DeviceManager::Create(Window *window, const DeviceParameters &params, nri::GraphicsAPI api)
    {
        switch (api)
        {
#if IGNITE_WITH_DX12
            case nri::GraphicsAPI::D3D12:
                s_DeviceManagerInstance = CreateD3D12(window, params);
                return s_DeviceManagerInstance;
#endif
#if IGNITE_WITH_VULKAN
            case nri::GraphicsAPI::VK:
                s_DeviceManagerInstance = CreateVK(window, params);
                return s_DeviceManagerInstance;
#endif
            default: LOG_ASSERT(false, "Unsupported Graphics API {}", (uint32_t)api);
            return nullptr;
        }
    }

    DeviceManager *DeviceManager::GetInstance()
    {
        return s_DeviceManagerInstance;
    }
}
