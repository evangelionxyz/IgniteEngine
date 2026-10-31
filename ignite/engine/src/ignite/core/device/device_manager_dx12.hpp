// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_DEVICE_MANAGER_DX12_HPP
#define IGN_DEVICE_MANAGER_DX12_HPP

#ifdef PLATFORM_WINDOWS

#include <vector>
#include <string>

#include "device_manager.hpp"
#include "ignite/core/types.hpp"
#include "ignite/graphics/render_target.hpp"

#include <Windows.h>
#include <wrl/client.h>
#include <dxgi1_5.h>
#include <dxgidebug.h>
#include <d3d12.h>

#include <NRI.h>
#include <NRIDescs.h>
#include <Extensions/NRIWrapperD3D12.h>
#include <Extensions/NRIHelper.h>

namespace ignite
{
    struct IGN_API DescriptorHeapAllocator
    {
        ID3D12DescriptorHeap *heap = nullptr;
        D3D12_DESCRIPTOR_HEAP_TYPE heapType = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
        D3D12_CPU_DESCRIPTOR_HANDLE heapStartCpu{};
        D3D12_GPU_DESCRIPTOR_HANDLE heapStartGpu{};
        UINT heapHandleIncrement = 0;
        std::vector<int> freeIndices;

        void Create(ID3D12Device *device, ID3D12DescriptorHeap *heap);
        void Destroy();
        void Alloc(D3D12_CPU_DESCRIPTOR_HANDLE *out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE *out_gpu_desc_handle);
        void Free(D3D12_CPU_DESCRIPTOR_HANDLE cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_desc_handle);
    };

    class IGN_API DeviceManager_DX12 final : public DeviceManager
    {
    public:
        DeviceManager_DX12(Window *window, const DeviceParameters &params);

        Microsoft::WRL::ComPtr<IDXGIFactory2> m_DxgiFactory2;
        Microsoft::WRL::ComPtr<ID3D12Device> m_Device12;
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_GraphicsQueue;
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_ComputeQueue;
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_CopyQueue;
        Microsoft::WRL::ComPtr<IDXGISwapChain3> m_SwapChain;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_RtvDescHeap;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_SrvDescHeap;
        DescriptorHeapAllocator m_SrvDescHeapAlloc;
        DXGI_SWAP_CHAIN_DESC1 m_SwapChainDesc{};
        DXGI_SWAP_CHAIN_FULLSCREEN_DESC m_FullScreenDesc{};
        Microsoft::WRL::ComPtr<IDXGIAdapter> m_DxgiAdapter;
        HWND m_Hwnd = nullptr;
        bool m_TearingSupported = false;
        const int SRV_HEAP_SIZE = 64;

        std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_SwapChainBuffers;

        // NRI device, interfaces and queues
        nri::Device *m_Device = nullptr;
        nri::CoreInterface m_NRI{};
        nri::HelperInterface m_HelperNRI{};
        nri::WrapperD3D12Interface m_WrapperD3D12{};
        nri::Queue *m_NriGraphicsQueue = nullptr;

        // NRI back buffers & depth buffers
        std::vector<Ref<RenderTarget>> m_BackBufferRenderTargets;
        std::vector<nri::Texture *> m_BackBufferTextures;
        std::vector<nri::Descriptor *> m_BackBufferRTVs;
        std::vector<nri::Texture *> m_DepthBufferTextures;
        std::vector<nri::Memory *> m_DepthBufferMemories;
        std::vector<nri::Descriptor *> m_DepthBufferDSVs;

        Microsoft::WRL::ComPtr<ID3D12Fence> m_FrameFence;
        std::vector<HANDLE> m_FrameFenceEvents;
        uint64_t m_FrameCount = 1;
        std::string m_RendererString;

    public:
        void ReportLiveObjects() override;
        bool EnumerateAdapters(std::vector<AdapterInfo> &outAdapters) override;

        static std::string GetAdapterName(DXGI_ADAPTER_DESC const &aDesc)
        {
            const size_t length = wcsnlen(aDesc.Description, _countof(aDesc.Description));
            std::string name;
            name.resize(length);
            WideCharToMultiByte(CP_ACP, 0, aDesc.Description, (int)length, name.data(), (int)name.size(), nullptr, nullptr);
            return name;
        }

        const char *GetRendererString() const override
        {
            return m_RendererString.c_str();
        }

        nri::Device *GetDevice() const override
        {
            return m_Device;
        }

        nri::GraphicsAPI GetGraphicsAPI() const override
        {
            return nri::GraphicsAPI::D3D12;
        }

        const nri::CoreInterface &GetCoreInterface() const override
        {
            return m_NRI;
        }

        const nri::HelperInterface &GetHelperInterface() const override
        {
            return m_HelperNRI;
        }

        nri::Queue *GetGraphicsQueue() const override
        {
            return m_NriGraphicsQueue;
        }

        void WaitForIdle() override;

        static DeviceManager_DX12 &GetInstance();

    protected:
        bool CreateInstanceInternal() override;
        bool CreateDevice() override;
        bool CreateSwapChain() override;
        void DestroyDeviceAndSwapChain() override;
        void ResizeSwapChain() override;

        nri::Texture *GetCurrentBackBuffer() override;
        nri::Texture *GetBackBuffer(uint32_t index) override;
        nri::Texture *GetBackDepthBuffer(uint32_t index) override;
        nri::Descriptor *GetCurrentBackBufferRTV() override;
        nri::Descriptor *GetBackBufferRTV(uint32_t index) override;
        nri::Descriptor *GetCurrentBackDepthBufferDSV() override;
        nri::Descriptor *GetBackDepthBufferDSV(uint32_t index) override;

        uint32_t GetCurrentBackBufferIndex() override;
        uint32_t GetBackBufferCount() override;
        RenderTarget *GetCurrentBackBufferRenderTarget() override;
        RenderTarget *GetBackBufferRenderTarget(uint32_t index) override;
        bool BeginFrame() override;
        bool Present() override;
        void Destroy() override;
        bool CreateRenderTargets();
        void ReleaseRenderTargets();
    };
}

#endif // PLATFORM_WINDOWS

#endif
