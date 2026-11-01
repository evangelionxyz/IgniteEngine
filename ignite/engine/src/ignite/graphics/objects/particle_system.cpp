// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "particle_system.hpp"
#include "ignite/graphics/graphics_pipeline.hpp"
#include "ignite/graphics/render_target.hpp"
#include "ignite/graphics/texture.hpp"
#include "ignite/graphics/buffers/constant_buffer.hpp"
#include "ignite/graphics/shader.hpp"
#include "ignite/scene/icamera.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/core/profiler/profiler.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/asset/asset_manager.hpp"

namespace ignite
{
    ParticleSystem::ParticleSystem(uint32_t maxParticles)
        : m_MaxParticles(maxParticles)
    {
        Init(maxParticles);
    }

    ParticleSystem::~ParticleSystem()
    {
        DestroyPipelines();
        DestroyBuffers();
    }

    void ParticleSystem::DestroyBuffers()
    {
        auto *dm = DeviceManager::GetInstance();
        if (!dm || !dm->GetDevice())
            return;

        auto &nri = dm->GetNRI();
        if (m_LinearSampler) { nri.DestroyDescriptor(m_LinearSampler); m_LinearSampler = nullptr; }
        if (m_ParticleSRV) { nri.DestroyDescriptor(m_ParticleSRV); m_ParticleSRV = nullptr; }
        if (m_ParticleUAV) { nri.DestroyDescriptor(m_ParticleUAV); m_ParticleUAV = nullptr; }
        if (m_ParticleBuffer) { nri.DestroyBuffer(m_ParticleBuffer); m_ParticleBuffer = nullptr; }
        if (m_ParticleMemory) { nri.FreeMemory(m_ParticleMemory); m_ParticleMemory = nullptr; }
        if (m_ResetStagingBuffer) { nri.DestroyBuffer(m_ResetStagingBuffer); m_ResetStagingBuffer = nullptr; }
        if (m_ResetStagingMemory) { nri.FreeMemory(m_ResetStagingMemory); m_ResetStagingMemory = nullptr; }

        m_SimulateCB = nullptr;
        m_RenderCB = nullptr;
    }

    void ParticleSystem::DestroyPipelines()
    {
        auto *dm = DeviceManager::GetInstance();
        if (dm && dm->GetDevice())
        {
            auto &nri = dm->GetNRI();
            if (m_ComputePipeline) { nri.DestroyPipeline(m_ComputePipeline); m_ComputePipeline = nullptr; }
            if (m_ComputePipelineLayout) { nri.DestroyPipelineLayout(m_ComputePipelineLayout); m_ComputePipelineLayout = nullptr; }
        }
        m_ComputeBindingSet = nullptr;
        m_RenderBindingSet = nullptr;
        m_RenderPipelineAlphaCache.clear();
        m_RenderPipelineAdditiveCache.clear();
        m_RenderPipelineAlpha = nullptr;
        m_RenderPipelineAdditive = nullptr;
    }

    Ref<ParticleSystem> ParticleSystem::Create(uint32_t maxParticles)
    {
        return CreateRef<ParticleSystem>(maxParticles);
    }

    void ParticleSystem::Init(uint32_t maxParticles)
    {
        DestroyPipelines();
        DestroyBuffers();

        m_MaxParticles = maxParticles;
        CreateBuffers(maxParticles);
        CreatePipelines();
    }

    void ParticleSystem::CreateBuffers(uint32_t maxParticles)
    {
        auto &nri = DeviceManager::GetInstance()->GetNRI();
        nri::Device *device = DeviceManager::GetInstance()->GetDevice();

        const uint64_t byteSize = static_cast<uint64_t>(maxParticles) * sizeof(GPUParticle);

        // 1. GPU StructuredBuffer for Particles (UAV for compute, SRV for vertex shader)
        nri::BufferDesc bufferDesc = {};
        bufferDesc.size = byteSize;
        bufferDesc.structureStride = sizeof(GPUParticle);
        bufferDesc.usage = nri::BufferUsageBits::SHADER_RESOURCE | nri::BufferUsageBits::SHADER_RESOURCE_STORAGE;
        nri.CreateCommittedBuffer(*device, nri::MemoryLocation::DEVICE, 1.0f, bufferDesc, m_ParticleBuffer);
        nri.SetDebugName((nri::Object*)m_ParticleBuffer, "GPU Particle StructuredBuffer");

        nri::BufferViewDesc srvDesc = {};
        srvDesc.buffer = m_ParticleBuffer;
        srvDesc.type = nri::BufferView::STRUCTURED_BUFFER;
        srvDesc.offset = 0;
        srvDesc.size = byteSize;
        srvDesc.structureStride = sizeof(GPUParticle);
        nri.CreateBufferView(srvDesc, m_ParticleSRV);

        nri::BufferViewDesc uavDesc = {};
        uavDesc.buffer = m_ParticleBuffer;
        uavDesc.type = nri::BufferView::STORAGE_STRUCTURED_BUFFER;
        uavDesc.offset = 0;
        uavDesc.size = byteSize;
        uavDesc.structureStride = sizeof(GPUParticle);
        nri.CreateBufferView(uavDesc, m_ParticleUAV);

        // Reset staging buffer pre-populated with dead particles
        nri::BufferDesc stagingDesc = {};
        stagingDesc.size = byteSize;
        stagingDesc.usage = nri::BufferUsageBits::NONE;
        nri.CreateCommittedBuffer(*device, nri::MemoryLocation::HOST_UPLOAD, 1.0f, stagingDesc, m_ResetStagingBuffer);

        std::vector<GPUParticle> initial(maxParticles);
        for (auto &p : initial)
        {
            p.lifetime = 999999.0f;
            p.maxLifetime = 0.0f;
            p.size = glm::vec2(0.0f);
        }

        void *mapped = nri.MapBuffer(*m_ResetStagingBuffer, 0, byteSize);
        if (mapped)
        {
            std::memcpy(mapped, initial.data(), byteSize);
            nri.UnmapBuffer(*m_ResetStagingBuffer);
        }

        // Upload initial state synchronously
        {
            nri::Queue *graphicsQueue = nullptr;
            nri.GetQueue(*device, nri::QueueType::GRAPHICS, 0, graphicsQueue);
            nri::BufferUploadDesc uploadDesc = {};
            uploadDesc.buffer = m_ParticleBuffer;
            uploadDesc.data = initial.data();
            uploadDesc.after = { nri::AccessBits::SHADER_RESOURCE, nri::StageBits::VERTEX_SHADER };
            std::lock_guard<std::mutex> queueLock(GPUUploadSync::GetQueueMutex());
            DeviceManager::GetInstance()->GetHelperInterface().UploadData(*graphicsQueue, nullptr, 0, &uploadDesc, 1);
        }

        // 2. Constant Buffers
        m_SimulateCB = ConstantBuffer::Create(sizeof(ParticleSimCB), true, 16, "[Particle System] Sim CB");
        m_RenderCB = ConstantBuffer::Create(sizeof(ParticleRenderCB), true, 16, "[Particle System] Render CB");

        // 3. Bilinear Clamp Sampler for textures
        nri::SamplerDesc samplerDesc = {};
        samplerDesc.filters = { nri::Filter::LINEAR, nri::Filter::LINEAR, nri::Filter::LINEAR };
        samplerDesc.addressModes = { nri::AddressMode::CLAMP_TO_EDGE, nri::AddressMode::CLAMP_TO_EDGE, nri::AddressMode::CLAMP_TO_EDGE };
        samplerDesc.mipMax = 16.0f;
        nri.CreateSampler(*device, samplerDesc, m_LinearSampler);
    }

    void ParticleSystem::CreatePipelines()
    {
        auto &nri = DeviceManager::GetInstance()->GetNRI();
        nri::Device *device = DeviceManager::GetInstance()->GetDevice();

        // Load / Compile Shaders
        m_ComputeShader = Shader::Create("resources/shaders/particle.compute.hlsl", UMBRA_SHADER_TYPE_COMPUTE, true);
        m_VertexShader = Shader::Create("resources/shaders/particle.vertex.hlsl", UMBRA_SHADER_TYPE_VERTEX, true);
        m_PixelShader = Shader::Create("resources/shaders/particle.pixel.hlsl", UMBRA_SHADER_TYPE_PIXEL, true);

        // Compute Pipeline
        {
            m_ComputeBindingLayout.SetVisibility(nri::StageBits::COMPUTE_SHADER);
            m_ComputeBindingLayout.bindings =
            {
                BindingLayoutItem::VolatileConstantBuffer(0),
                BindingLayoutItem::StructuredBuffer_UAV(0)
            };
            m_ComputePipelineLayout = m_ComputeBindingLayout.CreatePipelineLayout(nri::StageBits::COMPUTE_SHADER);

            BindingSetDesc computeSetDesc;
            computeSetDesc.bindings =
            {
                BindingSetItem::ConstantBuffer(0, m_SimulateCB->GetDescriptor()),
                BindingSetItem::StructuredBuffer_UAV(0, m_ParticleUAV)
            };
            m_ComputeBindingSet = BindingSet::Create("[Particle System] Compute", m_ComputePipelineLayout, 0, computeSetDesc);

            nri::ComputePipelineDesc computePipeDesc = {};
            computePipeDesc.pipelineLayout = m_ComputePipelineLayout;
            computePipeDesc.shader = m_ComputeShader->GetShaderDesc();
            nri.CreateComputePipeline(*device, computePipeDesc, m_ComputePipeline);
            LOG_ASSERT(m_ComputePipeline, "[ParticleSystem] Failed to create compute pipeline");
        }

        // Render Binding Layout
        {
            m_RenderBindingLayout.SetVisibility(nri::StageBits::ALL);
            m_RenderBindingLayout.bindings =
            {
                BindingLayoutItem::VolatileConstantBuffer(0), // Camera buffer (b0)
                BindingLayoutItem::VolatileConstantBuffer(1), // Particle render params (b1)
                BindingLayoutItem::StructuredBufferSRV(0),   // Particles (t0)
                BindingLayoutItem::TextureSRV(1),            // Texture (t1)
                BindingLayoutItem::Sampler(0)                 // Sampler (s0)
            };
        }
    }

    Ref<GraphicsPipeline> ParticleSystem::GetOrCreateRenderPipeline(RenderTarget *framebuffer, ParticleBlendMode blendMode)
    {
        if (!framebuffer)
            return nullptr;

        auto key = MakeFramebufferKey(framebuffer);
        auto &cache = (blendMode == ParticleBlendMode::Alpha) ? m_RenderPipelineAlphaCache : m_RenderPipelineAdditiveCache;
        auto it = cache.find(key);
        if (it != cache.end())
            return it->second;

        bool hasDepthAttachment = framebuffer->HasDepthAttachment();

        GraphicsPipelineParams params;
        params.enableDepthTest = hasDepthAttachment;
        params.enableDepthWrite = false; // Transparent particles don't write to depth buffer
        params.depthFunc = hasDepthAttachment ? nri::CompareOp::LESS_EQUAL : nri::CompareOp::ALWAYS;
        params.cullMode = nri::CullMode::NONE;
        params.fillMode = nri::FillMode::SOLID;
        params.enableBlend = true;

        if (blendMode == ParticleBlendMode::Alpha)
        {
            params.srcBlend = nri::BlendFactor::SRC_ALPHA;
            params.destBlend = nri::BlendFactor::ONE_MINUS_SRC_ALPHA;
            params.srcBlendAlpha = nri::BlendFactor::ONE;
            params.destBlendAlpha = nri::BlendFactor::ONE_MINUS_SRC_ALPHA;
        }
        else // Additive
        {
            params.srcBlend = nri::BlendFactor::SRC_ALPHA;
            params.destBlend = nri::BlendFactor::ONE;
            params.srcBlendAlpha = nri::BlendFactor::ONE;
            params.destBlendAlpha = nri::BlendFactor::ONE;
        }

        std::string name = (blendMode == ParticleBlendMode::Alpha) ? "Particle Pipeline (Alpha)" : "Particle Pipeline (Additive)";
        Ref<GraphicsPipeline> pipeline = GraphicsPipeline::Create(name);
        pipeline->SetShaders({ m_VertexShader, m_PixelShader })
                 .AddBindingLayout(m_RenderBindingLayout)
                 .Build(framebuffer, params);

        cache.emplace(key, pipeline);
        return pipeline;
    }

    void ParticleSystem::UpdateRenderBindingSet(nri::Descriptor *cameraBufferDescriptor, Ref<Texture> texture, nri::PipelineLayout *pipelineLayout)
    {
        Ref<Texture> resolvedTex = (texture && texture->IsReady()) ? texture : Renderer::GetWhiteTexture();
        nri::Texture *texHandle = resolvedTex ? resolvedTex->GetHandle() : nullptr;

        if (m_RenderBindingSet && m_LastCameraBufferDescriptor == cameraBufferDescriptor && m_LastBoundTexture == texHandle)
            return;

        m_LastCameraBufferDescriptor = cameraBufferDescriptor;
        m_LastBoundTexture = texHandle;

        BindingSetDesc setDesc;
        setDesc.bindings =
        {
            BindingSetItem::ConstantBuffer(0, cameraBufferDescriptor),
            BindingSetItem::ConstantBuffer(1, m_RenderCB->GetDescriptor()),
            BindingSetItem::StructuredBuffer_SRV(0, m_ParticleSRV),
            BindingSetItem::TextureSRV(1, resolvedTex->GetShaderResourceView()),
            BindingSetItem::Sampler(0, m_LinearSampler)
        };
        BindingSet::CreateOrUpdate(m_RenderBindingSet, "[ParticleSystem] RenderBindingSet", pipelineLayout, 0, setDesc);
        LOG_ASSERT(m_RenderBindingSet, "[ParticleSystem] Failed to create render binding set");
    }

    void ParticleSystem::Simulate(nri::CommandBuffer *cmd, float dt, const glm::vec3 &emitterPos, const ParticleEmitterSettings &settings)
    {
        IGN_PROFILE_FUNCTION();

        if (!cmd || dt <= 0.0f || m_MaxParticles == 0)
            return;

        auto &nri = DeviceManager::GetInstance()->GetNRI();

        m_TotalTime += dt;

        glm::vec3 emitterVelocity = (m_TotalTime > dt) ? (emitterPos - m_LastEmitterPosition) / dt : glm::vec3(0.0f);
        m_LastEmitterPosition = emitterPos;

        // Calculate spawn count for this frame
        m_SpawnAccumulator += settings.emissionRate * dt;
        uint32_t spawnCount = static_cast<uint32_t>(m_SpawnAccumulator);
        m_SpawnAccumulator -= static_cast<float>(spawnCount);
        if (spawnCount > m_MaxParticles)
            spawnCount = m_MaxParticles;

        ParticleSimCB simCB;
        simCB.emitterPosition = emitterPos;
        simCB.deltaTime = dt;
        simCB.emitterVelocity = emitterVelocity;
        simCB.totalTime = m_TotalTime;
        simCB.velocityMin = settings.velocityMin;
        simCB.lifetimeMin = settings.lifetimeMin;
        simCB.velocityMax = settings.velocityMax;
        simCB.lifetimeMax = settings.lifetimeMax;
        simCB.acceleration = settings.acceleration;
        simCB.drag = settings.drag;
        simCB.startColor = settings.startColor;
        simCB.endColor = settings.endColor;
        simCB.startSize = settings.startSize;
        simCB.endSize = settings.endSize;
        simCB.rotationSpeedMin = settings.rotationSpeedMin;
        simCB.rotationSpeedMax = settings.rotationSpeedMax;
        simCB.spawnRadius = settings.spawnRadius;
        simCB.velocityAlignment = settings.velocityAlignment;
        simCB.maxParticles = m_MaxParticles;
        simCB.spawnCount = spawnCount;
        simCB.spawnOffset = m_CurrentSpawnIndex;
        simCB.emitterType = static_cast<uint32_t>(settings.type);

        m_CurrentSpawnIndex = (m_CurrentSpawnIndex + spawnCount) % m_MaxParticles;

        m_SimulateCB->SetData(Buffer(&simCB, sizeof(simCB)));

        // Barrier: Transition particle buffer to UAV for compute write
        {
            nri::BufferBarrierDesc bufBarrier = {};
            bufBarrier.buffer = m_ParticleBuffer;
            bufBarrier.before = { nri::AccessBits::SHADER_RESOURCE, nri::StageBits::VERTEX_SHADER };
            bufBarrier.after = { nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER };

            nri::BarrierDesc barrierDesc = {};
            barrierDesc.buffers = &bufBarrier;
            barrierDesc.bufferNum = 1;
            nri.CmdBarrier(*cmd, barrierDesc);
        }

        nri.CmdSetPipelineLayout(*cmd, nri::BindPoint::COMPUTE, *m_ComputePipelineLayout);
        nri.CmdSetPipeline(*cmd, *m_ComputePipeline);

        nri::SetDescriptorSetDesc setDesc = {};
        setDesc.setIndex = 0;
        setDesc.descriptorSet = m_ComputeBindingSet->GetHandle();
        setDesc.bindPoint = nri::BindPoint::COMPUTE;
        nri.CmdSetDescriptorSet(*cmd, setDesc);

        const uint32_t groupsX = (m_MaxParticles + 63) / 64;
        nri.CmdDispatch(*cmd, { groupsX, 1, 1 });

        // Transition particle buffer back to ShaderResource for vertex shader read
        {
            nri::BufferBarrierDesc bufBarrier = {};
            bufBarrier.buffer = m_ParticleBuffer;
            bufBarrier.before = { nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER };
            bufBarrier.after = { nri::AccessBits::SHADER_RESOURCE, nri::StageBits::VERTEX_SHADER };

            nri::BarrierDesc barrierDesc = {};
            barrierDesc.buffers = &bufBarrier;
            barrierDesc.bufferNum = 1;
            nri.CmdBarrier(*cmd, barrierDesc);
        }
    }

    void ParticleSystem::Render(nri::CommandBuffer *cmd, ICamera *camera, RenderTarget *framebuffer, const ParticleEmitterSettings &settings, uint32_t objectID)
    {
        IGN_PROFILE_FUNCTION();

        if (!cmd || !camera || !framebuffer || m_MaxParticles == 0)
            return;

        auto &nri = DeviceManager::GetInstance()->GetNRI();

        Ref<GraphicsPipeline> pipeline = GetOrCreateRenderPipeline(framebuffer, settings.blendMode);
        if (!pipeline || pipeline->GetHandle() == nullptr)
            return;

        // Resolve optional texture
        Ref<Texture> texture = nullptr;
        if (settings.textureHandle != AssetHandle(0))
        {
            texture = AssetManager::GetInstance()->GetAssetImmediate<Texture>(settings.textureHandle);
        }

        // Upload render params
        ParticleRenderCB renderCB;
        renderCB.invView = glm::inverse(camera->GetView());
        renderCB.viewportSize = glm::vec2(static_cast<float>(framebuffer->GetWidth()), static_cast<float>(framebuffer->GetHeight()));
        renderCB.useTexture = (texture && texture->IsReady()) ? 1 : 0;
        renderCB.objectID = objectID;
        m_RenderCB->SetData(Buffer(&renderCB, sizeof(renderCB)));

        FrameContext *frameContext = Renderer::GetCurrentFrameContext();
        UpdateRenderBindingSet(frameContext->cameraBuffer.GetDescriptor(), texture, pipeline->GetPipelineLayout());

        const bool startedRendering = !framebuffer->IsRendering();
        if (startedRendering)
            framebuffer->BeginRendering(cmd, false, false);

        nri.CmdSetPipelineLayout(*cmd, nri::BindPoint::GRAPHICS, *pipeline->GetPipelineLayout());
        nri.CmdSetPipeline(*cmd, *pipeline->GetHandle());

        nri::SetDescriptorSetDesc setDesc = {};
        setDesc.setIndex = 0;
        setDesc.descriptorSet = m_RenderBindingSet->GetHandle();
        setDesc.bindPoint = nri::BindPoint::GRAPHICS;
        nri.CmdSetDescriptorSet(*cmd, setDesc);

        nri::DrawDesc drawDesc = {};
        drawDesc.vertexNum = 6;
        drawDesc.instanceNum = m_MaxParticles;
        nri.CmdDraw(*cmd, drawDesc);

        if (startedRendering)
            framebuffer->EndRendering(cmd);
    }

    void ParticleSystem::Reset(nri::CommandBuffer *cmd)
    {
        if (!cmd || !m_ParticleBuffer || !m_ResetStagingBuffer)
            return;

        auto &nri = DeviceManager::GetInstance()->GetNRI();
        const uint64_t byteSize = static_cast<uint64_t>(m_MaxParticles) * sizeof(GPUParticle);

        nri::BufferBarrierDesc beforeBarrier = {};
        beforeBarrier.buffer = m_ParticleBuffer;
        beforeBarrier.before = { nri::AccessBits::SHADER_RESOURCE, nri::StageBits::VERTEX_SHADER };
        beforeBarrier.after = { nri::AccessBits::COPY_DESTINATION, nri::StageBits::COPY };
        nri::BarrierDesc barrierDesc = {};
        barrierDesc.buffers = &beforeBarrier;
        barrierDesc.bufferNum = 1;
        nri.CmdBarrier(*cmd, barrierDesc);

        nri.CmdCopyBuffer(*cmd, *m_ParticleBuffer, 0, *m_ResetStagingBuffer, 0, byteSize);

        nri::BufferBarrierDesc afterBarrier = {};
        afterBarrier.buffer = m_ParticleBuffer;
        afterBarrier.before = { nri::AccessBits::COPY_DESTINATION, nri::StageBits::COPY };
        afterBarrier.after = { nri::AccessBits::SHADER_RESOURCE, nri::StageBits::VERTEX_SHADER };
        barrierDesc.buffers = &afterBarrier;
        nri.CmdBarrier(*cmd, barrierDesc);

        m_CurrentSpawnIndex = 0;
        m_SpawnAccumulator = 0.0f;
        m_TotalTime = 0.0f;
    }
}
