// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "binding_cache.hpp"
#include "ignite/core/logger.hpp"

#include <unordered_map>
#include <shared_mutex>

namespace ignite
{

    struct BindingCacheData
    {
	    nvrhi::IDevice *device;
	    std::unordered_map<size_t, nvrhi::BindingSetHandle> bindingSets;
	    std::shared_mutex mutex;
    };

	BindingCacheData g_BindingCacheData;

    void BindingCache::Init(nvrhi::IDevice *device)
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

	nvrhi::BindingSetHandle BindingCache::GetCachedBindingSet(const nvrhi::BindingSetDesc &desc, nvrhi::IBindingLayout *layout)
    {
        size_t hash = 0;
        nvrhi::hash_combine(hash, desc);
        nvrhi::hash_combine(hash, layout);

        g_BindingCacheData.mutex.lock_shared();

        nvrhi::BindingSetHandle result = nullptr;
        auto it = g_BindingCacheData.bindingSets.find(hash);
        if (it != g_BindingCacheData.bindingSets.end())
            result = it->second;

        g_BindingCacheData.mutex.unlock_shared();

        if (result)
        {
            LOG_ASSERT(result->getDesc(), "[Binding Cache] Cached binding set has no description!");
            LOG_ASSERT(*result->getDesc() == desc, "[Binding Cache] Cached binding set description does not match the requested description!");
        }

        return result;
    }

    nvrhi::BindingSetHandle BindingCache::GetOrCreateBindingSet(const nvrhi::BindingSetDesc &desc, nvrhi::IBindingLayout *layout)
    {
        size_t hash = 0;
        nvrhi::hash_combine(hash, desc);
        nvrhi::hash_combine(hash, layout);

        g_BindingCacheData.mutex.lock_shared();

        nvrhi::BindingSetHandle result;
        auto it = g_BindingCacheData.bindingSets.find(hash);
        if (it != g_BindingCacheData.bindingSets.end())
            result = it->second;

        g_BindingCacheData.mutex.unlock_shared();

        if (!result)
        {
            g_BindingCacheData.mutex.lock();

            nvrhi::BindingSetHandle &entry = g_BindingCacheData.bindingSets[hash];
            if (!entry)
            {
                result = g_BindingCacheData.device->createBindingSet(desc, layout);
                entry = result;
            }
            else
            {
                result = entry;
            }

            g_BindingCacheData.mutex.unlock();
        }

        if (result)
        {
            LOG_ASSERT(result->getDesc(), "[Binding Cache] Cached binding set has no description!");
            LOG_ASSERT(*result->getDesc() == desc, "[Binding Cache] Cached binding set description does not match the requested description!");
        }

        return result;
    }

	void BindingCache::RemoveBindingSet(const nvrhi::BindingSetDesc &desc, nvrhi::IBindingLayout *layout)
	{
		size_t hash = 0;
		nvrhi::hash_combine(hash, desc);
		nvrhi::hash_combine(hash, layout);

		g_BindingCacheData.mutex.lock_shared();

		nvrhi::BindingSetHandle result;
		auto it = g_BindingCacheData.bindingSets.find(hash);
		if (it != g_BindingCacheData.bindingSets.end())
			it = g_BindingCacheData.bindingSets.erase(it);

		g_BindingCacheData.mutex.unlock_shared();
	}

}
