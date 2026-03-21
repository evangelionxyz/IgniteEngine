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

#include <string>
#include <queue>
#include <unordered_set>

#include "device_manager.hpp"
#include "ignite/graphics/render_target.hpp"

#include "ignite/core/logger.hpp"

#include <nvrhi/vulkan.h>
#include <nvrhi/validation.h>

#define VK_NO_PROTOTYPES
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

namespace ignite
{
    class DeviceManager_VK : public DeviceManager
    {
    public:
		DeviceManager_VK(Window* window, const DeviceParameters& params);

        bool EnumerateAdapters(std::vector<AdapterInfo> &outAdapters) override;

        bool CreateInstanceInternal() override;
        bool CreateDevice() override;
        bool CreateSwapChain() override;
        void DestroyDeviceAndSwapChain() override;

        void ResizeSwapChain() override;

        nvrhi::ITexture *GetCurrentBackBuffer() override;
        nvrhi::ITexture *GetBackBuffer(uint32_t index) override;
        nvrhi::ITexture *GetBackDepthBuffer(uint32_t index) override;
        uint32_t GetCurrentBackBufferIndex() override;
        uint32_t GetBackBufferCount() override;

        bool BeginFrame() override;
        bool Present() override;

        const char *GetRendererString() const override;
        bool IsVulkanInstanceExtensionEnabled(const char *extensionName) const override;
        bool IsVulkanDeviceExtensionEnabled(const char *extensionName) const override;
        bool IsVulkanLayerEnabled(const char *layerName) const override;

        void GetEnabledVulkanInstanceExtensions(std::vector<std::string> &extensions) const override;
        void GetEnabledVulkanDeviceExtensions(std::vector<std::string> &extensions) const override;
        void GetEnabledVulkanLayers(std::vector<std::string> &layers) const override;

        bool CreateInstance();
        bool CreateWindowSurface();
        void InstallDebugCallback();
        bool PickPhysicalDevice();
        bool FindQueueFamilies(vk::PhysicalDevice physicalDevice);
        bool CreateVkDevice();
        bool CreateVkSwapChain();
        void DestroySwapChain();
        void CreateDescriptorPool();

        void WaitForIdle() override;

        nvrhi::IDevice* GetDevice() const override;
        nvrhi::GraphicsAPI GetGraphicsAPI() const override;

        static DeviceManager_VK* GetInstance();


        struct VulkanExtensionSet
        {
            std::unordered_set<std::string> instance;
            std::unordered_set<std::string> layers;
            std::unordered_set<std::string> device;
        };

        // minimal set of required extensions
        VulkanExtensionSet enabledExtensions =
        {
            // instance
            { VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME },
            // layers
            { }, 
            // device
            { VK_KHR_MAINTENANCE1_EXTENSION_NAME } 
        };

        // optional extensions
        VulkanExtensionSet optionalExtensions =
        {
            // instance
            {
                VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
                VK_EXT_SAMPLER_FILTER_MINMAX_EXTENSION_NAME
            }, 
            // layers
            { },
            // device
            {
              VK_EXT_DEBUG_MARKER_EXTENSION_NAME,
              VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
              VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
              VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME,
              VK_KHR_MAINTENANCE_4_EXTENSION_NAME,
              VK_KHR_SWAPCHAIN_MUTABLE_FORMAT_EXTENSION_NAME,
              VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
              VK_NV_MESH_SHADER_EXTENSION_NAME,
             } 
        };

        std::unordered_set<std::string> m_RayTracingExtensions =
        {
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
            VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
            VK_KHR_RAY_QUERY_EXTENSION_NAME,
        };

        std::string m_RendererString;
        vk::Instance m_VulkanInstance;
        vk::DebugReportCallbackEXT m_DebugReportCallback;

        vk::PhysicalDevice m_VulkanPhysicalDevice;
        int m_GraphicsQueueFamily = -1;
        int m_ComputeQueueFamily = -1;
        int m_TransferQueueFamily = -1;
        int m_PresentQueueFamily = -1;

        vk::Device m_VulkanDevice;
        vk::Queue m_GraphicsQueue;
        vk::Queue m_ComputeQueue;
        vk::Queue m_TransferQueue;
        vk::Queue m_PresentQueue;

        vk::SurfaceKHR m_WindowSurface;
        vk::SurfaceFormatKHR m_SwapChainFormat;
        vk::SwapchainKHR m_SwapChain;
        vk::RenderPass m_RenderPass;
        vk::PipelineCache m_PipelineCache;
        vk::DescriptorPool m_DescriptorPool;

        bool m_SwapChainMutableFormatSupported = false;

        // Swapchain data
        std::vector<vk::Image> m_SwapchainImages;
        std::vector<Ref<RenderTarget>> m_SwapChainRenderTargets;
        uint32_t m_SwapChainIndex = static_cast<u32>(-1);

        nvrhi::vulkan::DeviceHandle m_NvrhiDevice;
        nvrhi::DeviceHandle m_ValidationLayer;

        std::vector<vk::Semaphore> m_AcquireSemaphores;
        std::vector<vk::Semaphore> m_PresentSemaphores;

        uint32_t m_AcquireSemaphoreIndex = 0;
        uint32_t m_PresentSemaphoreIndex = 0;

        std::queue<nvrhi::EventQueryHandle> m_FramesInFlight;
        std::vector<nvrhi::EventQueryHandle> m_QueryPool;

        bool m_BufferDeviceAddressSupported = false;

#if defined(VK_HEADER_VERSION) && (VK_HEADER_VERSION >= 301)
        typedef vk::detail::DynamicLoader VulkanDynamicLoader;
#else
        typedef vk::DynamicLoader VulkanDynamicLoader;
#endif

        std::unique_ptr<VulkanDynamicLoader> m_DynamicLoader;
    };
}
