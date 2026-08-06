// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

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
        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();

        m_SeedCB = ConstantBuffer::Create(sizeof(JFASeedCB), true, 64, "[JFA Seed] Constant buffer");
        m_FloodCB = ConstantBuffer::Create(sizeof(JFAFloodCB), true, 256, "[JFA Flood] Constant buffer");
        m_OutlineCB = ConstantBuffer::Create(sizeof(JFAOutlineCB), true, 64, "[JFA Outline] Constant buffer");

        auto bufferDesc = nvrhi::BufferDesc();
        bufferDesc.byteSize = sizeof(uint32_t) * 100;
        bufferDesc.structStride = sizeof(uint32_t);
        bufferDesc.cpuAccess = nvrhi::CpuAccessMode::None;
        bufferDesc.debugName = "JFA Selected Object IDs";
        bufferDesc.keepInitialState = true;
        bufferDesc.initialState = nvrhi::ResourceStates::ShaderResource;
        m_SelectedIDBuffer = device->createBuffer(bufferDesc);

        // Seed Binding Layout
        {
            nvrhi::BindingLayoutDesc layoutDesc;
            layoutDesc.visibility = nvrhi::ShaderType::All;
            layoutDesc.bindings =
            {
                nvrhi::BindingLayoutItem::VolatileConstantBuffer(0),
                nvrhi::BindingLayoutItem::Texture_SRV(0),             // Object ID texture
                nvrhi::BindingLayoutItem::StructuredBuffer_SRV(1),    // Selected IDs
                nvrhi::BindingLayoutItem::Texture_UAV(0)              // Ping texture output
            };
            m_SeedBindingLayout = device->createBindingLayout(layoutDesc);
        }

        // Flood Binding Layout
        {
            nvrhi::BindingLayoutDesc layoutDesc;
            layoutDesc.visibility = nvrhi::ShaderType::All;
            layoutDesc.bindings =
            {
                nvrhi::BindingLayoutItem::VolatileConstantBuffer(0),
                nvrhi::BindingLayoutItem::Texture_SRV(0),             // Input JFA texture
                nvrhi::BindingLayoutItem::Texture_UAV(0)              // Output JFA texture
            };
            m_FloodBindingLayout = device->createBindingLayout(layoutDesc);
        }

        // Outline Binding Layout
        {
            nvrhi::BindingLayoutDesc layoutDesc;
            layoutDesc.visibility = nvrhi::ShaderType::All;
            layoutDesc.bindings =
            {
                nvrhi::BindingLayoutItem::VolatileConstantBuffer(0),
                nvrhi::BindingLayoutItem::Texture_SRV(0),             // Final JFA texture
                nvrhi::BindingLayoutItem::Texture_UAV(0)              // Output RGBA8 texture
            };
            m_OutlineBindingLayout = device->createBindingLayout(layoutDesc);
        }

        m_SeedShader = Shader::Create("resources/shaders/jfa_seed.compute.hlsl", UMBRA_SHADER_TYPE_COMPUTE, false);
        m_FloodShader = Shader::Create("resources/shaders/jfa_flood.compute.hlsl", UMBRA_SHADER_TYPE_COMPUTE, false);
        m_OutlineShader = Shader::Create("resources/shaders/jfa_outline.compute.hlsl", UMBRA_SHADER_TYPE_COMPUTE, false);
    }

    OutlineJFA::~OutlineJFA()
    {
    }

    void OutlineJFA::CreatePipeline()
    {
        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();

        {
            nvrhi::ComputePipelineDesc computeDesc;
            computeDesc.CS = m_SeedShader->GetHandle();
            computeDesc.bindingLayouts = { m_SeedBindingLayout };
            m_SeedPipeline = device->createComputePipeline(computeDesc);
        }

        {
            nvrhi::ComputePipelineDesc computeDesc;
            computeDesc.CS = m_FloodShader->GetHandle();
            computeDesc.bindingLayouts = { m_FloodBindingLayout };
            m_FloodPipeline = device->createComputePipeline(computeDesc);
        }

        {
            nvrhi::ComputePipelineDesc computeDesc;
            computeDesc.CS = m_OutlineShader->GetHandle();
            computeDesc.bindingLayouts = { m_OutlineBindingLayout };
            m_OutlinePipeline = device->createComputePipeline(computeDesc);
        }
    }

    void OutlineJFA::CreateOutputTexture(uint32_t width, uint32_t height)
    {
        TextureCreateInfo createInfoRG;
        createInfoRG.width = width;
        createInfoRG.height = height;
        createInfoRG.format = nvrhi::Format::RG16_SINT;
        createInfoRG.initialState = nvrhi::ResourceStates::ShaderResource;
        createInfoRG.keepInitialState = true;
        createInfoRG.isUAV = true;

        m_JFAPing = Texture::Create(createInfoRG, "[JFA] Ping Texture");
        m_JFAPong = Texture::Create(createInfoRG, "[JFA] Pong Texture");

        TextureCreateInfo createInfoRGBA;
        createInfoRGBA.width = width;
        createInfoRGBA.height = height;
        createInfoRGBA.format = nvrhi::Format::RGBA8_UNORM;
        createInfoRGBA.initialState = nvrhi::ResourceStates::ShaderResource;
        createInfoRGBA.keepInitialState = true;
        createInfoRGBA.isUAV = true;

        m_OutputTexture = Texture::Create(createInfoRGBA, "[JFA] Output Texture");
    }

    void OutlineJFA::UpdateBindingSet(const Ref<Texture> &objectIDTexture)
    {
        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();

        // Seed Binding Set
        {
            nvrhi::BindingSetDesc desc;
            desc.bindings =
            {
                nvrhi::BindingSetItem::ConstantBuffer(0, m_SeedCB->GetHandle()),
                nvrhi::BindingSetItem::Texture_SRV(0, objectIDTexture->GetHandle()),
                nvrhi::BindingSetItem::StructuredBuffer_SRV(1, m_SelectedIDBuffer),
                nvrhi::BindingSetItem::Texture_UAV(0, m_JFAPing->GetHandle()),
            };
            m_SeedBindingSet = device->createBindingSet(desc, m_SeedBindingLayout);
            LOG_ASSERT(m_SeedBindingSet, "[JFA] Failed to create seed binding set");
        }

        // Flood Binding Sets (Ping->Pong and Pong->Ping)
        {
            nvrhi::BindingSetDesc descPingToPong;
            descPingToPong.bindings =
            {
                nvrhi::BindingSetItem::ConstantBuffer(0, m_FloodCB->GetHandle()),
                nvrhi::BindingSetItem::Texture_SRV(0, m_JFAPing->GetHandle()),
                nvrhi::BindingSetItem::Texture_UAV(0, m_JFAPong->GetHandle()),
            };
            m_FloodBindingSetPingToPong = device->createBindingSet(descPingToPong, m_FloodBindingLayout);

            nvrhi::BindingSetDesc descPongToPing;
            descPongToPing.bindings =
            {
                nvrhi::BindingSetItem::ConstantBuffer(0, m_FloodCB->GetHandle()),
                nvrhi::BindingSetItem::Texture_SRV(0, m_JFAPong->GetHandle()),
                nvrhi::BindingSetItem::Texture_UAV(0, m_JFAPing->GetHandle()),
            };
            m_FloodBindingSetPongToPing = device->createBindingSet(descPongToPing, m_FloodBindingLayout);
        }

        // Outline Binding Sets (Ping input vs Pong input)
        {
            nvrhi::BindingSetDesc descPing;
            descPing.bindings =
            {
                nvrhi::BindingSetItem::ConstantBuffer(0, m_OutlineCB->GetHandle()),
                nvrhi::BindingSetItem::Texture_SRV(0, m_JFAPing->GetHandle()),
                nvrhi::BindingSetItem::Texture_UAV(0, m_OutputTexture->GetHandle()),
            };
            m_OutlineBindingSetPing = device->createBindingSet(descPing, m_OutlineBindingLayout);

            nvrhi::BindingSetDesc descPong;
            descPong.bindings =
            {
                nvrhi::BindingSetItem::ConstantBuffer(0, m_OutlineCB->GetHandle()),
                nvrhi::BindingSetItem::Texture_SRV(0, m_JFAPong->GetHandle()),
                nvrhi::BindingSetItem::Texture_UAV(0, m_OutputTexture->GetHandle()),
            };
            m_OutlineBindingSetPong = device->createBindingSet(descPong, m_OutlineBindingLayout);
        }
    }

    void OutlineJFA::ExecuteCompute(nvrhi::ICommandList *commandList, const OutlineJFAParameter &params, uint32_t width, uint32_t height)
    {
        uint32_t groupsX = (width + 7) / 8;
        uint32_t groupsY = (height + 7) / 8;

        // 1. Seed Pass
        JFASeedCB seedCB;
        seedCB.selectedCount = params.selectedCount;
        commandList->writeBuffer(m_SeedCB->GetHandle(), &seedCB, sizeof(seedCB));

        commandList->setTextureState(m_JFAPing->GetHandle(), nvrhi::TextureSubresourceSet(), nvrhi::ResourceStates::UnorderedAccess);
        commandList->commitBarriers();

        nvrhi::ComputeState seedState;
        seedState.pipeline = m_SeedPipeline;
        seedState.bindings = { m_SeedBindingSet };
        commandList->setComputeState(seedState);
        commandList->dispatch(groupsX, groupsY, 1);

        // 2. Flood Passes
        uint32_t maxDim = std::max(width, height);
        int32_t stepSize = 1;
        while (stepSize * 2 < static_cast<int32_t>(maxDim))
        {
            stepSize *= 2;
        }

        bool pingIsSource = true;
        while (stepSize >= 1)
        {
            JFAFloodCB floodCB;
            floodCB.stepSize = stepSize;
            commandList->writeBuffer(m_FloodCB->GetHandle(), &floodCB, sizeof(floodCB));

            if (pingIsSource)
            {
                commandList->setTextureState(m_JFAPing->GetHandle(), nvrhi::TextureSubresourceSet(), nvrhi::ResourceStates::ShaderResource);
                commandList->setTextureState(m_JFAPong->GetHandle(), nvrhi::TextureSubresourceSet(), nvrhi::ResourceStates::UnorderedAccess);
                commandList->commitBarriers();

                nvrhi::ComputeState floodState;
                floodState.pipeline = m_FloodPipeline;
                floodState.bindings = { m_FloodBindingSetPingToPong };
                commandList->setComputeState(floodState);
                commandList->dispatch(groupsX, groupsY, 1);
            }
            else
            {
                commandList->setTextureState(m_JFAPong->GetHandle(), nvrhi::TextureSubresourceSet(), nvrhi::ResourceStates::ShaderResource);
                commandList->setTextureState(m_JFAPing->GetHandle(), nvrhi::TextureSubresourceSet(), nvrhi::ResourceStates::UnorderedAccess);
                commandList->commitBarriers();

                nvrhi::ComputeState floodState;
                floodState.pipeline = m_FloodPipeline;
                floodState.bindings = { m_FloodBindingSetPongToPing };
                commandList->setComputeState(floodState);
                commandList->dispatch(groupsX, groupsY, 1);
            }

            pingIsSource = !pingIsSource;
            stepSize /= 2;
        }

        // 3. Outline Pass
        JFAOutlineCB outlineCB;
        outlineCB.outlineColor = params.outlineColor;
        outlineCB.outlineWidth = params.outlineWidth;
        commandList->writeBuffer(m_OutlineCB->GetHandle(), &outlineCB, sizeof(outlineCB));

        commandList->setTextureState(m_OutputTexture->GetHandle(), nvrhi::TextureSubresourceSet(), nvrhi::ResourceStates::UnorderedAccess);

        nvrhi::ComputeState outlineState;
        outlineState.pipeline = m_OutlinePipeline;
        if (pingIsSource)
        {
            // Last flood wrote to Ping, so Ping is source
            commandList->setTextureState(m_JFAPing->GetHandle(), nvrhi::TextureSubresourceSet(), nvrhi::ResourceStates::ShaderResource);
            commandList->commitBarriers();
            outlineState.bindings = { m_OutlineBindingSetPing };
        }
        else
        {
            // Last flood wrote to Pong, so Pong is source
            commandList->setTextureState(m_JFAPong->GetHandle(), nvrhi::TextureSubresourceSet(), nvrhi::ResourceStates::ShaderResource);
            commandList->commitBarriers();
            outlineState.bindings = { m_OutlineBindingSetPong };
        }

        commandList->setComputeState(outlineState);
        commandList->dispatch(groupsX, groupsY, 1);

        // Transition final outline texture to ShaderResource for composite pass
        commandList->setTextureState(m_OutputTexture->GetHandle(), nvrhi::TextureSubresourceSet(), nvrhi::ResourceStates::ShaderResource);
        commandList->commitBarriers();
    }

    Ref<OutlineJFA> OutlineJFA::Create()
    {
        return CreateRef<OutlineJFA>();
    }
}
