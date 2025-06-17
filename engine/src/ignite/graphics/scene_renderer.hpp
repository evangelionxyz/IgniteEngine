#pragma once

#include "environment.hpp"
#include "graphics_pipeline.hpp"
#include "render_target.hpp"
#include "ignite/scene/entity.hpp"

#include <nvrhi/nvrhi.h>
#include <nvrhi/utils.h>

namespace ignite
{
    class Scene;
    class ICamera;
    class RenderTarget;

    class SceneRenderer
    {
    public:
        SceneRenderer();
        ~SceneRenderer();
        
        void Create();
        void SetActiveScene(Scene *scene);
        bool ShouldResize() const;
        void Update();
        void Resize(uint32_t width, uint32_t height) const;
        void CreatePipelines(nvrhi::IFramebuffer *framebuffer) const;
        void Render(const ICamera *camera, bool renderEnvironment = true);
        void SetFillMode(nvrhi::RasterFillMode mode) const;

        Ref<GraphicsPipeline> &GetBatchQuadPipeline() { return m_BatchQuadPipeline; }
        Ref<GraphicsPipeline> &GetBatchLinePipeline() { return m_BatchLinePipeline; }
        Ref<GraphicsPipeline> &GetEnvironmentPipeline() { return m_EnvironmentPipeline; }
        Ref<GraphicsPipeline> &GetGeometryPipeline() { return m_GeometryPipeline; }
        Ref<Environment> &GetEnvironment() { return m_Environment; }
        Ref<RenderTarget> &GetRenderTarget() { return m_RenderTarget; }

    private:
        void CreateEnvironment();

        Ref<Environment> m_Environment;

        Ref<GraphicsPipeline> m_BatchQuadPipeline;
        Ref<GraphicsPipeline> m_BatchLinePipeline;
        Ref<GraphicsPipeline> m_EnvironmentPipeline;

        Ref<GraphicsPipeline> m_GeometryPipeline;
        Ref<RenderTarget> m_RenderTarget;

        nvrhi::CommandListHandle m_CommandList;
        nvrhi::IDevice *m_Device = nullptr;

        Scene *m_Scene = nullptr;
    };
}
