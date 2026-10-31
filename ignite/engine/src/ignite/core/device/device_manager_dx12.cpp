// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#ifdef PLATFORM_WINDOWS
#include "ignite/graphics/window.hpp"

#include "device_manager.hpp"
#include "device_manager_dx12.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"

#include <Windows.h>
#include <wrl/client.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_system.h>

#include <array>
#include <optional>

#include <dxgi1_5.h>
#include <dxgidebug.h>

using Microsoft::WRL::ComPtr;

#define HR_RETURN(hr) if (FAILED(hr)) return false
static bool IsNvDevice(const UINT ID) { return ID == 0x10DE; }

namespace ignite
{
    static void CALLBACK D3D12MessageCallback(D3D12_MESSAGE_CATEGORY Category, D3D12_MESSAGE_SEVERITY Severity,
        D3D12_MESSAGE_ID ID, LPCSTR pDescription, void *pContext)
    {
        (void)Category;
        (void)pContext;

        if (ID == D3D12_MESSAGE_ID_UNKNOWN)
            return;

        switch (Severity)
        {
            case D3D12_MESSAGE_SEVERITY_CORRUPTION:
                LOG_ASSERT(false, "[D3D12 CORRUPTION] {}", pDescription);
                break;
            case D3D12_MESSAGE_SEVERITY_ERROR:
                LOG_ASSERT(false, "[D3D12 ERROR] {}", pDescription);
                break;
            case D3D12_MESSAGE_SEVERITY_WARNING:
                LOG_WARN("[D3D12 WARNING] {}", pDescription);
                break;
            case D3D12_MESSAGE_SEVERITY_INFO:
                LOG_INFO("[D3D12 INFO] {}", pDescription);
                break;
            case D3D12_MESSAGE_SEVERITY_MESSAGE:
                LOG_INFO("[D3D12 MESSAGE] {}", pDescription);
                break;
        }
    }

    static DeviceManager_DX12 *s_D3D12DeviceInstance = nullptr;

    void DescriptorHeapAllocator::Create(ID3D12Device *device, ID3D12DescriptorHeap *descriptorHeap)
    {
        heap = descriptorHeap;
        heap->SetName(L"DescriptorHeapAllocator");

        D3D12_DESCRIPTOR_HEAP_DESC desc = heap->GetDesc();
        heapType = desc.Type;
        heapStartCpu = heap->GetCPUDescriptorHandleForHeapStart();
        heapStartGpu = heap->GetGPUDescriptorHandleForHeapStart();
        heapHandleIncrement = device->GetDescriptorHandleIncrementSize(heapType);

        freeIndices.resize(0);
        freeIndices.reserve(desc.NumDescriptors);
        for (int i = desc.NumDescriptors; i > 0; --i)
            freeIndices.push_back(i - 1);
    }

    void DescriptorHeapAllocator::Destroy()
    {
        heap = nullptr;
        freeIndices.clear();
    }

    void DescriptorHeapAllocator::Alloc(D3D12_CPU_DESCRIPTOR_HANDLE *outCpuDescHandle, D3D12_GPU_DESCRIPTOR_HANDLE *outGpuDescHandle)
    {
        assert(freeIndices.size() > 0);
        const int idx = freeIndices.back();
        freeIndices.pop_back();
        outCpuDescHandle->ptr = heapStartCpu.ptr + (idx * heapHandleIncrement);
        outGpuDescHandle->ptr = heapStartGpu.ptr + (idx * heapHandleIncrement);
    }

    void DescriptorHeapAllocator::Free(D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandle, D3D12_GPU_DESCRIPTOR_HANDLE gpuDescHandle)
    {
        const int cpuIdx = (int)((cpuDescHandle.ptr - heapStartCpu.ptr) / heapHandleIncrement);
        const int gpuIdx = (int)((gpuDescHandle.ptr - heapStartGpu.ptr) / heapHandleIncrement);
        assert(cpuIdx == gpuIdx);
        freeIndices.push_back(cpuIdx);
    }

    static bool MoveWindowOntoAdapter(IDXGIAdapter *targetAdapter, RECT &rect)
    {
        assert(targetAdapter != NULL);

        HRESULT hr = S_OK;
        uint32_t outputNo = 0;
        while (SUCCEEDED(hr))
        {
            ComPtr<IDXGIOutput> pOutput;
            hr = targetAdapter->EnumOutputs(outputNo++, &pOutput);

            if (SUCCEEDED(hr) && pOutput)
            {
                DXGI_OUTPUT_DESC outputDesc;
                pOutput->GetDesc(&outputDesc);
                const RECT desktop = outputDesc.DesktopCoordinates;
                const int centerX = (int)desktop.left + (int)(desktop.right - desktop.left) / 2;
                const int centerY = (int)desktop.top + (int)(desktop.bottom - desktop.top) / 2;
                const int winW = rect.right - rect.left;
                const int winH = rect.bottom - rect.top;
                const int left = centerX - winW / 2;
                const int right = left + winW;
                const int top = centerY - winH / 2;
                const int bottom = top + winH;
                rect.left = std::max(left, (int)desktop.left);
                rect.right = std::min(right, (int)desktop.right);
                rect.top = std::max(top, (int)desktop.top);
                rect.bottom = std::min(bottom, (int)desktop.bottom);

                return true;
            }
        }

        return false;
    }

    DeviceManager_DX12::DeviceManager_DX12(Window *window, const DeviceParameters &params)
    {
        m_Window = window;
        m_DeviceParameters = params;
    }

    void DeviceManager_DX12::ReportLiveObjects()
    {
        ComPtr<IDXGIDebug> pDebug;
        DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug));

        if (pDebug)
        {
            DXGI_DEBUG_RLO_FLAGS flags = (DXGI_DEBUG_RLO_FLAGS)(DXGI_DEBUG_RLO_IGNORE_INTERNAL | DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_DETAIL);
            HRESULT hr = pDebug->ReportLiveObjects(DXGI_DEBUG_ALL, flags);
            if (FAILED(hr))
            {
                printf("ReportLiveObjects failed, HRESULT = 0x%08x", hr);
            }
        }
    }

    bool DeviceManager_DX12::CreateInstanceInternal()
    {
        if (!m_DxgiFactory2)
        {
            HRESULT hr = CreateDXGIFactory2(m_DeviceParameters.enableDebugRuntime ? DXGI_CREATE_FACTORY_DEBUG : 0, IID_PPV_ARGS(&m_DxgiFactory2));
            LOG_ASSERT(hr == S_OK, "Failed to create DXGIFactory2, for more info, get log from debug D3D Runtime");
            if (hr != S_OK)
                return false;
        }
        return true;
    }

    bool DeviceManager_DX12::EnumerateAdapters(std::vector<AdapterInfo> &outAdapters)
    {
        if (!m_DxgiFactory2)
            return false;

        outAdapters.clear();

        while (true)
        {
            ComPtr<IDXGIAdapter> adapter;
            HRESULT hr = m_DxgiFactory2->EnumAdapters(static_cast<uint32_t>(outAdapters.size()), &adapter);
            if (FAILED(hr))
                return true;

            DXGI_ADAPTER_DESC desc;
            hr = adapter->GetDesc(&desc);
            if (FAILED(hr))
                return false;

            AdapterInfo adapterInfo;
            adapterInfo.name = GetAdapterName(desc);
            adapterInfo.dxgiAdapter = adapter;
            adapterInfo.vendorID = desc.VendorId;
            adapterInfo.deviceID = desc.DeviceId;
            adapterInfo.dedicatedVideoMemory = desc.DedicatedVideoMemory;

            std::array<uint8_t, 8> luid;
            static_assert(luid.size() == sizeof(desc.AdapterLuid));
            memcpy(luid.data(), &desc.AdapterLuid, luid.size());
            adapterInfo.luid = luid;
            outAdapters.push_back(std::move(adapterInfo));
        }
    }

    bool DeviceManager_DX12::CreateDevice()
    {
        if (m_DeviceParameters.enableDebugRuntime)
        {
            ComPtr<ID3D12Debug> pDebug;
            const HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(&pDebug));
            if (SUCCEEDED(hr)) pDebug->EnableDebugLayer();
            else printf("Cannot enable DX12 debug runtime, ID3D12Debug is not available.");
        }

        if (m_DeviceParameters.enableGPUValidation)
        {
            ComPtr<ID3D12Debug3> debugController3;
            const HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(&debugController3));
            if (SUCCEEDED(hr)) debugController3->SetEnableGPUBasedValidation(true);
            else printf("Cannot enable GPU-based validation, ID3D12Debug3 is not available.");
        }

        int adapterIndex = m_DeviceParameters.adapterIndex;

        if (adapterIndex < 0) adapterIndex = 0;

        if (FAILED(m_DxgiFactory2->EnumAdapters(adapterIndex, &m_DxgiAdapter)))
        {
            if (adapterIndex == 0) printf("Cannot find any DXGI adapters in the system.");
            else printf("The specified DXGI adapter %d does not exist.", adapterIndex);
            return false;
        }

        {
            DXGI_ADAPTER_DESC aDesc;
            m_DxgiAdapter->GetDesc(&aDesc);
            m_RendererString = GetAdapterName(aDesc);
            m_IsNvidia = IsNvDevice(aDesc.VendorId);
        }

        HRESULT hr = D3D12CreateDevice(
            m_DxgiAdapter.Get(),
            m_DeviceParameters.featureLevel,
            IID_PPV_ARGS(&m_Device12)
        );

        if (FAILED(hr))
        {
            printf("D3D12CreateDevice failed, error HRESULT = 0x%08x", hr);
            return false;
        }

        if (m_DeviceParameters.enableDebugRuntime)
        {
            ComPtr<ID3D12InfoQueue> pInfoQueue;
            m_Device12.As(&pInfoQueue);

            if (pInfoQueue)
            {
#ifdef _DEBUG
                if (m_DeviceParameters.enableWarningAsErrors)
                {
                    pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
                }

                pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
                pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
#endif
                D3D12_MESSAGE_ID disableMessageIDs[] =
                {
                    D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
                    D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
                    D3D12_MESSAGE_ID_COMMAND_LIST_STATIC_DESCRIPTOR_RESOURCE_DIMENSION_MISMATCH,
                };

                D3D12_INFO_QUEUE_FILTER filter = {};
                filter.DenyList.pIDList = disableMessageIDs;
                filter.DenyList.NumIDs = std::size(disableMessageIDs);
                pInfoQueue->AddStorageFilterEntries(&filter);
            }

            ComPtr<ID3D12InfoQueue1> pInfoQueue1;
            if (SUCCEEDED(m_Device12->QueryInterface(IID_PPV_ARGS(&pInfoQueue1))))
            {
                DWORD cookie = 0;
                pInfoQueue1->RegisterMessageCallback(
                    D3D12MessageCallback,
                    D3D12_MESSAGE_CALLBACK_FLAG_NONE,
                    nullptr,
                    &cookie
                );
            }
        }

        {
            D3D12_DESCRIPTOR_HEAP_DESC desc = {};
            desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            desc.NumDescriptors = m_DeviceParameters.maxFramesInFlight;
            desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            desc.NodeMask = 1;
            hr = m_Device12->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_RtvDescHeap));
            LOG_ASSERT(hr == S_OK, "Failed to create RTV descriptor heap");
            HR_RETURN(hr);
        }

        {
            D3D12_DESCRIPTOR_HEAP_DESC desc = {};
            desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            desc.NumDescriptors = SRV_HEAP_SIZE;
            desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            hr = m_Device12->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_SrvDescHeap));
            LOG_ASSERT(hr == S_OK, "Failed to create SRV descriptor heap");
            HR_RETURN(hr);

            m_SrvDescHeapAlloc.Create(m_Device12.Get(), m_SrvDescHeap.Get());
        }

        D3D12_COMMAND_QUEUE_DESC queueDesc;
        ZeroMemory(&queueDesc, sizeof(queueDesc));
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        queueDesc.NodeMask = 1;
        hr = m_Device12->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_GraphicsQueue));
        HR_RETURN(hr);
        m_GraphicsQueue->SetName(L"Graphics Queue");

        if (m_DeviceParameters.enableComputeQueue)
        {
            queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
            hr = m_Device12->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_ComputeQueue));
            HR_RETURN(hr);
            m_ComputeQueue->SetName(L"Compute Queue");
        }

        if (m_DeviceParameters.enableCopyQueue)
        {
            queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
            hr = m_Device12->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_CopyQueue));
            HR_RETURN(hr);
            m_CopyQueue->SetName(L"Copy Queue");
        }

        // Initialize NRI device from D3D12
        std::vector<nri::QueueFamilyD3D12Desc> queueFamilies;
        ID3D12CommandQueue *graphicsQueues[] = { m_GraphicsQueue.Get() };
        queueFamilies.push_back({ graphicsQueues, 1, nri::QueueType::GRAPHICS });

        ID3D12CommandQueue *computeQueues[] = { m_ComputeQueue.Get() };
        if (m_ComputeQueue)
        {
            queueFamilies.push_back({ computeQueues, 1, nri::QueueType::COMPUTE });
        }

        ID3D12CommandQueue *copyQueues[] = { m_CopyQueue.Get() };
        if (m_CopyQueue)
        {
            queueFamilies.push_back({ copyQueues, 1, nri::QueueType::COPY });
        }

        nri::DeviceCreationD3D12Desc nriDeviceDesc{};
        nriDeviceDesc.d3d12Device = m_Device12.Get();
        nriDeviceDesc.queueFamilies = queueFamilies.data();
        nriDeviceDesc.queueFamilyNum = static_cast<uint32_t>(queueFamilies.size());
        nriDeviceDesc.callbackInterface = DefaultMessageCallback::GetInterface();
        nriDeviceDesc.enableNRIValidation = m_DeviceParameters.enableNRIValidation || m_DeviceParameters.enableNvrhiValidationLayer;

        nri::Result res = nriCreateDeviceFromD3D12Device(nriDeviceDesc, m_Device);
        if (res != nri::Result::SUCCESS)
        {
            LOG_ERROR("Failed to create NRI device from D3D12 device");
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

        if (nriGetInterface(*m_Device, NRI_INTERFACE(nri::WrapperD3D12Interface), &m_WrapperD3D12) != nri::Result::SUCCESS)
        {
            LOG_ERROR("Failed to get NRI WrapperD3D12Interface");
            return false;
        }

        if (m_NRI.GetQueue(*m_Device, nri::QueueType::GRAPHICS, 0, m_NriGraphicsQueue) != nri::Result::SUCCESS)
        {
            LOG_ERROR("Failed to get NRI graphics queue");
            return false;
        }

        const auto &desc = m_NRI.GetDeviceDesc(*m_Device);
        LOG_INFO("[D3D12 Device] Initialized NRI device with {} (Adapter: {})", nriGetGraphicsAPIString(desc.graphicsAPI), desc.adapterDesc.name);

        return true;
    }

    bool DeviceManager_DX12::CreateSwapChain()
    {
        UINT windowStyle = m_DeviceParameters.startFullscreen
            ? (WS_POPUP | WS_SYSMENU | WS_VISIBLE) : m_DeviceParameters.startMaximized
            ? (WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_MAXIMIZE) : (WS_OVERLAPPEDWINDOW | WS_VISIBLE);

        RECT rect = { 0, 0, LONG(m_DeviceParameters.backBufferWidth), LONG(m_DeviceParameters.backBufferHeight) };
        AdjustWindowRect(&rect, windowStyle, false);

        if (MoveWindowOntoAdapter(m_DxgiAdapter.Get(), rect))
        {
            m_DeviceParameters.windowPosX = rect.left;
            m_DeviceParameters.windowPosY = rect.top;
        }

        HRESULT hr = S_OK;

        m_Hwnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties((SDL_Window *)m_Window->GetNativeWindow()),
            SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);

        if (m_DeviceParameters.supportExplicitDisplayScaling)
        {
            m_DPIScaleFactorX = 1.0f;
            m_DPIScaleFactorY = 1.0f;
        }
        else
        {
            m_DPIScaleFactorX = float(rect.right - rect.left) / float(m_DeviceParameters.backBufferWidth);
            m_DPIScaleFactorY = float(rect.bottom - rect.top) / float(m_DeviceParameters.backBufferHeight);
        }

        DXGI_SWAP_CHAIN_DESC1 desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.Width = m_DeviceParameters.backBufferWidth;
        desc.Height = m_DeviceParameters.backBufferHeight;
        desc.SampleDesc.Count = m_DeviceParameters.swapChainSampleCount;
        desc.SampleDesc.Quality = m_DeviceParameters.swapChainSampleQuality;
        desc.BufferUsage = m_DeviceParameters.swapChainUsage;
        desc.BufferCount = m_DeviceParameters.swapChainBufferCount;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        desc.Flags = m_DeviceParameters.allowModeSwitch ? DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH : 0;
        desc.Format = static_cast<DXGI_FORMAT>(nriConvertNRIFormatToDXGI(m_DeviceParameters.swapChainFormat));

        // Special handling for sRGB formats
        switch (desc.Format)
        {
            case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; break;
            case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; break;
            case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB: desc.Format = DXGI_FORMAT_B8G8R8X8_UNORM; break;
        }

        ComPtr<IDXGIFactory5> pDxgiFactory5;
        if (SUCCEEDED(m_DxgiFactory2->QueryInterface(IID_PPV_ARGS(&pDxgiFactory5))))
        {
            BOOL supported = 0;
            if (SUCCEEDED(pDxgiFactory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &supported, sizeof(supported))))
                m_TearingSupported = (supported != 0);
        }

        if (m_TearingSupported)
        {
            desc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
        }

        DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsDesc;
        ZeroMemory(&fsDesc, sizeof(fsDesc));
        fsDesc.RefreshRate.Numerator = m_DeviceParameters.refreshRate;
        fsDesc.RefreshRate.Denominator = 1;
        fsDesc.Windowed = !m_DeviceParameters.startFullscreen;

        ComPtr<IDXGISwapChain1> pSwapChain1;
        hr = m_DxgiFactory2->CreateSwapChainForHwnd(m_GraphicsQueue.Get(), m_Hwnd, &desc, &fsDesc, nullptr, &pSwapChain1);
        HR_RETURN(hr);

        hr = pSwapChain1->QueryInterface(IID_PPV_ARGS(&m_SwapChain));
        HR_RETURN(hr);

        m_SwapChainDesc = desc;
        m_FullScreenDesc = fsDesc;

        if (!CreateRenderTargets())
        {
            return false;
        }

        hr = m_Device12->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_FrameFence));
        HR_RETURN(hr);

        for (UINT bufferIndex = 0; bufferIndex < m_SwapChainDesc.BufferCount; bufferIndex++)
        {
            m_FrameFenceEvents.push_back(CreateEvent(nullptr, false, true, nullptr));
        }

        return true;
    }

    void DeviceManager_DX12::DestroyDeviceAndSwapChain()
    {
        m_RendererString.clear();

        ReleaseRenderTargets();

        for (auto fenceEvent : m_FrameFenceEvents)
        {
            WaitForSingleObject(fenceEvent, INFINITE);
            CloseHandle(fenceEvent);
        }
        m_FrameFenceEvents.clear();

        if (m_SwapChain)
        {
            m_SwapChain->SetFullscreenState(false, nullptr);
        }

        if (m_Device)
        {
            m_NRI.DeviceWaitIdle(m_Device);
            nriDestroyDevice(m_Device);
            m_Device = nullptr;
            m_NriGraphicsQueue = nullptr;
        }

        m_SrvDescHeapAlloc.Destroy();

        m_RtvDescHeap = nullptr;
        m_SrvDescHeap = nullptr;

        m_FrameFence = nullptr;
        m_SwapChain = nullptr;
        m_GraphicsQueue = nullptr;
        m_ComputeQueue = nullptr;
        m_CopyQueue = nullptr;
        m_Device12 = nullptr;
    }

    bool DeviceManager_DX12::CreateRenderTargets()
    {
        const uint32_t bufferCount = m_SwapChainDesc.BufferCount;
        m_SwapChainBuffers.resize(bufferCount);
        m_BackBufferTextures.resize(bufferCount, nullptr);
        m_BackBufferRTVs.resize(bufferCount, nullptr);
        m_DepthBufferTextures.resize(bufferCount, nullptr);
        m_DepthBufferMemories.resize(bufferCount, nullptr);
        m_DepthBufferDSVs.resize(bufferCount, nullptr);

        for (UINT n = 0; n < bufferCount; n++)
        {
            const HRESULT hr = m_SwapChain->GetBuffer(n, IID_PPV_ARGS(&m_SwapChainBuffers[n]));
            HR_RETURN(hr);

            // Wrap D3D12 swapchain backbuffer into NRI texture
            nri::TextureD3D12Desc texDesc{};
            texDesc.d3d12Resource = m_SwapChainBuffers[n].Get();
            if (m_WrapperD3D12.CreateTextureD3D12(*m_Device, texDesc, m_BackBufferTextures[n]) != nri::Result::SUCCESS)
            {
                LOG_ERROR("Failed to wrap D3D12 swapchain buffer {} into NRI texture", n);
                return false;
            }

            // Create Color Attachment (RTV) descriptor
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

            // Create Depth Stencil texture
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
                LOG_ERROR("Failed to create depth texture for back buffer {}", n);
                return false;
            }

            // Allocate and bind memory for depth texture
            nri::ResourceGroupDesc resGroup{};
            resGroup.textureNum = 1;
            resGroup.textures = &m_DepthBufferTextures[n];
            resGroup.memoryLocation = nri::MemoryLocation::DEVICE;
            if (m_HelperNRI.AllocateAndBindMemory(*m_Device, resGroup, &m_DepthBufferMemories[n]) != nri::Result::SUCCESS)
            {
                LOG_ERROR("Failed to allocate and bind memory for depth texture {}", n);
                return false;
            }

            // Create Depth Stencil (DSV) descriptor
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

        m_BackBufferRenderTargets.resize(bufferCount);
        for (uint32_t n = 0; n < bufferCount; ++n)
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

        return true;
    }

    void DeviceManager_DX12::WaitForIdle()
    {
        if (m_Device)
        {
            std::scoped_lock lock(GPUUploadSync::GetWaitIdleMutex(), GPUUploadSync::GetQueueMutex());
            m_NRI.DeviceWaitIdle(m_Device);
        }
    }

    void DeviceManager_DX12::ReleaseRenderTargets()
    {
        WaitForIdle();

        m_BackBufferRenderTargets.clear();

        for (auto event : m_FrameFenceEvents)
        {
            SetEvent(event);
        }

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

        m_SwapChainBuffers.clear();
    }

    void DeviceManager_DX12::ResizeSwapChain()
    {
        ReleaseRenderTargets();

        if (!m_Device || !m_SwapChain)
            return;

        const HRESULT hr = m_SwapChain->ResizeBuffers(m_DeviceParameters.swapChainBufferCount,
            m_DeviceParameters.backBufferWidth, m_DeviceParameters.backBufferHeight,
            m_SwapChainDesc.Format, m_SwapChainDesc.Flags);

        LOG_ASSERT(hr == S_OK, "ResizeBuffers failed");

        const bool created = CreateRenderTargets();
        LOG_ASSERT(created, "CreateRenderTarget failed");
    }

    bool DeviceManager_DX12::BeginFrame()
    {
        if (m_DeviceParameters.headlessDevice)
            return true;

        DXGI_SWAP_CHAIN_DESC1 newSwapChainDesc;
        DXGI_SWAP_CHAIN_FULLSCREEN_DESC newFullScreenDesc;

        if (SUCCEEDED(m_SwapChain->GetDesc1(&newSwapChainDesc)) && SUCCEEDED(m_SwapChain->GetFullscreenDesc(&newFullScreenDesc)))
        {
            if (m_FullScreenDesc.Windowed != newFullScreenDesc.Windowed)
            {
                m_FullScreenDesc = newFullScreenDesc;
                m_SwapChainDesc = newSwapChainDesc;
                m_DeviceParameters.backBufferWidth = newSwapChainDesc.Width;
                m_DeviceParameters.backBufferHeight = newSwapChainDesc.Height;

                ResizeSwapChain();
                CreateBackBuffers();
            }
        }

        const UINT bufferIndex = m_SwapChain->GetCurrentBackBufferIndex();
        const DWORD waitResult = WaitForSingleObject(m_FrameFenceEvents[bufferIndex], INFINITE);
        if (waitResult != WAIT_OBJECT_0)
        {
            return false;
        }
        return true;
    }

    nri::Texture *DeviceManager_DX12::GetCurrentBackBuffer()
    {
        return GetBackBuffer(GetCurrentBackBufferIndex());
    }

    nri::Texture *DeviceManager_DX12::GetBackBuffer(uint32_t index)
    {
        if (index < m_BackBufferTextures.size())
            return m_BackBufferTextures[index];
        return nullptr;
    }

    nri::Texture *DeviceManager_DX12::GetBackDepthBuffer(uint32_t index)
    {
        if (index < m_DepthBufferTextures.size())
            return m_DepthBufferTextures[index];
        return nullptr;
    }

    nri::Descriptor *DeviceManager_DX12::GetCurrentBackBufferRTV()
    {
        return GetBackBufferRTV(GetCurrentBackBufferIndex());
    }

    nri::Descriptor *DeviceManager_DX12::GetBackBufferRTV(uint32_t index)
    {
        if (index < m_BackBufferRTVs.size())
            return m_BackBufferRTVs[index];
        return nullptr;
    }

    nri::Descriptor *DeviceManager_DX12::GetCurrentBackDepthBufferDSV()
    {
        return GetBackDepthBufferDSV(GetCurrentBackBufferIndex());
    }

    nri::Descriptor *DeviceManager_DX12::GetBackDepthBufferDSV(uint32_t index)
    {
        if (index < m_DepthBufferDSVs.size())
            return m_DepthBufferDSVs[index];
        return nullptr;
    }

    uint32_t DeviceManager_DX12::GetCurrentBackBufferIndex()
    {
        return m_SwapChain ? m_SwapChain->GetCurrentBackBufferIndex() : 0;
    }

    uint32_t DeviceManager_DX12::GetBackBufferCount()
    {
        return m_SwapChainDesc.BufferCount;
    }

    RenderTarget *DeviceManager_DX12::GetCurrentBackBufferRenderTarget()
    {
        return GetBackBufferRenderTarget(GetCurrentBackBufferIndex());
    }

    RenderTarget *DeviceManager_DX12::GetBackBufferRenderTarget(uint32_t index)
    {
        if (index < m_BackBufferRenderTargets.size())
            return m_BackBufferRenderTargets[index].get();
        return nullptr;
    }

    bool DeviceManager_DX12::Present()
    {
        if (!m_Window->IsVisible())
            return true;

        const UINT bufferIndex = m_SwapChain->GetCurrentBackBufferIndex();

        UINT presentFlags = 0;
        if (!m_DeviceParameters.vsyncEnable && m_FullScreenDesc.Windowed && m_TearingSupported)
            presentFlags |= DXGI_PRESENT_ALLOW_TEARING;

        HRESULT hr = m_SwapChain->Present(m_DeviceParameters.vsyncEnable ? 1 : 0, presentFlags);

        m_FrameFence->SetEventOnCompletion(m_FrameCount, m_FrameFenceEvents[bufferIndex]);
        m_GraphicsQueue->Signal(m_FrameFence.Get(), m_FrameCount);
        m_FrameCount++;

        return SUCCEEDED(hr);
    }

    void DeviceManager_DX12::Destroy()
    {
        DeviceManager::Destroy();

        m_DxgiAdapter = nullptr;
        m_DxgiFactory2 = nullptr;

        if (m_DeviceParameters.enableDebugRuntime)
        {
            ReportLiveObjects();
        }
    }

    DeviceManager *DeviceManager::CreateD3D12(Window *window, const DeviceParameters &params)
    {
        s_D3D12DeviceInstance = new DeviceManager_DX12(window, params);
        return s_D3D12DeviceInstance;
    }

    DeviceManager_DX12 &DeviceManager_DX12::GetInstance()
    {
        return *s_D3D12DeviceInstance;
    }
}

#endif // PLATFORM_WINDOWS
