// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_BLOOM_HPP
#define IGN_BLOOM_HPP

#include "render_target.hpp"
#include "buffers/constant_buffer.hpp"

#include <vector>

namespace ignite
{
    class Shader;
    class VertexBuffer;
    class Texture;

    struct BloomSettings
    {
        int iterations = 6;        // More levels for higher quality
        float threshold = 1.0f;    // HDR threshold
        float knee = 0.5f;         // Soft knee for smooth transition
        float radius = 1.0f;       // Blur radius multiplier
        float intensity = 5.0f;    // Final bloom intensity
    };

    class Bloom
    {
    public:
        Bloom(int width, int height);
        ~Bloom();

        void Build(nvrhi::ICommandList *cmd, const Ref<Texture> &sourceTexture, const Ref<VertexBuffer> &fullscreenVertexBuffer);
        void Resize(uint32_t width, uint32_t height);
        Ref<Texture> GetBloomTexture() const;

        BloomSettings settings;

    private:
        struct Level
        {
            int width = 0;
            int height = 0;

            // Three framebuffers for high quality bloom
            Ref<RenderTarget> downsampledRT; // Downsampled result
            Ref<RenderTarget> blurHorizontalRT; // Horizonal blur result
            Ref<RenderTarget> blurVerticalRT; // Final vertial blur result
        };

        struct DownsampleParams
        {
            float threshold = 1.0f;
            float intensity = 1.0f;
            float knee = 0.5f;
            float _padding = 0.0f;
        };

        struct BlurParams
        {
            int horizontal = 1;
            float _padding[3] = { 0.0f, 0.0f, 0.0f };
        };

        struct UpsampleParams
        {
            float radius = 1.0f;
            float _padding[3] = { 0.0f, 0.0f, 0.0f };
        };

        void CreateRenderTargets(uint32_t width, uint32_t height);
        void InvalidatePipelines();
        void EnsurePipelines();
        void DrawFullscreen(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer,
            nvrhi::GraphicsPipelineHandle pipeline, nvrhi::BindingSetHandle bindingSet,
            const Ref<VertexBuffer> &fullscreenVertexBuffer) const;

        std::vector<Level> m_Levels;
        Ref<RenderTarget> m_FinalRT;
        uint32_t m_Width = 0;
        uint32_t m_Height = 0;

        nvrhi::BindingLayoutHandle m_DownsampleLayout;
        nvrhi::BindingLayoutHandle m_BlurLayout;
        nvrhi::BindingLayoutHandle m_UpsampleLayout;

        nvrhi::GraphicsPipelineHandle m_DownsamplePipeline;
        nvrhi::GraphicsPipelineHandle m_BlurPipeline;
        nvrhi::GraphicsPipelineHandle m_UpsamplePipeline;
        nvrhi::InputLayoutHandle m_InputLayout;
        nvrhi::SamplerHandle m_Sampler;

        Ref<ConstantBuffer> m_DownsampleParamsBuffer;
        Ref<ConstantBuffer> m_BlurParamsBuffer;
        Ref<ConstantBuffer> m_UpsampleParamsBuffer;

        Ref<Shader> m_FullscreenVertexShader;
        Ref<Shader> m_DownsamplePixelShader;
        Ref<Shader> m_BlurPixelShader;
        Ref<Shader> m_UpsamplePixelShader;
    };
}

#endif