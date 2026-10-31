// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "edge_detection.hpp"
#include "texture.hpp"
#include "shader.hpp"
#include "renderer.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/logger.hpp"

namespace ignite
{

    EdgeDetection::EdgeDetection()
    {
        auto &nri = DeviceManager::GetInstance()->GetNRI();
        nri::Device *device = DeviceManager::GetInstance()->GetDevice();

        m_ConstantBuffer = ConstantBuffer::Create(sizeof(EdgeDetectionParameter), true, 16, "[Edge Detection] Constant buffer");

        nri::BufferDesc bufferDesc = {};
        bufferDesc.size = sizeof(uint32_t) * 100; // allocate enough memory for selection
        bufferDesc.structureStride = sizeof(uint32_t);
        bufferDesc.usage = nri::BufferUsageBits::SHADER_RESOURCE;
        nri.CreateCommittedBuffer(*device, nri::MemoryLocation::HOST_UPLOAD, 1.0f, bufferDesc, m_SelectedIDBuffer);
        nri.SetDebugName((nri::Object*)m_SelectedIDBuffer, "Selected Object IDs");

        nri::BufferViewDesc viewDesc = {};
        viewDesc.buffer = m_SelectedIDBuffer;
        viewDesc.type = nri::BufferView::STRUCTURED_BUFFER;
        viewDesc.offset = 0;
        viewDesc.size = bufferDesc.size;
        viewDesc.structureStride = sizeof(uint32_t);
        nri.CreateBufferView(viewDesc, m_SelectedIDDescriptor);

        // Create binding layout
        m_BindingLayout.SetVisibility(nri::StageBits::ALL);
        m_BindingLayout.bindings =
        {
            // constant buffer
            BindingLayoutItem::VolatileConstantBuffer(0),

            // Textures
            BindingLayoutItem::TextureSRV(0), // Scene texture
            BindingLayoutItem::TextureSRV(1), // Object ID texture
            BindingLayoutItem::TextureSRV(2), // Depth texture
            BindingLayoutItem::StructuredBufferSRV(3), // Selected Object IDs

            // Sampler
            BindingLayoutItem::Sampler(0),

            // Output texture (for Compute)
            BindingLayoutItem::TextureUAV(0)
        };

        m_PipelineLayout = m_BindingLayout.CreatePipelineLayout(nri::StageBits::COMPUTE_SHADER);

        m_Shader = Shader::Create("resources/shaders/sobel_edge_detection.compute.hlsl", UMBRA_SHADER_TYPE_COMPUTE, false);
    }

    EdgeDetection::~EdgeDetection()
    {
        auto &nri = DeviceManager::GetInstance()->GetNRI();
        if (m_Pipeline)
        {
            nri.DestroyPipeline(m_Pipeline);
            m_Pipeline = nullptr;
        }
        if (m_PipelineLayout)
        {
            nri.DestroyPipelineLayout(m_PipelineLayout);
            m_PipelineLayout = nullptr;
        }
        if (m_Sampler)
        {
            nri.DestroyDescriptor(m_Sampler);
            m_Sampler = nullptr;
        }
        if (m_SelectedIDDescriptor)
        {
            nri.DestroyDescriptor(m_SelectedIDDescriptor);
            m_SelectedIDDescriptor = nullptr;
        }
        if (m_SelectedIDBuffer)
        {
            nri.DestroyBuffer(m_SelectedIDBuffer);
            m_SelectedIDBuffer = nullptr;
        }
        if (m_SelectedIDMemory)
        {
            nri.FreeMemory(m_SelectedIDMemory);
            m_SelectedIDMemory = nullptr;
        }
    }

    void EdgeDetection::CreatePipeline()
    {
        auto &nri = DeviceManager::GetInstance()->GetNRI();
        nri::Device *device = DeviceManager::GetInstance()->GetDevice();

        if (m_Pipeline)
        {
            nri.DestroyPipeline(m_Pipeline);
            m_Pipeline = nullptr;
        }

        nri::ComputePipelineDesc computeDesc = {};
        computeDesc.pipelineLayout = m_PipelineLayout;
        computeDesc.shader = m_Shader->GetShaderDesc();
        nri.CreateComputePipeline(*device, computeDesc, m_Pipeline);
    }

    void EdgeDetection::UpdateBindingSet(const Ref<Texture> &sceneTexture, const Ref<Texture> &objectIDTexture, const Ref<Texture> &depth)
    {
        BindingSetDesc desc;
        desc.bindings =
        {
            BindingSetItem::ConstantBuffer(0, m_ConstantBuffer->GetDescriptor()),
            BindingSetItem::TextureSRV(0, sceneTexture->GetShaderResourceView()),
            BindingSetItem::TextureSRV(1, objectIDTexture->GetShaderResourceView()),
            BindingSetItem::TextureSRV(2, depth->GetShaderResourceView()),
            BindingSetItem::StructuredBufferSRV(3, m_SelectedIDDescriptor),
            BindingSetItem::Sampler(0, m_Sampler),
            BindingSetItem::TextureUAV(0, m_OutputTexture->GetStorageView(0)),
        };

        BindingSet::CreateOrUpdate(m_BindingSet, "[Edge Detection] BindingSet", m_PipelineLayout, 0, desc);
        LOG_ASSERT(m_BindingSet, "[Edge Detection] Failed to create binding set");
    }

    void EdgeDetection::SetSelectedIDs(const uint32_t *ids, uint32_t count)
    {
        if (!m_SelectedIDBuffer || !ids || count == 0)
            return;

        const uint32_t clampedCount = std::min(count, 100u);
        auto &nri = DeviceManager::GetInstance()->GetNRI();
        void *mapped = nri.MapBuffer(*m_SelectedIDBuffer, 0, sizeof(uint32_t) * clampedCount);
        if (mapped)
        {
            std::memcpy(mapped, ids, sizeof(uint32_t) * clampedCount);
            nri.UnmapBuffer(*m_SelectedIDBuffer);
        }
    }

    void EdgeDetection::ExecuteCompute(nri::CommandBuffer *commandBuffer, const EdgeDetectionParameter &params, uint32_t width, uint32_t height)
    {
        auto &nri = DeviceManager::GetInstance()->GetNRI();

        // Update constant buffer
        m_ConstantBuffer->SetData(&params, sizeof(params));

        // Transition output texture to UAV state before compute
        {
            nri::TextureBarrierDesc texBarrier = {};
            texBarrier.texture = m_OutputTexture->GetHandle();
            texBarrier.before = { m_OutputTexture->GetCurrentAccess(), m_OutputTexture->GetCurrentLayout(), m_OutputTexture->GetCurrentStages() };
            texBarrier.after = { nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::Layout::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER };
            texBarrier.layerNum = 1;
            texBarrier.mipNum = 1;

            nri::BarrierDesc barrierDesc = {};
            barrierDesc.textures = &texBarrier;
            barrierDesc.textureNum = 1;
            nri.CmdBarrier(*commandBuffer, barrierDesc);
            m_OutputTexture->SetCurrentState(texBarrier.after.layout, texBarrier.after.access, texBarrier.after.stages);
        }

        // Set compute pipeline
        nri.CmdSetPipelineLayout(*commandBuffer, nri::BindPoint::COMPUTE, *m_PipelineLayout);
        nri.CmdSetPipeline(*commandBuffer, *m_Pipeline);

        nri::SetDescriptorSetDesc setDesc = {};
        setDesc.setIndex = 0;
        setDesc.descriptorSet = m_BindingSet->GetHandle();
        setDesc.bindPoint = nri::BindPoint::COMPUTE;
        nri.CmdSetDescriptorSet(*commandBuffer, setDesc);

        // Dispatch compute shader
        uint32_t groupsX = (width + 7) / 8; // 8x8 threads groups
        uint32_t groupsY = (height + 7) / 8;
        nri.CmdDispatch(*commandBuffer, { groupsX, groupsY, 1 });

        // Transition edge detection output from UAV to ShaderResource for composite pass
        {
            nri::TextureBarrierDesc texBarrier = {};
            texBarrier.texture = m_OutputTexture->GetHandle();
            texBarrier.before = { m_OutputTexture->GetCurrentAccess(), m_OutputTexture->GetCurrentLayout(), m_OutputTexture->GetCurrentStages() };
            texBarrier.after = { nri::AccessBits::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE, nri::StageBits::FRAGMENT_SHADER | nri::StageBits::COMPUTE_SHADER };
            texBarrier.layerNum = 1;
            texBarrier.mipNum = 1;

            nri::BarrierDesc barrierDesc = {};
            barrierDesc.textures = &texBarrier;
            barrierDesc.textureNum = 1;
            nri.CmdBarrier(*commandBuffer, barrierDesc);
            m_OutputTexture->SetCurrentState(texBarrier.after.layout, texBarrier.after.access, texBarrier.after.stages);
        }
    }

    Ref<EdgeDetection> EdgeDetection::Create()
    {
        return CreateRef<EdgeDetection>();
    }

    void EdgeDetection::CreateOutputTexture(uint32_t width, uint32_t height)
    {
        auto &nri = DeviceManager::GetInstance()->GetNRI();
        nri::Device *device = DeviceManager::GetInstance()->GetDevice();

        TextureCreateInfo createInfo;
        createInfo.width = width;
        createInfo.height = height;
        createInfo.format = nri::Format::RGBA8_UNORM;
        createInfo.initialLayout = nri::Layout::SHADER_RESOURCE;
        createInfo.keepInitialState = true;
        createInfo.isUAV = true;

        m_OutputTexture = Texture::Create(createInfo, "[Edge Detection] Output Texture");

        if (!m_Sampler)
        {
            nri::SamplerDesc samplerDesc = {};
            samplerDesc.filters = { nri::Filter::LINEAR, nri::Filter::LINEAR, nri::Filter::LINEAR };
            samplerDesc.addressModes = { nri::AddressMode::REPEAT, nri::AddressMode::REPEAT, nri::AddressMode::REPEAT };
            samplerDesc.mipMax = 16.0f;
            nri.CreateSampler(*device, samplerDesc, m_Sampler);
            LOG_ASSERT(m_Sampler, "Failed to create sampler");
        }
    }

}
