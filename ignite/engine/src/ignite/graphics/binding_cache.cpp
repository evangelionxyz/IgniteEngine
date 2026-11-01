// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "binding_cache.hpp"
#include "ignite/core/hashing.hpp"
#include "ignite/core/logger.hpp"

#include <algorithm>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace ignite
{
    namespace
    {
        size_t ComputeBindingCacheHash(const BindingSetDesc &desc, nri::PipelineLayout *pipelineLayout, uint32_t setIndex)
        {
            size_t seed = Hashing::HashCombineAll(pipelineLayout, setIndex);
            for (const auto &item : desc.bindings)
            {
                Hashing::HashCombine(seed, item.rangeIndex);
                Hashing::HashCombine(seed, item.baseDescriptor);
                Hashing::HashCombine(seed, reinterpret_cast<size_t>(item.descriptor));
                Hashing::HashCombine(seed, item.isPushConstants);
            }
            return seed;
        }
    }

    struct BindingCacheEntry
    {
        nri::PipelineLayout *pipelineLayout = nullptr;
        uint32_t setIndex = 0;
        BindingSetDesc desc;
        Ref<BindingSet> bindingSet;
    };

    struct BindingCacheData
    {
        nri::Device *device = nullptr;
        std::unordered_map<size_t, std::vector<BindingCacheEntry>> bindingSets;
        std::shared_mutex mutex;
    };

    static BindingCacheData g_BindingCacheData;

    namespace
    {
        BindingCacheEntry *FindEntry(BindingCacheData &cache, size_t hash, const BindingSetDesc &desc,
            nri::PipelineLayout *pipelineLayout, uint32_t setIndex)
        {
            auto it = cache.bindingSets.find(hash);
            if (it == cache.bindingSets.end())
                return nullptr;

            for (auto &entry : it->second)
            {
                if (entry.pipelineLayout == pipelineLayout && entry.setIndex == setIndex && entry.desc == desc)
                    return &entry;
            }
            return nullptr;
        }
    }

    void BindingCache::Init(nri::Device *device)
    {
        std::unique_lock lock(g_BindingCacheData.mutex);
        g_BindingCacheData.device = device;
    }

    void BindingCache::Shutdown()
    {
        std::unique_lock lock(g_BindingCacheData.mutex);
        g_BindingCacheData.bindingSets.clear();
        g_BindingCacheData.device = nullptr;
    }

    void BindingCache::Clear()
    {
        std::unique_lock lock(g_BindingCacheData.mutex);
        g_BindingCacheData.bindingSets.clear();
    }

    Ref<BindingSet> BindingCache::GetCachedBindingSet(const BindingSetDesc &desc, nri::PipelineLayout *pipelineLayout, uint32_t setIndex)
    {
        const size_t hash = ComputeBindingCacheHash(desc, pipelineLayout, setIndex);
        std::shared_lock lock(g_BindingCacheData.mutex);
        BindingCacheEntry *entry = FindEntry(g_BindingCacheData, hash, desc, pipelineLayout, setIndex);
        return entry ? entry->bindingSet : nullptr;
    }

    Ref<BindingSet> BindingCache::GetOrCreateBindingSet(const BindingSetDesc &desc, nri::PipelineLayout *pipelineLayout, uint32_t setIndex, const std::string &debugName)
    {
        const size_t hash = ComputeBindingCacheHash(desc, pipelineLayout, setIndex);
        std::unique_lock lock(g_BindingCacheData.mutex);

        if (BindingCacheEntry *entry = FindEntry(g_BindingCacheData, hash, desc, pipelineLayout, setIndex))
            return entry->bindingSet;

        nri::DescriptorSet *descriptorSet = BindingSet::AllocateAndUpdate(pipelineLayout, setIndex, desc);
        if (!descriptorSet)
            return nullptr;

        Ref<BindingSet> bindingSet = BindingSet::Create(debugName.empty() ? "[Cached BindingSet]" : debugName, descriptorSet);
        g_BindingCacheData.bindingSets[hash].push_back({ pipelineLayout, setIndex, desc, bindingSet });
        return bindingSet;
    }

    void BindingCache::RemoveBindingSet(const BindingSetDesc &desc, nri::PipelineLayout *pipelineLayout, uint32_t setIndex)
    {
        const size_t hash = ComputeBindingCacheHash(desc, pipelineLayout, setIndex);
        std::unique_lock lock(g_BindingCacheData.mutex);
        auto it = g_BindingCacheData.bindingSets.find(hash);
        if (it == g_BindingCacheData.bindingSets.end())
            return;

        auto &entries = it->second;
        entries.erase(std::remove_if(entries.begin(), entries.end(), [&](const BindingCacheEntry &entry)
        {
            return entry.pipelineLayout == pipelineLayout && entry.setIndex == setIndex && entry.desc == desc;
        }), entries.end());
        if (entries.empty())
            g_BindingCacheData.bindingSets.erase(it);
    }
}
