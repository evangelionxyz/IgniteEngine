// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_GTAO_HPP
#define IGN_GTAO_HPP

#include "ignite/core/base.hpp"
#include "ignite/core/types.hpp"
#include "ignite/graphics/binding_set.hpp"
#include "ignite/graphics/buffers/constant_buffer.hpp"
#include "ignite/scene/icamera.hpp"

#include <NRI.h>
#include <glm/glm.hpp>
#include <array>
#include <cstdint>

namespace ignite
{
    class Shader;
    class Texture;

    namespace gi
    {
        // Jimenez 2016 multi-bounce ambient occlusion approximation.
        IGN_API glm::vec3 MultiBounceAO(float ao, const glm::vec3 &albedo);
    }

    struct GTAOQualitySettings
    {
        uint32_t sliceCount = 3;
        uint32_t stepsPerSlice = 3;
        uint32_t denoiseRadius = 3;
        uint32_t upsampleMode = 1; // 0 = half-res bilinear, 1 = fast 2x2 bilateral, 2 = full 3x3 bilateral
    };

    // Half-resolution XeGTAO compute pipeline:
    //   1. Depth prefilter (full-res HW depth -> scaled linear viewZ)
    //   2. GTAO main pass (scalable slices x steps per side, optional prepass view-space normals)
    //   3. Separable bilateral edge-aware denoise (H + V) + optional bilateral upsample
    class IGN_API GTAO
    {
    public:
        static constexpr uint32_t kMaxDepthMips = 5;

        GTAO(uint32_t fullWidth, uint32_t fullHeight, float resolutionScale = 0.5f);
        ~GTAO();

        GTAO(const GTAO &) = delete;
        GTAO &operator=(const GTAO &) = delete;

        void Resize(uint32_t fullWidth, uint32_t fullHeight, float resolutionScale = 0.5f);
        void Build(nri::CommandBuffer *cmd, const Ref<Texture> &depthTexture, const Ref<Texture> &normalTexture,
            ICamera *camera, const PostProcessing &settings, uint64_t taaFrameIndex,
            const GTAOQualitySettings &quality = {});

        Ref<Texture> GetAOTexture() const { return m_LastOutputTexture ? m_LastOutputTexture : m_AOTexB; }
        uint32_t GetHalfWidth() const { return m_HalfWidth; }
        uint32_t GetHalfHeight() const { return m_HalfHeight; }
        uint32_t GetDepthMipCount() const { return m_DepthMipCount; }

    private:
        struct PrefilterCB
        {
            glm::mat4 projectionInv = glm::mat4(1.0f);
            glm::uvec2 srcSize = glm::uvec2(1u);
            glm::uvec2 dstSize = glm::uvec2(1u);
            float nearPlane = 0.1f;
            float farPlane = 500.0f;
            uint32_t passMode = 0;
            float effectRadius = 0.65f;
};
        struct MainCB
        {
            glm::vec2 ndcToViewMul = glm::vec2(1.0f);
            glm::vec2 ndcToViewAdd = glm::vec2(0.0f);
            glm::vec2 invHalfRes = glm::vec2(1.0f);
            glm::vec2 fullRes = glm::vec2(1.0f);
            glm::uvec2 halfRes = glm::uvec2(1u);
            uint32_t maxDepthMip = 0;
            uint32_t noiseIndex = 0;
            float effectRadius = 0.65f;
            float falloffRange = 0.615f;
            float radiusMultiplier = 1.457f;
            float sampleDistributionPower = 2.0f;
            float thinOccluderCompensation = 0.1f;
            float projScaleY = 1.0f;
            float farPlane = 500.0f;
            uint32_t flags = 0;
            uint32_t sliceCount = 3;
            uint32_t stepsPerSlice = 3;
            glm::uvec2 _pad = glm::uvec2(0u);
        };

        struct DenoiseCB
        {
            glm::mat4 projectionInv = glm::mat4(1.0f);
            glm::uvec2 halfRes = glm::uvec2(1u);
            glm::uvec2 fullRes = glm::uvec2(1u);
            glm::ivec2 direction = glm::ivec2(1, 0);
            float depthSigma = 0.08f;
            float farPlane = 500.0f;
            float nearPlane = 0.1f;
            uint32_t hasNormals = 0;
            uint32_t passMode = 0;
            uint32_t denoiseRadius = 3;
        };

        void CreateTextures(uint32_t fullWidth, uint32_t fullHeight, float resolutionScale);
        void EnsurePipelines();
        void InvalidatePipelines();

        uint32_t m_FullWidth = 0;
        uint32_t m_FullHeight = 0;
        uint32_t m_HalfWidth = 0;
        uint32_t m_HalfHeight = 0;
        uint32_t m_DepthMipCount = 1;
        float m_ResolutionScale = 0.5f;

        Ref<Texture> m_PrefilteredDepth;
        Ref<Texture> m_AOTexA;
        Ref<Texture> m_AOTexHalfB;
        Ref<Texture> m_AOTexB;
        Ref<Texture> m_LastOutputTexture;

        BindingLayoutDesc m_PrefilterLayout;
        BindingLayoutDesc m_MainLayout;
        BindingLayoutDesc m_DenoiseLayout;

        nri::PipelineLayout *m_PrefilterPipelineLayout = nullptr;
        nri::PipelineLayout *m_MainPipelineLayout = nullptr;
        nri::PipelineLayout *m_DenoisePipelineLayout = nullptr;

        nri::Pipeline *m_PrefilterPipeline = nullptr;
        nri::Pipeline *m_MainPipeline = nullptr;
        nri::Pipeline *m_DenoisePipeline = nullptr;

        nri::Descriptor *m_PointClampSampler = nullptr;

        std::array<Ref<ConstantBuffer>, kMaxDepthMips> m_PrefilterCBs;
        Ref<ConstantBuffer> m_MainCB;
        Ref<ConstantBuffer> m_DenoiseHCB;
        Ref<ConstantBuffer> m_DenoiseVCB;
        Ref<ConstantBuffer> m_DenoiseUpsampleCB;

        Ref<Shader> m_PrefilterShader;
        Ref<Shader> m_MainShader;
        Ref<Shader> m_DenoiseShader;
    };
}

#endif
