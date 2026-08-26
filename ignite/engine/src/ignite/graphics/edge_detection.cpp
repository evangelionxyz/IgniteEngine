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
        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();

        m_ConstantBuffer = ConstantBuffer::Create(sizeof(EdgeDetectionParameter), true, 16, "[Edge Detection] Constant buffer");

        auto bufferDesc = nvrhi::BufferDesc();
        bufferDesc.byteSize = sizeof(uint32_t) * 100; // allocate enough memory for selection
        bufferDesc.structStride = sizeof(uint32_t);
        bufferDesc.cpuAccess = nvrhi::CpuAccessMode::None;
        bufferDesc.debugName = "Selected Object IDs";
        bufferDesc.keepInitialState = true;
        bufferDesc.initialState = nvrhi::ResourceStates::ShaderResource;
        m_SelectedIDBuffer = device->createBuffer(bufferDesc);

        // Create binding layout
        nvrhi::BindingLayoutDesc layoutDesc;
        layoutDesc.visibility = nvrhi::ShaderType::All;
        layoutDesc.bindings =
        {
            // constant buffer
            nvrhi::BindingLayoutItem::VolatileConstantBuffer(0),

            // Textures
            nvrhi::BindingLayoutItem::Texture_SRV(0), // Scene texture
            nvrhi::BindingLayoutItem::Texture_SRV(1), // Object ID texture
            nvrhi::BindingLayoutItem::Texture_SRV(2), // Depth texture
            nvrhi::BindingLayoutItem::StructuredBuffer_SRV(3), // Object ID texture

            // Sampler
            nvrhi::BindingLayoutItem::Sampler(0),

            // Output texture (for Compute)
            nvrhi::BindingLayoutItem::Texture_UAV(0)
        };

        m_BindingLayout = device->createBindingLayout(layoutDesc);

        m_Shader = Shader::Create("resources/shaders/sobel_edge_detection.compute.hlsl", UMBRA_SHADER_TYPE_COMPUTE, false);
    }

    EdgeDetection::~EdgeDetection()
    {
        m_Sampler = nullptr;
    }

    void EdgeDetection::CreatePipeline()
    {
        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();

        // Create compute pipeline
        nvrhi::ComputePipelineDesc computeDesc;
        computeDesc.CS = m_Shader->GetHandle();
        computeDesc.bindingLayouts = { m_BindingLayout };
        m_Pipeline = device->createComputePipeline(computeDesc);
    }

    void EdgeDetection::UpdateBindingSet(const Ref<Texture> &sceneTexture, const Ref<Texture> &objectIDTexture, const Ref<Texture> &depth)
    {
        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();

        nvrhi::BindingSetDesc desc;
        desc.bindings =
        {
            nvrhi::BindingSetItem::ConstantBuffer(0, m_ConstantBuffer->GetHandle()),
            nvrhi::BindingSetItem::Texture_SRV(0, sceneTexture->GetHandle()),
            nvrhi::BindingSetItem::Texture_SRV(1, objectIDTexture->GetHandle()),
            nvrhi::BindingSetItem::Texture_SRV(2, depth->GetHandle()),
            nvrhi::BindingSetItem::StructuredBuffer_SRV(3, m_SelectedIDBuffer),
            nvrhi::BindingSetItem::Sampler(0, m_Sampler),
            nvrhi::BindingSetItem::Texture_UAV(0, m_OutputTexture->GetHandle()),
        };

        m_BindingSet = device->createBindingSet(desc, m_BindingLayout);
        LOG_ASSERT(m_BindingSet, "[Edge Detection] Failed to create binding set");
    }

    void EdgeDetection::ExecuteCompute(nvrhi::ICommandList *commandList, const EdgeDetectionParameter &params, uint32_t width, uint32_t height)
    {
        // Update constant buffer
        commandList->writeBuffer(m_ConstantBuffer->GetHandle(), &params, sizeof(params));

        // Transition output texture to UAV state before compute
        commandList->setTextureState(m_OutputTexture->GetHandle(), nvrhi::TextureSubresourceSet(), nvrhi::ResourceStates::UnorderedAccess);
        commandList->commitBarriers(); // commit the barriers

        // Set compute pipeline
        nvrhi::ComputeState computeState;
        computeState.pipeline = m_Pipeline;
        computeState.bindings = { m_BindingSet };
        commandList->setComputeState(computeState);

        // Dispatch compute shader
        uint32_t groupsX = (width + 7) / 8; // 8x8 threads groups
        uint32_t groupsY = (height + 7) / 8;
        commandList->dispatch(groupsX, groupsY, 1);

        // Transition edge detection output from UAV to ShaderResource for composite pass
        commandList->setTextureState(m_OutputTexture->GetHandle(), nvrhi::TextureSubresourceSet(), nvrhi::ResourceStates::ShaderResource);
        commandList->commitBarriers(); // Commit the barrier before composite pass
    }

    Ref<EdgeDetection> EdgeDetection::Create()
    {
        return CreateRef<EdgeDetection>();
    }

    void EdgeDetection::CreateOutputTexture(uint32_t width, uint32_t height)
    {
        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();

        // Texture creation -> Common state
        // First use in Compute: Common -> UnorderedAccess (for writing)
        // After compute       : UnorderedAccess -> ShaderResource (for reading in composite pass)
        TextureCreateInfo createInfo;
        createInfo.width = width;
        createInfo.height = height;
        createInfo.format = nvrhi::Format::RGBA8_UNORM;
        createInfo.initialState = nvrhi::ResourceStates::ShaderResource;
        createInfo.keepInitialState = true;
        createInfo.isUAV = true;

        m_OutputTexture = Texture::Create(createInfo, "[Edge Detection] Output Texture");

    	auto samplerDesc = nvrhi::SamplerDesc();
    	samplerDesc.addressU = nvrhi::SamplerAddressMode::Repeat;
    	m_Sampler = device->createSampler(samplerDesc);
    	LOG_ASSERT(m_Sampler, "Failed to create sampler");
    }

}
