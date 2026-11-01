// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_BLOOM_HPP
#define IGN_BLOOM_HPP

#include "render_target.hpp"
#include "graphics_pipeline.hpp"
#include "buffers/constant_buffer.hpp"

#include <NRI.h>
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
        float radius = 1.25f;      // Blur radius multiplier
        float intensity = 1.5f;    // Final composite intensity
    };

    class Bloom
    {
    public:
        Bloom(int width, int height);
        ~Bloom();

        void Build(nri::CommandBuffer *cmd, const Ref<Texture> &sourceTexture, const Ref<VertexBuffer> &fullscreenVertexBuffer);
        void Resize(uint32_t width, uint32_t height);
        Ref<Texture> GetBloomTexture() const;

        BloomSettings settings;

    private:
        struct Level
        {
            int width = 0;
            int height = 0;

            // Four render targets for high quality bloom
            Ref<RenderTarget> downsampledRT;    // Downsampled result
            Ref<RenderTarget> upsampledRT;      // Upsampled result
            Ref<RenderTarget> blurHorizontalRT; // Horizontal blur result
            Ref<RenderTarget> blurVerticalRT;   // Final vertical blur result

            Ref<ConstantBuffer> downsampleParamsBuffer;
            Ref<ConstantBuffer> upsampleParamsBuffer;
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
            float radius = 1.0f;
            float _padding[2] = { 0.0f, 0.0f };
        };

        struct UpsampleParams
        {
            float radius = 1.0f;
            float _padding[3] = { 0.0f, 0.0f, 0.0f };
        };

        void CreateRenderTargets(uint32_t width, uint32_t height);
        void InvalidatePipelines();
        void EnsurePipelines();
        void DrawFullscreen(nri::CommandBuffer *cmd, RenderTarget *renderTarget,
            const Ref<GraphicsPipeline> &pipeline, nri::DescriptorSet *bindingSet,
            const Ref<VertexBuffer> &fullscreenVertexBuffer) const;

        std::vector<Level> m_Levels;
        Ref<RenderTarget> m_FinalRT;
        uint32_t m_Width = 0;
        uint32_t m_Height = 0;

        BindingLayoutDesc m_DownsampleLayout;
        BindingLayoutDesc m_BlurLayout;
        BindingLayoutDesc m_UpsampleLayout;

        Ref<GraphicsPipeline> m_DownsamplePipeline;
        Ref<GraphicsPipeline> m_BlurPipeline;
        Ref<GraphicsPipeline> m_UpsamplePipeline;
        nri::Descriptor *m_Sampler = nullptr;

        Ref<ConstantBuffer> m_BlurHorizontalParamsBuffer;
        Ref<ConstantBuffer> m_BlurVerticalParamsBuffer;
        Ref<ConstantBuffer> m_SingleLevelUpsampleParamsBuffer;

        Ref<Shader> m_FullscreenVertexShader;
        Ref<Shader> m_DownsamplePixelShader;
        Ref<Shader> m_BlurPixelShader;
        Ref<Shader> m_UpsamplePixelShader;
    };
}

#endif
