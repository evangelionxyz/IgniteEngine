// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "binding_cache.hpp"
#include "ignite/core/hashing.hpp"
#include "ignite/core/logger.hpp"

#include <unordered_map>
#include <shared_mutex>

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
                Hashing::HashCombine(seed, (size_t)item.descriptor);
            }
            return seed;
        }
    }

    struct BindingCacheData
    {
        nri::Device *device = nullptr;
        std::unordered_map<size_t, Ref<BindingSet>> bindingSets;
        std::shared_mutex mutex;
    };

    static BindingCacheData g_BindingCacheData;

    void BindingCache::Init(nri::Device *device)
    {
        g_BindingCacheData.device = device;
    }

    void BindingCache::Shutdown()
    {
        g_BindingCacheData.mutex.lock();
        g_BindingCacheData.bindingSets.clear();
        g_BindingCacheData.mutex.unlock();

        g_BindingCacheData.device = nullptr;
    }

    void BindingCache::Clear()
    {
        g_BindingCacheData.mutex.lock();
        g_BindingCacheData.bindingSets.clear();
        g_BindingCacheData.mutex.unlock();
    }

    Ref<BindingSet> BindingCache::GetCachedBindingSet(const BindingSetDesc &desc, nri::PipelineLayout *pipelineLayout, uint32_t setIndex)
    {
        const size_t hash = ComputeBindingCacheHash(desc, pipelineLayout, setIndex);

        g_BindingCacheData.mutex.lock_shared();
        Ref<BindingSet> result = nullptr;
        auto it = g_BindingCacheData.bindingSets.find(hash);
        if (it != g_BindingCacheData.bindingSets.end())
            result = it->second;
        g_BindingCacheData.mutex.unlock_shared();

        return result;
    }

    Ref<BindingSet> BindingCache::GetOrCreateBindingSet(const BindingSetDesc &desc, nri::PipelineLayout *pipelineLayout, uint32_t setIndex, const std::string &debugName)
    {
        const size_t hash = ComputeBindingCacheHash(desc, pipelineLayout, setIndex);

        g_BindingCacheData.mutex.lock_shared();
        Ref<BindingSet> result = nullptr;
        auto it = g_BindingCacheData.bindingSets.find(hash);
        if (it != g_BindingCacheData.bindingSets.end())
            result = it->second;
        g_BindingCacheData.mutex.unlock_shared();

        if (!result)
        {
            g_BindingCacheData.mutex.lock();
            Ref<BindingSet> &entry = g_BindingCacheData.bindingSets[hash];
            if (!entry)
            {
                nri::DescriptorSet *descriptorSet = BindingSet::AllocateAndUpdate(pipelineLayout, setIndex, desc);
                if (descriptorSet)
                {
                    result = BindingSet::Create(debugName.empty() ? "[Cached BindingSet]" : debugName, descriptorSet);
                    entry = result;
                }
            }
            else
            {
                result = entry;
            }
            g_BindingCacheData.mutex.unlock();
        }

        return result;
    }

    void BindingCache::RemoveBindingSet(const BindingSetDesc &desc, nri::PipelineLayout *pipelineLayout, uint32_t setIndex)
    {
        const size_t hash = ComputeBindingCacheHash(desc, pipelineLayout, setIndex);

        g_BindingCacheData.mutex.lock();
        auto it = g_BindingCacheData.bindingSets.find(hash);
        if (it != g_BindingCacheData.bindingSets.end())
            g_BindingCacheData.bindingSets.erase(it);
        g_BindingCacheData.mutex.unlock();
    }
}
