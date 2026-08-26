// Copyright (c) 2026 Evangelion Manuhutu
#pragma once
#ifndef IGN_BINDING_SET_HPP
#define IGN_BINDING_SET_HPP

#include "ignite/core/types.hpp"

#include <nvrhi/nvrhi.h>
#include <string>

namespace ignite
{
    class BindingSet
    {
    public:
        BindingSet(const std::string& name, nvrhi::BindingSetHandle handle);
        ~BindingSet();

        nvrhi::IBindingSet* Get() const { return m_Handle.Get(); }
        nvrhi::BindingSetHandle GetHandle() const { return m_Handle; }
        const std::string& GetName() const { return m_Name; }

        operator nvrhi::BindingSetHandle() const { return m_Handle; }
        operator nvrhi::IBindingSet*() const { return m_Handle.Get(); }
        nvrhi::IBindingSet* operator->() const { return m_Handle.Get(); }

        static Ref<BindingSet> Create(const std::string& name, nvrhi::BindingSetHandle handle);

    private:
        std::string m_Name;
        nvrhi::BindingSetHandle m_Handle;
    };
}

#endif
