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
        auto *dm = DeviceManager::GetInstance();
        if (dm && dm->GetDevice())
        {
            auto &nri = dm->GetNRI();
            if (m_TransmittancePipeline) nri.DestroyPipeline(m_TransmittancePipeline);
            if (m_MultiScatteringPipeline) nri.DestroyPipeline(m_MultiScatteringPipeline);
            if (m_SkyViewPipeline) nri.DestroyPipeline(m_SkyViewPipeline);

            if (m_TransmittancePipelineLayout) nri.DestroyPipelineLayout(m_TransmittancePipelineLayout);
            if (m_MultiScatteringPipelineLayout) nri.DestroyPipelineLayout(m_MultiScatteringPipelineLayout);
            if (m_SkyViewPipelineLayout) nri.DestroyPipelineLayout(m_SkyViewPipelineLayout);

            if (m_LinearSampler) nri.DestroyDescriptor(m_LinearSampler);
        }


        m_TransmittanceBindingSet = nullptr;
        m_MultiScatteringBindingSet = nullptr;
        m_SkyViewBindingSet = nullptr;

        m_AtmosphereBuffer = nullptr;
        m_LinearSampler = nullptr;

        m_TransmittanceLUTTexture = nullptr;
        m_MultiScatteringLUTTexture = nullptr;
        m_SkyViewLUTTexture = nullptr;
    }

    void ProceduralSky::Init()
    {
        if (m_Initialized) return;

        auto &nri = DeviceManager::GetInstance()->GetNRI();
        nri::Device *device = DeviceManager::GetInstance()->GetDevice();
        LOG_ASSERT(device, "[ProceduralSky] NRI device is null!");

        // Constant Buffer
        m_AtmosphereBuffer = ConstantBuffer::Create(sizeof(AtmosphereParams), true, 16, "AtmosphereParams Buffer");

        // Sampler
        nri::SamplerDesc samplerDesc = {};
        samplerDesc.filters = { nri::Filter::LINEAR, nri::Filter::LINEAR, nri::Filter::LINEAR };
        samplerDesc.addressModes = { nri::AddressMode::CLAMP_TO_EDGE, nri::AddressMode::CLAMP_TO_EDGE, nri::AddressMode::CLAMP_TO_EDGE };
        samplerDesc.mipMax = 16.0f;

        nri::Result result = nri.CreateSampler(*device, samplerDesc, m_LinearSampler);
        LOG_ASSERT(result == nri::Result::SUCCESS, "[Procedural Sky] Failed to create sampler!");

        CreateTextures();
        CreatePipelines();

        m_Initialized = true;
    }

    void ProceduralSky::CreateTextures()
    {
        TextureCreateInfo transCI;
        transCI.width = 256;
        transCI.height = 64;
        transCI.format = nri::Format::RGBA16_SFLOAT;
        transCI.isUAV = true;
        transCI.initialLayout = nri::Layout::SHADER_RESOURCE_STORAGE;
        transCI.keepInitialState = true;
        m_TransmittanceLUTTexture = Texture::Create(transCI, "Transmittance LUT");

        TextureCreateInfo multiCI;
        multiCI.width = 32;
        multiCI.height = 32;
        multiCI.format = nri::Format::RGBA16_SFLOAT;
        multiCI.isUAV = true;
        multiCI.initialLayout = nri::Layout::SHADER_RESOURCE_STORAGE;
        multiCI.keepInitialState = true;
        m_MultiScatteringLUTTexture = Texture::Create(multiCI, "MultiScattering LUT");

        TextureCreateInfo skyCI;
        skyCI.width = 192;
        skyCI.height = 108;
        skyCI.format = nri::Format::RGBA16_SFLOAT;
        skyCI.isUAV = true;
        skyCI.initialLayout = nri::Layout::SHADER_RESOURCE_STORAGE;
        skyCI.keepInitialState = true;
        m_SkyViewLUTTexture = Texture::Create(skyCI, "SkyView LUT");
    }

    void ProceduralSky::CreatePipelines()
    {
        auto &nri = DeviceManager::GetInstance()->GetNRI();
        nri::Device *device = DeviceManager::GetInstance()->GetDevice();

        // 1. Transmittance
        {
            m_TransmittanceLayout.SetVisibility(nri::StageBits::ALL);
            m_TransmittanceLayout.AddItem(BindingLayoutItem::VolatileConstantBuffer(0));
            m_TransmittanceLayout.AddItem(BindingLayoutItem::TextureUAV(0));
            m_TransmittancePipelineLayout = m_TransmittanceLayout.CreatePipelineLayout(nri::StageBits::COMPUTE_SHADER);

            m_TransmittanceShader = Shader::Create("resources/shaders/transmittance_lut.compute.hlsl", UMBRA_SHADER_TYPE_COMPUTE, false);

            nri::ComputePipelineDesc cpDesc = {};
            cpDesc.pipelineLayout = m_TransmittancePipelineLayout;
            cpDesc.shader = m_TransmittanceShader->GetShaderDesc();
            const nri::Result result = nri.CreateComputePipeline(*device, cpDesc, m_TransmittancePipeline);
            LOG_ASSERT(result == nri::Result::SUCCESS, "[Procedural Sky] Failed to create transmittance pipeline!");

            BindingSetDesc transBSDesc;
            transBSDesc.AddItem(BindingSetItem::ConstantBuffer(0, m_AtmosphereBuffer->GetDescriptor()));
            transBSDesc.AddItem(BindingSetItem::TextureUAV(0, m_TransmittanceLUTTexture->GetStorageView(0)));
            m_TransmittanceBindingSet = BindingSet::Create("Procedural Sky Transmittance", m_TransmittancePipelineLayout, 0, transBSDesc);
        }

        // 2. Multi-Scattering
        {
            m_MultiScatteringLayout.SetVisibility(nri::StageBits::ALL);
            m_MultiScatteringLayout.AddItem(BindingLayoutItem::VolatileConstantBuffer(0));
            m_MultiScatteringLayout.AddItem(BindingLayoutItem::TextureSRV(0));
            m_MultiScatteringLayout.AddItem(BindingLayoutItem::Sampler(0));
            m_MultiScatteringLayout.AddItem(BindingLayoutItem::TextureUAV(0));
            m_MultiScatteringPipelineLayout = m_MultiScatteringLayout.CreatePipelineLayout(nri::StageBits::COMPUTE_SHADER);

            m_MultiScatteringShader = Shader::Create("resources/shaders/multiscattering_lut.compute.hlsl", UMBRA_SHADER_TYPE_COMPUTE, false);

            nri::ComputePipelineDesc cpDesc = {};
            cpDesc.pipelineLayout = m_MultiScatteringPipelineLayout;
            cpDesc.shader = m_MultiScatteringShader->GetShaderDesc();

            const nri::Result result = nri.CreateComputePipeline(*device, cpDesc, m_MultiScatteringPipeline);
            LOG_ASSERT(result == nri::Result::SUCCESS, "[Procedural Sky] Failed to create multi scattering pipeline!");

            BindingSetDesc multiBSDesc;
            multiBSDesc.AddItem(BindingSetItem::ConstantBuffer(0, m_AtmosphereBuffer->GetDescriptor()));
            multiBSDesc.AddItem(BindingSetItem::TextureSRV(0, m_TransmittanceLUTTexture->GetShaderResourceView()));
            multiBSDesc.AddItem(BindingSetItem::Sampler(0, m_LinearSampler));
            multiBSDesc.AddItem(BindingSetItem::TextureUAV(0, m_MultiScatteringLUTTexture->GetStorageView(0)));
            m_MultiScatteringBindingSet = BindingSet::Create("Procedural Sky MultiScattering", m_MultiScatteringPipelineLayout, 0, multiBSDesc);
        }

        // 3. Sky-View
        {
            m_SkyViewLayout.SetVisibility(nri::StageBits::ALL);
            m_SkyViewLayout.AddItem(BindingLayoutItem::VolatileConstantBuffer(0));
            m_SkyViewLayout.AddItem(BindingLayoutItem::TextureSRV(0));
            m_SkyViewLayout.AddItem(BindingLayoutItem::TextureSRV(1));
            m_SkyViewLayout.AddItem(BindingLayoutItem::Sampler(0));
            m_SkyViewLayout.AddItem(BindingLayoutItem::TextureUAV(0));
            m_SkyViewPipelineLayout = m_SkyViewLayout.CreatePipelineLayout(nri::StageBits::COMPUTE_SHADER);

            m_SkyViewShader = Shader::Create("resources/shaders/sky_view_lut.compute.hlsl", UMBRA_SHADER_TYPE_COMPUTE, false);

            nri::ComputePipelineDesc cpDesc = {};
            cpDesc.pipelineLayout = m_SkyViewPipelineLayout;
            cpDesc.shader = m_SkyViewShader->GetShaderDesc();

            const nri::Result result = nri.CreateComputePipeline(*device, cpDesc, m_SkyViewPipeline);
            LOG_ASSERT(result == nri::Result::SUCCESS, "[Procedural Sky] Failed to create sky view pipeline!");

            BindingSetDesc skyBSDesc;
            skyBSDesc.AddItem(BindingSetItem::ConstantBuffer(0, m_AtmosphereBuffer->GetDescriptor()));
            skyBSDesc.AddItem(BindingSetItem::TextureSRV(0, m_TransmittanceLUTTexture->GetShaderResourceView()));
            skyBSDesc.AddItem(BindingSetItem::TextureSRV(1, m_MultiScatteringLUTTexture->GetShaderResourceView()));
            skyBSDesc.AddItem(BindingSetItem::Sampler(0, m_LinearSampler));
            skyBSDesc.AddItem(BindingSetItem::TextureUAV(0, m_SkyViewLUTTexture->GetStorageView(0)));
            m_SkyViewBindingSet = BindingSet::Create("Procedural Sky SkyView", m_SkyViewPipelineLayout, 0, skyBSDesc);
        }
    }

    void ProceduralSky::RenderLUTs(nri::CommandBuffer *cmd, const glm::vec3 &sunDir, const glm::vec3 &sunColor, float sunIntensity, float sunAngularRadius, const glm::vec3 &cameraPos)
    {
        if (!cmd)
            return;

        if (!m_Initialized)
        {
            Init();
        }

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
            nri.CmdBarrier(*cmd, barrierDesc);
            tex->SetCurrentState(layout, access, stages);
        };

        m_Params.sunDirectionAndIntensity  = glm::vec4(sunDir, sunIntensity);
        m_Params.sunColorAndRadius         = glm::vec4(sunColor, sunAngularRadius);
        m_Params.cameraPositionAndAltitude = glm::vec4(cameraPos, cameraPos.y / 1000.0f);

        m_AtmosphereBuffer->SetData(&m_Params, sizeof(AtmosphereParams));

        // Transmittance & Multi-Scattering LUTs (Computed when atmosphere parameters change)
        if (m_DirtyAtmosphere)
        {
            // Transmittance LUT Pass
            transitionTexture(m_TransmittanceLUTTexture, nri::Layout::SHADER_RESOURCE_STORAGE, nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER);

            nri.CmdSetPipelineLayout(*cmd, nri::BindPoint::COMPUTE, *m_TransmittancePipelineLayout);
            nri.CmdSetPipeline(*cmd, *m_TransmittancePipeline);
            {
                nri::SetDescriptorSetDesc setDesc = {};
                setDesc.setIndex = 0;
                setDesc.descriptorSet = m_TransmittanceBindingSet->GetHandle();
                setDesc.bindPoint = nri::BindPoint::COMPUTE;
                nri.CmdSetDescriptorSet(*cmd, setDesc);
            }
            nri.CmdDispatch(*cmd, { 32, 8, 1 });

            transitionTexture(m_TransmittanceLUTTexture, nri::Layout::SHADER_RESOURCE, nri::AccessBits::SHADER_RESOURCE, nri::StageBits::COMPUTE_SHADER);

            // MultiScattering LUT Pass
            transitionTexture(m_MultiScatteringLUTTexture, nri::Layout::SHADER_RESOURCE_STORAGE, nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER);

            nri.CmdSetPipelineLayout(*cmd, nri::BindPoint::COMPUTE, *m_MultiScatteringPipelineLayout);
            nri.CmdSetPipeline(*cmd, *m_MultiScatteringPipeline);
            {
                nri::SetDescriptorSetDesc setDesc = {};
                setDesc.setIndex = 0;
                setDesc.descriptorSet = m_MultiScatteringBindingSet->GetHandle();
                setDesc.bindPoint = nri::BindPoint::COMPUTE;
                nri.CmdSetDescriptorSet(*cmd, setDesc);
            }
            nri.CmdDispatch(*cmd, { 4, 4, 1 });

            transitionTexture(m_MultiScatteringLUTTexture, nri::Layout::SHADER_RESOURCE, nri::AccessBits::SHADER_RESOURCE, nri::StageBits::COMPUTE_SHADER);

            m_DirtyAtmosphere = false;
        }

        // Sky-View LUT Pass (Recomputed per frame)
        transitionTexture(m_SkyViewLUTTexture, nri::Layout::SHADER_RESOURCE_STORAGE, nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER);

        nri.CmdSetPipelineLayout(*cmd, nri::BindPoint::COMPUTE, *m_SkyViewPipelineLayout);
        nri.CmdSetPipeline(*cmd, *m_SkyViewPipeline);
        {
            nri::SetDescriptorSetDesc setDesc = {};
            setDesc.setIndex = 0;
            setDesc.descriptorSet = m_SkyViewBindingSet->GetHandle();
            setDesc.bindPoint = nri::BindPoint::COMPUTE;
            nri.CmdSetDescriptorSet(*cmd, setDesc);
        }
        nri.CmdDispatch(*cmd, { 24, 14, 1 });

        transitionTexture(m_SkyViewLUTTexture, nri::Layout::SHADER_RESOURCE, nri::AccessBits::SHADER_RESOURCE, nri::StageBits::FRAGMENT_SHADER | nri::StageBits::COMPUTE_SHADER);
    }
}
