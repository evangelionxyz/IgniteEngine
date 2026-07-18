// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"
#include "bindless_system.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/graphics/texture.hpp"
#include "ignite/graphics/renderer.hpp"

namespace ignite
{
    nvrhi::IDevice* BindlessSystem::s_Device = nullptr;
    nvrhi::BindingLayoutHandle BindlessSystem::s_BindlessLayout = nullptr;
    nvrhi::DescriptorTableHandle BindlessSystem::s_DescriptorTable = nullptr;
    nvrhi::BindingLayoutHandle BindlessSystem::s_DummyLayout = nullptr;
    nvrhi::BindingSetHandle BindlessSystem::s_DummyBindingSet = nullptr;
    std::vector<uint32_t> BindlessSystem::s_FreeIndices;
    std::vector<BindlessSystem::PendingWrite> BindlessSystem::s_PendingWrites;
    std::vector<BindlessSystem::DeferredFree> BindlessSystem::s_DeferredFrees;
    uint32_t BindlessSystem::s_CurrentFrame = 0;
    uint32_t BindlessSystem::s_NextIndex = 1; // 0 is reserved for fallback/white texture
    std::mutex BindlessSystem::s_Mutex;
    bool BindlessSystem::s_Initialized = false;

    void BindlessSystem::Initialize(nvrhi::IDevice* device)
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        if (s_Initialized) return;

        s_Device = device;

        nvrhi::BindlessLayoutDesc layoutDesc;
        layoutDesc.setVisibility(nvrhi::ShaderType::All);
        layoutDesc.setLayoutType(nvrhi::BindlessLayoutDesc::LayoutType::MutableSrvUavCbv);
        layoutDesc.setMaxCapacity(16384);
        
        s_BindlessLayout = device->createBindlessLayout(layoutDesc);
        LOG_ASSERT(s_BindlessLayout, "[BindlessSystem] Failed to create bindless layout");

        s_DescriptorTable = device->createDescriptorTable(s_BindlessLayout);
        LOG_ASSERT(s_DescriptorTable, "[BindlessSystem] Failed to create descriptor table");

        device->resizeDescriptorTable(s_DescriptorTable, 16384);

        // Create empty layout and binding set for set 1 alignment
        nvrhi::BindingLayoutDesc emptyLayoutDesc;
        emptyLayoutDesc.setVisibility(nvrhi::ShaderType::All);
        s_DummyLayout = device->createBindingLayout(emptyLayoutDesc);
        LOG_ASSERT(s_DummyLayout, "[BindlessSystem] Failed to create dummy layout");

        nvrhi::BindingSetDesc emptySetDesc;
        s_DummyBindingSet = device->createBindingSet(emptySetDesc, s_DummyLayout);
        LOG_ASSERT(s_DummyBindingSet, "[BindlessSystem] Failed to create dummy binding set");

        s_Initialized = true;
        LOG_INFO("[BindlessSystem] Initialized bindless descriptor heap with capacity of 16384 slots");
    }

    void BindlessSystem::Shutdown()
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        if (!s_Initialized) return;

        s_DescriptorTable = nullptr;
        s_BindlessLayout = nullptr;
        s_DummyBindingSet = nullptr;
        s_DummyLayout = nullptr;
        s_FreeIndices.clear();
        s_PendingWrites.clear();
        s_DeferredFrees.clear();
        s_CurrentFrame = 0;
        s_NextIndex = 1;
        s_Device = nullptr;
        s_Initialized = false;
        LOG_INFO("[BindlessSystem] Shutdown completed");
    }

    uint32_t BindlessSystem::RegisterTexture(nvrhi::ITexture* texture)
    {
        if (!texture) return 0;

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

        s_PendingWrites.push_back({ index, texture });
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

    void BindlessSystem::FlushPendingWrites()
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        if (!s_Initialized) return;

        s_CurrentFrame++;

        nvrhi::ITexture* fallbackTex = Renderer::GetWhiteTexture() ? Renderer::GetWhiteTexture()->GetHandle() : nullptr;
        
        static bool fallbackBound = false;
        if (!fallbackBound && fallbackTex)
        {
            s_Device->writeDescriptorTable(s_DescriptorTable, nvrhi::BindingSetItem::Texture_SRV(0, fallbackTex));
            fallbackBound = true;
        }

        for (const auto& write : s_PendingWrites)
        {
            nvrhi::ITexture* tex = write.texture ? write.texture.Get() : fallbackTex;
            if (tex)
            {
                s_Device->writeDescriptorTable(s_DescriptorTable, nvrhi::BindingSetItem::Texture_SRV(write.index, tex));
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

    nvrhi::BindingLayoutHandle BindlessSystem::GetBindingLayout()
    {
        return s_BindlessLayout;
    }

    nvrhi::DescriptorTableHandle BindlessSystem::GetDescriptorTable()
    {
        return s_DescriptorTable;
    }

    nvrhi::BindingLayoutHandle BindlessSystem::GetDummyLayout()
    {
        return s_DummyLayout;
    }

    nvrhi::BindingSetHandle BindlessSystem::GetDummyBindingSet()
    {
        return s_DummyBindingSet;
    }
}
