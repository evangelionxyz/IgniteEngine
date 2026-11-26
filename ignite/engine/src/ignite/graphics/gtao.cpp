// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "gtao.hpp"
#include "bindless_system.hpp"
#include "binding_cache.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/profiler/profiler.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/shader.hpp"
#include "ignite/graphics/texture.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace ignite
{
    namespace gi
    {
        glm::vec3 MultiBounceAO(float ao, const glm::vec3 &albedo)
        {
            const float clampedAO = std::clamp(ao, 0.0f, 1.0f);
            const glm::vec3 clampedAlbedo = glm::clamp(albedo, glm::vec3(0.0f), glm::vec3(1.0f));
            const glm::vec3 a = 2.0404f * clampedAlbedo - 0.3324f;
            const glm::vec3 b = -4.7951f * clampedAlbedo + 0.6417f;
            const glm::vec3 c = 2.7552f * clampedAlbedo + 0.6903f;
            const glm::vec3 x(clampedAO);
            const glm::vec3 poly = ((x * a + b) * x + c) * x;
            return glm::clamp(glm::max(x, poly), glm::vec3(0.0f), glm::vec3(1.0f));
        }
    }

    namespace
    {
        nri::PlaneBits GetPlanesForFormat(nri::Format fmt)
        {
            if (fmt == nri::Format::D16_UNORM || fmt == nri::Format::D32_SFLOAT)
                return nri::PlaneBits::DEPTH;
            if (fmt == nri::Format::D24_UNORM_S8_UINT || fmt == nri::Format::D32_SFLOAT_S8_UINT)
                return nri::PlaneBits::DEPTH | nri::PlaneBits::STENCIL;
            return nri::PlaneBits::ALL;
        }

        void TransitionInputToSRV(nri::CommandBuffer *cmd, const Ref<Texture> &texture)
        {
            if (!cmd || !texture || !texture->GetHandle())
                return;

            const nri::Layout beforeLayout = texture->GetCurrentLayout();
            const nri::AccessBits beforeAccess = texture->GetCurrentAccess();
            const nri::StageBits beforeStages = texture->GetCurrentStages();
            const bool readableByCompute = beforeLayout == nri::Layout::SHADER_RESOURCE
                && (beforeAccess & nri::AccessBits::SHADER_RESOURCE) != 0
                && (beforeStages & nri::StageBits::COMPUTE_SHADER) != 0;
            if (readableByCompute)
                return;

            nri::TextureBarrierDesc barrier = {};
            barrier.texture = texture->GetHandle();
            if (beforeLayout == nri::Layout::UNDEFINED)
            {
                barrier.before = { nri::AccessBits::NONE, nri::Layout::UNDEFINED, nri::StageBits::NONE };
            }
            else
            {
                barrier.before = { beforeAccess, beforeLayout, beforeStages };
            }
            barrier.after = { nri::AccessBits::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE,
                nri::StageBits::COMPUTE_SHADER | nri::StageBits::FRAGMENT_SHADER };
            barrier.mipOffset = 0;
            barrier.mipNum = nri::REMAINING;
            barrier.layerOffset = 0;
            barrier.layerNum = nri::REMAINING;
            barrier.planes = GetPlanesForFormat(texture->GetFormat());

            nri::BarrierDesc bd = {};
            bd.textures = &barrier;
            bd.textureNum = 1;
            DeviceManager::GetInstance()->GetCoreInterface().CmdBarrier(*cmd, bd);
            texture->SetCurrentState(barrier.after.layout, barrier.after.access, barrier.after.stages);
        }
    }

    GTAO::GTAO(uint32_t fullWidth, uint32_t fullHeight)
    {
        auto *dm = DeviceManager::GetInstance();
        nri::Device *device = dm ? dm->GetDevice() : nullptr;
        if (device)
        {
            const auto &nri = dm->GetCoreInterface();
            nri::SamplerDesc samplerDesc = {};
            samplerDesc.filters = { nri::Filter::NEAREST, nri::Filter::NEAREST, nri::Filter::NEAREST };
            samplerDesc.addressModes = { nri::AddressMode::CLAMP_TO_EDGE, nri::AddressMode::CLAMP_TO_EDGE, nri::AddressMode::CLAMP_TO_EDGE };
            samplerDesc.mipMax = 16.0f;
            nri.CreateSampler(*device, samplerDesc, m_PointClampSampler);
        }

        m_PrefilterLayout.SetVisibility(nri::StageBits::COMPUTE_SHADER)
            .AddItem(BindingLayoutItem::VolatileConstantBuffer(0))
            .AddItem(BindingLayoutItem::TextureSRV(0))
            .AddItem(BindingLayoutItem::TextureUAV(0));

        m_MainLayout.SetVisibility(nri::StageBits::COMPUTE_SHADER)
            .AddItem(BindingLayoutItem::VolatileConstantBuffer(0))
            .AddItem(BindingLayoutItem::TextureSRV(0))
            .AddItem(BindingLayoutItem::TextureSRV(1))
            .AddItem(BindingLayoutItem::TextureUAV(0))
            .AddItem(BindingLayoutItem::Sampler(0));

        m_DenoiseLayout.SetVisibility(nri::StageBits::COMPUTE_SHADER)
            .AddItem(BindingLayoutItem::VolatileConstantBuffer(0))
            .AddItem(BindingLayoutItem::TextureSRV(0))
            .AddItem(BindingLayoutItem::TextureSRV(1))
            .AddItem(BindingLayoutItem::TextureSRV(2))
            .AddItem(BindingLayoutItem::TextureSRV(3))
            .AddItem(BindingLayoutItem::TextureUAV(0));

        for (uint32_t m = 0; m < kMaxDepthMips; ++m)
        {
            m_PrefilterCBs[m] = ConstantBuffer::Create(sizeof(PrefilterCB), true, 1, "GTAO Prefilter CB " + std::to_string(m));
        }
        m_MainCB = ConstantBuffer::Create(sizeof(MainCB), true, 1, "GTAO Main CB");
        m_DenoiseHCB = ConstantBuffer::Create(sizeof(DenoiseCB), true, 1, "GTAO Denoise H CB");
        m_DenoiseVCB = ConstantBuffer::Create(sizeof(DenoiseCB), true, 1, "GTAO Denoise V CB");
        m_DenoiseUpsampleCB = ConstantBuffer::Create(sizeof(DenoiseCB), true, 1, "GTAO Upsample CB");

        m_PrefilterShader = Shader::Create("resources/shaders/gtao_prefilter_depth.compute.hlsl", UMBRA_SHADER_TYPE_COMPUTE, true);
        m_MainShader = Shader::Create("resources/shaders/gtao_main.compute.hlsl", UMBRA_SHADER_TYPE_COMPUTE, true);
        m_DenoiseShader = Shader::Create("resources/shaders/gtao_denoise.compute.hlsl", UMBRA_SHADER_TYPE_COMPUTE, true);

        CreateTextures(fullWidth, fullHeight);
    }

    GTAO::~GTAO()
    {
        auto *dm = DeviceManager::GetInstance();
        if (dm && dm->GetDevice())
        {
            GPUUploadSync::DeviceWaitIdle(dm->GetDevice());
        }

        InvalidatePipelines();
        m_PrefilteredDepth.reset();
        m_AOTexA.reset();
        m_AOTexHalfB.reset();
        m_AOTexB.reset();

        if (dm && dm->GetDevice() && m_PointClampSampler)
        {
            dm->GetCoreInterface().DestroyDescriptor(m_PointClampSampler);
            m_PointClampSampler = nullptr;
        }
    }

    void GTAO::CreateTextures(uint32_t fullWidth, uint32_t fullHeight)
    {
        const bool hadTextures = (m_PrefilteredDepth || m_AOTexA || m_AOTexHalfB || m_AOTexB);
        if (hadTextures)
        {
            if (auto *dm = DeviceManager::GetInstance(); dm && dm->GetDevice())
            {
                GPUUploadSync::DeviceWaitIdle(dm->GetDevice());
            }
            m_PrefilteredDepth.reset();
            m_AOTexA.reset();
            m_AOTexHalfB.reset();
            m_AOTexB.reset();
            BindingCache::Clear();
        }

        m_FullWidth = std::max(1u, fullWidth);
        m_FullHeight = std::max(1u, fullHeight);
        m_HalfWidth = std::max(1u, (m_FullWidth + 1u) / 2u);
        m_HalfHeight = std::max(1u, (m_FullHeight + 1u) / 2u);
        m_DepthMipCount = std::min(kMaxDepthMips, std::max(1u, Texture::CalculateMaxMipLevels(m_HalfWidth, m_HalfHeight, 1)));

        TextureCreateInfo depthInfo;
        depthInfo.width = m_HalfWidth;
        depthInfo.height = m_HalfHeight;
        depthInfo.depth = 1;
        depthInfo.mipLevels = m_DepthMipCount;
        depthInfo.arraySize = 1;
        depthInfo.sampleCount = 1;
        depthInfo.dimension = nri::TextureType::TEXTURE_2D;
        depthInfo.format = nri::Format::R32_SFLOAT;
        depthInfo.isUAV = true;
        depthInfo.isRenderTarget = false;
        depthInfo.bindless = false;
        depthInfo.samplerLinearFiltering = false;
        depthInfo.samplerAddressU = nri::AddressMode::CLAMP_TO_EDGE;
        depthInfo.samplerAddressV = nri::AddressMode::CLAMP_TO_EDGE;
        m_PrefilteredDepth = Texture::Create(depthInfo, "[GTAO PrefilteredDepth]");

        TextureCreateInfo aoTempInfo;
        aoTempInfo.width = m_HalfWidth;
        aoTempInfo.height = m_HalfHeight;
        aoTempInfo.depth = 1;
        aoTempInfo.mipLevels = 1;
        aoTempInfo.arraySize = 1;
        aoTempInfo.sampleCount = 1;
        aoTempInfo.dimension = nri::TextureType::TEXTURE_2D;
        aoTempInfo.format = nri::Format::R8_UNORM;
        aoTempInfo.isUAV = true;
        aoTempInfo.isRenderTarget = false;
        aoTempInfo.bindless = false;
        aoTempInfo.samplerLinearFiltering = true;
        aoTempInfo.samplerAddressU = nri::AddressMode::CLAMP_TO_EDGE;
        aoTempInfo.samplerAddressV = nri::AddressMode::CLAMP_TO_EDGE;
        m_AOTexA = Texture::Create(aoTempInfo, "[GTAO Scratch A]");
        m_AOTexHalfB = Texture::Create(aoTempInfo, "[GTAO Scratch HalfB]");

        TextureCreateInfo aoOutInfo = aoTempInfo;
        aoOutInfo.width = m_FullWidth;
        aoOutInfo.height = m_FullHeight;
        aoOutInfo.bindless = true;
        aoOutInfo.forceBindless = true;
        m_AOTexB = Texture::Create(aoOutInfo, "[GTAO Final]");

        if (BindlessSystem::HasPendingWrites())
        {
            BindlessSystem::FlushPendingWrites();
        }
    }

    void GTAO::Resize(uint32_t fullWidth, uint32_t fullHeight)
    {
        const uint32_t safeW = std::max(1u, fullWidth);
        const uint32_t safeH = std::max(1u, fullHeight);
        if (m_FullWidth == safeW && m_FullHeight == safeH && m_PrefilteredDepth && m_AOTexA && m_AOTexHalfB && m_AOTexB)
        {
            return;
        }
        CreateTextures(safeW, safeH);
    }

    void GTAO::InvalidatePipelines()
    {
        auto *dm = DeviceManager::GetInstance();
        if (!dm || !dm->GetDevice())
            return;

        const auto &nri = dm->GetCoreInterface();
        if (m_PrefilterPipeline) { nri.DestroyPipeline(m_PrefilterPipeline); m_PrefilterPipeline = nullptr; }
        if (m_MainPipeline) { nri.DestroyPipeline(m_MainPipeline); m_MainPipeline = nullptr; }
        if (m_DenoisePipeline) { nri.DestroyPipeline(m_DenoisePipeline); m_DenoisePipeline = nullptr; }

        if (m_PrefilterPipelineLayout) { BindingSet::OnPipelineLayoutDestroyed(m_PrefilterPipelineLayout); nri.DestroyPipelineLayout(m_PrefilterPipelineLayout); m_PrefilterPipelineLayout = nullptr; }
        if (m_MainPipelineLayout) { BindingSet::OnPipelineLayoutDestroyed(m_MainPipelineLayout); nri.DestroyPipelineLayout(m_MainPipelineLayout); m_MainPipelineLayout = nullptr; }
        if (m_DenoisePipelineLayout) { BindingSet::OnPipelineLayoutDestroyed(m_DenoisePipelineLayout); nri.DestroyPipelineLayout(m_DenoisePipelineLayout); m_DenoisePipelineLayout = nullptr; }
    }

    void GTAO::EnsurePipelines()
    {
        if (m_PrefilterPipeline && m_MainPipeline && m_DenoisePipeline)
            return;

        auto *dm = DeviceManager::GetInstance();
        if (!dm || !dm->GetDevice())
            return;
        nri::Device *device = dm->GetDevice();
        const auto &nri = dm->GetCoreInterface();

        if (!m_PrefilterPipelineLayout)
            m_PrefilterPipelineLayout = m_PrefilterLayout.CreatePipelineLayout(nri::StageBits::COMPUTE_SHADER, device);
        if (!m_PrefilterPipeline && m_PrefilterShader && m_PrefilterShader->IsValid())
        {
            nri::ComputePipelineDesc desc = {};
            desc.pipelineLayout = m_PrefilterPipelineLayout;
            desc.shader = m_PrefilterShader->GetShaderDesc();
            nri.CreateComputePipeline(*device, desc, m_PrefilterPipeline);
        }

        if (!m_MainPipelineLayout)
            m_MainPipelineLayout = m_MainLayout.CreatePipelineLayout(nri::StageBits::COMPUTE_SHADER, device);
        if (!m_MainPipeline && m_MainShader && m_MainShader->IsValid())
        {
            nri::ComputePipelineDesc desc = {};
            desc.pipelineLayout = m_MainPipelineLayout;
            desc.shader = m_MainShader->GetShaderDesc();
            nri.CreateComputePipeline(*device, desc, m_MainPipeline);
        }

        if (!m_DenoisePipelineLayout)
            m_DenoisePipelineLayout = m_DenoiseLayout.CreatePipelineLayout(nri::StageBits::COMPUTE_SHADER, device);
        if (!m_DenoisePipeline && m_DenoiseShader && m_DenoiseShader->IsValid())
        {
            nri::ComputePipelineDesc desc = {};
            desc.pipelineLayout = m_DenoisePipelineLayout;
            desc.shader = m_DenoiseShader->GetShaderDesc();
            nri.CreateComputePipeline(*device, desc, m_DenoisePipeline);
        }
    }

    void GTAO::Build(nri::CommandBuffer *cmd, const Ref<Texture> &depthTexture, const Ref<Texture> &normalTexture,
        ICamera *camera, const PostProcessing &settings, uint64_t taaFrameIndex)
    {
        IGN_PROFILE_FUNCTION();

        if (!cmd || !depthTexture || !depthTexture->GetHandle() || !camera || !m_PrefilteredDepth || !m_AOTexA || !m_AOTexHalfB || !m_AOTexB)
            return;

        EnsurePipelines();
        if (!m_PrefilterPipeline || !m_MainPipeline || !m_DenoisePipeline)
            return;

        const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();
        const bool hasNormals = (normalTexture && normalTexture->GetHandle() && normalTexture->GetShaderResourceView());
        Ref<Texture> safeNormalTex = hasNormals ? normalTexture : Renderer::GetBlackTexture();
        if (!safeNormalTex || !safeNormalTex->GetShaderResourceView())
            return;

        TransitionInputToSRV(cmd, depthTexture);
        if (hasNormals)
        {
            TransitionInputToSRV(cmd, safeNormalTex);
        }

        const glm::mat4 proj = camera->GetProjection();
        const glm::mat4 projInv = glm::inverse(proj);
        const float nearPlane = std::max(camera->nearPlane, 1e-3f);
        const float farPlane = std::max(camera->farPlane, nearPlane + 1.0f);
        const float radius = std::max(settings.aoRadius, 0.05f);

        // Prepare all prefiltered depth mips + AO scratch/output buffers for compute UAV writes
        {
            std::vector<nri::TextureBarrierDesc> initBarriers;
            initBarriers.reserve(4);

            const auto currentState = [](const Ref<Texture> &texture)
            {
                if (texture->GetCurrentLayout() == nri::Layout::UNDEFINED)
                    return nri::AccessLayoutStage{ nri::AccessBits::NONE, nri::Layout::UNDEFINED, nri::StageBits::NONE };
                return nri::AccessLayoutStage{ texture->GetCurrentAccess(), texture->GetCurrentLayout(), texture->GetCurrentStages() };
            };

            nri::TextureBarrierDesc depthMipB = {};
            depthMipB.texture = m_PrefilteredDepth->GetHandle();
            depthMipB.before = currentState(m_PrefilteredDepth);
            depthMipB.after = { nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::Layout::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER };
            depthMipB.mipOffset = 0;
            depthMipB.mipNum = static_cast<nri::Dim_t>(m_DepthMipCount);
            depthMipB.layerOffset = 0;
            depthMipB.layerNum = 1;
            depthMipB.planes = nri::PlaneBits::ALL;
            initBarriers.push_back(depthMipB);

            nri::TextureBarrierDesc aoAB = {};
            aoAB.texture = m_AOTexA->GetHandle();
            aoAB.before = currentState(m_AOTexA);
            aoAB.after = { nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::Layout::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER };
            aoAB.mipOffset = 0;
            aoAB.mipNum = 1;
            aoAB.layerOffset = 0;
            aoAB.layerNum = 1;
            aoAB.planes = nri::PlaneBits::ALL;
            initBarriers.push_back(aoAB);

            nri::TextureBarrierDesc aoHalfBB = {};
            aoHalfBB.texture = m_AOTexHalfB->GetHandle();
            aoHalfBB.before = currentState(m_AOTexHalfB);
            aoHalfBB.after = { nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::Layout::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER };
            aoHalfBB.mipOffset = 0;
            aoHalfBB.mipNum = 1;
            aoHalfBB.layerOffset = 0;
            aoHalfBB.layerNum = 1;
            aoHalfBB.planes = nri::PlaneBits::ALL;
            initBarriers.push_back(aoHalfBB);

            nri::TextureBarrierDesc aoBB = {};
            aoBB.texture = m_AOTexB->GetHandle();
            aoBB.before = currentState(m_AOTexB);
            aoBB.after = { nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::Layout::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER };
            aoBB.mipOffset = 0;
            aoBB.mipNum = 1;
            aoBB.layerOffset = 0;
            aoBB.layerNum = 1;
            aoBB.planes = nri::PlaneBits::ALL;
            initBarriers.push_back(aoBB);

            nri::BarrierDesc bd = {};
            bd.textures = initBarriers.data();
            bd.textureNum = static_cast<uint32_t>(initBarriers.size());
            nri.CmdBarrier(*cmd, bd);
        }

        // -------------------------------------------------------------------
        // Pass 1: Prefilter Depth (mip 0 from full-res HW depth, mips 1..N-1 downsampled)
        // -------------------------------------------------------------------
        nri.CmdSetPipelineLayout(*cmd, nri::BindPoint::COMPUTE, *m_PrefilterPipelineLayout);
        nri.CmdSetPipeline(*cmd, *m_PrefilterPipeline);

        uint32_t srcW = static_cast<uint32_t>(std::max(depthTexture->GetWidth(), 1));
        uint32_t srcH = static_cast<uint32_t>(std::max(depthTexture->GetHeight(), 1));
        uint32_t dstW = m_HalfWidth;
        uint32_t dstH = m_HalfHeight;

        for (uint32_t mip = 0; mip < m_DepthMipCount; ++mip)
        {
            PrefilterCB cb{};
            cb.projectionInv = projInv;
            cb.srcSize = glm::uvec2(srcW, srcH);
            cb.dstSize = glm::uvec2(dstW, dstH);
            cb.nearPlane = nearPlane;
            cb.farPlane = farPlane;
            cb.passMode = (mip == 0u) ? 0u : 1u;
            cb.effectRadius = radius;
            m_PrefilterCBs[mip]->SetData(&cb, sizeof(cb));

            nri::Descriptor *srcSRV = (mip == 0u)
                ? depthTexture->GetShaderResourceView()
                : m_PrefilteredDepth->GetMipShaderResourceView(mip - 1u);
            nri::Descriptor *dstUAV = m_PrefilteredDepth->GetStorageView(mip);
            if (!srcSRV || !dstUAV)
                break;

            BindingSetDesc bsDesc;
            bsDesc.AddItem(BindingSetItem::ConstantBuffer(0, m_PrefilterCBs[mip]->GetDescriptor()));
            bsDesc.AddItem(BindingSetItem::TextureSRV(0, srcSRV));
            bsDesc.AddItem(BindingSetItem::TextureUAV(0, dstUAV));
            Ref<BindingSet> bs = BindingCache::GetOrCreateBindingSet(bsDesc, m_PrefilterPipelineLayout, 0, "GTAO Prefilter");
            if (!bs || !bs->GetHandle())
                return;

            nri::SetDescriptorSetDesc setDesc = { 0, bs->GetHandle(), nri::BindPoint::COMPUTE };
            nri.CmdSetDescriptorSet(*cmd, setDesc);

            nri::DispatchDesc disp = { (dstW + 7u) / 8u, (dstH + 7u) / 8u, 1u };
            nri.CmdDispatch(*cmd, disp);

            nri::TextureBarrierDesc mipDone = {};
            mipDone.texture = m_PrefilteredDepth->GetHandle();
            mipDone.before = { nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::Layout::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER };
            mipDone.after = { nri::AccessBits::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE, nri::StageBits::COMPUTE_SHADER };
            mipDone.mipOffset = static_cast<nri::Dim_t>(mip);
            mipDone.mipNum = 1;
            mipDone.layerOffset = 0;
            mipDone.layerNum = 1;
            mipDone.planes = nri::PlaneBits::ALL;

            nri::BarrierDesc bd = {};
            bd.textures = &mipDone;
            bd.textureNum = 1;
            nri.CmdBarrier(*cmd, bd);

            srcW = dstW;
            srcH = dstH;
            dstW = std::max(1u, dstW / 2u);
            dstH = std::max(1u, dstH / 2u);
        }

        // -------------------------------------------------------------------
        // Pass 2: XeGTAO Main (writes raw half-res AO to m_AOTexHalfB)
        // -------------------------------------------------------------------
        {
            const bool isOrtho = (camera->projectionType == ProjectionType::Orthographic);
            const float proj00 = (std::abs(proj[0][0]) > 1e-6f) ? proj[0][0] : 1.0f;
            const float proj11 = (std::abs(proj[1][1]) > 1e-6f) ? proj[1][1] : 1.0f;

            MainCB mainCB{};
            mainCB.ndcToViewMul = glm::vec2(1.0f / proj00, 1.0f / proj11);
            mainCB.ndcToViewAdd = isOrtho
                ? glm::vec2(-proj[3][0] / proj00, -proj[3][1] / proj11)
                : glm::vec2(proj[2][0] / proj00, proj[2][1] / proj11);
            mainCB.invHalfRes = glm::vec2(1.0f / static_cast<float>(m_HalfWidth), 1.0f / static_cast<float>(m_HalfHeight));
            mainCB.fullRes = glm::vec2(static_cast<float>(m_FullWidth), static_cast<float>(m_FullHeight));
            mainCB.halfRes = glm::uvec2(m_HalfWidth, m_HalfHeight);
            mainCB.maxDepthMip = (m_DepthMipCount > 0u) ? (m_DepthMipCount - 1u) : 0u;
            mainCB.noiseIndex = settings.taaProperties.enable ? static_cast<uint32_t>(taaFrameIndex % 64u) : 0u;
            mainCB.effectRadius = radius;
            mainCB.falloffRange = 0.615f;
            mainCB.radiusMultiplier = 1.457f;
            mainCB.sampleDistributionPower = 2.0f;
            mainCB.thinOccluderCompensation = 0.1f;
            mainCB.projScaleY = std::abs(proj11) * 0.5f * static_cast<float>(m_HalfHeight);
            mainCB.farPlane = farPlane;
            mainCB.flags = (hasNormals ? 1u : 0u) | (isOrtho ? 2u : 0u);
            m_MainCB->SetData(&mainCB, sizeof(mainCB));

            BindingSetDesc bsDesc;
            bsDesc.AddItem(BindingSetItem::ConstantBuffer(0, m_MainCB->GetDescriptor()));
            bsDesc.AddItem(BindingSetItem::TextureSRV(0, m_PrefilteredDepth->GetShaderResourceView()));
            bsDesc.AddItem(BindingSetItem::TextureSRV(1, safeNormalTex->GetShaderResourceView()));
            bsDesc.AddItem(BindingSetItem::TextureUAV(0, m_AOTexHalfB->GetStorageView(0)));
            bsDesc.AddItem(BindingSetItem::Sampler(0, m_PointClampSampler));
            Ref<BindingSet> bs = BindingCache::GetOrCreateBindingSet(bsDesc, m_MainPipelineLayout, 0, "GTAO Main");
            if (!bs || !bs->GetHandle())
                return;

            nri.CmdSetPipelineLayout(*cmd, nri::BindPoint::COMPUTE, *m_MainPipelineLayout);
            nri.CmdSetPipeline(*cmd, *m_MainPipeline);
            nri::SetDescriptorSetDesc setDesc = { 0, bs->GetHandle(), nri::BindPoint::COMPUTE };
            nri.CmdSetDescriptorSet(*cmd, setDesc);

            nri::DispatchDesc disp = { (m_HalfWidth + 7u) / 8u, (m_HalfHeight + 7u) / 8u, 1u };
            nri.CmdDispatch(*cmd, disp);

            nri::TextureBarrierDesc mainDone = {};
            mainDone.texture = m_AOTexHalfB->GetHandle();
            mainDone.before = { nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::Layout::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER };
            mainDone.after = { nri::AccessBits::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE, nri::StageBits::COMPUTE_SHADER };
            mainDone.mipOffset = 0;
            mainDone.mipNum = 1;
            mainDone.layerOffset = 0;
            mainDone.layerNum = 1;
            mainDone.planes = nri::PlaneBits::ALL;

            nri::BarrierDesc bd = {};
            bd.textures = &mainDone;
            bd.textureNum = 1;
            nri.CmdBarrier(*cmd, bd);
        }

        // -------------------------------------------------------------------
        // Pass 3: Bilateral Edge-Aware Denoise (H: HalfB -> A, V: A -> HalfB) + Full-Res Upsample (HalfB -> B)
        // -------------------------------------------------------------------
        nri::Descriptor *depthMip0SRV = (m_DepthMipCount > 1u)
            ? m_PrefilteredDepth->GetMipShaderResourceView(0)
            : m_PrefilteredDepth->GetShaderResourceView();

        nri.CmdSetPipelineLayout(*cmd, nri::BindPoint::COMPUTE, *m_DenoisePipelineLayout);
        nri.CmdSetPipeline(*cmd, *m_DenoisePipeline);

        // Horizontal: m_AOTexHalfB -> m_AOTexA
        {
            DenoiseCB hcb{};
            hcb.projectionInv = projInv;
            hcb.halfRes = glm::uvec2(m_HalfWidth, m_HalfHeight);
            hcb.fullRes = glm::uvec2(m_FullWidth, m_FullHeight);
            hcb.direction = glm::ivec2(1, 0);
            hcb.depthSigma = 0.08f;
            hcb.farPlane = farPlane;
            hcb.nearPlane = nearPlane;
            hcb.hasNormals = hasNormals ? 1u : 0u;
            hcb.passMode = 0u;
            m_DenoiseHCB->SetData(&hcb, sizeof(hcb));

            BindingSetDesc bsDesc;
            bsDesc.AddItem(BindingSetItem::ConstantBuffer(0, m_DenoiseHCB->GetDescriptor()));
            bsDesc.AddItem(BindingSetItem::TextureSRV(0, m_AOTexHalfB->GetShaderResourceView()));
            bsDesc.AddItem(BindingSetItem::TextureSRV(1, depthMip0SRV));
            bsDesc.AddItem(BindingSetItem::TextureSRV(2, safeNormalTex->GetShaderResourceView()));
            bsDesc.AddItem(BindingSetItem::TextureSRV(3, depthTexture->GetShaderResourceView()));
            bsDesc.AddItem(BindingSetItem::TextureUAV(0, m_AOTexA->GetStorageView(0)));
            Ref<BindingSet> bs = BindingCache::GetOrCreateBindingSet(bsDesc, m_DenoisePipelineLayout, 0, "GTAO Denoise H");
            if (!bs || !bs->GetHandle())
                return;

            nri::SetDescriptorSetDesc setDesc = { 0, bs->GetHandle(), nri::BindPoint::COMPUTE };
            nri.CmdSetDescriptorSet(*cmd, setDesc);

            nri::DispatchDesc disp = { (m_HalfWidth + 7u) / 8u, (m_HalfHeight + 7u) / 8u, 1u };
            nri.CmdDispatch(*cmd, disp);

            nri::TextureBarrierDesc barriers[2] = {};
            barriers[0].texture = m_AOTexA->GetHandle();
            barriers[0].before = { nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::Layout::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER };
            barriers[0].after = { nri::AccessBits::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE, nri::StageBits::COMPUTE_SHADER };
            barriers[0].mipNum = 1;
            barriers[0].layerNum = 1;
            barriers[0].planes = nri::PlaneBits::ALL;

            barriers[1].texture = m_AOTexHalfB->GetHandle();
            barriers[1].before = { nri::AccessBits::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE, nri::StageBits::COMPUTE_SHADER };
            barriers[1].after = { nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::Layout::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER };
            barriers[1].mipNum = 1;
            barriers[1].layerNum = 1;
            barriers[1].planes = nri::PlaneBits::ALL;

            nri::BarrierDesc bd = {};
            bd.textures = barriers;
            bd.textureNum = 2;
            nri.CmdBarrier(*cmd, bd);
        }

        // Vertical: m_AOTexA -> m_AOTexHalfB
        {
            DenoiseCB vcb{};
            vcb.projectionInv = projInv;
            vcb.halfRes = glm::uvec2(m_HalfWidth, m_HalfHeight);
            vcb.fullRes = glm::uvec2(m_FullWidth, m_FullHeight);
            vcb.direction = glm::ivec2(0, 1);
            vcb.depthSigma = 0.08f;
            vcb.farPlane = farPlane;
            vcb.nearPlane = nearPlane;
            vcb.hasNormals = hasNormals ? 1u : 0u;
            vcb.passMode = 1u;
            m_DenoiseVCB->SetData(&vcb, sizeof(vcb));

            BindingSetDesc bsDesc;
            bsDesc.AddItem(BindingSetItem::ConstantBuffer(0, m_DenoiseVCB->GetDescriptor()));
            bsDesc.AddItem(BindingSetItem::TextureSRV(0, m_AOTexA->GetShaderResourceView()));
            bsDesc.AddItem(BindingSetItem::TextureSRV(1, depthMip0SRV));
            bsDesc.AddItem(BindingSetItem::TextureSRV(2, safeNormalTex->GetShaderResourceView()));
            bsDesc.AddItem(BindingSetItem::TextureSRV(3, depthTexture->GetShaderResourceView()));
            bsDesc.AddItem(BindingSetItem::TextureUAV(0, m_AOTexHalfB->GetStorageView(0)));
            Ref<BindingSet> bs = BindingCache::GetOrCreateBindingSet(bsDesc, m_DenoisePipelineLayout, 0, "GTAO Denoise V");
            if (!bs || !bs->GetHandle())
                return;

            nri::SetDescriptorSetDesc setDesc = { 0, bs->GetHandle(), nri::BindPoint::COMPUTE };
            nri.CmdSetDescriptorSet(*cmd, setDesc);

            nri::DispatchDesc disp = { (m_HalfWidth + 7u) / 8u, (m_HalfHeight + 7u) / 8u, 1u };
            nri.CmdDispatch(*cmd, disp);

            nri::TextureBarrierDesc halfDone = {};
            halfDone.texture = m_AOTexHalfB->GetHandle();
            halfDone.before = { nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::Layout::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER };
            halfDone.after = { nri::AccessBits::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE, nri::StageBits::COMPUTE_SHADER };
            halfDone.mipNum = 1;
            halfDone.layerNum = 1;
            halfDone.planes = nri::PlaneBits::ALL;

            nri::BarrierDesc bd = {};
            bd.textures = &halfDone;
            bd.textureNum = 1;
            nri.CmdBarrier(*cmd, bd);
        }

        // Full-res Bilateral Upsample: m_AOTexHalfB -> m_AOTexB
        {
            DenoiseCB ucb{};
            ucb.projectionInv = projInv;
            ucb.halfRes = glm::uvec2(m_HalfWidth, m_HalfHeight);
            ucb.fullRes = glm::uvec2(m_FullWidth, m_FullHeight);
            ucb.direction = glm::ivec2(0, 0);
            ucb.depthSigma = 0.08f;
            ucb.farPlane = farPlane;
            ucb.nearPlane = nearPlane;
            ucb.hasNormals = hasNormals ? 1u : 0u;
            ucb.passMode = 2u;
            m_DenoiseUpsampleCB->SetData(&ucb, sizeof(ucb));

            BindingSetDesc bsDesc;
            bsDesc.AddItem(BindingSetItem::ConstantBuffer(0, m_DenoiseUpsampleCB->GetDescriptor()));
            bsDesc.AddItem(BindingSetItem::TextureSRV(0, m_AOTexHalfB->GetShaderResourceView()));
            bsDesc.AddItem(BindingSetItem::TextureSRV(1, depthMip0SRV));
            bsDesc.AddItem(BindingSetItem::TextureSRV(2, safeNormalTex->GetShaderResourceView()));
            bsDesc.AddItem(BindingSetItem::TextureSRV(3, depthTexture->GetShaderResourceView()));
            bsDesc.AddItem(BindingSetItem::TextureUAV(0, m_AOTexB->GetStorageView(0)));
            Ref<BindingSet> bs = BindingCache::GetOrCreateBindingSet(bsDesc, m_DenoisePipelineLayout, 0, "GTAO Upsample");
            if (!bs || !bs->GetHandle())
                return;

            nri::SetDescriptorSetDesc setDesc = { 0, bs->GetHandle(), nri::BindPoint::COMPUTE };
            nri.CmdSetDescriptorSet(*cmd, setDesc);

            nri::DispatchDesc disp = { (m_FullWidth + 7u) / 8u, (m_FullHeight + 7u) / 8u, 1u };
            nri.CmdDispatch(*cmd, disp);

            nri::TextureBarrierDesc outDone = {};
            outDone.texture = m_AOTexB->GetHandle();
            outDone.before = { nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::Layout::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER };
            outDone.after = { nri::AccessBits::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE,
                nri::StageBits::FRAGMENT_SHADER | nri::StageBits::COMPUTE_SHADER };
            outDone.mipNum = 1;
            outDone.layerNum = 1;
            outDone.planes = nri::PlaneBits::ALL;

            nri::BarrierDesc bd = {};
            bd.textures = &outDone;
            bd.textureNum = 1;
            nri.CmdBarrier(*cmd, bd);
        }

        m_PrefilteredDepth->SetCurrentState(nri::Layout::SHADER_RESOURCE, nri::AccessBits::SHADER_RESOURCE, nri::StageBits::COMPUTE_SHADER);
        m_AOTexA->SetCurrentState(nri::Layout::SHADER_RESOURCE, nri::AccessBits::SHADER_RESOURCE, nri::StageBits::COMPUTE_SHADER);
        m_AOTexHalfB->SetCurrentState(nri::Layout::SHADER_RESOURCE, nri::AccessBits::SHADER_RESOURCE, nri::StageBits::COMPUTE_SHADER);
        m_AOTexB->SetCurrentState(nri::Layout::SHADER_RESOURCE, nri::AccessBits::SHADER_RESOURCE,
            nri::StageBits::FRAGMENT_SHADER | nri::StageBits::COMPUTE_SHADER);
    }
}
