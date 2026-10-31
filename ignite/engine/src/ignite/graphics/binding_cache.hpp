// Copyright (c) 2026 Evangelion Manuhutu
#pragma once
#ifndef IGN_BINDING_CACHE_HPP
#define IGN_BINDING_CACHE_HPP

#include "binding_set.hpp"
#include <NRI.h>

namespace ignite
{
    class BindingCache
    {
    public:
        static void Init(nri::Device *device);
        static void Shutdown();
        static void Clear();

        static Ref<BindingSet> GetCachedBindingSet(const BindingSetDesc &desc, nri::PipelineLayout *pipelineLayout, uint32_t setIndex = 0);
        static Ref<BindingSet> GetOrCreateBindingSet(const BindingSetDesc &desc, nri::PipelineLayout *pipelineLayout, uint32_t setIndex = 0, const std::string &debugName = "");
        static void RemoveBindingSet(const BindingSetDesc &desc, nri::PipelineLayout *pipelineLayout, uint32_t setIndex = 0);
    };
}

#endif
