// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "ignite/core/profiler/profiler.hpp"

#include "outline_jfa.hpp"
#include "texture.hpp"
#include "shader.hpp"
#include "renderer.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/logger.hpp"

namespace ignite
{
    struct JFASeedCB
    {
        uint32_t selectedCount = 0;
        uint32_t _padding[3] = { 0, 0, 0 };
    };

    struct JFAFloodCB
    {
        int32_t stepSize = 1;
        int32_t _padding[3] = { 0, 0, 0 };
    };

    struct JFAOutlineCB
    {
        glm::vec4 outlineColor = glm::vec4(1.0f, 0.5f, 0.1f, 1.0f);
        float outlineWidth = 2.5f;
        float _padding[3] = { 0.0f, 0.0f, 0.0f };
    };

    OutlineJFA::OutlineJFA()
    {
        auto &nri = DeviceManager::GetInstance()->GetNRI();
        nri::Device *device = DeviceManager::GetInstance()->GetDevice();

        m_SeedCB = ConstantBuffer::Create(sizeof(JFASeedCB), true, 64, "[JFA Seed] Constant buffer");
        for (size_t i = 0; i < kMaxFloodSteps; ++i)
        {
            m_FloodCBs[i] = ConstantBuffer::Create(sizeof(JFAFloodCB), true, 64, "[JFA Flood] Constant buffer");
            JFAFloodCB floodData;
            floodData.stepSize = static_cast<int32_t>(1u << i);
            m_FloodCBs[i]->SetData(Buffer(&floodData, sizeof(floodData)));
        }
        m_OutlineCB = ConstantBuffer::Create(sizeof(JFAOutlineCB), true, 64, "[JFA Outline] Constant buffer");

        nri::BufferDesc bufferDesc = {};
        bufferDesc.size = sizeof(uint32_t) * 100;
        bufferDesc.structureStride = sizeof(uint32_t);
        bufferDesc.usage = nri::BufferUsageBits::SHADER_RESOURCE;
        nri.CreateCommittedBuffer(*device, nri::MemoryLocation::HOST_UPLOAD, 1.0f, bufferDesc, m_SelectedIDBuffer);
        nri.SetDebugName((nri::Object*)m_SelectedIDBuffer, "JFA Selected Object IDs");

        nri::BufferViewDesc viewDesc = {};
        viewDesc.buffer = m_SelectedIDBuffer;
        viewDesc.type = nri::BufferView::STRUCTURED_BUFFER;
        viewDesc.offset = 0;
        viewDesc.size = bufferDesc.size;
        viewDesc.structureStride = sizeof(uint32_t);
        nri.CreateBufferView(viewDesc, m_SelectedIDDescriptor);

        // Seed Binding Layout
        {
            m_SeedBindingLayout.SetVisibility(nri::StageBits::ALL);
            m_SeedBindingLayout.bindings =
            {
                BindingLayoutItem::VolatileConstantBuffer(0),
                BindingLayoutItem::TextureSRV(0),             // Object ID texture
                BindingLayoutItem::StructuredBuffer_SRV(1),    // Selected IDs
                BindingLayoutItem::Texture_UAV(0)              // Ping texture output
            };
            m_SeedPipelineLayout = m_SeedBindingLayout.CreatePipelineLayout(nri::StageBits::COMPUTE_SHADER);
        }

        // Flood Binding Layout
        {
            m_FloodBindingLayout.SetVisibility(nri::StageBits::ALL);
            m_FloodBindingLayout.bindings =
            {
                BindingLayoutItem::VolatileConstantBuffer(0),
                BindingLayoutItem::TextureSRV(0),             // Input JFA texture
                BindingLayoutItem::Texture_UAV(0)              // Output JFA texture
            };
            m_FloodPipelineLayout = m_FloodBindingLayout.CreatePipelineLayout(nri::StageBits::COMPUTE_SHADER);
        }

        // Outline Binding Layout
        {
            m_OutlineBindingLayout.SetVisibility(nri::StageBits::ALL);
            m_OutlineBindingLayout.bindings =
            {
                BindingLayoutItem::VolatileConstantBuffer(0),
                BindingLayoutItem::TextureSRV(0),             // Final JFA texture
                BindingLayoutItem::Texture_UAV(0)              // Output RGBA8 texture
            };
            m_OutlinePipelineLayout = m_OutlineBindingLayout.CreatePipelineLayout(nri::StageBits::COMPUTE_SHADER);
        }

        m_SeedShader = Shader::Create("resources/shaders/jfa_seed.compute.hlsl", UMBRA_SHADER_TYPE_COMPUTE, false);
        m_FloodShader = Shader::Create("resources/shaders/jfa_flood.compute.hlsl", UMBRA_SHADER_TYPE_COMPUTE, false);
        m_OutlineShader = Shader::Create("resources/shaders/jfa_outline.compute.hlsl", UMBRA_SHADER_TYPE_COMPUTE, false);
    }

    OutlineJFA::~OutlineJFA()
    {
        auto &nri = DeviceManager::GetInstance()->GetNRI();
        if (m_SeedPipeline) nri.DestroyPipeline(m_SeedPipeline);
        if (m_FloodPipeline) nri.DestroyPipeline(m_FloodPipeline);
        if (m_OutlinePipeline) nri.DestroyPipeline(m_OutlinePipeline);
        if (m_SeedPipelineLayout) nri.DestroyPipelineLayout(m_SeedPipelineLayout);
        if (m_FloodPipelineLayout) nri.DestroyPipelineLayout(m_FloodPipelineLayout);
        if (m_OutlinePipelineLayout) nri.DestroyPipelineLayout(m_OutlinePipelineLayout);
        if (m_SelectedIDDescriptor) nri.DestroyDescriptor(m_SelectedIDDescriptor);
        if (m_SelectedIDBuffer) nri.DestroyBuffer(m_SelectedIDBuffer);
        if (m_SelectedIDMemory) nri.FreeMemory(m_SelectedIDMemory);
    }

    void OutlineJFA::CreatePipeline()
    {
        auto &nri = DeviceManager::GetInstance()->GetNRI();
        nri::Device *device = DeviceManager::GetInstance()->GetDevice();

        if (m_SeedPipeline) { nri.DestroyPipeline(m_SeedPipeline); m_SeedPipeline = nullptr; }
        if (m_FloodPipeline) { nri.DestroyPipeline(m_FloodPipeline); m_FloodPipeline = nullptr; }
        if (m_OutlinePipeline) { nri.DestroyPipeline(m_OutlinePipeline); m_OutlinePipeline = nullptr; }

        {
            nri::ComputePipelineDesc computeDesc = {};
            computeDesc.pipelineLayout = m_SeedPipelineLayout;
            computeDesc.shader = m_SeedShader->GetShaderDesc();
            nri.CreateComputePipeline(*device, computeDesc, m_SeedPipeline);
        }

        {
            nri::ComputePipelineDesc computeDesc = {};
            computeDesc.pipelineLayout = m_FloodPipelineLayout;
            computeDesc.shader = m_FloodShader->GetShaderDesc();
            nri.CreateComputePipeline(*device, computeDesc, m_FloodPipeline);
        }

        {
            nri::ComputePipelineDesc computeDesc = {};
            computeDesc.pipelineLayout = m_OutlinePipelineLayout;
            computeDesc.shader = m_OutlineShader->GetShaderDesc();
            nri.CreateComputePipeline(*device, computeDesc, m_OutlinePipeline);
        }
    }

    void OutlineJFA::CreateOutputTexture(uint32_t width, uint32_t height)
    {
        TextureCreateInfo createInfoRG;
        createInfoRG.width = width;
        createInfoRG.height = height;
        createInfoRG.format = nri::Format::RG16_SINT;
        createInfoRG.initialLayout = nri::Layout::SHADER_RESOURCE;
        createInfoRG.keepInitialState = true;
        createInfoRG.isUAV = true;

        m_JFAPing = Texture::Create(createInfoRG, "[JFA] Ping Texture");
        m_JFAPong = Texture::Create(createInfoRG, "[JFA] Pong Texture");

        TextureCreateInfo createInfoRGBA;
        createInfoRGBA.width = width;
        createInfoRGBA.height = height;
        createInfoRGBA.format = nri::Format::RGBA8_UNORM;
        createInfoRGBA.initialLayout = nri::Layout::SHADER_RESOURCE;
        createInfoRGBA.keepInitialState = true;
        createInfoRGBA.isUAV = true;

        m_OutputTexture = Texture::Create(createInfoRGBA, "[JFA] Output Texture");

        // Flood Binding Sets (Ping->Pong and Pong->Ping per step)
        for (size_t i = 0; i < kMaxFloodSteps; ++i)
        {
            BindingSetDesc descPingToPong;
            descPingToPong.bindings =
            {
                BindingSetItem::ConstantBuffer(0, m_FloodCBs[i]->GetDescriptor()),
                BindingSetItem::TextureSRV(0, m_JFAPing->GetShaderResourceView()),
                BindingSetItem::Texture_UAV(0, m_JFAPong->GetStorageView(0)),
            };
            m_FloodBindingSetPingToPong[i] = BindingSet::Create("[JFA Flood Ping->Pong]", m_FloodPipelineLayout, 0, descPingToPong);

            BindingSetDesc descPongToPing;
            descPongToPing.bindings =
            {
                BindingSetItem::ConstantBuffer(0, m_FloodCBs[i]->GetDescriptor()),
                BindingSetItem::TextureSRV(0, m_JFAPong->GetShaderResourceView()),
                BindingSetItem::Texture_UAV(0, m_JFAPing->GetStorageView(0)),
            };
            m_FloodBindingSetPongToPing[i] = BindingSet::Create("[JFA Flood Pong->Ping]", m_FloodPipelineLayout, 0, descPongToPing);
        }

        // Outline Binding Sets (Ping input vs Pong input)
        {
            BindingSetDesc descPing;
            descPing.bindings =
            {
                BindingSetItem::ConstantBuffer(0, m_OutlineCB->GetDescriptor()),
                BindingSetItem::TextureSRV(0, m_JFAPing->GetShaderResourceView()),
                BindingSetItem::Texture_UAV(0, m_OutputTexture->GetStorageView(0)),
            };
            m_OutlineBindingSetPing = BindingSet::Create("[JFA Outline Ping]", m_OutlinePipelineLayout, 0, descPing);

            BindingSetDesc descPong;
            descPong.bindings =
            {
                BindingSetItem::ConstantBuffer(0, m_OutlineCB->GetDescriptor()),
                BindingSetItem::TextureSRV(0, m_JFAPong->GetShaderResourceView()),
                BindingSetItem::Texture_UAV(0, m_OutputTexture->GetStorageView(0)),
            };
            m_OutlineBindingSetPong = BindingSet::Create("[JFA Outline Pong]", m_OutlinePipelineLayout, 0, descPong);
        }

        // Invalidate cached seed binding set so it rebinds to the new Ping texture
        m_CurrentObjectIDTexture = nullptr;
        m_SeedBindingSet = nullptr;
    }

    void OutlineJFA::UpdateBindingSet(const Ref<Texture> &objectIDTexture)
    {
        if (!objectIDTexture || !m_JFAPing)
            return;

        // Skip recreating descriptor sets if object ID texture and ping texture haven't changed
        if (m_CurrentObjectIDTexture == objectIDTexture->GetHandle() && m_SeedBindingSet != nullptr)
            return;

        m_CurrentObjectIDTexture = objectIDTexture->GetHandle();

        // Seed Binding Set
        {
            BindingSetDesc desc;
            desc.bindings =
            {
                BindingSetItem::ConstantBuffer(0, m_SeedCB->GetDescriptor()),
                BindingSetItem::TextureSRV(0, objectIDTexture->GetShaderResourceView()),
                BindingSetItem::StructuredBuffer_SRV(1, m_SelectedIDDescriptor),
                BindingSetItem::Texture_UAV(0, m_JFAPing->GetStorageView(0)),
            };
            m_SeedBindingSet = BindingSet::Create("[JFA Seed]", m_SeedPipelineLayout, 0, desc);
            LOG_ASSERT(m_SeedBindingSet, "[JFA] Failed to create seed binding set");
        }
    }

    void OutlineJFA::SetSelectedIDs(const uint32_t *ids, uint32_t count)
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

    void OutlineJFA::ExecuteCompute(nri::CommandBuffer *commandBuffer, const OutlineJFAParameter &params, uint32_t width, uint32_t height)
    {
        IGN_PROFILE_FUNCTION();

        auto &nri = DeviceManager::GetInstance()->GetNRI();

        auto transitionTexture = [&](const Ref<Texture> &tex, nri::Layout layout, nri::AccessBits access, nri::StageBits stages)
        {
            if (!tex || !tex->GetHandle())
                return;
            if (tex->GetCurrentLayout() == layout && tex->GetCurrentAccess() == access)
                return;

            nri::TextureBarrierDesc texBarrier = {};
            texBarrier.texture = tex->GetHandle();
            texBarrier.before = { tex->GetCurrentAccess(), tex->GetCurrentLayout(), tex->GetCurrentStages() };
            texBarrier.after = { access, layout, stages };
            texBarrier.layerNum = 1;
            texBarrier.mipNum = 1;

            nri::BarrierDesc barrierDesc = {};
            barrierDesc.textures = &texBarrier;
            barrierDesc.textureNum = 1;
            nri.CmdBarrier(*commandBuffer, barrierDesc);
            tex->SetCurrentState(layout, access, stages);
        };

        uint32_t groupsX = (width + 7) / 8;
        uint32_t groupsY = (height + 7) / 8;

        // 1. Seed Pass
        JFASeedCB seedCB;
        seedCB.selectedCount = params.selectedCount;
        m_SeedCB->SetData(Buffer(&seedCB, sizeof(seedCB)));

        transitionTexture(m_JFAPing, nri::Layout::SHADER_RESOURCE_STORAGE, nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER);

        nri.CmdSetPipelineLayout(*commandBuffer, nri::BindPoint::COMPUTE, *m_SeedPipelineLayout);
        nri.CmdSetPipeline(*commandBuffer, *m_SeedPipeline);
        {
            nri::SetDescriptorSetDesc setDesc = {};
            setDesc.setIndex = 0;
            setDesc.descriptorSet = m_SeedBindingSet->GetHandle();
            setDesc.bindPoint = nri::BindPoint::COMPUTE;
            nri.CmdSetDescriptorSet(*commandBuffer, setDesc);
        }
        nri.CmdDispatch(*commandBuffer, { groupsX, groupsY, 1 });

        // 2. Flood Passes
        // For outline rendering, we only need to propagate seeds up to the outline reach (~2x outline width for diagonal coverage).
        // Capping max step size to 64 avoids executing 7+ redundant full-screen dispatches and barrier commits per frame.
        int32_t targetStep = static_cast<int32_t>(std::ceil(params.outlineWidth * 2.0f));
        targetStep = std::max(1, std::min(targetStep, 64));
        int32_t maxStep = 1;
        size_t stepIndex = 0;
        while (maxStep < targetStep && stepIndex + 1 < kMaxFloodSteps)
        {
            maxStep *= 2;
            ++stepIndex;
        }

        nri.CmdSetPipelineLayout(*commandBuffer, nri::BindPoint::COMPUTE, *m_FloodPipelineLayout);
        nri.CmdSetPipeline(*commandBuffer, *m_FloodPipeline);

        int32_t stepSize = maxStep;
        bool pingIsSource = true;
        while (stepSize >= 1)
        {
            if (pingIsSource)
            {
                transitionTexture(m_JFAPing, nri::Layout::SHADER_RESOURCE, nri::AccessBits::SHADER_RESOURCE, nri::StageBits::COMPUTE_SHADER);
                transitionTexture(m_JFAPong, nri::Layout::SHADER_RESOURCE_STORAGE, nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER);

                nri::SetDescriptorSetDesc setDesc = {};
                setDesc.setIndex = 0;
                setDesc.descriptorSet = m_FloodBindingSetPingToPong[stepIndex]->GetHandle();
                setDesc.bindPoint = nri::BindPoint::COMPUTE;
                nri.CmdSetDescriptorSet(*commandBuffer, setDesc);
                nri.CmdDispatch(*commandBuffer, { groupsX, groupsY, 1 });
            }
            else
            {
                transitionTexture(m_JFAPong, nri::Layout::SHADER_RESOURCE, nri::AccessBits::SHADER_RESOURCE, nri::StageBits::COMPUTE_SHADER);
                transitionTexture(m_JFAPing, nri::Layout::SHADER_RESOURCE_STORAGE, nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER);

                nri::SetDescriptorSetDesc setDesc = {};
                setDesc.setIndex = 0;
                setDesc.descriptorSet = m_FloodBindingSetPongToPing[stepIndex]->GetHandle();
                setDesc.bindPoint = nri::BindPoint::COMPUTE;
                nri.CmdSetDescriptorSet(*commandBuffer, setDesc);
                nri.CmdDispatch(*commandBuffer, { groupsX, groupsY, 1 });
            }

            pingIsSource = !pingIsSource;
            stepSize /= 2;
            if (stepIndex > 0)
                --stepIndex;
        }

        // 3. Outline Pass
        JFAOutlineCB outlineCB;
        outlineCB.outlineColor = params.outlineColor;
        outlineCB.outlineWidth = params.outlineWidth;
        m_OutlineCB->SetData(Buffer(&outlineCB, sizeof(outlineCB)));

        transitionTexture(m_OutputTexture, nri::Layout::SHADER_RESOURCE_STORAGE, nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER);

        nri.CmdSetPipelineLayout(*commandBuffer, nri::BindPoint::COMPUTE, *m_OutlinePipelineLayout);
        nri.CmdSetPipeline(*commandBuffer, *m_OutlinePipeline);

        nri::SetDescriptorSetDesc outlineSetDesc = {};
        outlineSetDesc.setIndex = 0;
        outlineSetDesc.bindPoint = nri::BindPoint::COMPUTE;

        if (pingIsSource)
        {
            // Last flood wrote to Ping, so Ping is source
            transitionTexture(m_JFAPing, nri::Layout::SHADER_RESOURCE, nri::AccessBits::SHADER_RESOURCE, nri::StageBits::COMPUTE_SHADER);
            outlineSetDesc.descriptorSet = m_OutlineBindingSetPing->GetHandle();
        }
        else
        {
            // Last flood wrote to Pong, so Pong is source
            transitionTexture(m_JFAPong, nri::Layout::SHADER_RESOURCE, nri::AccessBits::SHADER_RESOURCE, nri::StageBits::COMPUTE_SHADER);
            outlineSetDesc.descriptorSet = m_OutlineBindingSetPong->GetHandle();
        }

        nri.CmdSetDescriptorSet(*commandBuffer, outlineSetDesc);
        nri.CmdDispatch(*commandBuffer, { groupsX, groupsY, 1 });

        // Transition final outline texture to ShaderResource for composite pass
        transitionTexture(m_OutputTexture, nri::Layout::SHADER_RESOURCE, nri::AccessBits::SHADER_RESOURCE, nri::StageBits::FRAGMENT_SHADER | nri::StageBits::COMPUTE_SHADER);
    }

    Ref<OutlineJFA> OutlineJFA::Create()
    {
        return CreateRef<OutlineJFA>();
    }
}
