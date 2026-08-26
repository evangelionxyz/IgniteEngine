// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_BINDLESS_SYSTEM_HPP
#define IGN_BINDLESS_SYSTEM_HPP

#include "ignite/core/base.hpp"

#include <nvrhi/nvrhi.h>
#include <vector>
#include <mutex>

namespace ignite
{
    class IGN_API BindlessSystem
    {
    public:
        static void Initialize(nvrhi::IDevice* device);
        static void Shutdown();

        static uint32_t RegisterTexture(nvrhi::ITexture* texture);
        static void UnregisterTexture(uint32_t index);

        static nvrhi::BindingLayoutHandle GetBindingLayout();
        static nvrhi::DescriptorTableHandle GetDescriptorTable();
        static nvrhi::BindingLayoutHandle GetDummyLayout();
        static nvrhi::BindingSetHandle GetDummyBindingSet();
        static void FlushPendingWrites();
        static bool HasPendingWrites();

    private:
        struct PendingWrite
        {
            uint32_t index;
            nvrhi::TextureHandle texture;
        };
        struct DeferredFree
        {
            uint32_t index;
            uint32_t frame;
        };

        static nvrhi::IDevice* s_Device;
        static nvrhi::BindingLayoutHandle s_BindlessLayout;
        static nvrhi::DescriptorTableHandle s_DescriptorTable;
        static nvrhi::BindingLayoutHandle s_DummyLayout;
        static nvrhi::BindingSetHandle s_DummyBindingSet;
        static std::vector<uint32_t> s_FreeIndices;
        static std::vector<PendingWrite> s_PendingWrites;
        static std::vector<DeferredFree> s_DeferredFrees;
        static uint32_t s_CurrentFrame;
        static uint32_t s_NextIndex;
        static std::mutex s_Mutex;
        static bool s_Initialized;
        static bool s_FallbackBound;
    };
}

#endif
