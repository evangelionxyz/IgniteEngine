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

#include "ignite_pch.hpp"
#include "ignite/graphics/window.hpp"
#include "ignite/graphics/texture.hpp"

#include "device_manager.hpp"
#include "device_manager_dx12.hpp"
#include "ignite/core/logger.hpp"

#include <Windows.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_system.h>

#include <array>
#include <optional>

#include <dxgi1_5.h>
#include <dxgidebug.h>

#include <nvrhi/d3d12.h>
#include <nvrhi/validation.h>

using nvrhi::RefCountPtr;

#define HR_RETURN(hr) if (FAILED(hr)) return false
static bool IsNvDevice(const UINT ID) { return ID == 0x10DE; }

namespace ignite
{
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
            RefCountPtr<IDXGIOutput> pOutput;
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

    DeviceManager_DX12::DeviceManager_DX12(Window* window, const DeviceParameters& params)
    {
		m_Window = window;
        m_DeviceParameters = params;
    }

    void DeviceManager_DX12::ReportLiveObjects()
    {
        RefCountPtr<IDXGIDebug> pDebug;
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

    bool DeviceManager_DX12::EnumerateAdapters(std::vector<AdapterInfo>& outAdapters)
    {
        if (!m_DxgiFactory2)
            return false;

        outAdapters.clear();

        while (true)
        {
            RefCountPtr<IDXGIAdapter> adapter;
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
            RefCountPtr<ID3D12Debug> pDebug;
            const HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(&pDebug));
            if (SUCCEEDED(hr)) pDebug->EnableDebugLayer();
            else printf("Cannot enable DX12 debug runtime, ID3D12Debug is not available.");
        }

        if (m_DeviceParameters.enableGPUValidation)
        {
            RefCountPtr<ID3D12Debug3> debugController3;
            const HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(&debugController3));
            if (SUCCEEDED(hr)) debugController3->SetEnableGPUBasedValidation(true);
            else printf("Cannot enable GPU-based validation, ID3D12Debug3 is not available.");
        }

        int adapterIndex = m_DeviceParameters.adapterIndex;

        if (adapterIndex < 0) adapterIndex = 0;

        if (FAILED(m_DxgiFactory2->EnumAdapters(adapterIndex, &m_DxgiAdapter)))
        {
            if (adapterIndex == 0) printf("Cannot find any DXGI adapters in the system.");
            else printf("The specified DXGI adatpter %d does not exists.", adapterIndex);
            return false;
        }

        {
            DXGI_ADAPTER_DESC aDesc;
            m_DxgiAdapter->GetDesc(&aDesc);
            m_RendererString = GetAdapterName(aDesc);
            m_IsNvidia = IsNvDevice(aDesc.VendorId);
        }

        HRESULT hr = D3D12CreateDevice(
            m_DxgiAdapter,
            m_DeviceParameters.featureLevel,
            IID_PPV_ARGS(&m_Device12)
        );

        if (FAILED(hr))
        {
            printf("DX12CreateDevice failed, error HRESULT = 0x%08x", hr);
            return false;
        }

        if (m_DeviceParameters.enableDebugRuntime)
        {
            RefCountPtr<ID3D12InfoQueue> pInfoQueue;
            m_Device12->QueryInterface(&pInfoQueue);

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
        }

        {

            D3D12_DESCRIPTOR_HEAP_DESC desc = {};
            desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            desc.NumDescriptors = m_DeviceParameters.maxFramesInFlight;
            desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            desc.NodeMask = 1;
            hr = m_Device12->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_RtvDescHeap));
            LOG_ASSERT(hr == S_OK, "Faield to create RVT descriptor heap");
            HR_RETURN(hr);
        }

        {
            D3D12_DESCRIPTOR_HEAP_DESC desc = {};
            desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            desc.NumDescriptors = SRV_HEAP_SIZE;
            desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            hr = m_Device12->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_SrvDescHeap));
            LOG_ASSERT(hr == S_OK, "Faield to create SRV descriptor heap");
            HR_RETURN(hr);

            m_SrvDescHeapAlloc.Create(m_Device12, m_SrvDescHeap);
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

        nvrhi::d3d12::DeviceDesc deviceDesc;
        deviceDesc.errorCB = m_DeviceParameters.messageCallback ? m_DeviceParameters.messageCallback : &DefaultMessageCallback::GetInstance();
        deviceDesc.pDevice = m_Device12;
        deviceDesc.pGraphicsCommandQueue = m_GraphicsQueue;
        deviceDesc.pComputeCommandQueue = m_ComputeQueue;
        deviceDesc.pCopyCommandQueue =m_CopyQueue;
        deviceDesc.logBufferLifetime = m_DeviceParameters.logBufferLifetime;
        deviceDesc.enableHeapDirectlyIndexed = m_DeviceParameters.enableHeapDirectlyIndexed;

        m_NvrhiDevice = nvrhi::d3d12::createDevice(deviceDesc);
        if (m_DeviceParameters.enableNvrhiValidationLayer)
        {
            m_NvrhiDevice = nvrhi::validation::createValidationLayer(m_NvrhiDevice);
        }

        return true;
    }

    bool DeviceManager_DX12::CreateSwapChain()
    {
        UINT windowStyle = m_DeviceParameters.startFullscreen
            ? (WS_POPUP | WS_SYSMENU | WS_VISIBLE) : m_DeviceParameters.startMaximized
            ? (WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_MAXIMIZE) : (WS_OVERLAPPEDWINDOW | WS_VISIBLE);

        RECT rect = { 0, 0, LONG(m_DeviceParameters.backBufferWidth), LONG(m_DeviceParameters.backBufferHeight) };
        AdjustWindowRect(&rect, windowStyle, false);

        if (MoveWindowOntoAdapter(m_DxgiAdapter, rect))
        {
			SDL_SetWindowPosition(m_Window->GetWindowHandle(), rect.left, rect.top);
        }

        // Retrieve HWND
        m_Hwnd = m_Window->GetNativeWindow();
        HRESULT hr = E_FAIL;

        RECT clientRect;
        GetClientRect(m_Hwnd, &clientRect);
        UINT width = clientRect.right - clientRect.left;
        UINT height = clientRect.bottom - clientRect.top;

        m_SwapChainDesc.Width = width;
        m_SwapChainDesc.Height = height;
        m_SwapChainDesc.SampleDesc.Count = m_DeviceParameters.swapChainSampleCount;
        m_SwapChainDesc.SampleDesc.Quality = 0;
        m_SwapChainDesc.BufferUsage = m_DeviceParameters.swapChainUsage;
        m_SwapChainDesc.BufferCount = m_DeviceParameters.swapChainBufferCount;
        m_SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        m_SwapChainDesc.Flags = m_DeviceParameters.allowModeSwitch ? DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH : 0;

        // Special processing for sRGB swap chain formats.
        // DXGI will not create a swap chain with an sRGB format, but its contents will be interpreted as sRGB.
        // So we need to use a non-sRGB format here, but store the true sRGB format for later framebuffer creation.

        switch (m_DeviceParameters.swapChainFormat) // NOLINT(clang-diagnostic-switch-enum)
        {
            case nvrhi::Format::SRGBA8_UNORM:
                m_SwapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            break;
            case nvrhi::Format::SBGRA8_UNORM:
                m_SwapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            break;
            default:
                m_SwapChainDesc.Format = nvrhi::d3d12::convertFormat(m_DeviceParameters.swapChainFormat);
            break;
        }

        RefCountPtr<IDXGIFactory5> pDxgiFactory5;
        if (SUCCEEDED(m_DxgiFactory2->QueryInterface(&pDxgiFactory5)))
        {
            BOOL supported = 0;
            if (SUCCEEDED(pDxgiFactory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &supported, sizeof(supported))))
                m_TearingSupported = (supported != 0);
        }

        if (m_TearingSupported)
        {
            m_SwapChainDesc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
        }

        m_FullScreenDesc = {};
        m_FullScreenDesc.RefreshRate.Numerator = m_DeviceParameters.refreshRate;
        m_FullScreenDesc.RefreshRate.Denominator = 1;
        m_FullScreenDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
        m_FullScreenDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
        m_FullScreenDesc.Windowed = !m_DeviceParameters.startFullscreen;

        RefCountPtr<IDXGISwapChain1> pSwapChain1;
        hr = m_DxgiFactory2->CreateSwapChainForHwnd(m_GraphicsQueue, m_Hwnd, &m_SwapChainDesc, &m_FullScreenDesc, nullptr, &pSwapChain1);
        LOG_ASSERT(hr == S_OK, "Failed to create swapchain for HWND");
        HR_RETURN(hr);

        hr = pSwapChain1->QueryInterface(IID_PPV_ARGS(&m_SwapChain));
        LOG_ASSERT(hr == S_OK, "Failed to Query swapchain interface");
        HR_RETURN(hr);

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
        m_SwapChainRenderTargets.clear();
        m_RendererString.clear();

        ReleaseRenderTargets();

        for (auto fenceEvent : m_FrameFenceEvents)
        {
            WaitForSingleObject(fenceEvent, INFINITE);
            CloseHandle(fenceEvent);
        }

        if (m_SwapChain)
        {
            m_SwapChain->SetFullscreenState(false, nullptr);
        }

        m_FrameFenceEvents.clear();
        m_SwapChainBuffers.clear();
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
        m_SwapChainBuffers.resize(m_SwapChainDesc.BufferCount);
        m_SwapChainRenderTargets.reserve(m_SwapChainDesc.BufferCount);

        for (UINT n = 0; n < m_SwapChainDesc.BufferCount; n++)
        {
            const HRESULT hr = m_SwapChain->GetBuffer(n, IID_PPV_ARGS(&m_SwapChainBuffers[n]));
            HR_RETURN(hr);

            RenderTargetCreateInfo createInfo;
            createInfo.width = m_DeviceParameters.backBufferWidth;
            createInfo.height = m_DeviceParameters.backBufferHeight;
            createInfo.sampleCount = m_DeviceParameters.swapChainSampleCount;
            createInfo.sampleQuality = m_DeviceParameters.swapChainSampleQuality;
            createInfo.attachments =
            {
                FramebufferAttachments {
                    .name = "Swapchain RT",
                    .format = nvrhi::Format::D32S8,
                    .state = nvrhi::ResourceStates::DepthWrite
                },
                FramebufferAttachments {
                    .name = "Swapchain RT",
                    .format = m_DeviceParameters.swapChainFormat,
                    .state = nvrhi::ResourceStates::Present,
                    .nativeObjectPtr = m_SwapChainBuffers[n],
                    .isNativeObject = true,
                    .nativeObjectType = nvrhi::ObjectTypes::D3D12_Resource
                }
            };

            m_SwapChainRenderTargets.emplace_back(RenderTarget::Create(createInfo, "Swapchain RT"));
        }

        return true;
    }

    void DeviceManager_DX12::WaitForIdle()
    {
        if (m_NvrhiDevice)
        {
            m_NvrhiDevice->waitForIdle();
        }
    }

    void DeviceManager_DX12::ReleaseRenderTargets()
    {
        if (m_NvrhiDevice)
        {
            m_NvrhiDevice->waitForIdle();
            m_NvrhiDevice->runGarbageCollection();
        }

        for (auto event : m_FrameFenceEvents)
        {
            SetEvent(event);
        }

        m_SwapChainRenderTargets.clear();
        m_SwapChainBuffers.clear();
    }

    void DeviceManager_DX12::ResizeSwapChain()
    {
        m_SwapChainFramebuffers.clear();

        ReleaseRenderTargets();

        if (!m_NvrhiDevice || !m_SwapChain)
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
        const DWORD waitResult = WaitForSingleObject(m_FrameFenceEvents[bufferIndex], 0);
        if (waitResult != WAIT_OBJECT_0)
        {
            return false;
        }
        return true;
    }

    nvrhi::ITexture *DeviceManager_DX12::GetCurrentBackBuffer()
    {
        return m_SwapChainRenderTargets[m_SwapChain->GetCurrentBackBufferIndex()]->GetColorAttachment(0)->GetHandle().Get();
    }

    nvrhi::ITexture *DeviceManager_DX12::GetBackBuffer(uint32_t index)
    {
        if (index < m_SwapChainRenderTargets.size())
            return m_SwapChainRenderTargets[index]->GetColorAttachment(0)->GetHandle().Get();
        return nullptr;
    }

    // TODO: Use render target
	nvrhi::ITexture *DeviceManager_DX12::GetBackDepthBuffer(uint32_t index)
	{
		if (index < m_SwapChainRenderTargets.size())
			return m_SwapChainRenderTargets[index]->GetDepthAttachment()->GetHandle().Get();
		return nullptr;
	}

	uint32_t DeviceManager_DX12::GetCurrentBackBufferIndex()
    {
        return m_SwapChain->GetCurrentBackBufferIndex();
    }

    uint32_t DeviceManager_DX12::GetBackBufferCount()
    {
        return m_SwapChainDesc.BufferCount;
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
        m_GraphicsQueue->Signal(m_FrameFence, m_FrameCount);
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

    DeviceManager *DeviceManager::CreateD3D12(Window *window, const DeviceParameters& params)
    {
        s_D3D12DeviceInstance = new DeviceManager_DX12(window, params);
        return s_D3D12DeviceInstance;
    }

    DeviceManager_DX12 &DeviceManager_DX12::GetInstance()
    {
        return *s_D3D12DeviceInstance;
    }
}
