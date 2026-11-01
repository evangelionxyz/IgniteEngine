// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_BINDLESS_SYSTEM_HPP
#define IGN_BINDLESS_SYSTEM_HPP

#include "ignite/core/base.hpp"
#include "binding_set.hpp"

#include <NRI.h>
#include <vector>
#include <mutex>

namespace ignite
{
    class IGN_API BindlessSystem
    {
    public:
        static void Initialize(nri::Device* device = nullptr);
        static void Shutdown();

        static uint32_t RegisterTexture(nri::Descriptor* textureDescriptor);
        static void UnregisterTexture(uint32_t index);

        // NRI accessors
        static nri::DescriptorPool* GetDescriptorPool();
        static nri::DescriptorSet* GetDescriptorSet();
        static nri::PipelineLayout* GetPipelineLayout();
        static const nri::DescriptorSetDesc& GetDescriptorSetDesc();
        static BindingLayoutDesc GetBindingLayout();

        static void FlushPendingWrites();
        static bool HasPendingWrites();

    private:
        struct PendingWrite
        {
            uint32_t index = 0;
            nri::Descriptor* descriptor = nullptr;
        };
        struct DeferredFree
        {
            uint32_t index = 0;
            uint32_t frame = 0;
        };

        static nri::Device* s_Device;
        static nri::DescriptorPool* s_DescriptorPool;
        static nri::DescriptorSet* s_DescriptorSet;
        static nri::PipelineLayout* s_PipelineLayout;
        static nri::DescriptorRangeDesc s_BindlessRange;
        static nri::DescriptorSetDesc s_BindlessSetDesc;

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
