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

#pragma once

#include "ignite/core/base.hpp"

#ifdef PLATFORM_WINDOWS
    #include <dxgi.h>
#endif

#ifdef IGNITE_WITH_DX12
    #include <d3d12.h>
#endif

#if IGNITE_WITH_VULKAN
    #define VK_NO_PROTOTYPES
    #define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
    #include <vulkan/vulkan.hpp>
#endif

#include <nvrhi/nvrhi.h>
#include <optional>
#include <array>
#include <functional>

#include "ignite/core/base.hpp"
#include "ignite/core/types.hpp"

namespace ignite
{
    class Window;

    struct DefaultMessageCallback final : public nvrhi::IMessageCallback
    {
        static DefaultMessageCallback &GetInstance();
        void message(nvrhi::MessageSeverity severity, const char *messageText) override;
    };

    struct InstanceParameters
    {
        bool enableDebugRuntime = false;
        bool enableWarningAsErrors = false;
        bool enableGPUValidation = false;
        bool headlessDevice = false;
        bool logBufferLifetime = false;
        bool enableHeapDirectlyIndexed = false;
        bool enablePerMonitorDPI = true;

#ifdef IGNITE_WITH_VULKAN
        std::string vulkanLibraryName;
        std::vector<std::string> requiredVulkanInstanceExtensions;
        std::vector<std::string> requiredVulkanLayers;
        std::vector<std::string> optionalVulkanInstanceExtensions;
        std::vector<std::string> optionalVulkanLayers;
#endif

    };

    struct DeviceParameters : public InstanceParameters
    {
        bool startMaximized = false;
        bool startFullscreen = false;
        bool startBorderless = false;
        bool allowModeSwitch = false;

        int windowPosX = -1; // -1 means use default placement
        int windowPosY = -1;
		int windowWidth = 1080;
		int windowHeight = 640;

        uint32_t backBufferWidth = 1080;
        uint32_t backBufferHeight = 640;

        uint32_t refreshRate = 0;
        uint32_t swapChainBufferCount = 3;
        uint32_t swapChainSampleCount = 1;
        uint32_t swapChainSampleQuality = 0;
        uint32_t maxFramesInFlight = 3;

        nvrhi::Format swapChainFormat = nvrhi::Format::RGBA8_UNORM;

        bool enableNvrhiValidationLayer = false;
        bool vsyncEnable = false;
        bool enableRayTracingExtensions = false; // for vulkan
        bool enableComputeQueue = false;
        bool enableCopyQueue = false;
        int adapterIndex = -1;
        bool supportExplicitDisplayScaling = false;
        bool resizeWindowWithDisplayScale = false;
        
        nvrhi::IMessageCallback *messageCallback = nullptr;

#ifdef PLATFORM_WINDOWS
        DXGI_USAGE swapChainUsage = DXGI_USAGE_SHADER_INPUT | DXGI_USAGE_RENDER_TARGET_OUTPUT;
        D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_1;
#endif

#ifdef IGNITE_WITH_VULKAN
        std::vector<std::string> requiredVulkanDeviceExtensions;
        std::vector<std::string> optionalVulkanDeviceExtensions;
        std::vector<size_t> ignoreVulkanValidationMessageLocations;
        std::function<void(VkDeviceCreateInfo &)> deviceCreateInfoCallback;
        void *physicalDeviceFeatures2Extensions = nullptr;
#endif
    };

    struct AdapterInfo
    {
        std::string name;
        uint32_t vendorID = 0;
        uint32_t deviceID = 0;
        uint64_t dedicatedVideoMemory = 0;

        std::optional<std::array<uint8_t, 16>> uuid;
        std::optional<std::array<uint8_t, 8>> luid;


#ifdef IGNITE_WITH_DX12
        nvrhi::RefCountPtr<IDXGIAdapter> dxgiAdapter;
#endif

#ifdef IGNITE_WITH_VULKAN
        VkPhysicalDevice vkPhysicalDevice = nullptr;
#endif
    };

    class DeviceManager
    {
    public:
        static DeviceManager *Create(Window *window, const DeviceParameters &params, nvrhi::GraphicsAPI api);

        static DeviceManager *GetInstance();

        bool CreateInstance(const InstanceParameters &params);
        virtual bool EnumerateAdapters(std::vector<AdapterInfo> &outAdapters) = 0;
        virtual void WaitForIdle() = 0;
        bool IsUpdateDPIScaleFactor();
        void GetDPIScaleInfo(float &x, float &y) const;
        void ResizeBackbuffer(uint32_t width, uint32_t height);

    public:
        // device specific methods
        virtual bool CreateInstanceInternal() = 0;
        virtual bool CreateDevice() = 0;
        virtual bool CreateSwapChain() = 0;
        virtual void DestroyDeviceAndSwapChain() = 0;
        virtual void ResizeSwapChain() = 0;
        virtual bool BeginFrame() = 0;
        virtual bool Present() = 0;

        virtual nvrhi::IDevice *GetDevice() const = 0;
        virtual const char *GetRendererString() const = 0;
        virtual nvrhi::GraphicsAPI GetGraphicsAPI() const = 0;

        DeviceParameters &GetDeviceParameters() { return m_DeviceParameters; }
        double GetAverageFrameTimeSeconds() const { return m_AverageFrameTime; }
        double GetPreviousFrameTimestamp() const { return m_PreviousFrameTimestamp; }
        
        void SetFrameTimeUpdateInterval(double seconds) { m_AverageTimeUpdateInterval = seconds; }
        void SetEnableRenderDuringWindowMovement(bool val) { m_EnableRenderDuringWindowMovement = val; }
        void SetDPISacaleFactors(float x, float y);
		void SetHeadLessDevice(bool headless) { m_DeviceParameters.headlessDevice = headless; }
        
        bool IsVsyncEnabled() const { return m_DeviceParameters.vsyncEnable; }

        void CreateBackBuffers();

        Window *GetWindow() const { return m_Window; }
        uint32_t GetFrameIndex() const { return m_FrameIndex; }

        virtual nvrhi::ITexture *GetCurrentBackBuffer() = 0;
        virtual nvrhi::ITexture *GetBackBuffer(uint32_t index) = 0;
        virtual nvrhi::ITexture *GetBackDepthBuffer(uint32_t index) = 0;
        virtual uint32_t GetCurrentBackBufferIndex() = 0;
        virtual uint32_t GetBackBufferCount() = 0;
        nvrhi::IFramebuffer *GetCurrentFramebuffer();
        nvrhi::IFramebuffer *GetFramebuffer(uint32_t index);


        virtual void Destroy();
        virtual ~DeviceManager() = default;

        virtual void ReportLiveObjects() {};

        virtual bool IsVulkanInstanceExtensionEnabled(const char *extensionName) const { return false; }
        virtual bool IsVulkanDeviceExtensionEnabled(const char *extensionName) const { return false; }
        virtual bool IsVulkanLayerEnabled(const char *layerName) const { return false; }
        virtual void GetEnabledVulkanInstanceExtensions(std::vector<std::string> &extensions) const {}
        virtual void GetEnabledVulkanDeviceExtensions(std::vector<std::string> &extensions) const {}
        virtual void GetEnabledVulkanLayers(std::vector<std::string> &layers) const {}

    protected:
        DeviceManager();

        bool m_SkipRenderOnFirstFrame = false;

        DeviceParameters m_DeviceParameters;
        Window *m_Window = nullptr;
        bool m_EnableRenderDuringWindowMovement = false;

        bool m_IsNvidia = false;
        double m_PreviousFrameTimestamp = 0.0f;

        // current DPI scale info (update when window moves)
        float m_DPIScaleFactorX = 1.0f;
        float m_DPIScaleFactorY = 1.0f;
        float m_PrevDPIScaleFactorX = 1.0f;
        float m_PrevDPIScaleFactorY = 1.0f;

        bool m_RequestedVSync = false;
        bool m_InstanceCreated = false;

        double m_AverageFrameTime = 0.0f;
        double m_AverageTimeUpdateInterval = 0.5;
        double m_FrameTimeSum = 0.0;
        int m_NumberOfAccumulatedFrames = 0;

        uint32_t m_FrameIndex = 0;

        std::vector<nvrhi::FramebufferHandle> m_SwapChainFramebuffers;

    private:
        static DeviceManager *CreateD3D12(Window *window, const DeviceParameters &params);
        static DeviceManager *CreateVK(Window *window, const DeviceParameters& params);

        std::string m_WindowTitle;
    };
}
