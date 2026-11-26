// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_SSR_HPP
#define IGN_SSR_HPP

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

    // Half-resolution Hi-Z Screen-Space Reflections (SSR) compute pipeline:
    //   1. Hi-Z min-depth pyramid (full-res HW depth -> half-res R32F mip chain)
    //   2. Half-res Hi-Z ray trace against previous-frame HDR scene color
    //   3. Edge-aware bilateral spatial denoise -> bindless RGBA16F output
    class IGN_API SSR
    {
    public:
        static constexpr uint32_t kMaxDepthMips = 6;

        SSR(uint32_t fullWidth, uint32_t fullHeight);
        ~SSR();

        SSR(const SSR &) = delete;
        SSR &operator=(const SSR &) = delete;

        void Resize(uint32_t fullWidth, uint32_t fullHeight);
        void Build(nri::CommandBuffer *cmd,
            const Ref<Texture> &depthTexture,
            const Ref<Texture> &normalTexture,
            const Ref<Texture> &prevSceneColor,
            ICamera *camera,
            const glm::mat4 &prevViewProj,
            float maxRoughness,
            uint64_t frameIndex);

        Ref<Texture> GetSSRTexture() const { return m_SSRFinal; }
        uint32_t GetHalfWidth() const { return m_HalfWidth; }
        uint32_t GetHalfHeight() const { return m_HalfHeight; }

    private:
        struct HiZCB
        {
            glm::uvec2 srcSize = glm::uvec2(1u);
            glm::uvec2 dstSize = glm::uvec2(1u);
            uint32_t passMode = 0;
            glm::uvec3 _pad = glm::uvec3(0u);
        };

        struct TraceCB
        {
            glm::mat4 projection = glm::mat4(1.0f);
            glm::mat4 projectionInv = glm::mat4(1.0f);
            glm::mat4 viewInv = glm::mat4(1.0f);
            glm::mat4 prevViewProj = glm::mat4(1.0f);
            glm::vec2 invHalfRes = glm::vec2(1.0f);
            glm::vec2 fullRes = glm::vec2(1.0f);
            glm::uvec2 halfRes = glm::uvec2(1u);
            uint32_t maxDepthMip = 0;
            uint32_t frameIndex = 0;
            float nearPlane = 0.1f;
            float farPlane = 500.0f;
            float maxRoughness = 0.6f;
            float _pad = 0.0f;
        };

        struct DenoiseCB
        {
            glm::mat4 projectionInv = glm::mat4(1.0f);
            glm::uvec2 halfRes = glm::uvec2(1u);
            glm::uvec2 fullRes = glm::uvec2(1u);
            glm::vec2 invHalfRes = glm::vec2(1.0f);
            float depthSigma = 0.04f;
            float _pad = 0.0f;
        };

        void CreateTextures(uint32_t fullWidth, uint32_t fullHeight);
        void EnsurePipelines();
        void InvalidatePipelines();

        uint32_t m_FullWidth = 0;
        uint32_t m_FullHeight = 0;
        uint32_t m_HalfWidth = 0;
        uint32_t m_HalfHeight = 0;
        uint32_t m_DepthMipCount = 1;

        Ref<Texture> m_HiZDepth;
        Ref<Texture> m_SSRRaw;
        Ref<Texture> m_SSRFinal;

        BindingLayoutDesc m_HiZLayout;
        BindingLayoutDesc m_TraceLayout;
        BindingLayoutDesc m_DenoiseLayout;

        nri::PipelineLayout *m_HiZPipelineLayout = nullptr;
        nri::PipelineLayout *m_TracePipelineLayout = nullptr;
        nri::PipelineLayout *m_DenoisePipelineLayout = nullptr;

        nri::Pipeline *m_HiZPipeline = nullptr;
        nri::Pipeline *m_TracePipeline = nullptr;
        nri::Pipeline *m_DenoisePipeline = nullptr;

        nri::Descriptor *m_LinearClampSampler = nullptr;

        std::array<Ref<ConstantBuffer>, kMaxDepthMips> m_HiZCBs;
        Ref<ConstantBuffer> m_TraceCB;
        Ref<ConstantBuffer> m_DenoiseCB;

        Ref<Shader> m_HiZShader;
        Ref<Shader> m_TraceShader;
        Ref<Shader> m_DenoiseShader;
    };
}

#endif
