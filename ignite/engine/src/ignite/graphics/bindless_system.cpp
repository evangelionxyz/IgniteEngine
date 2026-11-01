// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"
#include "bindless_system.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/graphics/texture.hpp"
#include "ignite/graphics/renderer.hpp"

namespace ignite
{
    nri::Device* BindlessSystem::s_Device = nullptr;
    nri::DescriptorPool* BindlessSystem::s_DescriptorPool = nullptr;
    nri::DescriptorSet* BindlessSystem::s_DescriptorSet = nullptr;
    nri::PipelineLayout* BindlessSystem::s_PipelineLayout = nullptr;
    nri::DescriptorRangeDesc BindlessSystem::s_BindlessRange = {};
    nri::DescriptorSetDesc BindlessSystem::s_BindlessSetDesc = {};

    std::vector<uint32_t> BindlessSystem::s_FreeIndices;
    std::vector<BindlessSystem::PendingWrite> BindlessSystem::s_PendingWrites;
    std::vector<BindlessSystem::DeferredFree> BindlessSystem::s_DeferredFrees;
    uint32_t BindlessSystem::s_CurrentFrame = 0;
    uint32_t BindlessSystem::s_NextIndex = 1; // 0 is reserved for fallback/white texture
    std::mutex BindlessSystem::s_Mutex;
    bool BindlessSystem::s_Initialized = false;
    bool BindlessSystem::s_FallbackBound = false;

    void BindlessSystem::Initialize(nri::Device* device)
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        if (s_Initialized) return;

        s_Device = device;
        s_FallbackBound = false;

        if (!s_Device)
        {
            auto *dm = DeviceManager::GetInstance();
            if (dm) s_Device = dm->GetDevice();
        }

        if (s_Device)
        {
            auto *dm = DeviceManager::GetInstance();
            const auto &nri = dm->GetCoreInterface();

            // 1. Create global descriptor pool (bindless set is allocated first at heap offset 0)
            nri::DescriptorPoolDesc poolDesc = {};
            poolDesc.descriptorSetMaxNum = 16384;
            poolDesc.mutableMaxNum = 16384;
            poolDesc.samplerMaxNum = 8192;
            poolDesc.constantBufferMaxNum = 32768;
            poolDesc.textureMaxNum = 32768;
            poolDesc.storageTextureMaxNum = 8192;
            poolDesc.bufferMaxNum = 4096;
            poolDesc.storageBufferMaxNum = 4096;
            poolDesc.structuredBufferMaxNum = 16384;
            poolDesc.storageStructuredBufferMaxNum = 8192;
            poolDesc.flags = nri::DescriptorPoolBits::ALLOW_UPDATE_AFTER_SET;

            nri::Result res = nri.CreateDescriptorPool(*s_Device, poolDesc, s_DescriptorPool);
            LOG_ASSERT(res == nri::Result::SUCCESS && s_DescriptorPool, "[BindlessSystem] Failed to create descriptor pool");

            // 2. Create pipeline layout describing the bindless array at space 2 (matches -fvk-bind-resource-heap 0 2)
            s_BindlessRange = {};
            s_BindlessRange.baseRegisterIndex = 0;
            s_BindlessRange.descriptorNum = 16384;
            s_BindlessRange.descriptorType = nri::DescriptorType::MUTABLE;
            s_BindlessRange.shaderStages = nri::StageBits::ALL;
            s_BindlessRange.flags = nri::DescriptorRangeBits::PARTIALLY_BOUND
                | nri::DescriptorRangeBits::ARRAY
                | nri::DescriptorRangeBits::ALLOW_UPDATE_AFTER_SET;

            s_BindlessSetDesc = {};
            s_BindlessSetDesc.registerSpace = 2;
            s_BindlessSetDesc.ranges = &s_BindlessRange;
            s_BindlessSetDesc.rangeNum = 1;

            nri::PipelineLayoutDesc layoutDesc = {};
            layoutDesc.descriptorSets = &s_BindlessSetDesc;
            layoutDesc.descriptorSetNum = 1;
            layoutDesc.shaderStages = nri::StageBits::ALL;
            layoutDesc.flags = nri::PipelineLayoutBits::RESOURCE_HEAP_DIRECTLY_INDEXED;

            res = nri.CreatePipelineLayout(*s_Device, layoutDesc, s_PipelineLayout);
            LOG_ASSERT(res == nri::Result::SUCCESS && s_PipelineLayout, "[BindlessSystem] Failed to create pipeline layout");

            // 3. Allocate descriptor set from the pool (at heap offset 0 for D3D12 ResourceDescriptorHeap)
            if (s_DescriptorPool && s_PipelineLayout)
            {
                res = nri.AllocateDescriptorSets(*s_DescriptorPool, *s_PipelineLayout, 0, &s_DescriptorSet, 1, 0);
                LOG_ASSERT(res == nri::Result::SUCCESS && s_DescriptorSet, "[BindlessSystem] Failed to allocate descriptor set");
            }
        }

        s_Initialized = true;
        LOG_INFO("[BindlessSystem] Initialized bindless descriptor heap with capacity of 16384 slots");
    }

    void BindlessSystem::Shutdown()
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        if (!s_Initialized) return;

        if (s_Device)
        {
            auto *dm = DeviceManager::GetInstance();
            if (dm)
            {
                const auto &nri = dm->GetCoreInterface();
                if (s_PipelineLayout)
                {
                    nri.DestroyPipelineLayout(s_PipelineLayout);
                    s_PipelineLayout = nullptr;
                }
                if (s_DescriptorPool)
                {
                    nri.DestroyDescriptorPool(s_DescriptorPool);
                    s_DescriptorPool = nullptr;
                }
            }
        }

        s_DescriptorSet = nullptr;
        s_FreeIndices.clear();
        s_PendingWrites.clear();
        s_DeferredFrees.clear();
        s_CurrentFrame = 0;
        s_NextIndex = 1;
        s_Device = nullptr;
        s_FallbackBound = false;
        s_Initialized = false;
        LOG_INFO("[BindlessSystem] Shutdown completed");
    }

    uint32_t BindlessSystem::RegisterTexture(nri::Descriptor* textureDescriptor)
    {
        if (!textureDescriptor) return 0;

        std::lock_guard<std::mutex> lock(s_Mutex);
        if (!s_Initialized) return 0;

        uint32_t index = 0;
        if (!s_FreeIndices.empty())
        {
            index = s_FreeIndices.back();
            s_FreeIndices.pop_back();
        }
        else
        {
            index = s_NextIndex++;
        }

        s_PendingWrites.push_back({ index, textureDescriptor });
        return index;
    }

    void BindlessSystem::UnregisterTexture(uint32_t index)
    {
        if (index == 0 || index == 0xFFFFFFFF) return;

        std::lock_guard<std::mutex> lock(s_Mutex);
        if (!s_Initialized) return;

        s_PendingWrites.push_back({ index, nullptr });
        s_DeferredFrees.push_back({ index, s_CurrentFrame });
    }

    bool BindlessSystem::HasPendingWrites()
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        if (!s_Initialized) return false;

        if (!s_FallbackBound && Renderer::GetWhiteTexture() && Renderer::GetWhiteTexture()->GetShaderResourceView())
        {
            return true;
        }

        return !s_PendingWrites.empty() || !s_DeferredFrees.empty();
    }

    void BindlessSystem::FlushPendingWrites()
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        if (!s_Initialized) return;

        s_CurrentFrame++;

        auto *dm = DeviceManager::GetInstance();
        if (s_Device && s_DescriptorSet && dm)
        {
            const auto &nri = dm->GetCoreInterface();
            nri::Descriptor* fallbackDesc = (Renderer::GetWhiteTexture() && Renderer::GetWhiteTexture()->GetShaderResourceView())
                ? Renderer::GetWhiteTexture()->GetShaderResourceView()
                : nullptr;

            if (!s_FallbackBound && fallbackDesc)
            {
                nri::UpdateDescriptorRangeDesc updateDesc = {};
                updateDesc.descriptorSet = s_DescriptorSet;
                updateDesc.rangeIndex = 0;
                updateDesc.baseDescriptor = 0;
                updateDesc.descriptorNum = 1;
                updateDesc.descriptors = &fallbackDesc;
                nri.UpdateDescriptorRanges(&updateDesc, 1);
                s_FallbackBound = true;
            }

            for (const auto &write : s_PendingWrites)
            {
                nri::Descriptor* desc = write.descriptor ? write.descriptor : fallbackDesc;
                if (desc)
                {
                    nri::UpdateDescriptorRangeDesc updateDesc = {};
                    updateDesc.descriptorSet = s_DescriptorSet;
                    updateDesc.rangeIndex = 0;
                    updateDesc.baseDescriptor = write.index;
                    updateDesc.descriptorNum = 1;
                    updateDesc.descriptors = &desc;
                    nri.UpdateDescriptorRanges(&updateDesc, 1);
                }
            }
        }

        s_PendingWrites.clear();

        auto it = s_DeferredFrees.begin();
        while (it != s_DeferredFrees.end())
        {
            if (it->frame + 4 < s_CurrentFrame)
            {
                s_FreeIndices.push_back(it->index);
                it = s_DeferredFrees.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    nri::DescriptorPool* BindlessSystem::GetDescriptorPool()
    {
        return s_DescriptorPool;
    }

    nri::DescriptorSet* BindlessSystem::GetDescriptorSet()
    {
        return s_DescriptorSet;
    }

    nri::PipelineLayout* BindlessSystem::GetPipelineLayout()
    {
        return s_PipelineLayout;
    }

    const nri::DescriptorSetDesc& BindlessSystem::GetDescriptorSetDesc()
    {
        return s_BindlessSetDesc;
    }

    BindingLayoutDesc BindlessSystem::GetBindingLayout()
    {
        return BindingLayoutDesc()
            .SetRegisterSpace(2)
            .SetVisibility(nri::StageBits::ALL)
            .AddItem(BindingLayoutItem::Mutable(0, 16384));
    }
}
