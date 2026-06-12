/* MIT License
* 
* Copyright (c) 2026 Evangelion Manuhutu
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#include <cstdio>
#include <iomanip>
#include <thread>
#include <sstream>

#include "ignite/graphics/window.hpp"

#include <nvrhi/utils.h>

#ifdef PLATFORM_WINDOWS
    #include <ShellScalingApi.h>
    #include <dwmapi.h>
    #pragma comment(lib, "Dwmapi.lib") // Link to DWM API
    #pragma comment(lib, "shcore.lib")
#endif

#include "device_manager.hpp"
#include "ignite/core/logger.hpp"
#include <glm/glm.hpp>

namespace ignite
{

    DeviceManager *s_DeviceManagerInstance = nullptr;

    DefaultMessageCallback &DefaultMessageCallback::GetInstance()
    {
        static DefaultMessageCallback *instance = nullptr;
        if (!instance)
            instance = new DefaultMessageCallback();
        return *instance;
    }

    void DefaultMessageCallback::message(nvrhi::MessageSeverity severity, const char *messageText)
    {
        switch (severity)
        {
            case nvrhi::MessageSeverity::Info:
            {
                LOG_INFO("NVHRI INFO: {}\n", messageText);
                break;
            }
            case nvrhi::MessageSeverity::Warning:
            {
                LOG_WARN("NVHRI WARN: {}\n", messageText);
                break;
            }
            case nvrhi::MessageSeverity::Error:
            {
                LOG_ASSERT(false, "NVHRI ERROR: {}\n", messageText);
                break;
            }
            case nvrhi::MessageSeverity::Fatal:
            {
                LOG_ASSERT(false, "NVHRI FATAL: {}\n", messageText);
                break;
            }
        }
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
        const uint32_t backBufferCount = GetBackBufferCount();
        m_SwapChainFramebuffers.resize(backBufferCount);
        for (uint32_t index = 0; index < backBufferCount; ++index)
        {
		   m_SwapChainFramebuffers[index] = GetDevice()->createFramebuffer(
               nvrhi::FramebufferDesc()
               .addColorAttachment(GetBackBuffer(index))
			   .setDepthAttachment(GetBackDepthBuffer(index))
           );
        }
    }

    DeviceManager::DeviceManager()
    {
    }

    void DeviceManager::Destroy()
    {
        m_SwapChainFramebuffers.clear();
        DestroyDeviceAndSwapChain();
        m_InstanceCreated = false;
    }

    nvrhi::IFramebuffer* DeviceManager::GetCurrentFramebuffer()
    {
        return GetFramebuffer(GetCurrentBackBufferIndex());
    }

    nvrhi::IFramebuffer* DeviceManager::GetFramebuffer(uint32_t index)
    {
        if (index < m_SwapChainFramebuffers.size())
            return m_SwapChainFramebuffers[index];

        LOG_ASSERT(false, "SwapChain framebuffer is empty");
        return nullptr;
    }

    DeviceManager* DeviceManager::Create(Window *window, const DeviceParameters &params, nvrhi::GraphicsAPI api)
    {
        switch (api)
        {
#if IGNITE_WITH_DX12
            case nvrhi::GraphicsAPI::D3D12:
                s_DeviceManagerInstance = CreateD3D12(window, params);
                return s_DeviceManagerInstance;
#endif
#if IGNITE_WITH_VULKAN
            case nvrhi::GraphicsAPI::VULKAN:
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
