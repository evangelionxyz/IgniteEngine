// Copyright (c) 2026 Evangelion Manuhutu
#include "ignite_pch.hpp"
#include "binding_set.hpp"
#include "bindless_system.hpp"
#include "renderer.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/logger.hpp"

namespace ignite
{
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

        setDescs.reserve(processedLayouts.size());
        for (auto &layout : processedLayouts)
        {
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
        pipelineLayoutDesc.shaderStages = shaderStages;
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
        Update(m_Handle, desc);
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
        nri::Result res = nri.AllocateDescriptorSets(*pool, *pipelineLayout, setIndex, &descriptorSet, 1, 0);
        LOG_ASSERT(res == nri::Result::SUCCESS && descriptorSet, "[BindingSet] Failed to allocate descriptor set!");

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
}
