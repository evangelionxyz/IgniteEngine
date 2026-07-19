// Copyright (c) 2026 Evangelion Manuhutu
#pragma once
#ifndef IGN_BINDING_CACHE_HPP
#define IGN_BINDING_CACHE_HPP

#include <nvrhi/nvrhi.h>

namespace ignite
{
    class BindingCache
    {
    public:
        static void Init(nvrhi::IDevice *device);
        static void Shutdown();
        static void Clear();

        static nvrhi::BindingSetHandle GetCachedBindingSet(const nvrhi::BindingSetDesc &desc, nvrhi::IBindingLayout *layout);
        static nvrhi::BindingSetHandle GetOrCreateBindingSet(const nvrhi::BindingSetDesc &desc, nvrhi::IBindingLayout *layout);
		static void RemoveBindingSet(const nvrhi::BindingSetDesc &desc, nvrhi::IBindingLayout *layout);
    };
}


#endif