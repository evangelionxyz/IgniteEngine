// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "ignite/graphics/window.hpp"
#include "ignite/graphics/texture.hpp"
#include "ignite/core/profiler/profiler.hpp"

#include "device_manager_vk.hpp"
#include "device_manager.hpp"
// #include "ignite/graphics/bindless_system.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"

#include <SDL3/SDL_vulkan.h>

// Define the Vulkan dynamic dispatcher - this needs to occur in exactly one cpp file in the program.
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace ignite
{
    static constexpr uint32_t kComputeQueueIndex = 0;
    static constexpr uint32_t kGraphicsQueueIndex = 0;
    static constexpr uint32_t kPresentQueueIndex = 0;
    static constexpr uint32_t kTransferQueueIndex = 0;

#define CHECK(a) if (!(a)) { return false; }

    static std::vector<const char *> StringSetToVector(const std::unordered_set<std::string> &set)
    {
        std::vector<const char *> ret;
        ret.reserve(set.size());
        for (const auto &s : set)
            ret.push_back(s.c_str());
        return ret;
    }

    template<typename T>
    static std::vector<T> SetToVector(const std::unordered_set<T> &set)
    {
        std::vector<T> ret;
        ret.reserve(set.size());
        for (const auto &s : set)
            ret.push_back(s);
        return ret;
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL VkDebugReportCallback(vk::DebugReportFlagsEXT flags, vk::DebugReportObjectTypeEXT objType,
        uint64_t obj, size_t location, int32_t code, const char *layerPrefix, const char *msg, void *userData)
    {
        if (auto manager = (DeviceManager_VK*)userData)
        {
            const auto &ignored = manager->GetDeviceParameters().ignoreVulkanValidationMessageLocations;
            const auto found = std::find(ignored.begin(), ignored.end(), location);
            if (found != ignored.end())
                return VK_FALSE;
        }

        LOG_WARN("[VULKAN: Location={} code {}, layerPrefix='{}'] \n\t{}\n", location, code, layerPrefix, msg);
        return VK_FALSE;
    }

    static DeviceManager_VK *s_VulkanDeviceInstance = nullptr;

    DeviceManager *DeviceManager::CreateVK(Window *window, const DeviceParameters &params)
    {
        s_VulkanDeviceInstance = new DeviceManager_VK(window, params);
        return s_VulkanDeviceInstance;
    }

    DeviceManager_VK *DeviceManager_VK::GetInstance()
    {
        return s_VulkanDeviceInstance;
    }

    DeviceManager_VK::DeviceManager_VK(Window* window, const DeviceParameters& params)
    {
        m_Window = window;
        m_DeviceParameters = params;
    }

    bool DeviceManager_VK::EnumerateAdapters(std::vector<AdapterInfo> &outAdapters)
    {
        if (!m_VulkanInstance)
            return false;

        std::vector<vk::PhysicalDevice> devices = m_VulkanInstance.enumeratePhysicalDevices();
        outAdapters.clear();

        for (auto physicalDevice : devices)
        {
            vk::PhysicalDeviceProperties2 properties2;
            vk::PhysicalDeviceIDProperties idProperties;
            properties2.pNext = &idProperties;
            physicalDevice.getProperties2(&properties2);

            auto const &properties = properties2.properties;

            AdapterInfo adapterInfo;
            adapterInfo.name = properties.deviceName.data();
            adapterInfo.vendorID = properties.vendorID;
            adapterInfo.deviceID = properties.deviceID;
            adapterInfo.vkPhysicalDevice = physicalDevice;
            adapterInfo.dedicatedVideoMemory = 0;

            std::array<uint8_t, 16> uuid;
            static_assert(uuid.size() == idProperties.deviceUUID.size());
            memcpy(uuid.data(), idProperties.deviceUUID.data(), uuid.size());
            adapterInfo.uuid = uuid;

            if (idProperties.deviceLUIDValid)
            {
                std::array<uint8_t, 8> luid;
                static_assert(luid.size() == idProperties.deviceLUID.size());
                memcpy(luid.data(), idProperties.deviceLUID.data(), luid.size());
                adapterInfo.luid = luid;
            }

            // Go through the memory types to figure out the amount of VRAM on this physical device.
            vk::PhysicalDeviceMemoryProperties memoryProperties = physicalDevice.getMemoryProperties();
            for (uint32_t heapIndex = 0; heapIndex < memoryProperties.memoryHeapCount; ++heapIndex)
            {
                vk::MemoryHeap const &heap = memoryProperties.memoryHeaps[heapIndex];
                if (heap.flags & vk::MemoryHeapFlagBits::eDeviceLocal)
                {
                    adapterInfo.dedicatedVideoMemory += heap.size;
                }
            }

            outAdapters.push_back(std::move(adapterInfo));
        }

        return true;
    }

    bool DeviceManager_VK::CreateInstanceInternal()
    {
        if (m_DeviceParameters.enableDebugRuntime)
        {
            enabledExtensions.instance.insert(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
            enabledExtensions.instance.insert(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

            enabledExtensions.layers.insert("VK_LAYER_KHRONOS_validation");
            // "VK_LAYER_KHRONOS_synchronization2" is deprecated and no longer ships with the modern
            // validation layers; synchronization validation is requested through
            // "VK_EXT_validation_features" instead (see below).
        }

        if (m_DeviceParameters.enableDebugRuntime
            || IsVulkanSyncValidationRequested()
            || IsVulkanGpuValidationRequested())
        {
            enabledExtensions.layers.insert("VK_LAYER_KHRONOS_validation");
            // All per-check toggles below are delivered as VkLayerSettingsCreateInfoEXT, which needs this
            // instance extension. It lives on the validation layer, so it is resolved in "CreateInstance".
            optionalExtensions.instance.insert(VK_EXT_LAYER_SETTINGS_EXTENSION_NAME);
        }

        if (IsVulkanSyncValidationRequested())
        {
            optionalExtensions.instance.insert(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
        }

        m_DynamicLoader = std::make_unique<VulkanDynamicLoader>(m_DeviceParameters.vulkanLibraryName);

        PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = m_DynamicLoader->getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");

        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

        return CreateInstance();
    }

    bool DeviceManager_VK::CreateDevice()
    {
        if (m_DeviceParameters.enableDebugRuntime)
        {
            InstallDebugCallback();
        }

        // add device extensions requested by the user
        for (const std::string &name : m_DeviceParameters.requiredVulkanDeviceExtensions)
        {
            enabledExtensions.device.insert(name);
        }
        for (const std::string &name : m_DeviceParameters.optionalVulkanDeviceExtensions)
        {
            optionalExtensions.device.insert(name);
        }

        if (!m_DeviceParameters.headlessDevice && !m_DeviceParameters.offscreen)
        {
            // Need to adjust the swap chain format before creating the device because it affects physical device selection
            if (m_DeviceParameters.swapChainFormat == nri::Format::RGBA8_SRGB)
                m_DeviceParameters.swapChainFormat = nri::Format::BGRA8_SRGB;
            else if (m_DeviceParameters.swapChainFormat == nri::Format::RGBA8_UNORM)
                m_DeviceParameters.swapChainFormat = nri::Format::BGRA8_UNORM;

            CHECK(CreateWindowSurface())
        }

        CHECK(PickPhysicalDevice())
        CHECK(FindQueueFamilies(m_VulkanPhysicalDevice))
        CHECK(CreateVkDevice())

        CreateDescriptorPool();

        auto vecInstanceExt = StringSetToVector(enabledExtensions.instance);
        auto vecLayers = StringSetToVector(enabledExtensions.layers);
        auto vecDeviceExt = StringSetToVector(enabledExtensions.device);

        std::vector<nri::QueueFamilyVKDesc> queueFamilies;
        queueFamilies.push_back({ 1, nri::QueueType::GRAPHICS, static_cast<uint32_t>(m_GraphicsQueueFamily) });
        if (m_DeviceParameters.enableComputeQueue && m_ComputeQueueFamily != -1)
        {
            queueFamilies.push_back({ 1, nri::QueueType::COMPUTE, static_cast<uint32_t>(m_ComputeQueueFamily) });
        }
        if (m_DeviceParameters.enableCopyQueue && m_TransferQueueFamily != -1)
        {
            queueFamilies.push_back({ 1, nri::QueueType::COPY, static_cast<uint32_t>(m_TransferQueueFamily) });
        }

        nri::VKExtensions vkExt{};
        vkExt.instanceExtensions = vecInstanceExt.data();
        vkExt.instanceExtensionNum = static_cast<uint32_t>(vecInstanceExt.size());
        vkExt.deviceExtensions = vecDeviceExt.data();
        vkExt.deviceExtensionNum = static_cast<uint32_t>(vecDeviceExt.size());

        nri::DeviceCreationVKDesc deviceCreationVKDesc{};
        deviceCreationVKDesc.callbackInterface = DefaultMessageCallback::GetInterface();
        deviceCreationVKDesc.vkInstance = (VKHandle)(VkInstance)m_VulkanInstance;
        deviceCreationVKDesc.vkDevice = (VKHandle)(VkDevice)m_VulkanDevice;
        deviceCreationVKDesc.vkPhysicalDevice = (VKHandle)(VkPhysicalDevice)m_VulkanPhysicalDevice;
        deviceCreationVKDesc.queueFamilies = queueFamilies.data();
        deviceCreationVKDesc.queueFamilyNum = static_cast<uint32_t>(queueFamilies.size());
        deviceCreationVKDesc.minorVersion = 2;
        deviceCreationVKDesc.vkExtensions = vkExt;
        deviceCreationVKDesc.vkBindingOffsets = { 128, 0, 256, 384 };
        deviceCreationVKDesc.enableNRIValidation = m_DeviceParameters.enableNRIValidation || m_DeviceParameters.enableNvrhiValidationLayer;

        nri::Result res = nriCreateDeviceFromVKDevice(deviceCreationVKDesc, m_Device);
        if (res != nri::Result::SUCCESS)
        {
            LOG_ERROR("Failed to create NRI device from Vulkan device");
            return false;
        }

        if (nriGetInterface(*m_Device, NRI_INTERFACE(nri::CoreInterface), &m_NRI) != nri::Result::SUCCESS)
        {
            LOG_ERROR("Failed to get NRI CoreInterface");
            return false;
        }

        if (nriGetInterface(*m_Device, NRI_INTERFACE(nri::HelperInterface), &m_HelperNRI) != nri::Result::SUCCESS)
        {
            LOG_ERROR("Failed to get NRI HelperInterface");
            return false;
        }

        if (nriGetInterface(*m_Device, NRI_INTERFACE(nri::WrapperVKInterface), &m_WrapperVK) != nri::Result::SUCCESS)
        {
            LOG_ERROR("Failed to get NRI WrapperVKInterface");
            return false;
        }

        if (m_NRI.GetQueue(*m_Device, nri::QueueType::GRAPHICS, 0, m_NriGraphicsQueue) != nri::Result::SUCCESS)
        {
            LOG_ERROR("Failed to get NRI graphics queue");
            return false;
        }

        const auto &devDesc = m_NRI.GetDeviceDesc(*m_Device);
        LOG_INFO("[Vulkan Device] Initialized NRI device with {} (Adapter: {})", nriGetGraphicsAPIString(devDesc.graphicsAPI), devDesc.adapterDesc.name);

        return true;
    }

    bool DeviceManager_VK::CreateSwapChain()
    {
        if (m_DeviceParameters.headlessDevice)
            return true;

        if (m_DeviceParameters.offscreen)
        {
            return CreateOffscreenBuffers();
        }

        CHECK(CreateVkSwapChain())

        if (m_PresentSemaphores.empty())
        {
            const uint32_t maxFrames = m_DeviceParameters.maxFramesInFlight;
            m_PresentSemaphores.reserve(maxFrames);
            m_AcquireSemaphores.reserve(maxFrames);
            m_InFlightFences.reserve(maxFrames);
            for (uint32_t i = 0; i < maxFrames; ++i)
            {
                m_PresentSemaphores.push_back(m_VulkanDevice.createSemaphore(vk::SemaphoreCreateInfo()));
                m_AcquireSemaphores.push_back(m_VulkanDevice.createSemaphore(vk::SemaphoreCreateInfo()));
                m_InFlightFences.push_back(m_VulkanDevice.createFence(vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled)));
            }
        }

        return true;
    }

    void DeviceManager_VK::DestroyDeviceAndSwapChain()
    {
        WaitForIdle();

        if (m_DeviceParameters.offscreen)
        {
            DestroyOffscreenBuffers();
        }
        else
        {
            DestroySwapChain();
        }

        for (auto &semaphore : m_PresentSemaphores)
        {
            if (semaphore)
            {
                m_VulkanDevice.destroySemaphore(semaphore);
                semaphore = nullptr;
            }
        }
        m_PresentSemaphores.clear();

        for (auto &semaphore : m_AcquireSemaphores)
        {
            if (semaphore)
            {
                m_VulkanDevice.destroySemaphore(semaphore);
                semaphore = nullptr;
            }
        }
        m_AcquireSemaphores.clear();

        for (auto &fence : m_InFlightFences)
        {
            if (fence)
            {
                m_VulkanDevice.destroyFence(fence);
                fence = nullptr;
            }
        }
        m_InFlightFences.clear();

        if (m_DescriptorPool)
        {
            m_VulkanDevice.destroyDescriptorPool(m_DescriptorPool);
            m_DescriptorPool = nullptr;
        }

        m_RendererString.clear();

        if (m_Device)
        {
            m_NRI.DeviceWaitIdle(m_Device);
            nriDestroyDevice(m_Device);
            m_Device = nullptr;
            m_NriGraphicsQueue = nullptr;
        }

        if (m_VulkanDevice)
        {
            m_VulkanDevice.destroy();
            m_VulkanDevice = nullptr;
        }

        // destroy instance
        if (m_WindowSurface)
        {
            assert(m_VulkanInstance);
            m_VulkanInstance.destroySurfaceKHR(m_WindowSurface);
            m_WindowSurface = nullptr;
        }

        if (m_DebugReportCallback)
        {
            m_VulkanInstance.destroyDebugReportCallbackEXT(m_DebugReportCallback);
            m_DebugReportCallback = nullptr;
        }

        if (m_VulkanInstance)
        {
            m_VulkanInstance.destroy();
            m_VulkanInstance = nullptr;
        }
    }

    void DeviceManager_VK::ResizeSwapChain()
    {
        if (m_VulkanDevice)
        {
            if (m_DeviceParameters.offscreen)
            {
                CreateOffscreenBuffers();
            }
            else
            {
                CreateVkSwapChain();
            }
        }
    }

    nri::Texture *DeviceManager_VK::GetCurrentBackBuffer()
    {
        return GetBackBuffer(GetCurrentBackBufferIndex());
    }

    nri::Texture *DeviceManager_VK::GetBackBuffer(uint32_t index)
    {
        if (index < m_BackBufferTextures.size())
            return m_BackBufferTextures[index];
        return nullptr;
    }

    nri::Texture *DeviceManager_VK::GetBackDepthBuffer(uint32_t index)
    {
        if (index < m_DepthBufferTextures.size())
            return m_DepthBufferTextures[index];
        return nullptr;
    }

    nri::Descriptor *DeviceManager_VK::GetCurrentBackBufferRTV()
    {
        return GetBackBufferRTV(GetCurrentBackBufferIndex());
    }

    nri::Descriptor *DeviceManager_VK::GetBackBufferRTV(uint32_t index)
    {
        if (index < m_BackBufferRTVs.size())
            return m_BackBufferRTVs[index];
        return nullptr;
    }

    nri::Descriptor *DeviceManager_VK::GetCurrentBackDepthBufferDSV()
    {
        return GetBackDepthBufferDSV(GetCurrentBackBufferIndex());
    }

    nri::Descriptor *DeviceManager_VK::GetBackDepthBufferDSV(uint32_t index)
    {
        if (index < m_DepthBufferDSVs.size())
            return m_DepthBufferDSVs[index];
        return nullptr;
    }

    uint32_t DeviceManager_VK::GetCurrentBackBufferIndex()
    {
        return m_SwapChainIndex;
    }

    uint32_t DeviceManager_VK::GetBackBufferCount()
    {
        return static_cast<uint32_t>(m_BackBufferTextures.size());
    }

    RenderTarget *DeviceManager_VK::GetCurrentBackBufferRenderTarget()
    {
        return GetBackBufferRenderTarget(GetCurrentBackBufferIndex());
    }

    RenderTarget *DeviceManager_VK::GetBackBufferRenderTarget(uint32_t index)
    {
        if (index < m_BackBufferRenderTargets.size())
            return m_BackBufferRenderTargets[index].get();
        return nullptr;
    }

    bool DeviceManager_VK::BeginFrame()
    {
        if (m_DeviceParameters.headlessDevice)
            return true;

        if (m_DeviceParameters.offscreen)
        {
            if (m_BackBufferTextures.empty())
                return false;

            m_SwapChainIndex = (m_SwapChainIndex + 1) % m_BackBufferTextures.size();
            return true;
        }

        if (m_InFlightFences.empty())
            return false;

        // Wait for previous frame in flight
        (void)m_VulkanDevice.waitForFences(1, &m_InFlightFences[m_CurrentFrame], VK_TRUE, UINT64_MAX);
        vk::Result resetRes = m_VulkanDevice.resetFences(1, &m_InFlightFences[m_CurrentFrame]); (void)resetRes;

        const auto &acquireSemaphore = m_AcquireSemaphores[m_CurrentFrame];

        vk::Result res = vk::Result::eErrorUnknown;
        if (m_SwapChain)
        {
            for (int attempt = 0; attempt < 3; ++attempt)
            {
                res = m_VulkanDevice.acquireNextImageKHR(
                    m_SwapChain,
                    std::numeric_limits<uint64_t>::max(),
                    acquireSemaphore,
                    vk::Fence(),
                    &m_SwapChainIndex);

                if (res == vk::Result::eErrorOutOfDateKHR)
                {
                    ResizeSwapChain();
                    return false;
                }

                if (res == vk::Result::eSuccess || res == vk::Result::eSuboptimalKHR)
                    break;
            }
        }
        

        if (res == vk::Result::eSuccess || res == vk::Result::eSuboptimalKHR)
        {
            // Submit acquire semaphore wait on graphics queue so all GPU work submitted this frame waits for image acquisition.
            // The very first thing the frame does is transition the acquired image out of its present/undefined
            // layout. That transition is recorded in a separate command buffer, and it is not guaranteed to be
            // ordered after an acquire wait narrowed to color-attachment-output or transfer stages, so wait at all stages.
            vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eAllCommands;
            vk::SubmitInfo submitInfo{};
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = &acquireSemaphore;
            submitInfo.pWaitDstStageMask = &waitStage;

            {
                std::lock_guard<std::mutex> queueLock(GPUUploadSync::GetQueueMutex());
                vk::Result submitRes = m_GraphicsQueue.submit(1, &submitInfo, vk::Fence()); (void)submitRes;
            }

            return true;
        }

        return false;
    }

    bool DeviceManager_VK::Present()
    {
        if (m_DeviceParameters.headlessDevice)
            return true;

        if (m_DeviceParameters.offscreen)
        {
            m_FrameIndex++;
            return true;
        }

        IGN_PROFILE_FUNCTION();
        const auto &presentSemaphore = m_PresentSemaphores[m_CurrentFrame];

        // Submit sync on graphics queue to signal present semaphore and in-flight fence after rendering
        vk::SubmitInfo submitInfo{};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &presentSemaphore;

        {
            std::lock_guard<std::mutex> queueLock(GPUUploadSync::GetQueueMutex());
            vk::Result submitRes = m_GraphicsQueue.submit(1, &submitInfo, m_InFlightFences[m_CurrentFrame]); (void)submitRes;
        }

        vk::PresentInfoKHR info = vk::PresentInfoKHR()
            .setWaitSemaphoreCount(1)
            .setPWaitSemaphores(&presentSemaphore)
            .setSwapchainCount(1)
            .setPSwapchains(&m_SwapChain)
            .setPImageIndices(&m_SwapChainIndex);

        vk::Result res;
        {
            std::lock_guard<std::mutex> queueLock(GPUUploadSync::GetQueueMutex());
            IGN_PROFILE_SCOPE("DeviceManager_VK::Present::PresentKHR");
            res = m_PresentQueue.presentKHR(&info);
        }

        if (res == vk::Result::eErrorOutOfDateKHR || res == vk::Result::eSuboptimalKHR)
        {
            ResizeSwapChain();
        }

        m_CurrentFrame = (m_CurrentFrame + 1) % m_DeviceParameters.maxFramesInFlight;
        m_FrameIndex++;

        return (res == vk::Result::eSuccess || res == vk::Result::eSuboptimalKHR);
    }

    const char *DeviceManager_VK::GetRendererString() const
    {
        return m_RendererString.c_str();
    }

    bool DeviceManager_VK::IsVulkanInstanceExtensionEnabled(const char *extensionName) const
    {
        return enabledExtensions.instance.contains(extensionName);
    }

    bool DeviceManager_VK::IsVulkanDeviceExtensionEnabled(const char *extensionName) const
    {
        return enabledExtensions.device.contains(extensionName);
    }

    bool DeviceManager_VK::IsVulkanLayerEnabled(const char *layerName) const
    {
        return enabledExtensions.layers.contains(layerName);
    }

    void DeviceManager_VK::GetEnabledVulkanInstanceExtensions(std::vector<std::string> &extensions) const
    {
        for (const auto &ext : enabledExtensions.instance)
            extensions.push_back(ext);
    }

    void DeviceManager_VK::GetEnabledVulkanDeviceExtensions(std::vector<std::string> &extensions) const
    {
        for (const auto &ext : enabledExtensions.device)
            extensions.push_back(ext);
    }

    void DeviceManager_VK::GetEnabledVulkanLayers(std::vector<std::string> &layers) const
    {
        for (const auto &ext : enabledExtensions.layers)
            layers.push_back(ext);
    }

    namespace
    {
        // Reads a boolean-ish environment variable. Returns "-1" when it is not set.
        int ReadBoolEnv(const char *name)
        {
            const char *raw = std::getenv(name);
            if (!raw)
                return -1;

            const std::string_view text(raw);
            return (text == "1" || text == "true" || text == "on") ? 1 : 0;
        }
    }

    bool DeviceManager_VK::IsVulkanSyncValidationRequested() const
    {
        if (m_DeviceParameters.enableVulkanSyncValidation || m_DeviceParameters.vkValidation.synchronization)
            return true;

        return ReadBoolEnv("IGNITE_VK_SYNC_VALIDATION") == 1;
    }

    bool DeviceManager_VK::IsVulkanGpuValidationRequested() const
    {
        if (m_DeviceParameters.enableGPUValidation || m_DeviceParameters.vkValidation.gpuAssisted)
            return true;

        return ReadBoolEnv("IGNITE_VK_GPU_VALIDATION") == 1;
    }

    void DeviceManager_VK::CollectValidationLayerSettings(std::vector<vk::LayerSettingEXT> &settings,
        std::vector<VkBool32> &valueStorage) const
    {
        settings.clear();
        valueStorage.clear();

        const VulkanValidationSettings &cfg = m_DeviceParameters.vkValidation;

        // GPU-AV and core validation running together is extremely slow and the layer reports a
        // VALIDATION-SETTINGS warning about it. GPU-AV takes precedence; core validation is only
        // enabled while GPU-AV is off.
        const bool gpuAssisted = IsVulkanGpuValidationRequested();
        const bool coreValidation = cfg.core && !gpuAssisted;

        struct Entry
        {
            const char *name;
            bool enabled;
        };

        const Entry entries[] = {
            { "validate_core", coreValidation },
            { "validate_sync", IsVulkanSyncValidationRequested() },
            { "validate_best_practices", cfg.bestPractices },
            { "thread_safety", cfg.threadSafety },
            { "stateless_param", cfg.statelessParameters },
            { "object_lifetime", cfg.objectLifetime },
            { "unique_handles", cfg.uniqueHandles },
            // Synchronization validation always parses SPIR-V to learn read/write access, so it warns when
            // shader validation is explicitly off. Keep it on while sync validation runs.
            { "check_shaders", cfg.shaderValidation || IsVulkanSyncValidationRequested() },
            { "gpuav_enable", gpuAssisted },
            { "printf_enable", cfg.debugPrintf },
            // GPU-AV sub-checks for features this engine never enables. Leaving them at the layer default
            // ("on") makes the layer announce that it had to disable unsupported instrumentation itself.
            // Note: this layer version has no "gpuav_validate_ray_hit_object" setting key - only the
            // internal flag behind "gpuav_validate_ray_query" and "gpuav_mesh_shading" exist.
            { "gpuav_validate_ray_query", gpuAssisted && cfg.gpuAssistedRayQuery },
            { "gpuav_mesh_shading", gpuAssisted && cfg.gpuAssistedMeshShading },
        };

        // The layer dereferences "pValues" lazily during "vkCreateInstance", so the backing storage must
        // outlive this call. It belongs to the caller's vector for that reason.
        valueStorage.reserve(std::size(entries));
        for (const Entry &entry : entries)
            valueStorage.push_back(entry.enabled ? VK_TRUE : VK_FALSE);

        settings.reserve(std::size(entries));
        for (size_t i = 0; i < std::size(entries); ++i)
        {
            settings.push_back(vk::LayerSettingEXT{
                "VK_LAYER_KHRONOS_validation",
                entries[i].name,
                vk::LayerSettingTypeEXT::eBool32,
                1,
                &valueStorage[i] });
        }
    }

    bool DeviceManager_VK::CreateInstance()
    {
        if (!m_DeviceParameters.headlessDevice)
        {
            uint32_t extCount;
            const auto extensions = SDL_Vulkan_GetInstanceExtensions(&extCount);
            LOG_ASSERT(extensions, " Failed to get required instance extensions");

            for (uint32_t i = 0; i < extCount; ++i)
            {
                enabledExtensions.instance.insert(std::string(extensions[i]));
            }
        }

        // add instance extensions requested by the user
        for (const std::string& name : m_DeviceParameters.requiredVulkanInstanceExtensions)
        {
            enabledExtensions.instance.insert(name);
        }

        for (const std::string& name : m_DeviceParameters.optionalVulkanInstanceExtensions)
        {
            optionalExtensions.instance.insert(name);
        }

        // add layers requested by the user
        for (const std::string& name : m_DeviceParameters.requiredVulkanLayers)
        {
            enabledExtensions.layers.insert(name);
        }

        for (const std::string& name : m_DeviceParameters.optionalVulkanLayers)
        {
            optionalExtensions.layers.insert(name);
        }

        std::unordered_set<std::string> requiredExtensions = enabledExtensions.instance;

        // figure out which optional extensions are supported
        for (const auto& instanceExt : vk::enumerateInstanceExtensionProperties())
        {
            const std::string name = instanceExt.extensionName;
            if (optionalExtensions.instance.contains(name))
                enabledExtensions.instance.insert(name);

            requiredExtensions.erase(name);
        }
        if (enabledExtensions.layers.contains("VK_LAYER_KHRONOS_validation"))
        {
            try
            {
                for (const auto& layerExt : vk::enumerateInstanceExtensionProperties(std::string("VK_LAYER_KHRONOS_validation")))
                {
                    const std::string name = layerExt.extensionName;
                    if (optionalExtensions.instance.contains(name))
                        enabledExtensions.instance.insert(name);

                    requiredExtensions.erase(name);
                }
            }
            catch (...)
            {
            }
        }

        if (!requiredExtensions.empty())
        {
            std::stringstream ss;
            ss << "Cannot create a Vulkan instance because the following required extension(s) are note supported: ";
            for (const auto& ext : requiredExtensions)
            {
                ss << '\n' << ' - ' << ext;
            }

            LOG_ASSERT(false, "{}", ss.str());
            return false;
        }

        LOG_WARN("Enabled Vulkan instance extensions: ");
        for (const auto& ext : enabledExtensions.instance)
        {
            LOG_TRACE("    {}", ext);
        }

        std::unordered_set<std::string> requiredLayers = enabledExtensions.layers;
        LOG_WARN("Available Vulkan Layers: ");
        for (const auto& layer : vk::enumerateInstanceLayerProperties())
        {
            const std::string name = layer.layerName;
            LOG_TRACE("    {}", name);
            if (optionalExtensions.layers.contains(name))
            {
                enabledExtensions.layers.insert(name);
            }

            requiredLayers.erase(name);
        }

        if (!requiredLayers.empty())
        {
            std::stringstream ss;
            ss << "Cannot create a Vulkan instance because the following required layer(s) are not support: ";
            for (const auto& ext : requiredLayers)
                ss << '\n' << " - " << ext;
            LOG_ASSERT(false, "{}", ss.str());
            return false;
        }

        LOG_WARN("Enabled Vulkan layers: ");
        for (const auto& layer : enabledExtensions.layers)
        {
            LOG_TRACE("    {}", layer.c_str());
        }

        auto instanceExtVec = StringSetToVector(enabledExtensions.instance);
        auto layerVec = StringSetToVector(enabledExtensions.layers);

        auto applicationInfo = vk::ApplicationInfo();

        vk::Result res = vk::enumerateInstanceVersion(&applicationInfo.apiVersion);

        // query the Vulkan API version supported on the system to make sure we use at lease 1.3 when that's present.

        if (res != vk::Result::eSuccess)
        {
            std::string resultStr = vk::to_string(res);
            LOG_ASSERT(false, "Call to vkEnumerateInstanceVerion failed, error code {}", resultStr);
            return false;
        }

        const uint32_t minimumVulkanVersion = VK_MAKE_API_VERSION(0, 1, 4, 0);
        if (applicationInfo.apiVersion < minimumVulkanVersion)
        {
            LOG_ASSERT(false, "The Vulkan API version supported on the system ({}.{}.{}) is too low, at least {}.{}.{} is required",
                VK_API_VERSION_MAJOR(applicationInfo.apiVersion),
                VK_API_VERSION_MINOR(applicationInfo.apiVersion),
                VK_API_VERSION_PATCH(applicationInfo.apiVersion),
                VK_API_VERSION_MAJOR(minimumVulkanVersion),
                VK_API_VERSION_MINOR(minimumVulkanVersion),
                VK_API_VERSION_PATCH(minimumVulkanVersion)
            );
            return false;
        }

        LOG_INFO("Vulkan API version supported on the system ({}.{}.{})",
            VK_API_VERSION_MAJOR(applicationInfo.apiVersion),
            VK_API_VERSION_MINOR(applicationInfo.apiVersion),
            VK_API_VERSION_PATCH(applicationInfo.apiVersion)
        );

        // spec says: a non zero variant indicates the API is a variant of the Vulkan API and applications will typically need to modified to run against it.
        if (VK_API_VERSION_VARIANT(applicationInfo.apiVersion) != 0)
        {
            LOG_ASSERT(false, "The Vulkan API supported on the system uses an unexpected variant: {}", VK_API_VERSION_VARIANT(applicationInfo.apiVersion));
            return false;
        }

        // create the Vulkan Instance
        auto instanceCreateInfo = vk::InstanceCreateInfo()
            .setEnabledLayerCount(uint32_t(layerVec.size()))
            .setPpEnabledLayerNames(layerVec.data())
            .setEnabledExtensionCount(uint32_t(instanceExtVec.size()))
            .setPpEnabledExtensionNames(instanceExtVec.data())
            .setPApplicationInfo(&applicationInfo);

        vk::ValidationFeaturesEXT validationFeatures{};
        std::vector<vk::ValidationFeatureEnableEXT> enabledValidationFeatures;
        const bool useValidationFeatures = IsVulkanSyncValidationRequested()
            && enabledExtensions.instance.contains(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
        if (useValidationFeatures)
        {
            enabledValidationFeatures.push_back(vk::ValidationFeatureEnableEXT::eSynchronizationValidation);
            validationFeatures.setEnabledValidationFeatureCount(static_cast<uint32_t>(enabledValidationFeatures.size()));
            validationFeatures.setPEnabledValidationFeatures(enabledValidationFeatures.data());

            LOG_INFO("[Vulkan] Enabling VK_EXT_validation_features synchronization validation");
        }

        std::vector<vk::LayerSettingEXT> validationSettings;
        std::vector<VkBool32> validationSettingValues;
        vk::LayerSettingsCreateInfoEXT layerSettings{};
        const bool useLayerSettings = enabledExtensions.instance.contains(VK_EXT_LAYER_SETTINGS_EXTENSION_NAME);
        if (useLayerSettings)
        {
            CollectValidationLayerSettings(validationSettings, validationSettingValues);
            if (!validationSettings.empty())
            {
                layerSettings.setPSettings(validationSettings.data())
                    .setSettingCount(static_cast<uint32_t>(validationSettings.size()));

                LOG_INFO("[Vulkan] Layer Settings: ");
                for (size_t i = 0; i < validationSettings.size(); ++i)
                {
                    LOG_INFO("    {} {}", validationSettings[i].pSettingName,
                        validationSettingValues[i] ? "Enabled" : "Disabled");
                }
            }
        }

        // Chain the validation structures (locals live until "vkCreateInstance" returns).
        void *validationChain = nullptr;
        if (!validationSettings.empty())
        {
            layerSettings.setPNext(validationChain);
            validationChain = &layerSettings;
        }
        if (useValidationFeatures)
        {
            validationFeatures.setPNext(validationChain);
            validationChain = &validationFeatures;
        }
        if (validationChain)
            instanceCreateInfo.setPNext(validationChain);

        res = vk::createInstance(&instanceCreateInfo, nullptr, &m_VulkanInstance);
        if (res != vk::Result::eSuccess)
        {
            std::string resultStr = vk::to_string(res);
            LOG_ASSERT(false, "Failed to create a Vulkan instance, error code: {}", resultStr);
        }

        VULKAN_HPP_DEFAULT_DISPATCHER.init(m_VulkanInstance);

        return true;
    }

    bool DeviceManager_VK::CreateWindowSurface()
    {
        bool success = SDL_Vulkan_CreateSurface(m_Window->GetWindowHandle(), m_VulkanInstance, nullptr, (VkSurfaceKHR*)&m_WindowSurface);
        LOG_ASSERT(success, "Failed to create SDL window surface, error code");

        return success;
    }

    void DeviceManager_VK::InstallDebugCallback()
    {
        constexpr auto flags =
              vk::DebugReportFlagBitsEXT::eError
#if _DEBUG
            | vk::DebugReportFlagBitsEXT::eDebug
#endif
            | vk::DebugReportFlagBitsEXT::eWarning
            | vk::DebugReportFlagBitsEXT::ePerformanceWarning;

        const auto info = vk::DebugReportCallbackCreateInfoEXT()
            .setFlags(flags)
            .setPfnCallback(VkDebugReportCallback)
            .setPUserData(this);

        vk::Result res = m_VulkanInstance.createDebugReportCallbackEXT(&info, nullptr, &m_DebugReportCallback);
        LOG_ASSERT(res == vk::Result::eSuccess, "Vulkan: Failed to create report debug callback");
    }

    bool DeviceManager_VK::PickPhysicalDevice()
    {
        const auto requestedFormat = static_cast<vk::Format>(nriConvertNRIFormatToVK(m_DeviceParameters.swapChainFormat));
        vk::Extent2D requestedExtent(m_DeviceParameters.backBufferWidth, m_DeviceParameters.backBufferHeight);

        auto devices = m_VulkanInstance.enumeratePhysicalDevices();
        int32_t adapterIndex = m_DeviceParameters.adapterIndex;

        int32_t firstDevice = 0;
        int32_t lastDevice = int32_t(devices.size()) - 1;

        if (adapterIndex >= 0)
        {
            LOG_ASSERT(adapterIndex <= lastDevice, "The specified Vulkan physical device {} does note exists.", adapterIndex);

            firstDevice = adapterIndex;
            lastDevice = adapterIndex;
        }

        std::stringstream errorStream;
        errorStream << "Cannot find a Vulkan device that supports all the required extensions and properties";

        // build a list of GPUs
        std::vector<vk::PhysicalDevice> discreteGPUs;
        std::vector<vk::PhysicalDevice> otherGPUs;
        for (int32_t deviceIndex = firstDevice; deviceIndex <= lastDevice; ++deviceIndex)
        {
            const vk::PhysicalDevice &dev = devices[deviceIndex];
            vk::PhysicalDeviceProperties prop = dev.getProperties();

            errorStream << '\n' << prop.deviceName.data() << ':';

            // check all required extensions are present
            std::unordered_set<std::string> requiredExtensions = enabledExtensions.device;
            auto deviceExtensions = dev.enumerateDeviceExtensionProperties();
            for (const auto &ext : deviceExtensions)
            {
                requiredExtensions.erase(std::string(ext.extensionName.data()));
            }

            bool deviceIsGood = true;

            if (!requiredExtensions.empty())
            {
                // device is missing one or more required extensions
                for (const auto &ext : requiredExtensions)
                {
                    errorStream << '\n' << "  - missing " << ext;
                }
                deviceIsGood = false;
            }

            auto deviceFeatures = dev.getFeatures();
            if (!deviceFeatures.samplerAnisotropy)
            {
                // device is toaster oven
                errorStream << '\n' << "  - does not support samplerAnisotropy";
                deviceIsGood = false;
            }

            if (!deviceFeatures.textureCompressionBC)
            {
                errorStream << '\n' << "  - does not support textureCompressionBC";
                deviceIsGood = false;
            }

            if (!FindQueueFamilies(dev))
            {
                // device does not have all the queue families we need
                errorStream << '\n' << "  - does note support the necessary queue types";
                deviceIsGood = false;
            }

            if (deviceIsGood && m_WindowSurface)
            {
                bool surfaceSupported = dev.getSurfaceSupportKHR(m_PresentQueueFamily, m_WindowSurface);
                if (!surfaceSupported)
                {
                    errorStream << '\n' << "  - does not support the window surface";
                    deviceIsGood = false;
                }
                else
                {
                    // check that this device supports out intended swap chain creation parameters
                    auto surfaceCaps = dev.getSurfaceCapabilitiesKHR(m_WindowSurface);
                    auto surfaceFmts = dev.getSurfaceFormatsKHR(m_WindowSurface);

                    if (surfaceCaps.minImageCount > m_DeviceParameters.swapChainBufferCount || (surfaceCaps.maxImageCount < m_DeviceParameters.swapChainBufferCount && surfaceCaps.maxImageCount > 0))
                    {
                        errorStream << '\n' << "  - cannot support the requested swap chain image count:";
                        errorStream << " requested " << m_DeviceParameters.swapChainBufferCount << ", available " << surfaceCaps.minImageCount << " - " << surfaceCaps.maxImageCount;

                        deviceIsGood = false;
                    }

                    if (surfaceCaps.minImageExtent.width > requestedExtent.width || surfaceCaps.minImageExtent.height > requestedExtent.height || surfaceCaps.maxImageExtent.width < requestedExtent.width || surfaceCaps.maxImageExtent.height < requestedExtent.height)
                    {
                        errorStream << '\n' << "  - cannot support requested swap chain image size:";
                        errorStream << " requested " << requestedExtent.width << "x" << requestedExtent.height << ", ";
                        errorStream << " available " << surfaceCaps.minImageExtent.width << "x" << surfaceCaps.minImageExtent.height;
                        errorStream << " - " << surfaceCaps.maxImageExtent.width << "x" << surfaceCaps.maxImageExtent.height;
                        deviceIsGood = false;
                    }

                    bool surfaceFormatPresent = false;
                    for (const vk::SurfaceFormatKHR &surfaceFmt : surfaceFmts)
                    {
                        if (surfaceFmt.format == requestedFormat)
                        {
                            surfaceFormatPresent = true;
                            break;
                        }
                    }

                    if (!surfaceFormatPresent)
                    {
                        // can't create a swapchain using the format requested
                        errorStream << '\n' << "  - does not support the requested swap chain format";
                        deviceIsGood = false;
                    }

                    const uint32_t canPresent = dev.getSurfaceSupportKHR(m_GraphicsQueueFamily, m_WindowSurface);
                    if (canPresent == 0)
                    {
                        errorStream << '\n' << "  - cannot present";
                        deviceIsGood = false;
                    }
                }
            }

            if (!deviceIsGood)
                continue;

            if (prop.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
            {
                discreteGPUs.push_back(dev);
            }
            else
            {
                otherGPUs.push_back(dev);
            }
        }

        // pick up first discrete GPU if it exists, otherwise the first integrated GPU
        if (!discreteGPUs.empty())
        {
            uint32_t selectedIndex = 0;
            m_VulkanPhysicalDevice = discreteGPUs[selectedIndex];

            return true;
        }

        if (!otherGPUs.empty())
        {
            uint32_t selectedIndex = 0;
            m_VulkanPhysicalDevice = otherGPUs[selectedIndex];

            return true;
        }

        LOG_ERROR("{}", errorStream.str());
        return false;
    }

    bool DeviceManager_VK::FindQueueFamilies(vk::PhysicalDevice physicalDevice)
    {
        auto props = physicalDevice.getQueueFamilyProperties();

        for (int i = 0; i < int(props.size()); i++)
        {
            const auto &queueFamily = props[i];

            if (m_GraphicsQueueFamily == -1)
            {
                if (queueFamily.queueCount > 0 &&
                    (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics))
                {
                    m_GraphicsQueueFamily = i;
                }
            }

            if (m_ComputeQueueFamily == -1)
            {
                if (queueFamily.queueCount > 0 &&
                    (queueFamily.queueFlags & vk::QueueFlagBits::eCompute) &&
                    !(queueFamily.queueFlags & vk::QueueFlagBits::eGraphics))
                {
                    m_ComputeQueueFamily = i;
                }
            }

            if (m_TransferQueueFamily == -1)
            {
                if (queueFamily.queueCount > 0 &&
                    (queueFamily.queueFlags & vk::QueueFlagBits::eTransfer) &&
                    !(queueFamily.queueFlags & vk::QueueFlagBits::eCompute) &&
                    !(queueFamily.queueFlags & vk::QueueFlagBits::eGraphics))
                {
                    m_TransferQueueFamily = i;
                }
            }

            if (m_PresentQueueFamily == -1 && !m_DeviceParameters.headlessDevice && !m_DeviceParameters.offscreen)
            {
                if (queueFamily.queueCount > 0 && SDL_Vulkan_GetPresentationSupport(m_VulkanInstance, physicalDevice, i))
                {
                    m_PresentQueueFamily = i;
                }
            }
        }

        if (m_GraphicsQueueFamily == -1 ||
            (m_PresentQueueFamily == -1 && !m_DeviceParameters.headlessDevice && !m_DeviceParameters.offscreen) ||
            (m_ComputeQueueFamily == -1 && m_DeviceParameters.enableComputeQueue) ||
            (m_TransferQueueFamily == -1 && m_DeviceParameters.enableCopyQueue))
        {
            return false;
        }

        return true;
    }

    bool DeviceManager_VK::CreateVkDevice()
    {
        optionalExtensions.device.insert(VK_EXT_MUTABLE_DESCRIPTOR_TYPE_EXTENSION_NAME);

        // figure out which optional extensions are supported
        auto deviceExtensions = m_VulkanPhysicalDevice.enumerateDeviceExtensionProperties();
        for (const auto &ext : deviceExtensions)
        {
            const std::string name = ext.extensionName;
            if (optionalExtensions.device.contains(name))
            {
                if (name == VK_KHR_SWAPCHAIN_MUTABLE_FORMAT_EXTENSION_NAME && m_DeviceParameters.headlessDevice)
                    continue;

                enabledExtensions.device.insert(name);
            }

            if (m_DeviceParameters.enableRayTracingExtensions && m_RayTracingExtensions.contains(name))
            {
                enabledExtensions.device.insert(name);
            }
        }

        if (!m_DeviceParameters.headlessDevice)
        {
            enabledExtensions.device.insert(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        }

        const vk::PhysicalDeviceProperties physicalDeviceProperties = m_VulkanPhysicalDevice.getProperties();
        m_RendererString = std::string(physicalDeviceProperties.deviceName.data());

        bool accelStructSupported = false;
        bool rayPipelineSupported = false;
        bool rayQuerySupported = false;
        bool meshletsSupported = false;
        bool vrsSupported = false;
        bool interlockSupported = false;
        bool barycentricSupported = false;
        bool storage16BitSupported = false;
        bool synchronization2Supported = false;
        bool maintenance4Supported = false;
        bool aftermathSupported = false;
        bool mutableDescriptorSupported = false;

        LOG_INFO("Enabled Vulkan device extensions: ");
        for (const auto &ext : enabledExtensions.device)
        {
            LOG_INFO("   {}", ext.c_str());

            if (ext == VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME)
                accelStructSupported = true;
            else if (ext == VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME)
                rayPipelineSupported = true;
            else if (ext == VK_KHR_RAY_QUERY_EXTENSION_NAME)
                rayQuerySupported = true;
            else if (ext == VK_NV_MESH_SHADER_EXTENSION_NAME)
                meshletsSupported = true;
            else if (ext == VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME)
                vrsSupported = true;
            else if (ext == VK_EXT_FRAGMENT_SHADER_INTERLOCK_EXTENSION_NAME)
                interlockSupported = true;
            else if (ext == VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME)
                barycentricSupported = true;
            else if (ext == VK_KHR_16BIT_STORAGE_EXTENSION_NAME)
                storage16BitSupported = true;
            else if (ext == VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME)
                synchronization2Supported = true;
            else if (ext == VK_KHR_MAINTENANCE_4_EXTENSION_NAME)
                maintenance4Supported = true;
            else if (ext == VK_KHR_SWAPCHAIN_MUTABLE_FORMAT_EXTENSION_NAME)
                m_SwapChainMutableFormatSupported = true;
            else if (ext == VK_NV_DEVICE_DIAGNOSTICS_CONFIG_EXTENSION_NAME)
                aftermathSupported = true;
            else if (ext == VK_EXT_MUTABLE_DESCRIPTOR_TYPE_EXTENSION_NAME)
                mutableDescriptorSupported = true;
        }

#define APPEND_EXTENSION(condition, desc) if (condition) { (desc).pNext = pNext; pNext = &(desc); }
        void *pNext = nullptr;

        vk::PhysicalDeviceFeatures2 physicalDeviceFeatures2;
        // determine support for buffer device address, the Vulkan 1.2 way
        auto bufferDeviceAddressFeatures = vk::PhysicalDeviceBufferDeviceAddressFeatures();
        // determine support for maintenance4
        auto maintenance4Features = vk::PhysicalDeviceMaintenance4Features();
        // determine support for aftermath
        auto aftermathPhysicalFeatures  = vk::PhysicalDeviceDiagnosticsConfigFeaturesNV();
        // determine support for Vulkan 1.2 features (update after bind, descriptor indexing, etc.)
        auto vulkan12PhysicalFeatures = vk::PhysicalDeviceVulkan12Features();
        // determine support for Vulkan 1.1 features (16-bit storage)
        auto vulkan11PhysicalFeatures = vk::PhysicalDeviceVulkan11Features();

        // put the user provided extension structure at the end of the chain
        pNext = m_DeviceParameters.physicalDeviceFeatures2Extensions;
        APPEND_EXTENSION(true, bufferDeviceAddressFeatures);
        APPEND_EXTENSION(maintenance4Supported, maintenance4Features);
        APPEND_EXTENSION(aftermathSupported, aftermathPhysicalFeatures);
        APPEND_EXTENSION(physicalDeviceProperties.apiVersion >= VK_API_VERSION_1_2, vulkan12PhysicalFeatures);
        APPEND_EXTENSION(physicalDeviceProperties.apiVersion >= VK_API_VERSION_1_1, vulkan11PhysicalFeatures);

        physicalDeviceFeatures2.pNext = pNext;
        m_VulkanPhysicalDevice.getFeatures2(&physicalDeviceFeatures2);

        std::unordered_set uniqueQueueFamilies = {
            m_GraphicsQueueFamily
        };

        if (!m_DeviceParameters.headlessDevice && !m_DeviceParameters.offscreen)
            uniqueQueueFamilies.insert(m_PresentQueueFamily);

        if (m_DeviceParameters.enableComputeQueue)
            uniqueQueueFamilies.insert(m_ComputeQueueFamily);

        if (m_DeviceParameters.enableCopyQueue)
            uniqueQueueFamilies.insert(m_TransferQueueFamily);

        f32 priority = 1.0f;
        std::vector<vk::DeviceQueueCreateInfo> queueDesc;
        queueDesc.reserve(uniqueQueueFamilies.size());

        for (int32_t queueFamily : uniqueQueueFamilies)
        {
            queueDesc.push_back(vk::DeviceQueueCreateInfo()
                .setQueueFamilyIndex(queueFamily)
                .setQueueCount(1)
                .setPQueuePriorities(&priority));
        }

        auto accelStructFeatures = vk::PhysicalDeviceAccelerationStructureFeaturesKHR()
            .setAccelerationStructure(true);
        auto rayPipelineFeatures = vk::PhysicalDeviceRayTracingPipelineFeaturesKHR()
            .setRayTracingPipeline(true)
            .setRayTraversalPrimitiveCulling(true);
        auto rayQueryFeatures = vk::PhysicalDeviceRayQueryFeaturesKHR()
            .setRayQuery(true);
        auto meshletFeatures = vk::PhysicalDeviceMeshShaderFeaturesNV()
            .setTaskShader(true)
            .setMeshShader(true);
        auto interlockFeatures = vk::PhysicalDeviceFragmentShaderInterlockFeaturesEXT()
            .setFragmentShaderPixelInterlock(true);
        auto barycentricFeatures = vk::PhysicalDeviceFragmentShaderBarycentricFeaturesKHR()
            .setFragmentShaderBarycentric(true);
        auto storage16BitFeatures = vk::PhysicalDevice16BitStorageFeatures()
            .setStorageBuffer16BitAccess(true);
        auto vrsFeatures = vk::PhysicalDeviceFragmentShadingRateFeaturesKHR()
            .setPipelineFragmentShadingRate(true)
            .setPrimitiveFragmentShadingRate(true)
            .setAttachmentFragmentShadingRate(true);
        auto vulkan13features = vk::PhysicalDeviceVulkan13Features()
            .setSynchronization2(synchronization2Supported)
            .setDynamicRendering(true)
            .setShaderDemoteToHelperInvocation(true)
            .setMaintenance4(maintenance4Features.maintenance4);
        auto aftermathFeatures = vk::DeviceDiagnosticsConfigCreateInfoNV()
            .setFlags(vk::DeviceDiagnosticsConfigFlagBitsNV::eEnableResourceTracking
            | vk::DeviceDiagnosticsConfigFlagBitsNV::eEnableShaderDebugInfo
            | vk::DeviceDiagnosticsConfigFlagBitsNV::eEnableShaderErrorReporting);
        auto mutableDescriptorFeatures = vk::PhysicalDeviceMutableDescriptorTypeFeaturesEXT()
            .setMutableDescriptorType(true);


        pNext = nullptr;
        APPEND_EXTENSION(accelStructSupported, accelStructFeatures)
        APPEND_EXTENSION(rayPipelineSupported, rayPipelineFeatures)
        APPEND_EXTENSION(rayQuerySupported, rayQueryFeatures)
        APPEND_EXTENSION(meshletsSupported, meshletFeatures)
        APPEND_EXTENSION(vrsSupported, vrsFeatures)
        APPEND_EXTENSION(interlockSupported, interlockFeatures)
        APPEND_EXTENSION(barycentricSupported, barycentricFeatures)
        APPEND_EXTENSION(storage16BitSupported, storage16BitFeatures)
        APPEND_EXTENSION(physicalDeviceProperties.apiVersion >= VK_API_VERSION_1_3, vulkan13features)
        APPEND_EXTENSION(physicalDeviceProperties.apiVersion < VK_API_VERSION_1_3 &&maintenance4Supported, maintenance4Features);
        APPEND_EXTENSION(aftermathSupported, aftermathFeatures);
        APPEND_EXTENSION(mutableDescriptorSupported, mutableDescriptorFeatures);

#undef APPEND_EXTENSION

        m_DepthClampSupported = physicalDeviceFeatures2.features.depthClamp;
        m_MultiViewportSupported = physicalDeviceFeatures2.features.multiViewport;

        auto deviceFeatures = vk::PhysicalDeviceFeatures()
            .setIndependentBlend(true)
            .setShaderImageGatherExtended(true)
            .setSamplerAnisotropy(true)
            .setTessellationShader(true)
            .setTextureCompressionBC(true)
            .setGeometryShader(true)
            .setImageCubeArray(true)
            .setShaderInt16(true)
            .setShaderInt64(physicalDeviceFeatures2.features.shaderInt64)
            .setFillModeNonSolid(true)
            .setFragmentStoresAndAtomics(true)
            .setDualSrcBlend(true)
            .setVertexPipelineStoresAndAtomics(true)
            .setDepthClamp(m_DepthClampSupported)
            .setMultiViewport(m_MultiViewportSupported);

        // add a Vulkan 1.1 structure with default settings to make it easier for apps to modify them
        //
        // "storageBuffer16BitAccess" is core Vulkan 1.1 functionality: on a 1.1+ device the feature can be
        // enabled without "VK_KHR_16bit_storage", so gating it on the extension alone would make the
        // validation layer force the feature on our behalf (the "validation is adjusting settings" warning).
        const bool storageBuffer16BitAccessSupported = vulkan11PhysicalFeatures.storageBuffer16BitAccess
            || (storage16BitSupported && physicalDeviceProperties.apiVersion < VK_API_VERSION_1_1);
        auto vulkan11features = vk::PhysicalDeviceVulkan11Features()
            .setStorageBuffer16BitAccess(storageBuffer16BitAccessSupported)
            .setPNext(pNext);

        // Vulkan memory model / scalar block layout / 8-bit storage are needed by the shader toolchain
        // (and by GPU-Assisted Validation). They are requested only when the device reports support, which
        // avoids the validation layer having to force them behind our back ("validation is adjusting
        // settings" warning).
        auto vulkan12features = vk::PhysicalDeviceVulkan12Features()
            .setDescriptorIndexing(vulkan12PhysicalFeatures.descriptorIndexing)
            .setRuntimeDescriptorArray(vulkan12PhysicalFeatures.runtimeDescriptorArray)
            .setDescriptorBindingPartiallyBound(vulkan12PhysicalFeatures.descriptorBindingPartiallyBound)
            .setDescriptorBindingVariableDescriptorCount(vulkan12PhysicalFeatures.descriptorBindingVariableDescriptorCount)
            .setDescriptorBindingSampledImageUpdateAfterBind(vulkan12PhysicalFeatures.descriptorBindingSampledImageUpdateAfterBind)
            .setDescriptorBindingStorageImageUpdateAfterBind(vulkan12PhysicalFeatures.descriptorBindingStorageImageUpdateAfterBind)
            .setDescriptorBindingStorageBufferUpdateAfterBind(vulkan12PhysicalFeatures.descriptorBindingStorageBufferUpdateAfterBind)
            .setDescriptorBindingUniformBufferUpdateAfterBind(vulkan12PhysicalFeatures.descriptorBindingUniformBufferUpdateAfterBind)
            .setDescriptorBindingUpdateUnusedWhilePending(vulkan12PhysicalFeatures.descriptorBindingUpdateUnusedWhilePending)
            .setTimelineSemaphore(vulkan12PhysicalFeatures.timelineSemaphore)
            .setShaderSampledImageArrayNonUniformIndexing(vulkan12PhysicalFeatures.shaderSampledImageArrayNonUniformIndexing)
            .setScalarBlockLayout(vulkan12PhysicalFeatures.scalarBlockLayout)
            .setVulkanMemoryModel(vulkan12PhysicalFeatures.vulkanMemoryModel)
            .setVulkanMemoryModelDeviceScope(vulkan12PhysicalFeatures.vulkanMemoryModelDeviceScope)
            .setStorageBuffer8BitAccess(vulkan12PhysicalFeatures.storageBuffer8BitAccess)
            .setBufferDeviceAddress(bufferDeviceAddressFeatures.bufferDeviceAddress)
            .setPNext(&vulkan11features);

        auto layerVec = StringSetToVector(enabledExtensions.layers);
        auto extVec = StringSetToVector(enabledExtensions.device);

        auto deviceDesc = vk::DeviceCreateInfo()
            .setPQueueCreateInfos(queueDesc.data())
            .setQueueCreateInfoCount(static_cast<uint32_t>(queueDesc.size()))
            .setPEnabledFeatures(&deviceFeatures)
            .setEnabledExtensionCount(static_cast<uint32_t>(extVec.size()))
            .setPpEnabledExtensionNames(extVec.data())
            .setPNext(&vulkan12features);

        if (m_DeviceParameters.deviceCreateInfoCallback)
            m_DeviceParameters.deviceCreateInfoCallback(deviceDesc);

        const vk::Result res = m_VulkanPhysicalDevice.createDevice(&deviceDesc, nullptr, &m_VulkanDevice);
        if (res != vk::Result::eSuccess)
        {
            LOG_ERROR("Failed to create a Vulkan physical device, error code: {}", vk::to_string(res));
            return false;
        }

        m_VulkanDevice.getQueue(m_GraphicsQueueFamily, kGraphicsQueueIndex, &m_GraphicsQueue);
        if (m_DeviceParameters.enableComputeQueue)
            m_VulkanDevice.getQueue(m_ComputeQueueFamily, kComputeQueueIndex, &m_ComputeQueue);
        if (m_DeviceParameters.enableCopyQueue)
            m_VulkanDevice.getQueue(m_TransferQueueFamily, kTransferQueueIndex, &m_TransferQueue);
        if (!m_DeviceParameters.headlessDevice && !m_DeviceParameters.offscreen)
            m_VulkanDevice.getQueue(m_PresentQueueFamily, kPresentQueueIndex, &m_PresentQueue);

        VULKAN_HPP_DEFAULT_DISPATCHER.init(m_VulkanDevice);

        // remember the bufferDeviceAddress feature enable
        m_BufferDeviceAddressSupported = vulkan12features.bufferDeviceAddress;

        LOG_INFO("Created Vulkan device: {}", m_RendererString.c_str());

        return true;
    }

    bool DeviceManager_VK::CreateVkSwapChain()
    {
        DestroySwapChain();

        m_SwapChainFormat =
        {
            vk::Format(nriConvertNRIFormatToVK(m_DeviceParameters.swapChainFormat)),
            vk::ColorSpaceKHR::eSrgbNonlinear
        };

        std::unordered_set<uint32_t> uniqueQueues =
        {
            uint32_t(m_GraphicsQueueFamily),
            uint32_t(m_PresentQueueFamily)
        };

        std::vector<uint32_t> queues = SetToVector(uniqueQueues);

        const bool enableSwapChainSharing = queues.size() > 1;

        vk::SurfaceCapabilitiesKHR capabilities = m_VulkanPhysicalDevice.getSurfaceCapabilitiesKHR(m_WindowSurface);

        // Get Surface Present Mode
        PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkPFN_vkGetPhysicalDeviceSurfacePresentModesKHRAddr = m_DynamicLoader->getProcAddress<PFN_vkGetPhysicalDeviceSurfacePresentModesKHR>("vkGetPhysicalDeviceSurfacePresentModesKHR");
        uint32_t presentModeCount = 0;
        vkPFN_vkGetPhysicalDeviceSurfacePresentModesKHRAddr(m_VulkanPhysicalDevice, m_WindowSurface, &presentModeCount, nullptr);
        std::vector<VkPresentModeKHR> presentModes(presentModeCount);
        vkPFN_vkGetPhysicalDeviceSurfacePresentModesKHRAddr(m_VulkanPhysicalDevice, m_WindowSurface, &presentModeCount, presentModes.data());

        const bool supportVSync = std::ranges::any_of(presentModes, [](const VkPresentModeKHR mode)
        {
            return mode == VK_PRESENT_MODE_FIFO_KHR;
        });

        const vk::PresentModeKHR optimalPresentMode = (m_DeviceParameters.vsyncEnable && supportVSync)
            ? vk::PresentModeKHR::eFifo
            : vk::PresentModeKHR::eImmediate;

        vk::Extent2D extent;
        if (capabilities.currentExtent.width != 0xFFFFFFFF)
        {
            extent = capabilities.currentExtent;
        }
        else
        {
            extent.width = std::clamp(m_DeviceParameters.backBufferWidth, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            extent.height = std::clamp(m_DeviceParameters.backBufferHeight, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        }

        if (extent.width == 0 || extent.height == 0)
        {
            return false;
        }

        m_DeviceParameters.backBufferWidth = extent.width;
        m_DeviceParameters.backBufferHeight = extent.height;

        auto desc = vk::SwapchainCreateInfoKHR()
            .setSurface(m_WindowSurface)
            .setMinImageCount(m_DeviceParameters.swapChainBufferCount)
            .setImageFormat(m_SwapChainFormat.format)
            .setImageColorSpace(m_SwapChainFormat.colorSpace)
            .setImageExtent(extent)
            .setImageArrayLayers(1)
            .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled)
            .setImageSharingMode(enableSwapChainSharing ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive)
            .setFlags(m_SwapChainMutableFormatSupported ? vk::SwapchainCreateFlagBitsKHR::eMutableFormat : vk::SwapchainCreateFlagBitsKHR(0))
            .setQueueFamilyIndexCount(enableSwapChainSharing ? uint32_t(queues.size()) : 0)
            .setPQueueFamilyIndices(enableSwapChainSharing ? queues.data() : nullptr)
            .setPreTransform(capabilities.currentTransform)
            .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
            .setPresentMode(optimalPresentMode)
            .setClipped(VK_TRUE)
            .setOldSwapchain(VK_NULL_HANDLE);

        std::vector<vk::Format> imageFormats = { m_SwapChainFormat.format };
        switch (m_SwapChainFormat.format)
        {
            case vk::Format::eR8G8B8A8Unorm:
                imageFormats.push_back(vk::Format::eR8G8B8A8Srgb);
                break;
            case vk::Format::eR8G8B8A8Srgb:
                imageFormats.push_back(vk::Format::eR8G8B8A8Unorm);
                break;
            case vk::Format::eB8G8R8A8Unorm:
                imageFormats.push_back(vk::Format::eB8G8R8A8Srgb);
                break;
            case vk::Format::eB8G8R8A8Srgb:
                imageFormats.push_back(vk::Format::eB8G8R8A8Unorm);
                break;
            default:
                break;
        }

        auto imageFormatListCreateInfo = vk::ImageFormatListCreateInfo()
            .setViewFormats(imageFormats);

        if (m_SwapChainMutableFormatSupported)
            desc.pNext = &imageFormatListCreateInfo;

        const vk::Result res = m_VulkanDevice.createSwapchainKHR(&desc, nullptr, &m_SwapChain);

        if (res != vk::Result::eSuccess)
        {
            LOG_ERROR("Failed to create a Vulkan swap chain, error code = {}", vk::to_string(res));
            return false;
        }

        // retrieve swap chain images
        m_SwapchainImages = m_VulkanDevice.getSwapchainImagesKHR(m_SwapChain);
        const uint32_t imageCount = static_cast<uint32_t>(m_SwapchainImages.size());

        m_BackBufferTextures.resize(imageCount, nullptr);
        m_BackBufferRTVs.resize(imageCount, nullptr);
        m_DepthBufferTextures.resize(imageCount, nullptr);
        m_DepthBufferMemories.resize(imageCount, nullptr);
        m_DepthBufferDSVs.resize(imageCount, nullptr);

        for (uint32_t n = 0; n < imageCount; ++n)
        {
            auto image = m_SwapchainImages[n];

            nri::TextureVKDesc texVKDesc{};
            texVKDesc.vkImage = (uint64_t)(VkImage)image;
            texVKDesc.vkFormat = (int32_t)m_SwapChainFormat.format;
            texVKDesc.vkImageType = (int32_t)VK_IMAGE_TYPE_2D;
            texVKDesc.vkImageUsageFlags = (uint32_t)(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
            texVKDesc.width = static_cast<nri::Dim_t>(m_DeviceParameters.backBufferWidth);
            texVKDesc.height = static_cast<nri::Dim_t>(m_DeviceParameters.backBufferHeight);
            texVKDesc.depth = 1;
            texVKDesc.mipNum = 1;
            texVKDesc.layerNum = 1;
            texVKDesc.sampleNum = 1;

            if (m_WrapperVK.CreateTextureVK(*m_Device, texVKDesc, m_BackBufferTextures[n]) != nri::Result::SUCCESS)
            {
                LOG_ERROR("Failed to wrap Vulkan swapchain image {} into NRI texture", n);
                return false;
            }

            m_NRI.SetDebugName((nri::Object *)m_BackBufferTextures[n], ("[BackBuffer Color " + std::to_string(n) + "]").c_str());
            nri::TextureViewDesc rtvDesc{};
            rtvDesc.texture = m_BackBufferTextures[n];
            rtvDesc.type = nri::TextureView::COLOR_ATTACHMENT;
            rtvDesc.format = m_DeviceParameters.swapChainFormat;
            rtvDesc.mipOffset = 0;
            rtvDesc.mipNum = 1;
            rtvDesc.layerOffset = 0;
            rtvDesc.layerNum = 1;
            if (m_NRI.CreateTextureView(rtvDesc, m_BackBufferRTVs[n]) != nri::Result::SUCCESS)
            {
                LOG_ERROR("Failed to create RTV descriptor for back buffer {}", n);
                return false;
            }

            // Depth Stencil texture
            nri::TextureDesc depthDesc{};
            depthDesc.type = nri::TextureType::TEXTURE_2D;
            depthDesc.usage = nri::TextureUsageBits::DEPTH_STENCIL_ATTACHMENT;
            depthDesc.format = nri::Format::D32_SFLOAT;
            depthDesc.width = static_cast<nri::Dim_t>(m_DeviceParameters.backBufferWidth);
            depthDesc.height = static_cast<nri::Dim_t>(m_DeviceParameters.backBufferHeight);
            depthDesc.depth = 1;
            depthDesc.mipNum = 1;
            depthDesc.layerNum = 1;
            depthDesc.sampleNum = 1;

            if (m_NRI.CreateTexture(*m_Device, depthDesc, m_DepthBufferTextures[n]) != nri::Result::SUCCESS)
            {
                LOG_ERROR("Failed to create depth texture for swapchain image {}", n);
                return false;
            }
            m_NRI.SetDebugName((nri::Object *)m_DepthBufferTextures[n], ("[BackBuffer Depth " + std::to_string(n) + "]").c_str());

            nri::ResourceGroupDesc resGroup{};
            resGroup.textureNum = 1;
            resGroup.textures = &m_DepthBufferTextures[n];
            resGroup.memoryLocation = nri::MemoryLocation::DEVICE;
            if (m_HelperNRI.AllocateAndBindMemory(*m_Device, resGroup, &m_DepthBufferMemories[n]) != nri::Result::SUCCESS)
            {
                LOG_ERROR("Failed to allocate and bind memory for depth texture {}", n);
                return false;
            }

            nri::TextureViewDesc dsvDesc{};
            dsvDesc.texture = m_DepthBufferTextures[n];
            dsvDesc.type = nri::TextureView::DEPTH_STENCIL_ATTACHMENT;
            dsvDesc.format = nri::Format::D32_SFLOAT;
            dsvDesc.mipOffset = 0;
            dsvDesc.mipNum = 1;
            dsvDesc.layerOffset = 0;
            dsvDesc.layerNum = 1;
            if (m_NRI.CreateTextureView(dsvDesc, m_DepthBufferDSVs[n]) != nri::Result::SUCCESS)
            {
                LOG_ERROR("Failed to create DSV descriptor for depth buffer {}", n);
                return false;
            }
        }

        m_BackBufferRenderTargets.resize(imageCount);
        for (uint32_t n = 0; n < imageCount; ++n)
        {
            RenderTargetCreateInfo rtDesc;
            rtDesc.width = m_DeviceParameters.backBufferWidth;
            rtDesc.height = m_DeviceParameters.backBufferHeight;
            rtDesc.attachments = {
                FramebufferAttachments {
                    .name = "[BackBufferColor " + std::to_string(n) + "]",
                    .format = m_DeviceParameters.swapChainFormat,
                    .state = nri::Layout::PRESENT,
                    .nativeObjectPtr = m_BackBufferTextures[n],
                    .isNativeObject = true,
                    .nativeObjectType = 2
                },
                FramebufferAttachments {
                    .name = "[BackBufferDepth " + std::to_string(n) + "]",
                    .format = nri::Format::D32_SFLOAT,
                    .state = nri::Layout::DEPTH_STENCIL_ATTACHMENT,
                    .nativeObjectPtr = m_DepthBufferTextures[n],
                    .isNativeObject = true,
                    .nativeObjectType = 2
                }
            };

            m_BackBufferRenderTargets[n] = RenderTarget::Create(rtDesc, "[BackBuffer RT " + std::to_string(n) + "]");
        }

        m_SwapChainIndex = 0;
        return true;
    }

    void DeviceManager_VK::DestroySwapChain()
    {
        WaitForIdle();

        m_BackBufferRenderTargets.clear();

        for (auto &rtv : m_BackBufferRTVs)
        {
            if (rtv)
            {
                m_NRI.DestroyDescriptor(rtv);
                rtv = nullptr;
            }
        }
        m_BackBufferRTVs.clear();

        for (auto &tex : m_BackBufferTextures)
        {
            if (tex)
            {
                m_NRI.DestroyTexture(tex);
                tex = nullptr;
            }
        }
        m_BackBufferTextures.clear();

        for (auto &dsv : m_DepthBufferDSVs)
        {
            if (dsv)
            {
                m_NRI.DestroyDescriptor(dsv);
                dsv = nullptr;
            }
        }
        m_DepthBufferDSVs.clear();

        for (auto &tex : m_DepthBufferTextures)
        {
            if (tex)
            {
                m_NRI.DestroyTexture(tex);
                tex = nullptr;
            }
        }
        m_DepthBufferTextures.clear();

        for (auto &mem : m_DepthBufferMemories)
        {
            if (mem)
            {
                m_NRI.FreeMemory(mem);
                mem = nullptr;
            }
        }
        m_DepthBufferMemories.clear();

        if (m_SwapChain)
        {
            m_VulkanDevice.destroySwapchainKHR(m_SwapChain);
            m_SwapChain = nullptr;
        }

        m_SwapchainImages.clear();
    }

    void DeviceManager_VK::CreateDescriptorPool()
    {
        vk::DescriptorPoolSize poolSizes[] =
        {
            { vk::DescriptorType::eCombinedImageSampler, 100 },
            { vk::DescriptorType::eSampledImage, 100 }
        };

        vk::DescriptorPoolCreateInfo createInfo = {};
        createInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
        createInfo.maxSets = 0;
        for (auto poolSize : poolSizes)
            createInfo.maxSets += poolSize.descriptorCount;
        createInfo.poolSizeCount = std::size(poolSizes);
        createInfo.pPoolSizes = poolSizes;

        m_DescriptorPool = m_VulkanDevice.createDescriptorPool(createInfo);
        LOG_ASSERT(m_DescriptorPool, "[Vulkan] Failed to create descriptor pool");
    }

    void DeviceManager_VK::WaitForIdle()
    {
        if (m_Device)
        {
            std::scoped_lock lock(GPUUploadSync::GetWaitIdleMutex(), GPUUploadSync::GetQueueMutex());
            m_NRI.DeviceWaitIdle(m_Device);
        }
        else if (m_VulkanDevice)
        {
            std::scoped_lock lock(GPUUploadSync::GetWaitIdleMutex(), GPUUploadSync::GetQueueMutex());
            m_VulkanDevice.waitIdle();
        }
    }

    bool DeviceManager_VK::CreateOffscreenBuffers()
    {
        DestroyOffscreenBuffers();

        uint32_t width = std::max(1u, m_DeviceParameters.backBufferWidth);
        uint32_t height = std::max(1u, m_DeviceParameters.backBufferHeight);
        uint32_t bufferCount = std::max(1u, m_DeviceParameters.swapChainBufferCount);

        m_BackBufferTextures.resize(bufferCount, nullptr);
        m_BackBufferRTVs.resize(bufferCount, nullptr);
        m_DepthBufferTextures.resize(bufferCount, nullptr);
        m_DepthBufferMemories.resize(bufferCount, nullptr);
        m_DepthBufferDSVs.resize(bufferCount, nullptr);
        m_OffscreenColorMemories.resize(bufferCount, nullptr);

        for (uint32_t i = 0; i < bufferCount; ++i)
        {
            // Color texture
            nri::TextureDesc colorDesc{};
            colorDesc.type = nri::TextureType::TEXTURE_2D;
            colorDesc.usage = nri::TextureUsageBits::COLOR_ATTACHMENT;
            if ((m_NRI.GetFormatSupport(*m_Device, m_DeviceParameters.swapChainFormat) & nri::FormatSupportBits::HOST_COPY) != 0)
            {
                colorDesc.usage |= nri::TextureUsageBits::HOST_TRANSFER;
            }
            colorDesc.format = m_DeviceParameters.swapChainFormat;
            colorDesc.width = static_cast<nri::Dim_t>(width);
            colorDesc.height = static_cast<nri::Dim_t>(height);
            colorDesc.depth = 1;
            colorDesc.mipNum = 1;
            colorDesc.layerNum = 1;
            colorDesc.sampleNum = 1;

            if (m_NRI.CreateTexture(*m_Device, colorDesc, m_BackBufferTextures[i]) != nri::Result::SUCCESS)
                return false;

            nri::ResourceGroupDesc colorResGroup{};
            colorResGroup.textureNum = 1;
            colorResGroup.textures = &m_BackBufferTextures[i];
            colorResGroup.memoryLocation = nri::MemoryLocation::DEVICE;
            if (m_HelperNRI.AllocateAndBindMemory(*m_Device, colorResGroup, &m_OffscreenColorMemories[i]) != nri::Result::SUCCESS)
                return false;

            m_NRI.SetDebugName((nri::Object *)m_BackBufferTextures[i], ("[Offscreen Color " + std::to_string(i) + "]").c_str());
            nri::TextureViewDesc rtvDesc{};
            rtvDesc.texture = m_BackBufferTextures[i];
            rtvDesc.type = nri::TextureView::COLOR_ATTACHMENT;
            rtvDesc.format = m_DeviceParameters.swapChainFormat;
            rtvDesc.mipOffset = 0;
            rtvDesc.mipNum = 1;
            rtvDesc.layerOffset = 0;
            rtvDesc.layerNum = 1;
            if (m_NRI.CreateTextureView(rtvDesc, m_BackBufferRTVs[i]) != nri::Result::SUCCESS)
                return false;

            // Depth texture
            nri::TextureDesc depthDesc{};
            depthDesc.type = nri::TextureType::TEXTURE_2D;
            depthDesc.usage = nri::TextureUsageBits::DEPTH_STENCIL_ATTACHMENT;
            depthDesc.format = nri::Format::D32_SFLOAT;
            depthDesc.width = static_cast<nri::Dim_t>(width);
            depthDesc.height = static_cast<nri::Dim_t>(height);
            depthDesc.depth = 1;
            depthDesc.mipNum = 1;
            depthDesc.layerNum = 1;
            depthDesc.sampleNum = 1;

            if (m_NRI.CreateTexture(*m_Device, depthDesc, m_DepthBufferTextures[i]) != nri::Result::SUCCESS)
                return false;
            m_NRI.SetDebugName((nri::Object *)m_DepthBufferTextures[i], ("[Offscreen Depth " + std::to_string(i) + "]").c_str());

            nri::ResourceGroupDesc depthResGroup{};
            depthResGroup.textureNum = 1;
            depthResGroup.textures = &m_DepthBufferTextures[i];
            depthResGroup.memoryLocation = nri::MemoryLocation::DEVICE;
            if (m_HelperNRI.AllocateAndBindMemory(*m_Device, depthResGroup, &m_DepthBufferMemories[i]) != nri::Result::SUCCESS)
                return false;

            nri::TextureViewDesc dsvDesc{};
            dsvDesc.texture = m_DepthBufferTextures[i];
            dsvDesc.type = nri::TextureView::DEPTH_STENCIL_ATTACHMENT;
            dsvDesc.format = nri::Format::D32_SFLOAT;
            dsvDesc.mipOffset = 0;
            dsvDesc.mipNum = 1;
            dsvDesc.layerOffset = 0;
            dsvDesc.layerNum = 1;
            if (m_NRI.CreateTextureView(dsvDesc, m_DepthBufferDSVs[i]) != nri::Result::SUCCESS)
                return false;
        }

        m_BackBufferRenderTargets.resize(bufferCount);
        for (uint32_t n = 0; n < bufferCount; ++n)
        {
            RenderTargetCreateInfo rtDesc;
            rtDesc.width = width;
            rtDesc.height = height;
            rtDesc.attachments = {
                FramebufferAttachments {
                    .name = "[OffscreenColor " + std::to_string(n) + "]",
                    .format = m_DeviceParameters.swapChainFormat,
                    .state = nri::Layout::PRESENT,
                    .nativeObjectPtr = m_BackBufferTextures[n],
                    .isNativeObject = true,
                    .nativeObjectType = 2
                },
                FramebufferAttachments {
                    .name = "[OffscreenDepth " + std::to_string(n) + "]",
                    .format = nri::Format::D32_SFLOAT,
                    .state = nri::Layout::DEPTH_STENCIL_ATTACHMENT,
                    .nativeObjectPtr = m_DepthBufferTextures[n],
                    .isNativeObject = true,
                    .nativeObjectType = 2
                }
            };
            m_BackBufferRenderTargets[n] = RenderTarget::Create(rtDesc, "[Offscreen RT " + std::to_string(n) + "]");
        }

        m_SwapChainIndex = 0;
        return true;
    }

    void DeviceManager_VK::DestroyOffscreenBuffers()
    {
        WaitForIdle();

        m_BackBufferRenderTargets.clear();

        for (auto &rtv : m_BackBufferRTVs)
        {
            if (rtv) m_NRI.DestroyDescriptor(rtv);
        }
        m_BackBufferRTVs.clear();

        for (auto &tex : m_BackBufferTextures)
        {
            if (tex) m_NRI.DestroyTexture(tex);
        }
        m_BackBufferTextures.clear();

        for (auto &mem : m_OffscreenColorMemories)
        {
            if (mem) m_NRI.FreeMemory(mem);
        }
        m_OffscreenColorMemories.clear();

        for (auto &dsv : m_DepthBufferDSVs)
        {
            if (dsv) m_NRI.DestroyDescriptor(dsv);
        }
        m_DepthBufferDSVs.clear();

        for (auto &tex : m_DepthBufferTextures)
        {
            if (tex) m_NRI.DestroyTexture(tex);
        }
        m_DepthBufferTextures.clear();

        for (auto &mem : m_DepthBufferMemories)
        {
            if (mem) m_NRI.FreeMemory(mem);
        }
        m_DepthBufferMemories.clear();
    }

    void *DeviceManager_VK::GetSharedBackBufferHandle()
    {
        nri::Texture *backBuffer = GetCurrentBackBuffer();
        if (!backBuffer)
            return nullptr;
        return (void*)(uintptr_t)m_NRI.GetTextureNativeObject(backBuffer);
    }

    bool DeviceManager_VK::ReadbackBackBuffer(void *outPixels, size_t bufferSize)
    {
        if (!outPixels || bufferSize == 0)
            return false;

        nri::Texture *backBuffer = GetCurrentBackBuffer();
        if (!backBuffer)
            return false;

        const auto &desc = m_NRI.GetTextureDesc(*backBuffer);
        size_t rowPitch = desc.width * 4;
        size_t requiredSize = rowPitch * desc.height;
        if (bufferSize < requiredSize)
            return false;

        nri::ReadbackTextureToHostMemoryDesc readbackDesc{};
        readbackDesc.srcTexture = backBuffer;
        readbackDesc.dstData = outPixels;
        readbackDesc.srcRegion = { 0, 0, 0, (nri::Dim_t)desc.width, (nri::Dim_t)desc.height, 1, 0, 0 };
        readbackDesc.dstRowPitch = (uint32_t)rowPitch;

        WaitForIdle();
        nri::Result res = m_NRI.ReadbackTextureToHostMemory(*m_NriGraphicsQueue, &readbackDesc, 1);
        return (res == nri::Result::SUCCESS);
    }
}
