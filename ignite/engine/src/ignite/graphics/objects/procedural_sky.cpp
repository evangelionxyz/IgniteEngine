// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"
#include "procedural_sky.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/logger.hpp"

namespace ignite
{
    ProceduralSky::ProceduralSky()
    {
    }

    ProceduralSky::~ProceduralSky()
    {
        m_TransmittancePipeline = nullptr;
        m_MultiScatteringPipeline = nullptr;
        m_SkyViewPipeline = nullptr;

        m_TransmittanceLayout = nullptr;
        m_MultiScatteringLayout = nullptr;
        m_SkyViewLayout = nullptr;

        m_AtmosphereBuffer = nullptr;
        m_LinearSampler = nullptr;

        m_TransmittanceLUTTexture = nullptr;
        m_MultiScatteringLUTTexture = nullptr;
        m_SkyViewLUTTexture = nullptr;
    }

    void ProceduralSky::Init()
    {
        if (m_Initialized) return;

        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();
        LOG_ASSERT(device, "[ProceduralSky] NVRHI device is null!");

        // Constant Buffer
        nvrhi::BufferDesc cbDesc;
        cbDesc.byteSize = sizeof(AtmosphereParams);
        cbDesc.isConstantBuffer = true;
        cbDesc.isVolatile = true;
        cbDesc.debugName = "AtmosphereParams Buffer";
        cbDesc.maxVersions = 16;
        m_AtmosphereBuffer = device->createBuffer(cbDesc);

        // Sampler
        nvrhi::SamplerDesc samplerDesc;
        samplerDesc.setAllFilters(true);
        samplerDesc.setAllAddressModes(nvrhi::SamplerAddressMode::Clamp);
        m_LinearSampler = device->createSampler(samplerDesc);

        CreateTextures();
        CreatePipelines();

        m_Initialized = true;
    }

    void ProceduralSky::CreateTextures()
    {
        TextureCreateInfo transCI;
        transCI.width = 256;
        transCI.height = 64;
        transCI.format = nvrhi::Format::RGBA16_FLOAT;
        transCI.isUAV = true;
        transCI.initialState = nvrhi::ResourceStates::UnorderedAccess;
        transCI.keepInitialState = true;
        m_TransmittanceLUTTexture = Texture::Create(transCI, "Transmittance LUT");

        TextureCreateInfo multiCI;
        multiCI.width = 32;
        multiCI.height = 32;
        multiCI.format = nvrhi::Format::RGBA16_FLOAT;
        multiCI.isUAV = true;
        multiCI.initialState = nvrhi::ResourceStates::UnorderedAccess;
        multiCI.keepInitialState = true;
        m_MultiScatteringLUTTexture = Texture::Create(multiCI, "MultiScattering LUT");

        TextureCreateInfo skyCI;
        skyCI.width = 192;
        skyCI.height = 108;
        skyCI.format = nvrhi::Format::RGBA16_FLOAT;
        skyCI.isUAV = true;
        skyCI.initialState = nvrhi::ResourceStates::UnorderedAccess;
        skyCI.keepInitialState = true;
        m_SkyViewLUTTexture = Texture::Create(skyCI, "SkyView LUT");
    }

    void ProceduralSky::CreatePipelines()
    {
        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();

        // 1. Transmittance
        {
            nvrhi::BindingLayoutDesc layoutDesc;
            layoutDesc.setVisibility(nvrhi::ShaderType::All);
            layoutDesc.addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(0));
            layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_UAV(0));
            m_TransmittanceLayout = device->createBindingLayout(layoutDesc);

            m_TransmittanceShader = Shader::Create("resources/shaders/transmittance_lut.compute.hlsl", UMBRA_SHADER_TYPE_COMPUTE, false);

            nvrhi::ComputePipelineDesc cpDesc;
            cpDesc.CS = m_TransmittanceShader->GetHandle();
            cpDesc.bindingLayouts = { m_TransmittanceLayout };
            m_TransmittancePipeline = device->createComputePipeline(cpDesc);
        }

        // 2. Multi-Scattering
        {
            nvrhi::BindingLayoutDesc layoutDesc;
            layoutDesc.setVisibility(nvrhi::ShaderType::All);
            layoutDesc.addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(0));
            layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(0));
            layoutDesc.addItem(nvrhi::BindingLayoutItem::Sampler(0));
            layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_UAV(0));
            m_MultiScatteringLayout = device->createBindingLayout(layoutDesc);

            m_MultiScatteringShader = Shader::Create("resources/shaders/multiscattering_lut.compute.hlsl", UMBRA_SHADER_TYPE_COMPUTE, false);

            nvrhi::ComputePipelineDesc cpDesc;
            cpDesc.CS = m_MultiScatteringShader->GetHandle();
            cpDesc.bindingLayouts = { m_MultiScatteringLayout };
            m_MultiScatteringPipeline = device->createComputePipeline(cpDesc);
        }

        // 3. Sky-View
        {
            nvrhi::BindingLayoutDesc layoutDesc;
            layoutDesc.setVisibility(nvrhi::ShaderType::All);
            layoutDesc.addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(0));
            layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(0));
            layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(1));
            layoutDesc.addItem(nvrhi::BindingLayoutItem::Sampler(0));
            layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_UAV(0));
            m_SkyViewLayout = device->createBindingLayout(layoutDesc);

            m_SkyViewShader = Shader::Create("resources/shaders/sky_view_lut.compute.hlsl", UMBRA_SHADER_TYPE_COMPUTE, false);

            nvrhi::ComputePipelineDesc cpDesc;
            cpDesc.CS = m_SkyViewShader->GetHandle();
            cpDesc.bindingLayouts = { m_SkyViewLayout };
            m_SkyViewPipeline = device->createComputePipeline(cpDesc);
        }
    }

    void ProceduralSky::RenderLUTs(nvrhi::ICommandList *cmd, const glm::vec3 &sunDir, const glm::vec3 &sunColor, float sunIntensity, float sunAngularRadius, const glm::vec3 &cameraPos)
    {
        if (!m_Initialized)
        {
            Init();
        }

        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();

        m_Params.sunDirectionAndIntensity  = glm::vec4(sunDir, sunIntensity);
        m_Params.sunColorAndRadius         = glm::vec4(sunColor, sunAngularRadius);
        m_Params.cameraPositionAndAltitude = glm::vec4(cameraPos, cameraPos.y / 1000.0f);

        cmd->writeBuffer(m_AtmosphereBuffer, &m_Params, sizeof(AtmosphereParams));

        // Transmittance & Multi-Scattering LUTs (Computed when atmosphere parameters change)
        if (m_DirtyAtmosphere)
        {
            // Transmittance LUT Pass
            cmd->setTextureState(m_TransmittanceLUTTexture->GetHandle(), nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);
            cmd->commitBarriers();

            nvrhi::BindingSetDesc transBSDesc;
            transBSDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, m_AtmosphereBuffer));
            transBSDesc.addItem(nvrhi::BindingSetItem::Texture_UAV(0, m_TransmittanceLUTTexture->GetHandle()));
            nvrhi::BindingSetHandle transBS = device->createBindingSet(transBSDesc, m_TransmittanceLayout);

            nvrhi::ComputeState transState;
            transState.pipeline = m_TransmittancePipeline;
            transState.bindings = { transBS };
            cmd->setComputeState(transState);
            cmd->dispatch(32, 8, 1);

            cmd->setTextureState(m_TransmittanceLUTTexture->GetHandle(), nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            cmd->commitBarriers();

            // MultiScattering LUT Pass
            cmd->setTextureState(m_MultiScatteringLUTTexture->GetHandle(), nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);
            cmd->commitBarriers();

            nvrhi::BindingSetDesc multiBSDesc;
            multiBSDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, m_AtmosphereBuffer));
            multiBSDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, m_TransmittanceLUTTexture->GetHandle()));
            multiBSDesc.addItem(nvrhi::BindingSetItem::Sampler(0, m_LinearSampler));
            multiBSDesc.addItem(nvrhi::BindingSetItem::Texture_UAV(0, m_MultiScatteringLUTTexture->GetHandle()));
            nvrhi::BindingSetHandle multiBS = device->createBindingSet(multiBSDesc, m_MultiScatteringLayout);

            nvrhi::ComputeState multiState;
            multiState.pipeline = m_MultiScatteringPipeline;
            multiState.bindings = { multiBS };
            cmd->setComputeState(multiState);
            cmd->dispatch(4, 4, 1);

            cmd->setTextureState(m_MultiScatteringLUTTexture->GetHandle(), nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            cmd->commitBarriers();

            m_DirtyAtmosphere = false;
        }

        // Sky-View LUT Pass (Recomputed per frame)
        cmd->setTextureState(m_SkyViewLUTTexture->GetHandle(), nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);
        cmd->commitBarriers();

        nvrhi::BindingSetDesc skyBSDesc;
        skyBSDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, m_AtmosphereBuffer));
        skyBSDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, m_TransmittanceLUTTexture->GetHandle()));
        skyBSDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(1, m_MultiScatteringLUTTexture->GetHandle()));
        skyBSDesc.addItem(nvrhi::BindingSetItem::Sampler(0, m_LinearSampler));
        skyBSDesc.addItem(nvrhi::BindingSetItem::Texture_UAV(0, m_SkyViewLUTTexture->GetHandle()));
        nvrhi::BindingSetHandle skyBS = device->createBindingSet(skyBSDesc, m_SkyViewLayout);

        nvrhi::ComputeState skyState;
        skyState.pipeline = m_SkyViewPipeline;
        skyState.bindings = { skyBS };
        cmd->setComputeState(skyState);
        cmd->dispatch(24, 14, 1);

        cmd->setTextureState(m_SkyViewLUTTexture->GetHandle(), nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        cmd->commitBarriers();
    }
}
