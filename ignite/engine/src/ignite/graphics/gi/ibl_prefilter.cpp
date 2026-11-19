// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "ibl_prefilter.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/profiler/profiler.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/graphics/binding_cache.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"

#include <algorithm>
#include <cstring>

namespace ignite
{
    namespace
    {
        struct EquirectToCubeCB
        {
            uint32_t faceSize = 128;
            int32_t skyType = 0;
            float cameraAltitudeKm = 0.0f;
            float _pad = 0.0f;
        };

        struct DownsampleCubeCB
        {
            uint32_t dstSize = 64;
            uint32_t srcSize = 128;
            uint32_t _pad0 = 0;
            uint32_t _pad1 = 0;
        };

        struct PrefilterGGXCB
        {
            uint32_t dstSize = 128;
            uint32_t srcSize = 128;
            float roughness = 0.0f;
            uint32_t sampleCount = 64;
        };

        struct ProjectSHCB
        {
            uint32_t faceSize = 32;
            uint32_t probeIndex = 0;
            uint32_t convolveOverPi = 1;
            float hanningWindow = 0.0f;
        };

        struct ProbeVisibilityCB
        {
            uint32_t probeLocalIndex = 0;
            uint32_t cubeRes = 32;
            uint32_t octRes = 8;
            uint32_t _pad0 = 0;
            float nearPlane = 0.05f;
            float farPlane = 500.0f;
            float maxDistance = 4.0f;
            float sharpness = 50.0f;
        };
    }

    IBLPrefilter::IBLPrefilter() = default;

    IBLPrefilter::~IBLPrefilter()
    {
        auto *dm = DeviceManager::GetInstance();
        if (dm)
        {
            const auto &nri = dm->GetCoreInterface();
            GPUUploadSync::DeviceWaitIdle(dm->GetDevice());

            if (m_SyncCmdBuffer)
            {
                nri.DestroyCommandBuffer(m_SyncCmdBuffer);
                m_SyncCmdBuffer = nullptr;
            }
            if (m_SyncCmdAllocator)
            {
                nri.DestroyCommandAllocator(m_SyncCmdAllocator);
                m_SyncCmdAllocator = nullptr;
            }
            if (m_SyncFence)
            {
                nri.DestroyFence(m_SyncFence);
                m_SyncFence = nullptr;
            }

            if (m_EquirectPipeline) { nri.DestroyPipeline(m_EquirectPipeline); m_EquirectPipeline = nullptr; }
            if (m_DownsamplePipeline) { nri.DestroyPipeline(m_DownsamplePipeline); m_DownsamplePipeline = nullptr; }
            if (m_PrefilterPipeline) { nri.DestroyPipeline(m_PrefilterPipeline); m_PrefilterPipeline = nullptr; }
            if (m_ProjectSHPipeline) { nri.DestroyPipeline(m_ProjectSHPipeline); m_ProjectSHPipeline = nullptr; }
            if (m_ProbeVisPipeline) { nri.DestroyPipeline(m_ProbeVisPipeline); m_ProbeVisPipeline = nullptr; }

            if (m_EquirectPipelineLayout) { nri.DestroyPipelineLayout(m_EquirectPipelineLayout); m_EquirectPipelineLayout = nullptr; }
            if (m_DownsamplePipelineLayout) { nri.DestroyPipelineLayout(m_DownsamplePipelineLayout); m_DownsamplePipelineLayout = nullptr; }
            if (m_PrefilterPipelineLayout) { nri.DestroyPipelineLayout(m_PrefilterPipelineLayout); m_PrefilterPipelineLayout = nullptr; }
            if (m_ProjectSHPipelineLayout) { nri.DestroyPipelineLayout(m_ProjectSHPipelineLayout); m_ProjectSHPipelineLayout = nullptr; }
            if (m_ProbeVisPipelineLayout) { nri.DestroyPipelineLayout(m_ProbeVisPipelineLayout); m_ProbeVisPipelineLayout = nullptr; }

            if (m_SingleSHGpuUAV) { nri.DestroyDescriptor(m_SingleSHGpuUAV); m_SingleSHGpuUAV = nullptr; }
            if (m_SingleSHGpuBuffer) { nri.DestroyBuffer(m_SingleSHGpuBuffer); m_SingleSHGpuBuffer = nullptr; }
            if (m_SingleSHReadbackBuffer) { nri.DestroyBuffer(m_SingleSHReadbackBuffer); m_SingleSHReadbackBuffer = nullptr; }

            if (m_LinearClampSampler) { nri.DestroyDescriptor(m_LinearClampSampler); m_LinearClampSampler = nullptr; }
        }

        m_EquirectShader.reset();
        m_DownsampleShader.reset();
        m_PrefilterShader.reset();
        m_ProjectSHShader.reset();
        m_ProbeVisShader.reset();
        m_EquirectCB.reset();
        for (auto &cb : m_DownsampleMipCBs) cb.reset();
        for (auto &cb : m_PrefilterMipCBs) cb.reset();
        for (auto &cb : m_ProjectSHCBs) cb.reset();
        for (auto &cb : m_ProbeVisCBs) cb.reset();
    }

    void IBLPrefilter::EnsureInitialized()
    {
        if (m_Initialized)
        {
            return;
        }

        auto *dm = DeviceManager::GetInstance();
        if (!dm || !dm->GetDevice())
        {
            return;
        }
        nri::Device *device = dm->GetDevice();
        const auto &nri = dm->GetCoreInterface();

        // Linear clamp sampler (prevents seam artifacts when sampling cubemap faces / LUTs)
        nri::SamplerDesc samplerDesc = {};
        samplerDesc.filters.min = nri::Filter::LINEAR;
        samplerDesc.filters.mag = nri::Filter::LINEAR;
        samplerDesc.filters.mip = nri::Filter::LINEAR;
        samplerDesc.addressModes.u = nri::AddressMode::CLAMP_TO_EDGE;
        samplerDesc.addressModes.v = nri::AddressMode::CLAMP_TO_EDGE;
        samplerDesc.addressModes.w = nri::AddressMode::CLAMP_TO_EDGE;
        samplerDesc.mipMax = 16.0f;
        nri.CreateSampler(*device, samplerDesc, m_LinearClampSampler);

        m_EquirectCB = ConstantBuffer::Create(sizeof(EquirectToCubeCB), true, 16, "[IBL Equirect CB]");
        for (uint32_t m = 0; m < kMaxMipLevels; ++m)
        {
            m_DownsampleMipCBs[m] = ConstantBuffer::Create(sizeof(DownsampleCubeCB), true, 16, "[IBL Downsample Mip CB]");
            m_PrefilterMipCBs[m] = ConstantBuffer::Create(sizeof(PrefilterGGXCB), true, 16, "[IBL Prefilter Mip CB]");
        }
        for (uint32_t i = 0; i < kMaxProjectSHCBCapacity; ++i)
        {
            m_ProjectSHCBs[i] = ConstantBuffer::Create(sizeof(ProjectSHCB), true, 16, "[IBL ProjectSH CB]");
            m_ProbeVisCBs[i] = ConstantBuffer::Create(sizeof(ProbeVisibilityCB), true, 16, "[IBL ProbeVis CB]");
        }

        m_EquirectShader = Shader::Create("resources/shaders/ibl_equirect_to_cube.compute.hlsl", UMBRA_SHADER_TYPE_COMPUTE, false);
        m_DownsampleShader = Shader::Create("resources/shaders/ibl_downsample_cube.compute.hlsl", UMBRA_SHADER_TYPE_COMPUTE, false);
        m_PrefilterShader = Shader::Create("resources/shaders/ibl_prefilter_ggx.compute.hlsl", UMBRA_SHADER_TYPE_COMPUTE, false);
        m_ProjectSHShader = Shader::Create("resources/shaders/gi_project_sh.compute.hlsl", UMBRA_SHADER_TYPE_COMPUTE, false);
        m_ProbeVisShader = Shader::Create("resources/shaders/gi_probe_visibility.compute.hlsl", UMBRA_SHADER_TYPE_COMPUTE, false);

        // 1. Equirect / SkyViewLUT -> Cube
        {
            m_EquirectLayout.SetVisibility(nri::StageBits::COMPUTE_SHADER);
            m_EquirectLayout.bindings = {
                BindingLayoutItem::VolatileConstantBuffer(0),
                BindingLayoutItem::TextureSRV(0),
                BindingLayoutItem::Sampler(0),
                BindingLayoutItem::Texture_UAV(0)
            };
            m_EquirectPipelineLayout = m_EquirectLayout.CreatePipelineLayout(nri::StageBits::COMPUTE_SHADER);

            nri::ComputePipelineDesc pipeDesc = {};
            pipeDesc.pipelineLayout = m_EquirectPipelineLayout;
            pipeDesc.shader = m_EquirectShader->GetShaderDesc();
            nri.CreateComputePipeline(*device, pipeDesc, m_EquirectPipeline);
        }

        // 2. Downsample Cube Mip
        {
            m_DownsampleLayout.SetVisibility(nri::StageBits::COMPUTE_SHADER);
            m_DownsampleLayout.bindings = {
                BindingLayoutItem::VolatileConstantBuffer(0),
                BindingLayoutItem::TextureSRV(0),
                BindingLayoutItem::Texture_UAV(0)
            };
            m_DownsamplePipelineLayout = m_DownsampleLayout.CreatePipelineLayout(nri::StageBits::COMPUTE_SHADER);

            nri::ComputePipelineDesc pipeDesc = {};
            pipeDesc.pipelineLayout = m_DownsamplePipelineLayout;
            pipeDesc.shader = m_DownsampleShader->GetShaderDesc();
            nri.CreateComputePipeline(*device, pipeDesc, m_DownsamplePipeline);
        }

        // 3. GGX Prefilter
        {
            m_PrefilterLayout.SetVisibility(nri::StageBits::COMPUTE_SHADER);
            m_PrefilterLayout.bindings = {
                BindingLayoutItem::VolatileConstantBuffer(0),
                BindingLayoutItem::TextureSRV(0),
                BindingLayoutItem::Sampler(0),
                BindingLayoutItem::Texture_UAV(0)
            };
            m_PrefilterPipelineLayout = m_PrefilterLayout.CreatePipelineLayout(nri::StageBits::COMPUTE_SHADER);

            nri::ComputePipelineDesc pipeDesc = {};
            pipeDesc.pipelineLayout = m_PrefilterPipelineLayout;
            pipeDesc.shader = m_PrefilterShader->GetShaderDesc();
            nri.CreateComputePipeline(*device, pipeDesc, m_PrefilterPipeline);
        }

        // 4. L2 SH Projection
        {
            m_ProjectSHLayout.SetVisibility(nri::StageBits::COMPUTE_SHADER);
            m_ProjectSHLayout.bindings = {
                BindingLayoutItem::VolatileConstantBuffer(0),
                BindingLayoutItem::TextureSRV(0),
                BindingLayoutItem::StructuredBufferUAV(0)
            };
            m_ProjectSHPipelineLayout = m_ProjectSHLayout.CreatePipelineLayout(nri::StageBits::COMPUTE_SHADER);

            nri::ComputePipelineDesc pipeDesc = {};
            pipeDesc.pipelineLayout = m_ProjectSHPipelineLayout;
            pipeDesc.shader = m_ProjectSHShader->GetShaderDesc();
            nri.CreateComputePipeline(*device, pipeDesc, m_ProjectSHPipeline);
        }

        // 5. Octahedral Probe Visibility Moments & Relocation Stats
        {
            m_ProbeVisLayout.SetVisibility(nri::StageBits::COMPUTE_SHADER);
            m_ProbeVisLayout.bindings = {
                BindingLayoutItem::VolatileConstantBuffer(0),
                BindingLayoutItem::TextureSRV(0),
                BindingLayoutItem::TextureSRV(1),
                BindingLayoutItem::StructuredBufferUAV(0),
                BindingLayoutItem::StructuredBufferUAV(1)
            };
            m_ProbeVisPipelineLayout = m_ProbeVisLayout.CreatePipelineLayout(nri::StageBits::COMPUTE_SHADER);

            nri::ComputePipelineDesc pipeDesc = {};
            pipeDesc.pipelineLayout = m_ProbeVisPipelineLayout;
            pipeDesc.shader = m_ProbeVisShader->GetShaderDesc();
            nri.CreateComputePipeline(*device, pipeDesc, m_ProbeVisPipeline);
        }

        m_Initialized = true;
    }

    void IBLPrefilter::EnsureSyncCommandResources()
    {
        if (m_SyncCmdBuffer && m_SyncFence)
        {
            return;
        }

        auto *dm = DeviceManager::GetInstance();
        if (!dm || !dm->GetDevice() || !dm->GetGraphicsQueue())
        {
            return;
        }

        const auto &nri = dm->GetCoreInterface();
        nri.CreateCommandAllocator(*dm->GetGraphicsQueue(), m_SyncCmdAllocator);
        nri.CreateCommandBuffer(*m_SyncCmdAllocator, m_SyncCmdBuffer);
        nri::FenceDesc fenceDesc = {};
        nri.CreateFence(*dm->GetDevice(), fenceDesc, m_SyncFence);
    }

    void IBLPrefilter::TransitionTextureRange(nri::CommandBuffer *cmd, const Ref<Texture> &tex,
        uint32_t mipOffset, uint32_t mipNum,
        nri::Layout beforeLayout, nri::AccessBits beforeAccess, nri::StageBits beforeStages,
        nri::Layout afterLayout, nri::AccessBits afterAccess, nri::StageBits afterStages)
    {
        if (!cmd || !tex || !tex->GetHandle())
        {
            return;
        }

        const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();
        const bool isDepth = (tex->GetFormat() == nri::Format::D32_SFLOAT || tex->GetFormat() == nri::Format::D24_UNORM_S8_UINT || tex->GetFormat() == nri::Format::D16_UNORM);
        nri::TextureBarrierDesc b = {};
        b.texture = tex->GetHandle();
        b.before = { beforeAccess, beforeLayout, beforeStages };
        b.after = { afterAccess, afterLayout, afterStages };
        b.mipOffset = static_cast<nri::Dim_t>(mipOffset);
        b.mipNum = static_cast<nri::Dim_t>(mipNum);
        b.layerOffset = 0;
        b.layerNum = nri::REMAINING;
        b.planes = isDepth ? nri::PlaneBits::DEPTH : nri::PlaneBits::ALL;

        nri::BarrierDesc barrierDesc = {};
        barrierDesc.textures = &b;
        barrierDesc.textureNum = 1;
        nri.CmdBarrier(*cmd, barrierDesc);
    }

    void IBLPrefilter::ConvertEnv2DToCube(nri::CommandBuffer *cmd, const Ref<Texture> &srcEnv2D, const Ref<Texture> &dstCube,
        int32_t skyType, float cameraAltitudeKm)
    {
        IGN_PROFILE_FUNCTION();

        if (!cmd || !srcEnv2D || !dstCube || !srcEnv2D->GetHandle() || !dstCube->GetHandle())
        {
            return;
        }

        EnsureInitialized();
        if (!m_EquirectPipeline || !m_EquirectPipelineLayout)
        {
            return;
        }

        const uint32_t faceSize = dstCube->GetWidth();
        const uint32_t dstMips = dstCube->GetCreateInfo().mipLevels;

        // Ensure source 2D texture is readable in compute
        if (srcEnv2D->GetCurrentLayout() != nri::Layout::SHADER_RESOURCE)
        {
            TransitionTextureRange(cmd, srcEnv2D, 0, nri::REMAINING,
                srcEnv2D->GetCurrentLayout(), srcEnv2D->GetCurrentAccess(), srcEnv2D->GetCurrentStages(),
                nri::Layout::SHADER_RESOURCE, nri::AccessBits::SHADER_RESOURCE, nri::StageBits::ALL);
            srcEnv2D->SetCurrentState(nri::Layout::SHADER_RESOURCE, nri::AccessBits::SHADER_RESOURCE, nri::StageBits::ALL);
        }

        // Transition ALL mips of dstCube to SHADER_RESOURCE_STORAGE so GenerateCubeMips has consistent state
        TransitionTextureRange(cmd, dstCube, 0, dstMips,
            dstCube->GetCurrentLayout(), dstCube->GetCurrentAccess(), dstCube->GetCurrentStages(),
            nri::Layout::SHADER_RESOURCE_STORAGE, nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER);
        dstCube->SetCurrentState(nri::Layout::SHADER_RESOURCE_STORAGE, nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER);

        EquirectToCubeCB cbData{};
        cbData.faceSize = faceSize;
        cbData.skyType = skyType;
        cbData.cameraAltitudeKm = cameraAltitudeKm;
        m_EquirectCB->SetData(&cbData, sizeof(cbData));

        BindingSetDesc bsDesc;
        bsDesc.bindings = {
            BindingSetItem::ConstantBuffer(0, m_EquirectCB->GetDescriptor()),
            BindingSetItem::TextureSRV(0, srcEnv2D->GetShaderResourceView()),
            BindingSetItem::Sampler(0, m_LinearClampSampler),
            BindingSetItem::Texture_UAV(0, dstCube->GetStorageView(0))
        };
        Ref<BindingSet> bindingSet = BindingCache::GetOrCreateBindingSet(bsDesc, m_EquirectPipelineLayout, 0, "[IBL Equirect->Cube]");
        if (!bindingSet)
        {
            return;
        }

        const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();
        nri.CmdSetPipelineLayout(*cmd, nri::BindPoint::COMPUTE, *m_EquirectPipelineLayout);
        nri.CmdSetPipeline(*cmd, *m_EquirectPipeline);

        nri::SetDescriptorSetDesc setDesc = { 0, bindingSet->GetHandle(), nri::BindPoint::COMPUTE };
        nri.CmdSetDescriptorSet(*cmd, setDesc);

        const uint32_t groups = (faceSize + 7u) / 8u;
        nri.CmdDispatch(*cmd, { groups, groups, 6u });
    }

    void IBLPrefilter::GenerateCubeMips(nri::CommandBuffer *cmd, const Ref<Texture> &cube)
    {
        IGN_PROFILE_FUNCTION();

        if (!cmd || !cube || !cube->GetHandle())
        {
            return;
        }

        EnsureInitialized();
        if (!m_DownsamplePipeline || !m_DownsamplePipelineLayout)
        {
            return;
        }

        const uint32_t mipCount = std::min(cube->GetCreateInfo().mipLevels, kMaxMipLevels);
        const uint32_t baseSize = cube->GetWidth();

        const nri::Layout initLayout = cube->GetCurrentLayout();
        const nri::AccessBits initAccess = cube->GetCurrentAccess();
        const nri::StageBits initStages = cube->GetCurrentStages();

        // Transition mip 0 to SHADER_RESOURCE (establishing compute W->R barrier if previously UAV)
        TransitionTextureRange(cmd, cube, 0, 1,
            initLayout, initAccess, initStages,
            nri::Layout::SHADER_RESOURCE, nri::AccessBits::SHADER_RESOURCE,
            nri::StageBits::COMPUTE_SHADER | nri::StageBits::FRAGMENT_SHADER);

        if (mipCount <= 1)
        {
            cube->SetCurrentState(nri::Layout::SHADER_RESOURCE, nri::AccessBits::SHADER_RESOURCE,
                nri::StageBits::COMPUTE_SHADER | nri::StageBits::FRAGMENT_SHADER);
            return;
        }

        // Transition mips 1..M-1 to SHADER_RESOURCE_STORAGE if not already there
        if (initLayout != nri::Layout::SHADER_RESOURCE_STORAGE)
        {
            TransitionTextureRange(cmd, cube, 1, mipCount - 1,
                initLayout, initAccess, initStages,
                nri::Layout::SHADER_RESOURCE_STORAGE, nri::AccessBits::SHADER_RESOURCE_STORAGE,
                nri::StageBits::COMPUTE_SHADER);
        }

        const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();
        nri.CmdSetPipelineLayout(*cmd, nri::BindPoint::COMPUTE, *m_DownsamplePipelineLayout);
        nri.CmdSetPipeline(*cmd, *m_DownsamplePipeline);

        for (uint32_t m = 1; m < mipCount; ++m)
        {
            const uint32_t srcSize = std::max(1u, baseSize >> (m - 1u));
            const uint32_t dstSize = std::max(1u, baseSize >> m);

            DownsampleCubeCB cbData{};
            cbData.dstSize = dstSize;
            cbData.srcSize = srcSize;
            m_DownsampleMipCBs[m]->SetData(&cbData, sizeof(cbData));

            nri::Descriptor *srcSRV = cube->GetMipShaderResourceView(m - 1u);
            nri::Descriptor *dstUAV = cube->GetStorageView(m);
            if (!srcSRV || !dstUAV)
            {
                break;
            }

            BindingSetDesc bsDesc;
            bsDesc.bindings = {
                BindingSetItem::ConstantBuffer(0, m_DownsampleMipCBs[m]->GetDescriptor()),
                BindingSetItem::TextureSRV(0, srcSRV),
                BindingSetItem::Texture_UAV(0, dstUAV)
            };
            Ref<BindingSet> bindingSet = BindingCache::GetOrCreateBindingSet(bsDesc, m_DownsamplePipelineLayout, 0, "[IBL DownsampleCube]");
            if (!bindingSet)
            {
                break;
            }

            nri::SetDescriptorSetDesc setDesc = { 0, bindingSet->GetHandle(), nri::BindPoint::COMPUTE };
            nri.CmdSetDescriptorSet(*cmd, setDesc);

            const uint32_t groups = (dstSize + 7u) / 8u;
            nri.CmdDispatch(*cmd, { groups, groups, 6u });

            // Transition mip m just written to SHADER_RESOURCE for mip m+1 (and final cube sampling)
            TransitionTextureRange(cmd, cube, m, 1,
                nri::Layout::SHADER_RESOURCE_STORAGE, nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER,
                nri::Layout::SHADER_RESOURCE, nri::AccessBits::SHADER_RESOURCE,
                nri::StageBits::COMPUTE_SHADER | nri::StageBits::FRAGMENT_SHADER);
        }

        cube->SetCurrentState(nri::Layout::SHADER_RESOURCE, nri::AccessBits::SHADER_RESOURCE,
            nri::StageBits::COMPUTE_SHADER | nri::StageBits::FRAGMENT_SHADER);
    }

    void IBLPrefilter::PrefilterGGX(nri::CommandBuffer *cmd, const Ref<Texture> &srcCubeWithMips, const Ref<Texture> &dstPrefilterCube,
        uint32_t sampleCount)
    {
        IGN_PROFILE_FUNCTION();

        if (!cmd || !srcCubeWithMips || !dstPrefilterCube || !srcCubeWithMips->GetHandle() || !dstPrefilterCube->GetHandle())
        {
            return;
        }

        EnsureInitialized();
        if (!m_PrefilterPipeline || !m_PrefilterPipelineLayout)
        {
            return;
        }

        const uint32_t srcSize = srcCubeWithMips->GetWidth();
        const uint32_t baseDstSize = dstPrefilterCube->GetWidth();
        const uint32_t dstMips = std::min(dstPrefilterCube->GetCreateInfo().mipLevels, kMaxMipLevels);

        if (srcCubeWithMips->GetCurrentLayout() != nri::Layout::SHADER_RESOURCE)
        {
            TransitionTextureRange(cmd, srcCubeWithMips, 0, srcCubeWithMips->GetCreateInfo().mipLevels,
                srcCubeWithMips->GetCurrentLayout(), srcCubeWithMips->GetCurrentAccess(), srcCubeWithMips->GetCurrentStages(),
                nri::Layout::SHADER_RESOURCE, nri::AccessBits::SHADER_RESOURCE,
                nri::StageBits::COMPUTE_SHADER | nri::StageBits::FRAGMENT_SHADER);
            srcCubeWithMips->SetCurrentState(nri::Layout::SHADER_RESOURCE, nri::AccessBits::SHADER_RESOURCE,
                nri::StageBits::COMPUTE_SHADER | nri::StageBits::FRAGMENT_SHADER);
        }

        TransitionTextureRange(cmd, dstPrefilterCube, 0, dstMips,
            dstPrefilterCube->GetCurrentLayout(), dstPrefilterCube->GetCurrentAccess(), dstPrefilterCube->GetCurrentStages(),
            nri::Layout::SHADER_RESOURCE_STORAGE, nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER);
        dstPrefilterCube->SetCurrentState(nri::Layout::SHADER_RESOURCE_STORAGE, nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER);

        const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();
        nri.CmdSetPipelineLayout(*cmd, nri::BindPoint::COMPUTE, *m_PrefilterPipelineLayout);
        nri.CmdSetPipeline(*cmd, *m_PrefilterPipeline);

        for (uint32_t m = 0; m < dstMips; ++m)
        {
            const uint32_t dstSize = std::max(1u, baseDstSize >> m);
            const float roughness = (dstMips > 1u) ? static_cast<float>(m) / static_cast<float>(dstMips - 1u) : 0.0f;

            PrefilterGGXCB cbData{};
            cbData.dstSize = dstSize;
            cbData.srcSize = srcSize;
            cbData.roughness = roughness;
            cbData.sampleCount = std::max(sampleCount, 1u);
            m_PrefilterMipCBs[m]->SetData(&cbData, sizeof(cbData));

            nri::Descriptor *dstUAV = dstPrefilterCube->GetStorageView(m);
            if (!dstUAV)
            {
                break;
            }

            BindingSetDesc bsDesc;
            bsDesc.bindings = {
                BindingSetItem::ConstantBuffer(0, m_PrefilterMipCBs[m]->GetDescriptor()),
                BindingSetItem::TextureSRV(0, srcCubeWithMips->GetShaderResourceView()),
                BindingSetItem::Sampler(0, m_LinearClampSampler),
                BindingSetItem::Texture_UAV(0, dstUAV)
            };
            Ref<BindingSet> bindingSet = BindingCache::GetOrCreateBindingSet(bsDesc, m_PrefilterPipelineLayout, 0, "[IBL PrefilterGGX]");
            if (!bindingSet)
            {
                break;
            }

            nri::SetDescriptorSetDesc setDesc = { 0, bindingSet->GetHandle(), nri::BindPoint::COMPUTE };
            nri.CmdSetDescriptorSet(*cmd, setDesc);

            const uint32_t groups = (dstSize + 7u) / 8u;
            nri.CmdDispatch(*cmd, { groups, groups, 6u });
        }

        TransitionTextureRange(cmd, dstPrefilterCube, 0, dstMips,
            nri::Layout::SHADER_RESOURCE_STORAGE, nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER,
            nri::Layout::SHADER_RESOURCE, nri::AccessBits::SHADER_RESOURCE, nri::StageBits::ALL);
        dstPrefilterCube->SetCurrentState(nri::Layout::SHADER_RESOURCE, nri::AccessBits::SHADER_RESOURCE, nri::StageBits::ALL);
    }

    bool IBLPrefilter::PrefilterCubeSync(const Ref<Texture> &srcCube, const Ref<Texture> &dstPrefilterCube, uint32_t sampleCount)
    {
        IGN_PROFILE_FUNCTION();

        auto *dm = DeviceManager::GetInstance();
        if (!dm || !dm->GetDevice() || !dm->GetGraphicsQueue() || !srcCube || !dstPrefilterCube)
        {
            return false;
        }

        EnsureInitialized();
        EnsureSyncCommandResources();
        if (!m_SyncCmdAllocator || !m_SyncCmdBuffer || !m_SyncFence)
        {
            return false;
        }

        const auto &nri = dm->GetCoreInterface();
        nri.ResetCommandAllocator(*m_SyncCmdAllocator);
        nri.BeginCommandBuffer(*m_SyncCmdBuffer, nullptr);

        GenerateCubeMips(m_SyncCmdBuffer, srcCube);
        PrefilterGGX(m_SyncCmdBuffer, srcCube, dstPrefilterCube, sampleCount);

        nri.EndCommandBuffer(*m_SyncCmdBuffer);

        {
            std::lock_guard<std::mutex> lock(GPUUploadSync::GetQueueMutex());
            nri::FenceSubmitDesc signalDesc = {};
            signalDesc.fence = m_SyncFence;
            signalDesc.value = m_SyncFenceValue;

            nri::QueueSubmitDesc submitDesc = {};
            submitDesc.commandBuffers = &m_SyncCmdBuffer;
            submitDesc.commandBufferNum = 1;
            submitDesc.signalFences = &signalDesc;
            submitDesc.signalFenceNum = 1;
            nri.QueueSubmit(*dm->GetGraphicsQueue(), submitDesc);
            nri.Wait(*m_SyncFence, m_SyncFenceValue);
            ++m_SyncFenceValue;
        }

        return true;
    }

    bool IBLPrefilter::PrefilterCubeFromMip0Sync(const Ref<Texture> &srcMip0Cube, const Ref<Texture> &dstPrefilterCube, uint32_t sampleCount)
    {
        IGN_PROFILE_FUNCTION();

        auto *dm = DeviceManager::GetInstance();
        if (!dm || !dm->GetDevice() || !dm->GetGraphicsQueue() || !srcMip0Cube || !srcMip0Cube->GetHandle()
            || !dstPrefilterCube || !dstPrefilterCube->GetHandle())
        {
            return false;
        }

        const uint32_t baseSize = static_cast<uint32_t>(std::max(1, srcMip0Cube->GetWidth()));
        const uint32_t mipCount = std::max(1u, Texture::CalculateMaxMipLevels(baseSize, baseSize, 1));

        TextureCreateInfo mippedInfo;
        mippedInfo.width = baseSize;
        mippedInfo.height = baseSize;
        mippedInfo.depth = 1;
        mippedInfo.mipLevels = mipCount;
        mippedInfo.arraySize = 6;
        mippedInfo.sampleCount = 1;
        mippedInfo.dimension = nri::TextureType::TEXTURE_2D;
        mippedInfo.format = nri::Format::RGBA16_SFLOAT;
        mippedInfo.isCubeMap = true;
        mippedInfo.isRenderTarget = false;
        mippedInfo.isUAV = true;
        mippedInfo.bindless = false;

        Ref<Texture> mippedSrcCube = Texture::Create(mippedInfo, "[IBL MippedCaptureStaging]");
        if (!mippedSrcCube || !mippedSrcCube->GetHandle())
        {
            return false;
        }

        EnsureInitialized();
        EnsureSyncCommandResources();
        if (!m_SyncCmdAllocator || !m_SyncCmdBuffer || !m_SyncFence)
        {
            return false;
        }

        const auto &nri = dm->GetCoreInterface();
        nri.ResetCommandAllocator(*m_SyncCmdAllocator);
        nri.BeginCommandBuffer(*m_SyncCmdBuffer, nullptr);

        const nri::Layout prevSrcLayout = (srcMip0Cube->GetCurrentLayout() == nri::Layout::UNDEFINED)
            ? nri::Layout::SHADER_RESOURCE
            : srcMip0Cube->GetCurrentLayout();
        const nri::AccessBits prevSrcAccess = (srcMip0Cube->GetCurrentLayout() == nri::Layout::UNDEFINED)
            ? nri::AccessBits::SHADER_RESOURCE
            : srcMip0Cube->GetCurrentAccess();
        const nri::StageBits prevSrcStages = (srcMip0Cube->GetCurrentLayout() == nri::Layout::UNDEFINED)
            ? nri::StageBits::ALL
            : srcMip0Cube->GetCurrentStages();

        {
            nri::TextureBarrierDesc preBarriers[2] = {};
            preBarriers[0].texture = srcMip0Cube->GetHandle();
            preBarriers[0].before = (srcMip0Cube->GetCurrentLayout() == nri::Layout::UNDEFINED)
                ? nri::AccessLayoutStage{ nri::AccessBits::NONE, nri::Layout::UNDEFINED, nri::StageBits::NONE }
                : nri::AccessLayoutStage{ srcMip0Cube->GetCurrentAccess(), srcMip0Cube->GetCurrentLayout(), srcMip0Cube->GetCurrentStages() };
            preBarriers[0].after = { nri::AccessBits::COPY_SOURCE, nri::Layout::COPY_SOURCE, nri::StageBits::COPY };
            preBarriers[0].mipOffset = 0;
            preBarriers[0].mipNum = nri::REMAINING;
            preBarriers[0].layerOffset = 0;
            preBarriers[0].layerNum = nri::REMAINING;
            preBarriers[0].planes = nri::PlaneBits::ALL;

            preBarriers[1].texture = mippedSrcCube->GetHandle();
            preBarriers[1].before = { nri::AccessBits::NONE, nri::Layout::UNDEFINED, nri::StageBits::NONE };
            preBarriers[1].after = { nri::AccessBits::COPY_DESTINATION, nri::Layout::COPY_DESTINATION, nri::StageBits::COPY };
            preBarriers[1].mipOffset = 0;
            preBarriers[1].mipNum = nri::REMAINING;
            preBarriers[1].layerOffset = 0;
            preBarriers[1].layerNum = nri::REMAINING;
            preBarriers[1].planes = nri::PlaneBits::ALL;

            nri::BarrierDesc bDesc = {};
            bDesc.textures = preBarriers;
            bDesc.textureNum = 2;
            nri.CmdBarrier(*m_SyncCmdBuffer, bDesc);
        }

        for (uint32_t face = 0; face < 6u; ++face)
        {
            nri::TextureRegionDesc region = {};
            region.width = static_cast<nri::Dim_t>(baseSize);
            region.height = static_cast<nri::Dim_t>(baseSize);
            region.depth = 1;
            region.mipOffset = 0;
            region.layerOffset = static_cast<nri::Dim_t>(face);
            region.planes = nri::PlaneBits::ALL;
            nri.CmdCopyTexture(*m_SyncCmdBuffer, *mippedSrcCube->GetHandle(), &region, *srcMip0Cube->GetHandle(), &region);
        }

        {
            nri::TextureBarrierDesc postBarriers[2] = {};
            postBarriers[0].texture = srcMip0Cube->GetHandle();
            postBarriers[0].before = { nri::AccessBits::COPY_SOURCE, nri::Layout::COPY_SOURCE, nri::StageBits::COPY };
            postBarriers[0].after = { prevSrcAccess, prevSrcLayout, prevSrcStages };
            postBarriers[0].mipOffset = 0;
            postBarriers[0].mipNum = nri::REMAINING;
            postBarriers[0].layerOffset = 0;
            postBarriers[0].layerNum = nri::REMAINING;
            postBarriers[0].planes = nri::PlaneBits::ALL;

            postBarriers[1].texture = mippedSrcCube->GetHandle();
            postBarriers[1].before = { nri::AccessBits::COPY_DESTINATION, nri::Layout::COPY_DESTINATION, nri::StageBits::COPY };
            postBarriers[1].after = { nri::AccessBits::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE, nri::StageBits::COMPUTE_SHADER | nri::StageBits::FRAGMENT_SHADER };
            postBarriers[1].mipOffset = 0;
            postBarriers[1].mipNum = nri::REMAINING;
            postBarriers[1].layerOffset = 0;
            postBarriers[1].layerNum = nri::REMAINING;
            postBarriers[1].planes = nri::PlaneBits::ALL;

            nri::BarrierDesc bDesc = {};
            bDesc.textures = postBarriers;
            bDesc.textureNum = 2;
            nri.CmdBarrier(*m_SyncCmdBuffer, bDesc);
        }

        srcMip0Cube->SetCurrentState(prevSrcLayout, prevSrcAccess, prevSrcStages);
        mippedSrcCube->SetCurrentState(nri::Layout::SHADER_RESOURCE, nri::AccessBits::SHADER_RESOURCE,
            nri::StageBits::COMPUTE_SHADER | nri::StageBits::FRAGMENT_SHADER);

        GenerateCubeMips(m_SyncCmdBuffer, mippedSrcCube);
        PrefilterGGX(m_SyncCmdBuffer, mippedSrcCube, dstPrefilterCube, sampleCount);

        nri.EndCommandBuffer(*m_SyncCmdBuffer);

        {
            std::lock_guard<std::mutex> lock(GPUUploadSync::GetQueueMutex());
            nri::FenceSubmitDesc signalDesc = {};
            signalDesc.fence = m_SyncFence;
            signalDesc.value = m_SyncFenceValue;

            nri::QueueSubmitDesc submitDesc = {};
            submitDesc.commandBuffers = &m_SyncCmdBuffer;
            submitDesc.commandBufferNum = 1;
            submitDesc.signalFences = &signalDesc;
            submitDesc.signalFenceNum = 1;
            nri.QueueSubmit(*dm->GetGraphicsQueue(), submitDesc);
            nri.Wait(*m_SyncFence, m_SyncFenceValue);
            ++m_SyncFenceValue;
        }

        return true;
    }

    void IBLPrefilter::EnsureSingleSHBuffers()
    {
        if (m_SingleSHGpuBuffer && m_SingleSHReadbackBuffer && m_SingleSHGpuUAV)
        {
            return;
        }

        auto *dm = DeviceManager::GetInstance();
        if (!dm || !dm->GetDevice())
        {
            return;
        }

        const auto &nri = dm->GetCoreInterface();
        nri::Device *device = dm->GetDevice();

        nri::BufferDesc gpuDesc = {};
        gpuDesc.size = sizeof(ProbeSHOutputGPU);
        gpuDesc.structureStride = sizeof(ProbeSHOutputGPU);
        gpuDesc.usage = nri::BufferUsageBits::SHADER_RESOURCE_STORAGE;
        nri.CreateCommittedBuffer(*device, nri::MemoryLocation::DEVICE, 1.0f, gpuDesc, m_SingleSHGpuBuffer);

        nri::BufferViewDesc viewDesc = {};
        viewDesc.buffer = m_SingleSHGpuBuffer;
        viewDesc.type = nri::BufferView::STORAGE_STRUCTURED_BUFFER;
        viewDesc.offset = 0;
        viewDesc.size = sizeof(ProbeSHOutputGPU);
        viewDesc.structureStride = sizeof(ProbeSHOutputGPU);
        nri.CreateBufferView(viewDesc, m_SingleSHGpuUAV);

        nri::BufferDesc rbDesc = {};
        rbDesc.size = sizeof(ProbeSHOutputGPU);
        rbDesc.usage = nri::BufferUsageBits::NONE;
        nri.CreateCommittedBuffer(*device, nri::MemoryLocation::HOST_READBACK, 1.0f, rbDesc, m_SingleSHReadbackBuffer);
    }

    void IBLPrefilter::ProjectCubeToSH(nri::CommandBuffer *cmd, const Ref<Texture> &srcCube, nri::Descriptor *outSHBufferUAV,
        uint32_t probeIndex, bool convolveOverPi, float hanningWindow)
    {
        IGN_PROFILE_FUNCTION();

        if (!cmd || !srcCube || !srcCube->GetHandle() || !outSHBufferUAV)
        {
            return;
        }

        EnsureInitialized();
        if (!m_ProjectSHPipeline || !m_ProjectSHPipelineLayout)
        {
            return;
        }

        if (srcCube->GetCurrentLayout() != nri::Layout::SHADER_RESOURCE)
        {
            TransitionTextureRange(cmd, srcCube, 0, srcCube->GetCreateInfo().mipLevels,
                srcCube->GetCurrentLayout(), srcCube->GetCurrentAccess(), srcCube->GetCurrentStages(),
                nri::Layout::SHADER_RESOURCE, nri::AccessBits::SHADER_RESOURCE,
                nri::StageBits::COMPUTE_SHADER | nri::StageBits::FRAGMENT_SHADER);
            srcCube->SetCurrentState(nri::Layout::SHADER_RESOURCE, nri::AccessBits::SHADER_RESOURCE,
                nri::StageBits::COMPUTE_SHADER | nri::StageBits::FRAGMENT_SHADER);
        }

        nri::Descriptor *cubeArraySRV = srcCube->GetArrayShaderResourceView();
        if (!cubeArraySRV)
        {
            return;
        }

        Ref<ConstantBuffer> &cb = m_ProjectSHCBs[m_ProjectSHCBCursor];
        m_ProjectSHCBCursor = (m_ProjectSHCBCursor + 1u) % kMaxProjectSHCBCapacity;

        ProjectSHCB cbData{};
        cbData.faceSize = srcCube->GetWidth();
        cbData.probeIndex = probeIndex;
        cbData.convolveOverPi = convolveOverPi ? 1u : 0u;
        cbData.hanningWindow = hanningWindow;
        cb->SetData(&cbData, sizeof(cbData));

        BindingSetDesc bsDesc;
        bsDesc.bindings = {
            BindingSetItem::ConstantBuffer(0, cb->GetDescriptor()),
            BindingSetItem::TextureSRV(0, cubeArraySRV),
            BindingSetItem::StructuredBufferUAV(0, outSHBufferUAV)
        };
        Ref<BindingSet> bindingSet = BindingCache::GetOrCreateBindingSet(bsDesc, m_ProjectSHPipelineLayout, 0, "[IBL ProjectSH]");
        if (!bindingSet)
        {
            return;
        }

        const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();
        nri.CmdSetPipelineLayout(*cmd, nri::BindPoint::COMPUTE, *m_ProjectSHPipelineLayout);
        nri.CmdSetPipeline(*cmd, *m_ProjectSHPipeline);

        nri::SetDescriptorSetDesc setDesc = { 0, bindingSet->GetHandle(), nri::BindPoint::COMPUTE };
        nri.CmdSetDescriptorSet(*cmd, setDesc);
        nri.CmdDispatch(*cmd, { 1u, 1u, 1u });
    }

    bool IBLPrefilter::ProjectCubeToSHSync(const Ref<Texture> &srcCube, gi::SHL2 &outSH, float *outBackfaceFraction,
        bool convolveOverPi, float hanningWindow)
    {
        IGN_PROFILE_FUNCTION();

        auto *dm = DeviceManager::GetInstance();
        if (!dm || !dm->GetDevice() || !dm->GetGraphicsQueue() || !srcCube)
        {
            return false;
        }

        EnsureInitialized();
        EnsureSyncCommandResources();
        EnsureSingleSHBuffers();
        if (!m_SyncCmdAllocator || !m_SyncCmdBuffer || !m_SyncFence || !m_SingleSHGpuBuffer || !m_SingleSHReadbackBuffer || !m_SingleSHGpuUAV)
        {
            return false;
        }

        const auto &nri = dm->GetCoreInterface();
        nri.ResetCommandAllocator(*m_SyncCmdAllocator);
        nri.BeginCommandBuffer(*m_SyncCmdBuffer, nullptr);

        if (m_SingleSHGpuWritten)
        {
            nri::BufferBarrierDesc preBarrier = {};
            preBarrier.buffer = m_SingleSHGpuBuffer;
            preBarrier.before = { nri::AccessBits::COPY_SOURCE, nri::StageBits::COPY };
            preBarrier.after = { nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER };

            nri::BarrierDesc bDesc = {};
            bDesc.buffers = &preBarrier;
            bDesc.bufferNum = 1;
            nri.CmdBarrier(*m_SyncCmdBuffer, bDesc);
        }

        ProjectCubeToSH(m_SyncCmdBuffer, srcCube, m_SingleSHGpuUAV, 0u, convolveOverPi, hanningWindow);

        {
            nri::BufferBarrierDesc postBarrier = {};
            postBarrier.buffer = m_SingleSHGpuBuffer;
            postBarrier.before = { nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER };
            postBarrier.after = { nri::AccessBits::COPY_SOURCE, nri::StageBits::COPY };

            nri::BarrierDesc bDesc = {};
            bDesc.buffers = &postBarrier;
            bDesc.bufferNum = 1;
            nri.CmdBarrier(*m_SyncCmdBuffer, bDesc);
        }

        nri.CmdCopyBuffer(*m_SyncCmdBuffer, *m_SingleSHReadbackBuffer, 0, *m_SingleSHGpuBuffer, 0, sizeof(ProbeSHOutputGPU));
        m_SingleSHGpuWritten = true;

        nri.EndCommandBuffer(*m_SyncCmdBuffer);

        {
            std::lock_guard<std::mutex> lock(GPUUploadSync::GetQueueMutex());
            nri::FenceSubmitDesc signalDesc = { m_SyncFence, m_SyncFenceValue };
            nri::QueueSubmitDesc submitDesc = {};
            submitDesc.commandBuffers = &m_SyncCmdBuffer;
            submitDesc.commandBufferNum = 1;
            submitDesc.signalFences = &signalDesc;
            submitDesc.signalFenceNum = 1;
            nri.QueueSubmit(*dm->GetGraphicsQueue(), submitDesc);
            nri.Wait(*m_SyncFence, m_SyncFenceValue);
            ++m_SyncFenceValue;
        }

        const void *mapped = nri.MapBuffer(*m_SingleSHReadbackBuffer, 0, sizeof(ProbeSHOutputGPU));
        if (!mapped)
        {
            return false;
        }

        ProbeSHOutputGPU gpuOut{};
        std::memcpy(&gpuOut, mapped, sizeof(ProbeSHOutputGPU));
        nri.UnmapBuffer(*m_SingleSHReadbackBuffer);

        for (int i = 0; i < 9; ++i)
        {
            outSH.c[i] = glm::vec3(gpuOut.coeff[i]);
        }
        if (outBackfaceFraction)
        {
            *outBackfaceFraction = gpuOut.backfaceFraction;
        }
        return true;
    }

    void IBLPrefilter::ComputeProbeVisibility(nri::CommandBuffer *cmd, const Ref<Texture> &colorCube, const Ref<Texture> &depthCube,
        nri::Descriptor *outStatsUAV, nri::Descriptor *outOctTilesUAV, uint32_t probeLocalIndex,
        float nearPlane, float farPlane, float maxDistance, float sharpness)
    {
        IGN_PROFILE_FUNCTION();

        if (!cmd || !colorCube || !depthCube || !colorCube->GetHandle() || !depthCube->GetHandle() || !outStatsUAV || !outOctTilesUAV)
        {
            return;
        }

        EnsureInitialized();
        if (!m_ProbeVisPipeline || !m_ProbeVisPipelineLayout)
        {
            return;
        }

        if (colorCube->GetCurrentLayout() != nri::Layout::SHADER_RESOURCE)
        {
            TransitionTextureRange(cmd, colorCube, 0, colorCube->GetCreateInfo().mipLevels,
                colorCube->GetCurrentLayout(), colorCube->GetCurrentAccess(), colorCube->GetCurrentStages(),
                nri::Layout::SHADER_RESOURCE, nri::AccessBits::SHADER_RESOURCE,
                nri::StageBits::COMPUTE_SHADER | nri::StageBits::FRAGMENT_SHADER);
            colorCube->SetCurrentState(nri::Layout::SHADER_RESOURCE, nri::AccessBits::SHADER_RESOURCE,
                nri::StageBits::COMPUTE_SHADER | nri::StageBits::FRAGMENT_SHADER);
        }

        if (depthCube->GetCurrentLayout() != nri::Layout::SHADER_RESOURCE)
        {
            TransitionTextureRange(cmd, depthCube, 0, depthCube->GetCreateInfo().mipLevels,
                depthCube->GetCurrentLayout(), depthCube->GetCurrentAccess(), depthCube->GetCurrentStages(),
                nri::Layout::SHADER_RESOURCE, nri::AccessBits::SHADER_RESOURCE,
                nri::StageBits::COMPUTE_SHADER | nri::StageBits::FRAGMENT_SHADER);
            depthCube->SetCurrentState(nri::Layout::SHADER_RESOURCE, nri::AccessBits::SHADER_RESOURCE,
                nri::StageBits::COMPUTE_SHADER | nri::StageBits::FRAGMENT_SHADER);
        }

        nri::Descriptor *colorArraySRV = colorCube->GetArrayShaderResourceView();
        nri::Descriptor *depthArraySRV = depthCube->GetArrayShaderResourceView();
        if (!colorArraySRV || !depthArraySRV)
        {
            return;
        }

        Ref<ConstantBuffer> &cb = m_ProbeVisCBs[m_ProbeVisCBCursor];
        m_ProbeVisCBCursor = (m_ProbeVisCBCursor + 1u) % kMaxProjectSHCBCapacity;

        ProbeVisibilityCB cbData{};
        cbData.probeLocalIndex = probeLocalIndex;
        cbData.cubeRes = static_cast<uint32_t>(std::max(1, colorCube->GetWidth()));
        cbData.octRes = 8;
        cbData.nearPlane = std::max(nearPlane, 1e-4f);
        cbData.farPlane = std::max(farPlane, nearPlane + 1.0f);
        cbData.maxDistance = std::max(maxDistance, 0.25f);
        cbData.sharpness = std::max(sharpness, 1.0f);
        cb->SetData(&cbData, sizeof(cbData));

        BindingSetDesc bsDesc;
        bsDesc.bindings = {
            BindingSetItem::ConstantBuffer(0, cb->GetDescriptor()),
            BindingSetItem::TextureSRV(0, colorArraySRV),
            BindingSetItem::TextureSRV(1, depthArraySRV),
            BindingSetItem::StructuredBufferUAV(0, outStatsUAV),
            BindingSetItem::StructuredBufferUAV(1, outOctTilesUAV)
        };
        Ref<BindingSet> bindingSet = BindingCache::GetOrCreateBindingSet(bsDesc, m_ProbeVisPipelineLayout, 0, "[IBL ProbeVisibility]");
        if (!bindingSet)
        {
            return;
        }

        const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();
        nri.CmdSetPipelineLayout(*cmd, nri::BindPoint::COMPUTE, *m_ProbeVisPipelineLayout);
        nri.CmdSetPipeline(*cmd, *m_ProbeVisPipeline);

        nri::SetDescriptorSetDesc setDesc = { 0, bindingSet->GetHandle(), nri::BindPoint::COMPUTE };
        nri.CmdSetDescriptorSet(*cmd, setDesc);
        nri.CmdDispatch(*cmd, { 1u, 1u, 1u });
    }
}

