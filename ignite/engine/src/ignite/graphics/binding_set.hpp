// Copyright (c) 2026 Evangelion Manuhutu
#pragma once
#ifndef IGN_BINDING_SET_HPP
#define IGN_BINDING_SET_HPP

#include "ignite/core/types.hpp"
#include "ignite/core/hashing.hpp"

#include <NRI.h>
#include <string>
#include <vector>

namespace ignite
{
    struct BindingLayoutItem
    {
        uint32_t slot = 0;
        uint32_t size = 1; // descriptor count (or byte size for push constants)
        nri::DescriptorType type = nri::DescriptorType::MAX_NUM;
        bool isPushConstants = false;
        nri::DescriptorRangeBits flags = nri::DescriptorRangeBits::NONE;

        static BindingLayoutItem ConstantBuffer(uint32_t slot)
        {
            return { slot, 1, nri::DescriptorType::CONSTANT_BUFFER, false, nri::DescriptorRangeBits::NONE };
        }

        static BindingLayoutItem VolatileConstantBuffer(uint32_t slot)
        {
            return { slot, 1, nri::DescriptorType::CONSTANT_BUFFER, false, nri::DescriptorRangeBits::NONE };
        }

        static BindingLayoutItem TextureSRV(uint32_t slot, uint32_t count = 1)
        {
            return { slot, count, nri::DescriptorType::TEXTURE, false, nri::DescriptorRangeBits::NONE };
        }

        static BindingLayoutItem TextureUAV(uint32_t slot, uint32_t count = 1)
        {
            return { slot, count, nri::DescriptorType::STORAGE_TEXTURE, false, nri::DescriptorRangeBits::NONE };
        }

        static BindingLayoutItem StructuredBufferSRV(uint32_t slot, uint32_t count = 1)
        {
            return { slot, count, nri::DescriptorType::STRUCTURED_BUFFER, false, nri::DescriptorRangeBits::NONE };
        }

        static BindingLayoutItem StructuredBufferUAV(uint32_t slot, uint32_t count = 1)
        {
            return { slot, count, nri::DescriptorType::STORAGE_STRUCTURED_BUFFER, false, nri::DescriptorRangeBits::NONE };
        }

        static BindingLayoutItem Sampler(uint32_t slot, uint32_t count = 1)
        {
            return { slot, count, nri::DescriptorType::SAMPLER, false, nri::DescriptorRangeBits::NONE };
        }

        static BindingLayoutItem Texture_SRV(uint32_t slot, uint32_t count = 1) { return TextureSRV(slot, count); }
        static BindingLayoutItem Texture_UAV(uint32_t slot, uint32_t count = 1) { return TextureUAV(slot, count); }
        static BindingLayoutItem StructuredBuffer_SRV(uint32_t slot, uint32_t count = 1) { return StructuredBufferSRV(slot, count); }
        static BindingLayoutItem StructuredBuffer_UAV(uint32_t slot, uint32_t count = 1) { return StructuredBufferUAV(slot, count); }

        static BindingLayoutItem PushConstants(uint32_t slot, uint32_t byteSize)
        {
            return { slot, byteSize, nri::DescriptorType::MAX_NUM, true, nri::DescriptorRangeBits::NONE };
        }

        static BindingLayoutItem Mutable(uint32_t slot, uint32_t count,
            nri::DescriptorRangeBits flags = nri::DescriptorRangeBits::ARRAY
                | nri::DescriptorRangeBits::PARTIALLY_BOUND
                | nri::DescriptorRangeBits::ALLOW_UPDATE_AFTER_SET)
        {
            return { slot, count, nri::DescriptorType::MUTABLE, false, flags };
        }
    };

    struct BindingLayoutDesc
    {
        uint32_t registerSpace = 0;
        nri::StageBits visibility = nri::StageBits::ALL;
        std::vector<BindingLayoutItem> bindings;
        std::vector<nri::DescriptorRangeDesc> ranges;
        std::vector<nri::RootConstantDesc> rootConstants;
        bool isBindless = false;

        BindingLayoutDesc &SetRegisterSpace(uint32_t space)
        {
            registerSpace = space;
            return *this;
        }
        BindingLayoutDesc &SetRegisterSpaceIsDescriptorSet(bool) { return *this; }

        BindingLayoutDesc &SetVisibility(nri::StageBits stages)
        {
            visibility = stages;
            for (auto &r : ranges)
                r.shaderStages = stages;
            for (auto &rc : rootConstants)
                rc.shaderStages = stages;
            return *this;
        }

        BindingLayoutDesc &AddItem(const BindingLayoutItem &item)
        {
            if (item.isPushConstants)
            {
                nri::RootConstantDesc rc = {};
                rc.registerIndex = item.slot;
                rc.size = item.size;
                rc.shaderStages = visibility;
                rootConstants.push_back(rc);
            }
            else
            {
                nri::DescriptorRangeDesc range = {};
                range.baseRegisterIndex = item.slot;
                range.descriptorNum = item.size;
                range.descriptorType = item.type;
                range.shaderStages = visibility;
                range.flags = item.flags;
                ranges.push_back(range);
                if (item.type == nri::DescriptorType::MUTABLE)
                    isBindless = true;
            }
            return *this;
        }

        nri::PipelineLayout *CreatePipelineLayout(nri::StageBits shaderStages = nri::StageBits::ALL, nri::Device *device = nullptr) const;
        static nri::PipelineLayout *CreatePipelineLayout(const std::vector<BindingLayoutDesc> &layouts, nri::StageBits shaderStages = nri::StageBits::ALL, nri::Device *device = nullptr);
    };

    struct BindingSetItem
    {
        uint32_t rangeIndex = 0;
        uint32_t baseDescriptor = 0;
        nri::Descriptor *descriptor = nullptr;
        bool isPushConstants = false;

        bool operator==(const BindingSetItem &other) const noexcept
        {
            return rangeIndex == other.rangeIndex
                && baseDescriptor == other.baseDescriptor
                && descriptor == other.descriptor
                && isPushConstants == other.isPushConstants;
        }

        static BindingSetItem ConstantBuffer(uint32_t slotOrRangeIndex, nri::Descriptor *descriptor, uint32_t baseDescriptor = 0)
        {
            return { slotOrRangeIndex, baseDescriptor, descriptor, false };
        }

        static BindingSetItem TextureSRV(uint32_t slotOrRangeIndex, nri::Descriptor *descriptor, uint32_t baseDescriptor = 0)
        {
            return { slotOrRangeIndex, baseDescriptor, descriptor, false };
        }

        static BindingSetItem TextureUAV(uint32_t slotOrRangeIndex, nri::Descriptor *descriptor, uint32_t baseDescriptor = 0)
        {
            return { slotOrRangeIndex, baseDescriptor, descriptor, false };
        }

        static BindingSetItem StructuredBufferSRV(uint32_t slotOrRangeIndex, nri::Descriptor *descriptor, uint32_t baseDescriptor = 0)
        {
            return { slotOrRangeIndex, baseDescriptor, descriptor, false };
        }

        static BindingSetItem StructuredBufferUAV(uint32_t slotOrRangeIndex, nri::Descriptor *descriptor, uint32_t baseDescriptor = 0)
        {
            return { slotOrRangeIndex, baseDescriptor, descriptor, false };
        }

        static BindingSetItem Sampler(uint32_t slotOrRangeIndex, nri::Descriptor *descriptor, uint32_t baseDescriptor = 0)
        {
            return { slotOrRangeIndex, baseDescriptor, descriptor, false };
        }

        static BindingSetItem Texture_SRV(uint32_t slotOrRangeIndex, nri::Descriptor *descriptor, uint32_t baseDescriptor = 0) { return TextureSRV(slotOrRangeIndex, descriptor, baseDescriptor); }
        static BindingSetItem Texture_UAV(uint32_t slotOrRangeIndex, nri::Descriptor *descriptor, uint32_t baseDescriptor = 0) { return TextureUAV(slotOrRangeIndex, descriptor, baseDescriptor); }
        static BindingSetItem StructuredBuffer_SRV(uint32_t slotOrRangeIndex, nri::Descriptor *descriptor, uint32_t baseDescriptor = 0) { return StructuredBufferSRV(slotOrRangeIndex, descriptor, baseDescriptor); }
        static BindingSetItem StructuredBuffer_UAV(uint32_t slotOrRangeIndex, nri::Descriptor *descriptor, uint32_t baseDescriptor = 0) { return StructuredBufferUAV(slotOrRangeIndex, descriptor, baseDescriptor); }

        static BindingSetItem PushConstants(uint32_t slot, uint32_t byteSize)
        {
            (void)slot;
            (void)byteSize;
            return { 0, 0, nullptr, true };
        }
    };

    struct BindingSetDesc
    {
        std::vector<BindingSetItem> bindings;

        BindingSetDesc &AddItem(const BindingSetItem &item)
        {
            if (item.isPushConstants)
                return *this;

            BindingSetItem indexedItem = item;
            indexedItem.rangeIndex = static_cast<uint32_t>(bindings.size());
            bindings.push_back(indexedItem);
            return *this;
        }
        BindingSetDesc &addItem(const BindingSetItem &item) { return AddItem(item); }

        bool operator==(const BindingSetDesc &other) const noexcept
        {
            return bindings == other.bindings;
        }
    };

    class BindingSet
    {
    public:
        BindingSet(const std::string &name, nri::DescriptorSet *handle);
        BindingSet(const std::string &name, nri::PipelineLayout *pipelineLayout, uint32_t setIndex, const BindingSetDesc &desc);
        ~BindingSet();

        nri::DescriptorSet *Get() const { return m_Handle; }
        nri::DescriptorSet *GetHandle() const { return m_Handle; }
        const std::string &GetName() const { return m_Name; }
        const BindingSetDesc &GetDesc() const { return m_Desc; }
        nri::PipelineLayout *GetPipelineLayout() const { return m_PipelineLayout; }
        uint32_t GetSetIndex() const { return m_SetIndex; }

        operator nri::DescriptorSet *() const { return m_Handle; }

        void Update(const BindingSetDesc &desc);

        static Ref<BindingSet> Create(const std::string &name, nri::DescriptorSet *handle);
        static Ref<BindingSet> Create(const std::string &name, nri::PipelineLayout *pipelineLayout, uint32_t setIndex, const BindingSetDesc &desc);
        static Ref<BindingSet> Create(const BindingSetDesc &desc, nri::PipelineLayout *pipelineLayout, uint32_t setIndex = 0, const std::string &name = "[BindingSet]") { return Create(name, pipelineLayout, setIndex, desc); }
        static Ref<BindingSet> CreateOrUpdate(Ref<BindingSet> &bindingSet, const std::string &name, nri::PipelineLayout *pipelineLayout, uint32_t setIndex, const BindingSetDesc &desc);
        static nri::DescriptorSet *AllocateAndUpdate(nri::PipelineLayout *pipelineLayout, uint32_t setIndex, const BindingSetDesc &desc);
        static nri::DescriptorSet *AllocateAndUpdate(const BindingSetDesc &desc, nri::PipelineLayout *pipelineLayout, uint32_t setIndex = 0);
        static void Update(nri::DescriptorSet *descriptorSet, const BindingSetDesc &desc);

    private:
        std::string m_Name;
        nri::DescriptorSet *m_Handle = nullptr;
        nri::PipelineLayout *m_PipelineLayout = nullptr;
        uint32_t m_SetIndex = 0;
        BindingSetDesc m_Desc;
    };
}

#endif
