// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "ssr.hpp"
#include "bindless_system.hpp"
#include "binding_cache.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/profiler/profiler.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/shader.hpp"
#include "ignite/graphics/texture.hpp"

#include <algorithm>
#include <vector>

namespace ignite
{
    namespace
    {
        nri::PlaneBits GetSSRPlanesForFormat(nri::Format fmt)
        {
            if (fmt == nri::Format::D16_UNORM || fmt == nri::Format::D32_SFLOAT)
                return nri::PlaneBits::DEPTH;
            if (fmt == nri::Format::D24_UNORM_S8_UINT || fmt == nri::Format::D32_SFLOAT_S8_UINT)
                return nri::PlaneBits::DEPTH | nri::PlaneBits::STENCIL;
            return nri::PlaneBits::ALL;
        }

        void TransitionSSRInputToSRV(nri::CommandBuffer *cmd, const Ref<Texture> &texture)
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
            barrier.before = (beforeLayout == nri::Layout::UNDEFINED)
                ? nri::AccessLayoutStage{ nri::AccessBits::NONE, nri::Layout::UNDEFINED, nri::StageBits::NONE }
                : nri::AccessLayoutStage{ beforeAccess, beforeLayout, beforeStages };
            barrier.after = { nri::AccessBits::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE,
                nri::StageBits::COMPUTE_SHADER | nri::StageBits::FRAGMENT_SHADER };
            barrier.mipOffset = 0;
            barrier.mipNum = nri::REMAINING;
            barrier.layerOffset = 0;
            barrier.layerNum = nri::REMAINING;
            barrier.planes = GetSSRPlanesForFormat(texture->GetFormat());

            nri::BarrierDesc bd = {};
            bd.textures = &barrier;
            bd.textureNum = 1;
            DeviceManager::GetInstance()->GetCoreInterface().CmdBarrier(*cmd, bd);
            texture->SetCurrentState(barrier.after.layout, barrier.after.access, barrier.after.stages);
        }
    }

    SSR::SSR(uint32_t fullWidth, uint32_t fullHeight)
    {
        auto *dm = DeviceManager::GetInstance();
        nri::Device *device = dm ? dm->GetDevice() : nullptr;
        if (device)
        {
            const auto &nri = dm->GetCoreInterface();
            nri::SamplerDesc samplerDesc = {};
            samplerDesc.filters = { nri::Filter::LINEAR, nri::Filter::LINEAR, nri::Filter::LINEAR };
            samplerDesc.addressModes = { nri::AddressMode::CLAMP_TO_EDGE, nri::AddressMode::CLAMP_TO_EDGE, nri::AddressMode::CLAMP_TO_EDGE };
            samplerDesc.mipMax = 16.0f;
            nri.CreateSampler(*device, samplerDesc, m_LinearClampSampler);
        }

        m_HiZLayout.SetVisibility(nri::StageBits::COMPUTE_SHADER)
            .AddItem(BindingLayoutItem::VolatileConstantBuffer(0))
            .AddItem(BindingLayoutItem::TextureSRV(0))
            .AddItem(BindingLayoutItem::TextureUAV(0));

        m_TraceLayout.SetVisibility(nri::StageBits::COMPUTE_SHADER)
            .AddItem(BindingLayoutItem::VolatileConstantBuffer(0))
            .AddItem(BindingLayoutItem::TextureSRV(0))
            .AddItem(BindingLayoutItem::TextureSRV(1))
            .AddItem(BindingLayoutItem::TextureSRV(2))
            .AddItem(BindingLayoutItem::TextureUAV(0))
            .AddItem(BindingLayoutItem::Sampler(0));

        m_DenoiseLayout.SetVisibility(nri::StageBits::COMPUTE_SHADER)
            .AddItem(BindingLayoutItem::VolatileConstantBuffer(0))
            .AddItem(BindingLayoutItem::TextureSRV(0))
            .AddItem(BindingLayoutItem::TextureSRV(1))
            .AddItem(BindingLayoutItem::TextureSRV(2))
            .AddItem(BindingLayoutItem::TextureUAV(0));

        for (uint32_t m = 0; m < kMaxDepthMips; ++m)
        {
            m_HiZCBs[m] = ConstantBuffer::Create(sizeof(HiZCB), true, 1, "SSR HiZ CB " + std::to_string(m));
        }
        m_TraceCB = ConstantBuffer::Create(sizeof(TraceCB), true, 1, "SSR Trace CB");
        m_DenoiseCB = ConstantBuffer::Create(sizeof(DenoiseCB), true, 1, "SSR Denoise CB");

        m_HiZShader = Shader::Create("resources/shaders/ssr_hiz.compute.hlsl", UMBRA_SHADER_TYPE_COMPUTE, true);
        m_TraceShader = Shader::Create("resources/shaders/ssr_trace.compute.hlsl", UMBRA_SHADER_TYPE_COMPUTE, true);
        m_DenoiseShader = Shader::Create("resources/shaders/ssr_denoise.compute.hlsl", UMBRA_SHADER_TYPE_COMPUTE, true);

        CreateTextures(fullWidth, fullHeight);
    }

    SSR::~SSR()
    {
        auto *dm = DeviceManager::GetInstance();
        if (dm && dm->GetDevice())
        {
            GPUUploadSync::DeviceWaitIdle(dm->GetDevice());
        }

        InvalidatePipelines();
        m_HiZDepth.reset();
        m_SSRRaw.reset();
        m_SSRFinal.reset();

        if (dm && dm->GetDevice() && m_LinearClampSampler)
        {
            dm->GetCoreInterface().DestroyDescriptor(m_LinearClampSampler);
            m_LinearClampSampler = nullptr;
        }
    }

    void SSR::CreateTextures(uint32_t fullWidth, uint32_t fullHeight)
    {
        if (m_HiZDepth || m_SSRRaw || m_SSRFinal)
        {
            if (auto *dm = DeviceManager::GetInstance(); dm && dm->GetDevice())
            {
                GPUUploadSync::DeviceWaitIdle(dm->GetDevice());
            }
            m_HiZDepth.reset();
            m_SSRRaw.reset();
            m_SSRFinal.reset();
            BindingCache::Clear();
        }

        m_FullWidth = std::max(1u, fullWidth);
        m_FullHeight = std::max(1u, fullHeight);
        m_HalfWidth = std::max(1u, (m_FullWidth + 1u) / 2u);
        m_HalfHeight = std::max(1u, (m_FullHeight + 1u) / 2u);
        m_DepthMipCount = std::min(kMaxDepthMips, std::max(1u, Texture::CalculateMaxMipLevels(m_HalfWidth, m_HalfHeight, 1)));

        TextureCreateInfo hizInfo;
        hizInfo.width = m_HalfWidth;
        hizInfo.height = m_HalfHeight;
        hizInfo.depth = 1;
        hizInfo.mipLevels = m_DepthMipCount;
        hizInfo.arraySize = 1;
        hizInfo.sampleCount = 1;
        hizInfo.dimension = nri::TextureType::TEXTURE_2D;
        hizInfo.format = nri::Format::R32_SFLOAT;
        hizInfo.isUAV = true;
        hizInfo.isRenderTarget = false;
        hizInfo.bindless = false;
        hizInfo.samplerLinearFiltering = false;
        hizInfo.samplerAddressU = nri::AddressMode::CLAMP_TO_EDGE;
        hizInfo.samplerAddressV = nri::AddressMode::CLAMP_TO_EDGE;
        m_HiZDepth = Texture::Create(hizInfo, "[SSR HiZ Depth]");

        TextureCreateInfo ssrInfo;
        ssrInfo.width = m_HalfWidth;
        ssrInfo.height = m_HalfHeight;
        ssrInfo.depth = 1;
        ssrInfo.mipLevels = 1;
        ssrInfo.arraySize = 1;
        ssrInfo.sampleCount = 1;
        ssrInfo.dimension = nri::TextureType::TEXTURE_2D;
        ssrInfo.format = nri::Format::RGBA16_SFLOAT;
        ssrInfo.isUAV = true;
        ssrInfo.isRenderTarget = false;
        ssrInfo.bindless = false;
        ssrInfo.samplerLinearFiltering = true;
        ssrInfo.samplerAddressU = nri::AddressMode::CLAMP_TO_EDGE;
        ssrInfo.samplerAddressV = nri::AddressMode::CLAMP_TO_EDGE;
        m_SSRRaw = Texture::Create(ssrInfo, "[SSR Raw]");

        ssrInfo.bindless = true;
        ssrInfo.forceBindless = true;
        m_SSRFinal = Texture::Create(ssrInfo, "[SSR Final]");

        if (BindlessSystem::HasPendingWrites())
        {
            BindlessSystem::FlushPendingWrites();
        }
    }

    void SSR::Resize(uint32_t fullWidth, uint32_t fullHeight)
    {
        const uint32_t safeW = std::max(1u, fullWidth);
        const uint32_t safeH = std::max(1u, fullHeight);
        if (m_FullWidth == safeW && m_FullHeight == safeH && m_HiZDepth && m_SSRRaw && m_SSRFinal)
        {
            return;
        }
        CreateTextures(safeW, safeH);
    }

    void SSR::InvalidatePipelines()
    {
        auto *dm = DeviceManager::GetInstance();
        if (!dm || !dm->GetDevice())
            return;

        const auto &nri = dm->GetCoreInterface();
        if (m_HiZPipeline) { nri.DestroyPipeline(m_HiZPipeline); m_HiZPipeline = nullptr; }
        if (m_TracePipeline) { nri.DestroyPipeline(m_TracePipeline); m_TracePipeline = nullptr; }
        if (m_DenoisePipeline) { nri.DestroyPipeline(m_DenoisePipeline); m_DenoisePipeline = nullptr; }

        if (m_HiZPipelineLayout) { nri.DestroyPipelineLayout(m_HiZPipelineLayout); m_HiZPipelineLayout = nullptr; }
        if (m_TracePipelineLayout) { nri.DestroyPipelineLayout(m_TracePipelineLayout); m_TracePipelineLayout = nullptr; }
        if (m_DenoisePipelineLayout) { nri.DestroyPipelineLayout(m_DenoisePipelineLayout); m_DenoisePipelineLayout = nullptr; }
    }

    void SSR::EnsurePipelines()
    {
        if (m_HiZPipeline && m_TracePipeline && m_DenoisePipeline)
            return;

        auto *dm = DeviceManager::GetInstance();
        if (!dm || !dm->GetDevice())
            return;
        nri::Device *device = dm->GetDevice();
        const auto &nri = dm->GetCoreInterface();

        if (!m_HiZPipelineLayout)
            m_HiZPipelineLayout = m_HiZLayout.CreatePipelineLayout(nri::StageBits::COMPUTE_SHADER, device);
        if (!m_HiZPipeline && m_HiZShader && m_HiZShader->IsValid())
        {
            nri::ComputePipelineDesc desc = {};
            desc.pipelineLayout = m_HiZPipelineLayout;
            desc.shader = m_HiZShader->GetShaderDesc();
            nri.CreateComputePipeline(*device, desc, m_HiZPipeline);
        }

        if (!m_TracePipelineLayout)
            m_TracePipelineLayout = m_TraceLayout.CreatePipelineLayout(nri::StageBits::COMPUTE_SHADER, device);
        if (!m_TracePipeline && m_TraceShader && m_TraceShader->IsValid())
        {
            nri::ComputePipelineDesc desc = {};
            desc.pipelineLayout = m_TracePipelineLayout;
            desc.shader = m_TraceShader->GetShaderDesc();
            nri.CreateComputePipeline(*device, desc, m_TracePipeline);
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

    void SSR::Build(nri::CommandBuffer *cmd,
        const Ref<Texture> &depthTexture,
        const Ref<Texture> &normalTexture,
        const Ref<Texture> &prevSceneColor,
        ICamera *camera,
        const glm::mat4 &prevViewProj,
        float maxRoughness,
        uint64_t frameIndex)
    {
        IGN_PROFILE_FUNCTION();

        if (!cmd || !depthTexture || !depthTexture->GetHandle() || !normalTexture || !normalTexture->GetHandle()
            || !prevSceneColor || !prevSceneColor->GetHandle() || !camera
            || !m_HiZDepth || !m_SSRRaw || !m_SSRFinal)
        {
            return;
        }

        EnsurePipelines();
        if (!m_HiZPipeline || !m_TracePipeline || !m_DenoisePipeline)
            return;

        const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();
        TransitionSSRInputToSRV(cmd, depthTexture);
        TransitionSSRInputToSRV(cmd, normalTexture);
        TransitionSSRInputToSRV(cmd, prevSceneColor);

        // Transition HiZ, Raw, Final into UAV
        {
            const auto currentState = [](const Ref<Texture> &texture)
            {
                if (texture->GetCurrentLayout() == nri::Layout::UNDEFINED)
                    return nri::AccessLayoutStage{ nri::AccessBits::NONE, nri::Layout::UNDEFINED, nri::StageBits::NONE };
                return nri::AccessLayoutStage{ texture->GetCurrentAccess(), texture->GetCurrentLayout(), texture->GetCurrentStages() };
            };

            nri::TextureBarrierDesc barriers[3] = {};
            barriers[0].texture = m_HiZDepth->GetHandle();
            barriers[0].before = currentState(m_HiZDepth);
            barriers[0].after = { nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::Layout::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER };
            barriers[0].mipNum = static_cast<nri::Dim_t>(m_DepthMipCount);
            barriers[0].layerNum = 1;
            barriers[0].planes = nri::PlaneBits::ALL;

            barriers[1].texture = m_SSRRaw->GetHandle();
            barriers[1].before = currentState(m_SSRRaw);
            barriers[1].after = { nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::Layout::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER };
            barriers[1].mipNum = 1;
            barriers[1].layerNum = 1;
            barriers[1].planes = nri::PlaneBits::ALL;

            barriers[2].texture = m_SSRFinal->GetHandle();
            barriers[2].before = currentState(m_SSRFinal);
            barriers[2].after = { nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::Layout::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER };
            barriers[2].mipNum = 1;
            barriers[2].layerNum = 1;
            barriers[2].planes = nri::PlaneBits::ALL;

            nri::BarrierDesc bd = {};
            bd.textures = barriers;
            bd.textureNum = 3;
            nri.CmdBarrier(*cmd, bd);
        }

        // Pass 1: Build Hi-Z depth pyramid
        nri.CmdSetPipelineLayout(*cmd, nri::BindPoint::COMPUTE, *m_HiZPipelineLayout);
        nri.CmdSetPipeline(*cmd, *m_HiZPipeline);

        uint32_t srcW = static_cast<uint32_t>(std::max(depthTexture->GetWidth(), 1));
        uint32_t srcH = static_cast<uint32_t>(std::max(depthTexture->GetHeight(), 1));
        uint32_t dstW = m_HalfWidth;
        uint32_t dstH = m_HalfHeight;

        for (uint32_t mip = 0; mip < m_DepthMipCount; ++mip)
        {
            HiZCB cb{};
            cb.srcSize = glm::uvec2(srcW, srcH);
            cb.dstSize = glm::uvec2(dstW, dstH);
            cb.passMode = (mip == 0u) ? 0u : 1u;
            m_HiZCBs[mip]->SetData(&cb, sizeof(cb));

            nri::Descriptor *srcSRV = (mip == 0u)
                ? depthTexture->GetShaderResourceView()
                : m_HiZDepth->GetMipShaderResourceView(mip - 1u);
            nri::Descriptor *dstUAV = m_HiZDepth->GetStorageView(mip);
            if (!srcSRV || !dstUAV)
                break;

            BindingSetDesc bsDesc;
            bsDesc.AddItem(BindingSetItem::ConstantBuffer(0, m_HiZCBs[mip]->GetDescriptor()));
            bsDesc.AddItem(BindingSetItem::TextureSRV(0, srcSRV));
            bsDesc.AddItem(BindingSetItem::TextureUAV(0, dstUAV));
            Ref<BindingSet> bs = BindingCache::GetOrCreateBindingSet(bsDesc, m_HiZPipelineLayout, 0, "SSR HiZ");
            if (!bs || !bs->GetHandle())
                return;

            nri::SetDescriptorSetDesc setDesc = { 0, bs->GetHandle(), nri::BindPoint::COMPUTE };
            nri.CmdSetDescriptorSet(*cmd, setDesc);
            nri.CmdDispatch(*cmd, { (dstW + 7u) / 8u, (dstH + 7u) / 8u, 1u });

            nri::TextureBarrierDesc mipDone = {};
            mipDone.texture = m_HiZDepth->GetHandle();
            mipDone.before = { nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::Layout::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER };
            mipDone.after = { nri::AccessBits::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE, nri::StageBits::COMPUTE_SHADER };
            mipDone.mipOffset = static_cast<nri::Dim_t>(mip);
            mipDone.mipNum = 1;
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

        // Pass 2: Half-res Hi-Z SSR Trace
        {
            const glm::mat4 proj = camera->GetProjection();
            TraceCB tcb{};
            tcb.projection = proj;
            tcb.projectionInv = glm::inverse(proj);
            tcb.viewInv = glm::inverse(camera->GetView());
            tcb.prevViewProj = prevViewProj;
            tcb.invHalfRes = glm::vec2(1.0f / static_cast<float>(m_HalfWidth), 1.0f / static_cast<float>(m_HalfHeight));
            tcb.fullRes = glm::vec2(static_cast<float>(m_FullWidth), static_cast<float>(m_FullHeight));
            tcb.halfRes = glm::uvec2(m_HalfWidth, m_HalfHeight);
            tcb.maxDepthMip = (m_DepthMipCount > 0u) ? (m_DepthMipCount - 1u) : 0u;
            tcb.frameIndex = static_cast<uint32_t>(frameIndex & 0xFFFFFFFFu);
            tcb.nearPlane = std::max(camera->nearPlane, 1e-3f);
            tcb.farPlane = std::max(camera->farPlane, tcb.nearPlane + 1.0f);
            tcb.maxRoughness = std::clamp(maxRoughness, 0.05f, 1.0f);
            m_TraceCB->SetData(&tcb, sizeof(tcb));

            BindingSetDesc bsDesc;
            bsDesc.AddItem(BindingSetItem::ConstantBuffer(0, m_TraceCB->GetDescriptor()));
            bsDesc.AddItem(BindingSetItem::TextureSRV(0, m_HiZDepth->GetShaderResourceView()));
            bsDesc.AddItem(BindingSetItem::TextureSRV(1, normalTexture->GetShaderResourceView()));
            bsDesc.AddItem(BindingSetItem::TextureSRV(2, prevSceneColor->GetShaderResourceView()));
            bsDesc.AddItem(BindingSetItem::TextureUAV(0, m_SSRRaw->GetStorageView(0)));
            bsDesc.AddItem(BindingSetItem::Sampler(0, m_LinearClampSampler));
            Ref<BindingSet> bs = BindingCache::GetOrCreateBindingSet(bsDesc, m_TracePipelineLayout, 0, "SSR Trace");
            if (!bs || !bs->GetHandle())
                return;

            nri.CmdSetPipelineLayout(*cmd, nri::BindPoint::COMPUTE, *m_TracePipelineLayout);
            nri.CmdSetPipeline(*cmd, *m_TracePipeline);
            nri::SetDescriptorSetDesc setDesc = { 0, bs->GetHandle(), nri::BindPoint::COMPUTE };
            nri.CmdSetDescriptorSet(*cmd, setDesc);
            nri.CmdDispatch(*cmd, { (m_HalfWidth + 7u) / 8u, (m_HalfHeight + 7u) / 8u, 1u });

            nri::TextureBarrierDesc traceDone = {};
            traceDone.texture = m_SSRRaw->GetHandle();
            traceDone.before = { nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::Layout::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER };
            traceDone.after = { nri::AccessBits::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE, nri::StageBits::COMPUTE_SHADER };
            traceDone.mipNum = 1;
            traceDone.layerNum = 1;
            traceDone.planes = nri::PlaneBits::ALL;

            nri::BarrierDesc bd = {};
            bd.textures = &traceDone;
            bd.textureNum = 1;
            nri.CmdBarrier(*cmd, bd);
        }

        // Pass 3: Bilateral Denoise -> m_SSRFinal
        {
            DenoiseCB dcb{};
            dcb.halfRes = glm::uvec2(m_HalfWidth, m_HalfHeight);
            dcb.fullRes = glm::uvec2(m_FullWidth, m_FullHeight);
            dcb.invHalfRes = glm::vec2(1.0f / static_cast<float>(m_HalfWidth), 1.0f / static_cast<float>(m_HalfHeight));
            dcb.depthSigma = 0.02f;
            m_DenoiseCB->SetData(&dcb, sizeof(dcb));

            BindingSetDesc bsDesc;
            bsDesc.AddItem(BindingSetItem::ConstantBuffer(0, m_DenoiseCB->GetDescriptor()));
            bsDesc.AddItem(BindingSetItem::TextureSRV(0, m_SSRRaw->GetShaderResourceView()));
            bsDesc.AddItem(BindingSetItem::TextureSRV(1, m_HiZDepth->GetShaderResourceView()));
            bsDesc.AddItem(BindingSetItem::TextureSRV(2, normalTexture->GetShaderResourceView()));
            bsDesc.AddItem(BindingSetItem::TextureUAV(0, m_SSRFinal->GetStorageView(0)));
            Ref<BindingSet> bs = BindingCache::GetOrCreateBindingSet(bsDesc, m_DenoisePipelineLayout, 0, "SSR Denoise");
            if (!bs || !bs->GetHandle())
                return;

            nri.CmdSetPipelineLayout(*cmd, nri::BindPoint::COMPUTE, *m_DenoisePipelineLayout);
            nri.CmdSetPipeline(*cmd, *m_DenoisePipeline);
            nri::SetDescriptorSetDesc setDesc = { 0, bs->GetHandle(), nri::BindPoint::COMPUTE };
            nri.CmdSetDescriptorSet(*cmd, setDesc);
            nri.CmdDispatch(*cmd, { (m_HalfWidth + 7u) / 8u, (m_HalfHeight + 7u) / 8u, 1u });

            nri::TextureBarrierDesc outDone = {};
            outDone.texture = m_SSRFinal->GetHandle();
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

        m_HiZDepth->SetCurrentState(nri::Layout::SHADER_RESOURCE, nri::AccessBits::SHADER_RESOURCE, nri::StageBits::COMPUTE_SHADER);
        m_SSRRaw->SetCurrentState(nri::Layout::SHADER_RESOURCE, nri::AccessBits::SHADER_RESOURCE, nri::StageBits::COMPUTE_SHADER);
        m_SSRFinal->SetCurrentState(nri::Layout::SHADER_RESOURCE, nri::AccessBits::SHADER_RESOURCE,
            nri::StageBits::FRAGMENT_SHADER | nri::StageBits::COMPUTE_SHADER);
    }
}
