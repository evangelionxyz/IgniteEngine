/* MIT License
* 
* Copyright (c) 2025 Evangelion Manuhutu | IGNITE STUDIO
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

        static_cast<InstanceParameters &>(m_DeviceParams) = params;
        if (!params.headlessDevice)
        {
#ifdef PLATFORM_WINDOWS
            if (!params.enablePerMonitorDPI)
                SetProcessDpiAwareness(PROCESS_DPI_UNAWARE);
#endif
        }

        if (!glfwInit())
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
        m_DeviceParams.backBufferWidth = width;
        m_DeviceParams.backBufferHeight = height;
    }

    void DeviceManager::CreateBackBuffers()
    {
        u32 backBufferCount = GetBackBufferCount();
        m_SwapChainFramebuffers.resize(backBufferCount);
        for (u32 index = 0; index < backBufferCount; ++index)
        {
            m_SwapChainFramebuffers[index] = GetDevice()->createFramebuffer(nvrhi::FramebufferDesc().addColorAttachment(GetBackBuffer(index)));
        }
    }

    const DeviceCreationParameters &DeviceManager::GetDeviceParams()
    {
        return m_DeviceParams;
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

    nvrhi::IFramebuffer* DeviceManager::GetFramebuffer(u32 index)
    {
        if (index < m_SwapChainFramebuffers.size())
            return m_SwapChainFramebuffers[index];

        LOG_ASSERT(false, "SwapChain framebuffer is empty");
        return nullptr;
    }

    DeviceManager* DeviceManager::Create(nvrhi::GraphicsAPI api)
    {
        switch (api)
        {
#if IGNITE_WITH_DX12
            case nvrhi::GraphicsAPI::D3D12: return CreateD3D12();
#endif
#if IGNITE_WITH_VULKAN
            case nvrhi::GraphicsAPI::VULKAN: return CreateVK();
#endif
            default: LOG_ASSERT(false, "Unsupported Graphics API {}", (u32)api);
            return nullptr;
        }
    }
}
