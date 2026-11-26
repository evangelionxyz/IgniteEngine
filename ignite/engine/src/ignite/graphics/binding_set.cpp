// Copyright (c) 2026 Evangelion Manuhutu
#include "ignite_pch.hpp"
#include "binding_set.hpp"
#include "binding_cache.hpp"
#include "bindless_system.hpp"
#include "renderer.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/core/hashing.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"

#include <algorithm>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace ignite
{
    namespace
    {
        struct DescriptorSetPoolKey
        {
            nri::PipelineLayout *pipelineLayout = nullptr;
            uint32_t setIndex = 0;

            bool operator==(const DescriptorSetPoolKey &other) const noexcept
            {
                return pipelineLayout == other.pipelineLayout && setIndex == other.setIndex;
            }
        };

        struct DescriptorSetPoolKeyHash
        {
            size_t operator()(const DescriptorSetPoolKey &key) const noexcept
            {
                return Hashing::HashCombineAll(key.pipelineLayout, key.setIndex);
            }
        };

        struct RetiredDescriptorSet
        {
            nri::DescriptorSet *handle = nullptr;
            DescriptorSetPoolKey key;
            uint64_t retiredFrame = 0;
        };

        struct DescriptorSetRecyclingPool
        {
            std::mutex mutex;
            uint64_t currentFrame = 0;
            std::unordered_map<nri::DescriptorSet *, DescriptorSetPoolKey> allocatedMeta;
            std::unordered_set<nri::DescriptorSet *> inUseSets;
            std::vector<RetiredDescriptorSet> retiredSets;
            std::unordered_map<DescriptorSetPoolKey, std::vector<nri::DescriptorSet *>, DescriptorSetPoolKeyHash> freeSets;
        };

        DescriptorSetRecyclingPool g_DescriptorSetPool;
        constexpr uint64_t kFramesBeforeDescriptorSetReuse = 4;
    }
    nri::PipelineLayout *BindingLayoutDesc::CreatePipelineLayout(nri::StageBits shaderStages, nri::Device *device) const
    {
        return CreatePipelineLayout(std::vector<BindingLayoutDesc>{ *this }, shaderStages, device);
    }

    nri::PipelineLayout *BindingLayoutDesc::CreatePipelineLayout(const std::vector<BindingLayoutDesc> &layouts, nri::StageBits shaderStages, nri::Device *device)
    {
        auto *dm = DeviceManager::GetInstance();
        if (!device && dm)
            device = dm->GetDevice();
        if (!device || !dm)
            return nullptr;

        const auto &nri = dm->GetCoreInterface();

        std::vector<BindingLayoutDesc> processedLayouts = layouts;
        std::vector<nri::DescriptorSetDesc> setDescs;
        std::vector<nri::RootConstantDesc> rootConstants;
        nri::PipelineLayoutBits flags = nri::PipelineLayoutBits::NONE;

        nri::StageBits effectiveShaderStages = shaderStages;
        if (effectiveShaderStages == nri::StageBits::ALL)
        {
            // Only include stages that are supported on standard desktop graphics devices without optional extensions (no mesh/task or RT unless enabled)
            effectiveShaderStages = nri::StageBits::VERTEX_SHADER
                                  | nri::StageBits::TESSELLATION_SHADERS
                                  | nri::StageBits::GEOMETRY_SHADER
                                  | nri::StageBits::FRAGMENT_SHADER
                                  | nri::StageBits::COMPUTE_SHADER;
            if (dm && dm->GetDeviceParameters().enableRayTracingExtensions)
            {
                effectiveShaderStages |= nri::StageBits::RAY_TRACING_SHADERS;
            }
        }

        setDescs.reserve(processedLayouts.size());
        for (auto &layout : processedLayouts)
        {
            if (layout.visibility == nri::StageBits::ALL)
                layout.SetVisibility(effectiveShaderStages);

            if (!layout.bindings.empty())
            {
                layout.ranges.clear();
                layout.rootConstants.clear();
                for (const auto &item : layout.bindings)
                {
                    layout.AddItem(item);
                }
            }

            if (!layout.ranges.empty())
            {
                nri::DescriptorSetDesc setDesc = {};
                setDesc.registerSpace = layout.registerSpace;
                setDesc.ranges = layout.ranges.data();
                setDesc.rangeNum = static_cast<uint32_t>(layout.ranges.size());
                setDescs.push_back(setDesc);
            }

            for (const auto &rc : layout.rootConstants)
            {
                rootConstants.push_back(rc);
            }

            if (layout.isBindless)
            {
                flags |= nri::PipelineLayoutBits::RESOURCE_HEAP_DIRECTLY_INDEXED;
            }
        }

        nri::PipelineLayoutDesc pipelineLayoutDesc = {};
        pipelineLayoutDesc.descriptorSets = setDescs.empty() ? nullptr : setDescs.data();
        pipelineLayoutDesc.descriptorSetNum = static_cast<uint32_t>(setDescs.size());
        pipelineLayoutDesc.rootConstants = rootConstants.empty() ? nullptr : rootConstants.data();
        pipelineLayoutDesc.rootConstantNum = static_cast<uint32_t>(rootConstants.size());
        pipelineLayoutDesc.shaderStages = effectiveShaderStages;
        pipelineLayoutDesc.flags = flags;

        nri::PipelineLayout *pipelineLayout = nullptr;
        nri::Result res = nri.CreatePipelineLayout(*device, pipelineLayoutDesc, pipelineLayout);
        LOG_ASSERT(res == nri::Result::SUCCESS && pipelineLayout, "[BindingLayoutDesc] Failed to create pipeline layout!");
        return pipelineLayout;
    }

    BindingSet::BindingSet(const std::string &name, nri::DescriptorSet *handle)
        : m_Name(name), m_Handle(handle)
    {
        Renderer::IncrementBindingSetCount(m_Name);
    }

    BindingSet::BindingSet(const std::string &name, nri::PipelineLayout *pipelineLayout, uint32_t setIndex, const BindingSetDesc &desc)
        : m_Name(name), m_PipelineLayout(pipelineLayout), m_SetIndex(setIndex), m_Desc(desc)
    {
        m_Handle = AllocateAndUpdate(pipelineLayout, setIndex, desc);
        Renderer::IncrementBindingSetCount(m_Name);
    }

    BindingSet::~BindingSet()
    {
        if (m_Handle && BindlessSystem::GetDescriptorPool())
        {
            std::lock_guard<std::mutex> lock(g_DescriptorSetPool.mutex);
            if (g_DescriptorSetPool.inUseSets.erase(m_Handle) > 0)
            {
                auto it = g_DescriptorSetPool.allocatedMeta.find(m_Handle);
                if (it != g_DescriptorSetPool.allocatedMeta.end())
                {
                    g_DescriptorSetPool.retiredSets.push_back({ m_Handle, it->second, g_DescriptorSetPool.currentFrame });
                }
            }
        }
        m_Handle = nullptr;
        Renderer::DecrementBindingSetCount(m_Name);
    }

    Ref<BindingSet> BindingSet::Create(const std::string &name, nri::DescriptorSet *handle)
    {
        return CreateRef<BindingSet>(name, handle);
    }

    Ref<BindingSet> BindingSet::Create(const std::string &name, nri::PipelineLayout *pipelineLayout, uint32_t setIndex, const BindingSetDesc &desc)
    {
        return CreateRef<BindingSet>(name, pipelineLayout, setIndex, desc);
    }

    Ref<BindingSet> BindingSet::CreateOrUpdate(Ref<BindingSet> &bindingSet, const std::string &name, nri::PipelineLayout *pipelineLayout, uint32_t setIndex, const BindingSetDesc &desc)
    {
        if (!bindingSet)
        {
            bindingSet = Create(name, pipelineLayout, setIndex, desc);
        }
        else
        {
            bindingSet->Update(desc);
        }
        return bindingSet;
    }

    void BindingSet::Update(const BindingSetDesc &desc)
    {
        m_Desc = desc;
        if (!m_Handle && m_PipelineLayout)
        {
            m_Handle = AllocateAndUpdate(m_PipelineLayout, m_SetIndex, desc);
            return;
        }
        if (m_Handle)
        {
            if (auto *dm = DeviceManager::GetInstance(); dm && dm->GetDevice())
            {
                GPUUploadSync::DeviceWaitIdle(dm->GetDevice());
            }
            Update(m_Handle, desc);
        }
    }

    nri::DescriptorSet *BindingSet::AllocateAndUpdate(nri::PipelineLayout *pipelineLayout, uint32_t setIndex, const BindingSetDesc &desc)
    {
        if (!pipelineLayout)
            return nullptr;

        auto *dm = DeviceManager::GetInstance();
        nri::DescriptorPool *pool = BindlessSystem::GetDescriptorPool();
        if (!dm || !pool)
            return nullptr;

        const auto &nri = dm->GetCoreInterface();
        nri::DescriptorSet *descriptorSet = nullptr;
        const DescriptorSetPoolKey key{ pipelineLayout, setIndex };

        {
            std::lock_guard<std::mutex> lock(g_DescriptorSetPool.mutex);
            auto freeIt = g_DescriptorSetPool.freeSets.find(key);
            if (freeIt != g_DescriptorSetPool.freeSets.end() && !freeIt->second.empty())
            {
                descriptorSet = freeIt->second.back();
                freeIt->second.pop_back();
                g_DescriptorSetPool.inUseSets.insert(descriptorSet);
            }
            else
            {
                nri::Result res = nri.AllocateDescriptorSets(*pool, *pipelineLayout, setIndex, &descriptorSet, 1, 0);
                LOG_ASSERT(res == nri::Result::SUCCESS && descriptorSet, "[BindingSet] Failed to allocate descriptor set!");
                if (res == nri::Result::SUCCESS && descriptorSet)
                {
                    g_DescriptorSetPool.allocatedMeta[descriptorSet] = key;
                    g_DescriptorSetPool.inUseSets.insert(descriptorSet);
                }
                else
                {
                    descriptorSet = nullptr;
                }
            }
        }

        if (descriptorSet)
        {
            Update(descriptorSet, desc);
        }

        return descriptorSet;
    }

    void BindingSet::Update(nri::DescriptorSet *descriptorSet, const BindingSetDesc &desc)
    {
        if (!descriptorSet || desc.bindings.empty())
            return;

        auto *dm = DeviceManager::GetInstance();
        if (!dm)
            return;

        const auto &nri = dm->GetCoreInterface();
        std::vector<nri::UpdateDescriptorRangeDesc> updates;
        updates.reserve(desc.bindings.size());

        for (size_t i = 0; i < desc.bindings.size(); ++i)
        {
            const auto &item = desc.bindings[i];
            if (!item.descriptor)
                continue;

            nri::UpdateDescriptorRangeDesc u = {};
            u.descriptorSet = descriptorSet;
            u.rangeIndex = static_cast<uint32_t>(i);
            u.baseDescriptor = item.baseDescriptor;
            u.descriptors = &item.descriptor;
            u.descriptorNum = 1;
            updates.push_back(u);
        }

        if (!updates.empty())
        {
            nri.UpdateDescriptorRanges(updates.data(), static_cast<uint32_t>(updates.size()));
        }
    }

    nri::DescriptorSet *BindingSet::AllocateAndUpdate(const BindingSetDesc &desc, nri::PipelineLayout *pipelineLayout, uint32_t setIndex)
    {
        return AllocateAndUpdate(pipelineLayout, setIndex, desc);
    }

    void BindingSet::AdvanceFrame(uint64_t frameCounter)
    {
        std::lock_guard<std::mutex> lock(g_DescriptorSetPool.mutex);
        g_DescriptorSetPool.currentFrame = frameCounter;

        auto &retired = g_DescriptorSetPool.retiredSets;
        size_t writeIdx = 0;
        for (size_t i = 0; i < retired.size(); ++i)
        {
            const auto &entry = retired[i];
            if (frameCounter >= entry.retiredFrame + kFramesBeforeDescriptorSetReuse)
            {
                g_DescriptorSetPool.freeSets[entry.key].push_back(entry.handle);
            }
            else
            {
                if (writeIdx != i)
                    retired[writeIdx] = entry;
                ++writeIdx;
            }
        }
        retired.resize(writeIdx);
    }

    void BindingSet::ReclaimRetiredBeforeCurrentFrame()
    {
        std::lock_guard<std::mutex> lock(g_DescriptorSetPool.mutex);
        const uint64_t currentFrame = g_DescriptorSetPool.currentFrame;

        auto &retired = g_DescriptorSetPool.retiredSets;
        size_t writeIdx = 0;
        for (size_t i = 0; i < retired.size(); ++i)
        {
            const auto &entry = retired[i];
            if (entry.retiredFrame < currentFrame)
            {
                g_DescriptorSetPool.freeSets[entry.key].push_back(entry.handle);
            }
            else
            {
                if (writeIdx != i)
                    retired[writeIdx] = entry;
                ++writeIdx;
            }
        }
        retired.resize(writeIdx);
    }

    void BindingSet::OnPipelineLayoutDestroyed(nri::PipelineLayout *pipelineLayout)
    {
        if (!pipelineLayout)
            return;

        BindingCache::RemovePipelineLayout(pipelineLayout);

        std::lock_guard<std::mutex> lock(g_DescriptorSetPool.mutex);

        for (auto it = g_DescriptorSetPool.freeSets.begin(); it != g_DescriptorSetPool.freeSets.end(); )
        {
            if (it->first.pipelineLayout == pipelineLayout)
                it = g_DescriptorSetPool.freeSets.erase(it);
            else
                ++it;
        }

        std::erase_if(g_DescriptorSetPool.retiredSets, [pipelineLayout](const RetiredDescriptorSet &entry)
        {
            return entry.key.pipelineLayout == pipelineLayout;
        });

        for (auto it = g_DescriptorSetPool.allocatedMeta.begin(); it != g_DescriptorSetPool.allocatedMeta.end(); )
        {
            if (it->second.pipelineLayout == pipelineLayout)
            {
                g_DescriptorSetPool.inUseSets.erase(it->first);
                it = g_DescriptorSetPool.allocatedMeta.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    void BindingSet::ShutdownPool()
    {
        std::lock_guard<std::mutex> lock(g_DescriptorSetPool.mutex);
        g_DescriptorSetPool.freeSets.clear();
        g_DescriptorSetPool.retiredSets.clear();
        g_DescriptorSetPool.inUseSets.clear();
        g_DescriptorSetPool.allocatedMeta.clear();
        g_DescriptorSetPool.currentFrame = 0;
    }
}
