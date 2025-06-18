#pragma once

#include "environment.hpp"
#include "graphics_pipeline.hpp"
#include "render_target.hpp"
#include "ignite/scene/entity.hpp"

#include "imgui.h"

#include <nvrhi/nvrhi.h>
#include <nvrhi/utils.h>

namespace ignite
{
    class Scene;
    class ICamera;
    class RenderTarget;

    struct EdgeDetectionParams
    {
        glm::vec2 texelSize;
        float edgeThreshold = 0.1f;
        float outlineWidth = 2.0f;
        glm::vec4 outlineColor = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
        float depthSensitivity = 100.0f;
        int useObjectID = 1;
        float padding[1];
    };

    struct SobelEdgeDetection
    {
        nvrhi::ShaderHandle computeShader;
        nvrhi::ShaderHandle pixelShader;
        nvrhi::ShaderHandle vertexShader;

        nvrhi::ComputePipelineHandle computePipeline;
        nvrhi::GraphicsPipelineHandle graphicsPipeline;

        // Resources
        nvrhi::BufferHandle constantBuffer;
        nvrhi::BufferHandle vertexBuffer;
        nvrhi::InputLayoutHandle vertexInputLayout;

        nvrhi::BindingLayoutHandle bindingLayout;
        nvrhi::BindingSetHandle bindingSet;
        nvrhi::SamplerHandle linearSampler;

        // Textures
        nvrhi::TextureHandle outputTexture;

        void Initialize();
        void CreateShaders();
        void CreateOutputTexture(uint32_t width, uint32_t height);
        void CreatePipelines(nvrhi::IFramebuffer *framebuffer);
        void UpdateBindingSet(const nvrhi::TextureHandle& sceneTexture, const nvrhi::TextureHandle& depthTexture, const nvrhi::TextureHandle& objectIDTexture);
        void ExecuteCompute(nvrhi::ICommandList *commandList, const EdgeDetectionParams &params, uint32_t width, uint32_t height);
        void ExecuteFullScreenQuad(nvrhi::ICommandList *commandList, const EdgeDetectionParams &params, nvrhi::IFramebuffer *framebuffer);
    };

    struct Composite
    {
        nvrhi::ShaderHandle pixelShader;
        nvrhi::ShaderHandle vertexShader;

        nvrhi::GraphicsPipelineHandle graphicsPipeline;

        nvrhi::BufferHandle vertexBuffer;
        nvrhi::InputLayoutHandle vertexInputLayout;

        nvrhi::BindingLayoutHandle bindingLayout;
        nvrhi::BindingSetHandle bindingSet;
        nvrhi::SamplerHandle linearSampler;

        void Initialize();
        void CreateShaders();
        void CreatePipelines(nvrhi::IFramebuffer *framebuffer);
        void UpdateBindingSet(const nvrhi::TextureHandle &texture);
        void Execute(nvrhi::ICommandList *commandList, nvrhi::IFramebuffer *framebuffer);
    };
    
    class SceneRenderer
    {
    public:
        SceneRenderer();
        ~SceneRenderer();
        
        void Create();
        void SetActiveScene(Scene *scene);
        bool ShouldResize() const;
        void Resize(uint32_t width, uint32_t height);
        void CreatePipelines() const;
        void Render(const ICamera *camera, bool renderEnvironment = true);
        void SetFillMode(nvrhi::RasterFillMode mode) const;

        void OnGuiRender();

        Ref<GraphicsPipeline> &GetBatchQuadPipeline() { return m_BatchQuadPipeline; }
        Ref<GraphicsPipeline> &GetBatchLinePipeline() { return m_BatchLinePipeline; }
        Ref<GraphicsPipeline> &GetEnvironmentPipeline() { return m_EnvironmentPipeline; }
        Ref<GraphicsPipeline> &GetGeometryPipeline() { return m_GeometryPipeline; }
        
        Ref<Environment> &GetEnvironment() { return m_Environment; }
        Ref<RenderTarget> &GetRenderTarget() { return m_RenderTarget; }

        SobelEdgeDetection &GetEdgeDetection() { return m_EdgeDetection; }

    private:
        void CreateEnvironment();

        Ref<Environment> m_Environment;
        Ref<GraphicsPipeline> m_BatchQuadPipeline;
        Ref<GraphicsPipeline> m_BatchLinePipeline;
        Ref<GraphicsPipeline> m_EnvironmentPipeline;
        Ref<GraphicsPipeline> m_GeometryPipeline;
        Ref<RenderTarget> m_RenderTarget;

        SobelEdgeDetection m_EdgeDetection;
        Composite m_Composite;

        nvrhi::CommandListHandle m_CommandList;
        nvrhi::IDevice *m_Device = nullptr;

        Scene *m_Scene = nullptr;
    };
}
