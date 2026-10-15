// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_PARTICLE_SYSTEM_HPP
#define IGN_PARTICLE_SYSTEM_HPP

#include "ignite/asset/asset.hpp"
#include "ignite/core/base.hpp"
#include "ignite/core/types.hpp"

#include "ignite/graphics/hash_keys.hpp"
#include <nvrhi/nvrhi.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>

namespace ignite
{
    class GraphicsPipeline;
    class Texture;
    class ConstantBuffer;
    class ICamera;
    class Shader;

    enum class ParticleEmitterType : uint32_t
    {
        Smoke = 0,
        Fire = 1,
        Spark = 2,
        Custom = 3
    };

    enum class ParticleBlendMode : uint32_t
    {
        Alpha = 0,
        Additive = 1
    };

    struct ParticleEmitterSettings
    {
        ParticleEmitterType type = ParticleEmitterType::Smoke;
        ParticleBlendMode blendMode = ParticleBlendMode::Alpha;

        uint32_t maxParticles = 5000;
        float emissionRate = 60.0f; // particles per second

        float lifetimeMin = 1.0f;
        float lifetimeMax = 2.0f;

        glm::vec3 velocityMin = glm::vec3(-0.5f, 1.0f, -0.5f);
        glm::vec3 velocityMax = glm::vec3(0.5f, 2.5f, 0.5f);

        glm::vec3 acceleration = glm::vec3(0.0f, 0.5f, 0.0f);
        float drag = 0.1f;

        glm::vec4 startColor = glm::vec4(0.8f, 0.8f, 0.8f, 0.7f);
        glm::vec4 endColor = glm::vec4(0.3f, 0.3f, 0.3f, 0.0f);

        glm::vec2 startSize = glm::vec2(0.25f, 0.25f);
        glm::vec2 endSize = glm::vec2(1.2f, 1.2f);

        float rotationSpeedMin = -1.0f;
        float rotationSpeedMax = 1.0f;

        float spawnRadius = 0.2f;
        float velocityAlignment = 0.0f; // 0.0 = pure camera billboard, 1.0 = aligned along velocity vector (sparks)

        AssetHandle textureHandle = AssetHandle(0);

        static inline ParticleEmitterSettings CreateSmoke()
        {
            ParticleEmitterSettings s;
            s.type = ParticleEmitterType::Smoke;
            s.blendMode = ParticleBlendMode::Alpha;
            s.maxParticles = 3000;
            s.emissionRate = 45.0f;
            s.lifetimeMin = 2.0f;
            s.lifetimeMax = 3.5f;
            s.velocityMin = glm::vec3(-0.4f, 1.2f, -0.4f);
            s.velocityMax = glm::vec3(0.4f, 2.2f, 0.4f);
            s.acceleration = glm::vec3(0.0f, 0.2f, 0.0f); // Gentle upward draft
            s.drag = 0.25f;
            s.startColor = glm::vec4(0.7f, 0.7f, 0.7f, 0.6f);
            s.endColor = glm::vec4(0.25f, 0.25f, 0.25f, 0.0f);
            s.startSize = glm::vec2(0.25f, 0.25f);
            s.endSize = glm::vec2(1.5f, 1.5f);
            s.rotationSpeedMin = -0.5f;
            s.rotationSpeedMax = 0.5f;
            s.spawnRadius = 0.15f;
            s.velocityAlignment = 0.0f;
            return s;
        }

        static inline ParticleEmitterSettings CreateFire()
        {
            ParticleEmitterSettings s;
            s.type = ParticleEmitterType::Fire;
            s.blendMode = ParticleBlendMode::Additive;
            s.maxParticles = 5000;
            s.emissionRate = 120.0f;
            s.lifetimeMin = 0.4f;
            s.lifetimeMax = 0.9f;
            s.velocityMin = glm::vec3(-0.3f, 2.0f, -0.3f);
            s.velocityMax = glm::vec3(0.3f, 3.8f, 0.3f);
            s.acceleration = glm::vec3(0.0f, 1.5f, 0.0f); // Upward flame rush
            s.drag = 0.1f;
            s.startColor = glm::vec4(1.0f, 0.9f, 0.3f, 1.0f); // Blazing yellow core
            s.endColor = glm::vec4(0.9f, 0.15f, 0.0f, 0.0f);   // Fades to deep red
            s.startSize = glm::vec2(0.35f, 0.35f);
            s.endSize = glm::vec2(0.08f, 0.08f);             // Flame pinches out
            s.rotationSpeedMin = -1.5f;
            s.rotationSpeedMax = 1.5f;
            s.spawnRadius = 0.12f;
            s.velocityAlignment = 0.0f;
            return s;
        }

        static inline ParticleEmitterSettings CreateSpark()
        {
            ParticleEmitterSettings s;
            s.type = ParticleEmitterType::Spark;
            s.blendMode = ParticleBlendMode::Additive;
            s.maxParticles = 2000;
            s.emissionRate = 80.0f;
            s.lifetimeMin = 0.3f;
            s.lifetimeMax = 1.0f;
            s.velocityMin = glm::vec3(-3.0f, 2.5f, -3.0f);
            s.velocityMax = glm::vec3(3.0f, 6.0f, 3.0f);
            s.acceleration = glm::vec3(0.0f, -9.81f, 0.0f); // Gravity pulls sparks down!
            s.drag = 0.4f;
            s.startColor = glm::vec4(1.0f, 0.85f, 0.4f, 1.0f); // Brilliant hot spark
            s.endColor = glm::vec4(1.0f, 0.25f, 0.0f, 0.0f);
            s.startSize = glm::vec2(0.05f, 0.18f); // Stretched along velocity
            s.endSize = glm::vec2(0.02f, 0.05f);
            s.rotationSpeedMin = 0.0f;
            s.rotationSpeedMax = 0.0f;
            s.spawnRadius = 0.05f;
            s.velocityAlignment = 1.0f; // Stretches along trajectory
            return s;
        }
    };

    struct alignas(16) GPUParticle
    {
        glm::vec3 position{ 0.0f };
        float lifetime = 999999.0f; // Starts dead

        glm::vec3 velocity{ 0.0f };
        float maxLifetime = 1.0f;

        glm::vec4 color{ 1.0f };

        glm::vec2 size{ 1.0f };
        float rotation = 0.0f;
        float rotationalVelocity = 0.0f;

        glm::vec4 startColor{ 1.0f };
        glm::vec4 endColor{ 1.0f, 1.0f, 1.0f, 0.0f };

        glm::vec2 startSize{ 0.2f };
        glm::vec2 endSize{ 1.0f };

        float velocityAlignment = 0.0f;
        uint32_t emitterType = 0;
        glm::vec2 _pad{ 0.0f };
    };

    struct alignas(16) ParticleSimCB
    {
        glm::vec3 emitterPosition{ 0.0f };
        float deltaTime = 0.016f;

        glm::vec3 emitterVelocity{ 0.0f };
        float totalTime = 0.0f;

        glm::vec3 velocityMin{ -0.5f, 1.0f, -0.5f };
        float lifetimeMin = 1.0f;

        glm::vec3 velocityMax{ 0.5f, 2.5f, 0.5f };
        float lifetimeMax = 2.0f;

        glm::vec3 acceleration{ 0.0f, 0.5f, 0.0f };
        float drag = 0.1f;

        glm::vec4 startColor{ 1.0f };
        glm::vec4 endColor{ 0.0f };

        glm::vec2 startSize{ 0.2f };
        glm::vec2 endSize{ 1.0f };

        float rotationSpeedMin = -1.0f;
        float rotationSpeedMax = 1.0f;
        float spawnRadius = 0.2f;
        float velocityAlignment = 0.0f;

        uint32_t maxParticles = 5000;
        uint32_t spawnCount = 0;
        uint32_t spawnOffset = 0;
        uint32_t emitterType = 0;
    };

    struct alignas(16) ParticleRenderCB
    {
        glm::mat4 invView{ 1.0f };
        glm::vec2 viewportSize{ 1280.0f, 720.0f };
        uint32_t useTexture = 0;
        uint32_t objectID = static_cast<uint32_t>(-1);
    };

    class IGN_API ParticleSystem
    {
    public:
        ParticleSystem(uint32_t maxParticles = 5000);
        ~ParticleSystem();

        void Init(uint32_t maxParticles);
        void Simulate(nvrhi::ICommandList *cmd, float dt, const glm::vec3 &emitterPos, const ParticleEmitterSettings &settings);
        void Render(nvrhi::ICommandList *cmd, ICamera *camera, nvrhi::IFramebuffer *framebuffer, const ParticleEmitterSettings &settings, uint32_t objectID);
        void Reset(nvrhi::ICommandList *cmd);

        uint32_t GetMaxParticles() const { return m_MaxParticles; }
        uint32_t GetCurrentSpawnIndex() const { return m_CurrentSpawnIndex; }

        static Ref<ParticleSystem> Create(uint32_t maxParticles = 5000);

    private:
        void CreateBuffers(uint32_t maxParticles);
        void CreatePipelines();
        Ref<GraphicsPipeline> GetOrCreateRenderPipeline(nvrhi::IFramebuffer *framebuffer, ParticleBlendMode blendMode);
        void UpdateRenderBindingSet(const nvrhi::BufferHandle &cameraBuffer, Ref<Texture> texture);

    private:
        uint32_t m_MaxParticles = 5000;
        uint32_t m_CurrentSpawnIndex = 0;
        float m_SpawnAccumulator = 0.0f;
        float m_TotalTime = 0.0f;
        glm::vec3 m_LastEmitterPosition{ 0.0f };
        uint64_t m_LastSimulatedFrame = UINT64_MAX;

        nvrhi::BufferHandle m_ParticleBuffer;
        Ref<ConstantBuffer> m_SimulateCB;
        Ref<ConstantBuffer> m_RenderCB;

        // Compute Pipeline for Simulation
        Ref<Shader> m_ComputeShader;
        nvrhi::BindingLayoutHandle m_ComputeBindingLayout;
        nvrhi::BindingSetHandle m_ComputeBindingSet;
        nvrhi::ComputePipelineHandle m_ComputePipeline;

        // Render Pipelines (Alpha and Additive caches keyed by Framebuffer format)
        Ref<Shader> m_VertexShader;
        Ref<Shader> m_PixelShader;
        nvrhi::BindingLayoutHandle m_RenderBindingLayout;
        nvrhi::BindingSetHandle m_RenderBindingSet;
        std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> m_RenderPipelineAlphaCache;
        std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> m_RenderPipelineAdditiveCache;
        Ref<GraphicsPipeline> m_RenderPipelineAlpha;
        Ref<GraphicsPipeline> m_RenderPipelineAdditive;
        nvrhi::SamplerHandle m_LinearSampler;

        nvrhi::IBuffer *m_LastCameraBuffer = nullptr;
        nvrhi::ITexture *m_LastBoundTexture = nullptr;
    };
}

#endif
