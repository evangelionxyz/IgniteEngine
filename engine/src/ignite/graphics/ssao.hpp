// Copyright (c) 2026 Evangelion Manuhutu

#ifndef SSAO_HPP
#define SSAO_HPP

#include "render_target.hpp"
#include "buffers/constant_buffer.hpp"
#include "ignite/scene/icamera.hpp"

#include <vector>

namespace ignite
{
    class ICamera;
    class Shader;
    class VertexBuffer;
    class Texture;

    class SSAO
    {
    public:
        SSAO(uint32_t width, uint32_t height);
        ~SSAO();

        void Build(nvrhi::ICommandList *cmd, const Ref<Texture> &depthTexture, ICamera *camera,
            const PostProcessing &settings, const Ref<VertexBuffer> &fullscreenVertexBuffer);
        void Resize(uint32_t width, uint32_t height);
        Ref<Texture> GetAOTexture() const;

    private:
        struct SSAOParams
        {
            glm::mat4 projection;
            glm::mat4 projectionInv;
            glm::vec4 samples[32];
            glm::vec4 params; // x=radius, y=bias, z=power, w=_padding
            glm::vec4 noiseScale; // x=noiseScaleX, y=noiseScaleY, z=padding, w=padding
        };

        struct BlurParams
        {
            float horizontal;
            float _padding[3];
        };

        void CreateTextures(uint32_t width, uint32_t height);
        void InvalidatePipelines();
        void EnsurePipelines();
        void BuildKernel();
        void BuildNoise();

        uint32_t m_Width = 0;
        uint32_t m_Height = 0;

        Ref<Texture> m_AOTex;
        Ref<Texture> m_BlurTex;

        Ref<Texture> m_NoiseTexture;
        std::vector<glm::vec4> m_Kernel;

        nvrhi::BindingLayoutHandle m_AOLayout;
        nvrhi::BindingLayoutHandle m_BlurLayout;

        nvrhi::ComputePipelineHandle m_AOComputePipeline;
        nvrhi::ComputePipelineHandle m_BlurComputePipeline;
        
        nvrhi::SamplerHandle m_ClampSampler;
        nvrhi::SamplerHandle m_RepeatSampler;

        Ref<ConstantBuffer> m_SSAOParamsBuffer;
        Ref<ConstantBuffer> m_BlurParamsBuffer;

        Ref<Shader> m_AOComputeShader;
        Ref<Shader> m_BlurComputeShader;
    };
}

#endif