// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "particle_system.hpp"
#include "ignite/graphics/graphics_pipeline.hpp"
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
        m_ParticleBuffer = nullptr;
        m_SimulateCB = nullptr;
        m_RenderCB = nullptr;
        m_ComputePipeline = nullptr;
        m_ComputeBindingSet = nullptr;
        m_ComputeBindingLayout = nullptr;
        m_RenderBindingSet = nullptr;
        m_RenderBindingLayout = nullptr;
        m_RenderPipelineAlpha = nullptr;
        m_RenderPipelineAdditive = nullptr;
        m_LinearSampler = nullptr;
    }

    Ref<ParticleSystem> ParticleSystem::Create(uint32_t maxParticles)
    {
        return CreateRef<ParticleSystem>(maxParticles);
    }

    void ParticleSystem::Init(uint32_t maxParticles)
    {
        m_MaxParticles = maxParticles;
        CreateBuffers(maxParticles);
        CreatePipelines();
    }

    void ParticleSystem::CreateBuffers(uint32_t maxParticles)
    {
        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();

        // 1. GPU StructuredBuffer for Particles (UAV for compute, SRV for vertex shader)
        nvrhi::BufferDesc bufferDesc;
        bufferDesc.byteSize = static_cast<uint64_t>(maxParticles) * sizeof(GPUParticle);
        bufferDesc.structStride = sizeof(GPUParticle);
        bufferDesc.canHaveUAVs = true;
        bufferDesc.initialState = nvrhi::ResourceStates::ShaderResource;
        bufferDesc.keepInitialState = true;
        bufferDesc.debugName = "GPU Particle StructuredBuffer";
        m_ParticleBuffer = device->createBuffer(bufferDesc);
        LOG_ASSERT(m_ParticleBuffer, "[ParticleSystem] Failed to create particle structured buffer");

        // 2. Constant Buffers
        m_SimulateCB = ConstantBuffer::Create(sizeof(ParticleSimCB), true, 16, "[Particle System] Sim CB");
        m_RenderCB = ConstantBuffer::Create(sizeof(ParticleRenderCB), true, 16, "[Particle System] Render CB");

        // 3. Bilinear Clamp Sampler for textures
        nvrhi::SamplerDesc samplerDesc;
        samplerDesc.setAllFilters(true);
        samplerDesc.setAllAddressModes(nvrhi::SamplerAddressMode::Clamp);
        m_LinearSampler = device->createSampler(samplerDesc);
    }

    void ParticleSystem::CreatePipelines()
    {
        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();

        // Load / Compile Shaders
        m_ComputeShader = Shader::Create("resources/shaders/particle.compute.hlsl", UMBRA_SHADER_TYPE_COMPUTE, true);
        m_VertexShader = Shader::Create("resources/shaders/particle.vertex.hlsl", UMBRA_SHADER_TYPE_VERTEX, true);
        m_PixelShader = Shader::Create("resources/shaders/particle.pixel.hlsl", UMBRA_SHADER_TYPE_PIXEL, true);

        // Compute Pipeline
        {
            nvrhi::BindingLayoutDesc computeLayoutDesc;
            computeLayoutDesc.visibility = nvrhi::ShaderType::Compute;
            computeLayoutDesc.bindings =
            {
                nvrhi::BindingLayoutItem::VolatileConstantBuffer(0),
                nvrhi::BindingLayoutItem::StructuredBuffer_UAV(0)
            };
            m_ComputeBindingLayout = device->createBindingLayout(computeLayoutDesc);

            nvrhi::BindingSetDesc computeSetDesc;
            computeSetDesc.bindings =
            {
                nvrhi::BindingSetItem::ConstantBuffer(0, m_SimulateCB->GetHandle()),
                nvrhi::BindingSetItem::StructuredBuffer_UAV(0, m_ParticleBuffer)
            };
            m_ComputeBindingSet = device->createBindingSet(computeSetDesc, m_ComputeBindingLayout);

            nvrhi::ComputePipelineDesc computePipeDesc;
            computePipeDesc.CS = m_ComputeShader->GetHandle();
            computePipeDesc.bindingLayouts = { m_ComputeBindingLayout };
            m_ComputePipeline = device->createComputePipeline(computePipeDesc);
            LOG_ASSERT(m_ComputePipeline, "[ParticleSystem] Failed to create compute pipeline");
        }

        // Render Binding Layout
        {
            nvrhi::BindingLayoutDesc renderLayoutDesc;
            renderLayoutDesc.visibility = nvrhi::ShaderType::All;
            renderLayoutDesc.bindings =
            {
                nvrhi::BindingLayoutItem::VolatileConstantBuffer(0), // Camera buffer (b0)
                nvrhi::BindingLayoutItem::VolatileConstantBuffer(1), // Particle render params (b1)
                nvrhi::BindingLayoutItem::StructuredBuffer_SRV(0),    // Particles (t0)
                nvrhi::BindingLayoutItem::Texture_SRV(1),             // Texture (t1)
                nvrhi::BindingLayoutItem::Sampler(0)                  // Sampler (s0)
            };
            m_RenderBindingLayout = device->createBindingLayout(renderLayoutDesc);
        }
    }

    Ref<GraphicsPipeline> ParticleSystem::GetOrCreateRenderPipeline(nvrhi::IFramebuffer *framebuffer, ParticleBlendMode blendMode)
    {
        if (!framebuffer)
            return nullptr;

        auto key = MakeFramebufferKey(framebuffer);
        auto &cache = (blendMode == ParticleBlendMode::Alpha) ? m_RenderPipelineAlphaCache : m_RenderPipelineAdditiveCache;
        auto it = cache.find(key);
        if (it != cache.end())
            return it->second;

        const nvrhi::FramebufferDesc &fbDesc = framebuffer->getDesc();
        bool hasDepthAttachment = fbDesc.depthAttachment.texture != nullptr;

        GraphicsPipelineParams params;
        params.enableDepthTest = hasDepthAttachment;
        params.enableDepthWrite = false; // Transparent particles don't write to depth buffer
        params.depthFunc = hasDepthAttachment ? nvrhi::ComparisonFunc::LessOrEqual : nvrhi::ComparisonFunc::Always;
        params.cullMode = nvrhi::RasterCullMode::None;
        params.fillMode = nvrhi::RasterFillMode::Solid;
        params.enableBlend = true;

        if (blendMode == ParticleBlendMode::Alpha)
        {
            params.srcBlend = nvrhi::BlendFactor::SrcAlpha;
            params.destBlend = nvrhi::BlendFactor::InvSrcAlpha;
            params.srcBlendAlpha = nvrhi::BlendFactor::One;
            params.destBlendAlpha = nvrhi::BlendFactor::InvSrcAlpha;
        }
        else // Additive
        {
            params.srcBlend = nvrhi::BlendFactor::SrcAlpha;
            params.destBlend = nvrhi::BlendFactor::One;
            params.srcBlendAlpha = nvrhi::BlendFactor::One;
            params.destBlendAlpha = nvrhi::BlendFactor::One;
        }

        std::string name = (blendMode == ParticleBlendMode::Alpha) ? "Particle Pipeline (Alpha)" : "Particle Pipeline (Additive)";
        Ref<GraphicsPipeline> pipeline = GraphicsPipeline::Create(name);
        pipeline->SetShaders({ m_VertexShader, m_PixelShader })
                 .AddBindingLayout(m_RenderBindingLayout)
                 .Build(framebuffer, params);

        cache.emplace(key, pipeline);
        return pipeline;
    }

    void ParticleSystem::UpdateRenderBindingSet(const nvrhi::BufferHandle &cameraBuffer, Ref<Texture> texture)
    {
        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();

        nvrhi::TextureHandle texHandle = (texture && texture->IsReady()) ? texture->GetHandle() : Renderer::GetWhiteTexture()->GetHandle();

        if (m_RenderBindingSet && m_LastCameraBuffer == cameraBuffer.Get() && m_LastBoundTexture == texHandle.Get())
            return;

        m_LastCameraBuffer = cameraBuffer.Get();
        m_LastBoundTexture = texHandle.Get();

        nvrhi::BindingSetDesc setDesc;
        setDesc.bindings =
        {
            nvrhi::BindingSetItem::ConstantBuffer(0, cameraBuffer),
            nvrhi::BindingSetItem::ConstantBuffer(1, m_RenderCB->GetHandle()),
            nvrhi::BindingSetItem::StructuredBuffer_SRV(0, m_ParticleBuffer),
            nvrhi::BindingSetItem::Texture_SRV(1, texHandle),
            nvrhi::BindingSetItem::Sampler(0, m_LinearSampler)
        };
        m_RenderBindingSet = device->createBindingSet(setDesc, m_RenderBindingLayout);
        LOG_ASSERT(m_RenderBindingSet, "[ParticleSystem] Failed to create render binding set");
    }

    void ParticleSystem::Simulate(nvrhi::ICommandList *cmd, float dt, const glm::vec3 &emitterPos, const ParticleEmitterSettings &settings)
    {
        IGN_PROFILE_FUNCTION();

        if (dt <= 0.0f || m_MaxParticles == 0)
            return;

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

        m_SimulateCB->SetData(cmd, &simCB, sizeof(simCB));

        // Barrier: Transition particle buffer to UAV for compute write
        cmd->setBufferState(m_ParticleBuffer, nvrhi::ResourceStates::UnorderedAccess);
        cmd->commitBarriers();

        nvrhi::ComputeState compState;
        compState.pipeline = m_ComputePipeline;
        compState.bindings = { m_ComputeBindingSet };
        cmd->setComputeState(compState);

        const uint32_t groupsX = (m_MaxParticles + 63) / 64;
        cmd->dispatch(groupsX, 1, 1);

    }

    void ParticleSystem::Render(nvrhi::ICommandList *cmd, ICamera *camera, nvrhi::IFramebuffer *framebuffer, const ParticleEmitterSettings &settings, uint32_t objectID)
    {
        IGN_PROFILE_FUNCTION();

        if (!camera || !framebuffer || m_MaxParticles == 0)
            return;

        // Transition particle buffer to ShaderResource for vertex shader read
        cmd->setBufferState(m_ParticleBuffer, nvrhi::ResourceStates::ShaderResource);
        cmd->commitBarriers();

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
        renderCB.viewportSize = glm::vec2(static_cast<float>(framebuffer->getFramebufferInfo().width),static_cast<float>(framebuffer->getFramebufferInfo().height));
        renderCB.useTexture = (texture && texture->IsReady()) ? 1 : 0;
        renderCB.objectID = objectID;
        m_RenderCB->SetData(cmd, &renderCB, sizeof(renderCB));

        FrameContext *frameContext = Renderer::GetCurrentFrameContext();
        UpdateRenderBindingSet(frameContext->cameraBuffer.GetHandle(), texture);

        nvrhi::GraphicsState state;
        state.pipeline = *pipeline;
        state.framebuffer = framebuffer;
        state.viewport = nvrhi::ViewportState().addViewportAndScissorRect(framebuffer->getFramebufferInfo().getViewport());
        state.bindings = { m_RenderBindingSet };

        cmd->setGraphicsState(state);

        nvrhi::DrawArguments args;
        args.vertexCount = 6;
        args.instanceCount = m_MaxParticles;
        cmd->draw(args);
    }

    void ParticleSystem::Reset(nvrhi::ICommandList *cmd)
    {
        std::vector<GPUParticle> initial(m_MaxParticles);
        for (auto &p : initial)
        {
            p.lifetime = 999999.0f;
            p.maxLifetime = 0.0f;
            p.size = glm::vec2(0.0f);
        }

        cmd->writeBuffer(m_ParticleBuffer, initial.data(), initial.size() * sizeof(GPUParticle));
        m_CurrentSpawnIndex = 0;
        m_SpawnAccumulator = 0.0f;
        m_TotalTime = 0.0f;
    }
}
