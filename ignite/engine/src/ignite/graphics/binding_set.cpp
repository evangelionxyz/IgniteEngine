// Copyright (c) 2026 Evangelion Manuhutu
#include "ignite_pch.hpp"
#include "binding_set.hpp"
#include "renderer.hpp"

namespace ignite
{
    BindingSet::BindingSet(const std::string& name, nvrhi::BindingSetHandle handle)
        : m_Name(name), m_Handle(handle)
    {
        Renderer::IncrementBindingSetCount(m_Name);
    }

    BindingSet::~BindingSet()
    {
        Renderer::DecrementBindingSetCount(m_Name);
    }

    Ref<BindingSet> BindingSet::Create(const std::string& name, nvrhi::BindingSetHandle handle)
    {
        return CreateRef<BindingSet>(name, handle);
    }
}
