// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_SSAO_HPP
#define IGN_SSAO_HPP

#include "render_target.hpp"
#include "binding_set.hpp"
#include "buffers/constant_buffer.hpp"
#include "ignite/scene/icamera.hpp"

#include <NRI.h>
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
        SSAO(nri::CommandBuffer *cmd, uint32_t width, uint32_t height);
        ~SSAO();

        void Build(nri::CommandBuffer *cmd, const Ref<Texture> &depthTexture, ICamera *camera,
            const PostProcessing &settings, const Ref<VertexBuffer> &fullscreenVertexBuffer);

        void Resize(uint32_t width, uint32_t height);
        Ref<Texture> GetAOTexture() const;

    private:
        struct SSAOParams
        {
            glm::mat4 projection;
            glm::mat4 projectionInv;
            glm::vec4 samples[32];
            glm::vec4 params;     // x=radius, y=bias, z=power, w=_padding
            glm::vec4 noiseScale; // x=noiseScaleX, y=noiseScaleY, z=padding, w=padding
        };

        struct BlurParams
        {
            float horizontal;     // 1.0 = horizontal pass, 0.0 = vertical pass
            float nearPlane;      // camera near plane for depth linearization
            float farPlane;       // camera far plane for depth linearization
            float depthSharpness; // bilateral weight sharpness (higher = more edge-preserving)
        };

        void CreateTextures(uint32_t width, uint32_t height);
        void InvalidatePipelines();
        void EnsurePipelines();
        void BuildKernel();
        void BuildNoise(nri::CommandBuffer *cmd);

        uint32_t m_Width = 0;
        uint32_t m_Height = 0;

        Ref<Texture> m_AOTex;
        Ref<Texture> m_BlurTex;

        Ref<Texture> m_NoiseTexture;
        std::vector<glm::vec4> m_Kernel;

        BindingLayoutDesc m_AOLayout;
        BindingLayoutDesc m_BlurLayout;

        nri::PipelineLayout *m_AOPipelineLayout = nullptr;
        nri::PipelineLayout *m_BlurPipelineLayout = nullptr;
        nri::Pipeline *m_AOComputePipeline = nullptr;
        nri::Pipeline *m_BlurComputePipeline = nullptr;

        nri::Descriptor *m_ClampSampler = nullptr;
        nri::Descriptor *m_RepeatSampler = nullptr;

        Ref<ConstantBuffer> m_SSAOParamsBuffer;
        Ref<ConstantBuffer> m_BlurHorizontalParamsBuffer;
        Ref<ConstantBuffer> m_BlurVerticalParamsBuffer;

        Ref<Shader> m_AOComputeShader;
        Ref<Shader> m_BlurComputeShader;
    };
}

#endif
