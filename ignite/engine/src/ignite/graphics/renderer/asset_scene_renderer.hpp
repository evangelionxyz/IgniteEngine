// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_ASSET_SCENE_RENDERER_HPP
#define IGN_ASSET_SCENE_RENDERER_HPP

#include "iscene_renderer.hpp"
#include "ignite/graphics/objects/mesh.hpp"

namespace ignite
{
    class Project;
    class WidgetCanvas;
    class WidgetRenderer;
    class Environment;
    class Texture;

    class IGN_API AssetSceneRenderer : public ISceneRenderer
    {
    public:
        AssetSceneRenderer();
        ~AssetSceneRenderer() override;

        void BeginFrame();
        void SetPreviewMaterial(const Ref<Material> &material);
        void SetPreviewMesh(const Ref<Mesh> &mesh);
        void SetEnvironmentTexture(AssetHandle textureHandle);
        
        void SetBoneTransforms(const std::vector<glm::mat4> &boneTransforms);
        void SetProject(Project *project);
        void SetPreviewWidget(const Ref<WidgetCanvas> &widget);
        void SetPreviewMouseState(uint32_t mouseX, uint32_t mouseY, bool hovered);

        void Render(ICamera *camera, const Ref<RenderTarget> &sceneRT, const Ref<RenderTarget> &uiRT, const Ref<RenderTarget> &compositeRT);

    private:
        void SyncRuntimeMaterialFromSource();

        void DrawEnvironment(nvrhi::ICommandList *cmd, ICamera *camera, nvrhi::IFramebuffer *framebuffer);
        void DrawPreviewMesh(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer);

        void CompositePass(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer, Ref<Texture> sceneTexture, Ref<Texture> uiTexture);

    private:
        Ref<Mesh> m_PreviewMesh;
        Ref<WidgetCanvas> m_PreviewWidget;
        Ref<WidgetRenderer> m_WidgetRenderer;
        Ref<Material> m_SourceMaterial;
        Ref<Material> m_RuntimeMaterial;

        Ref<Environment> m_Environment;
        static Ref<Texture> m_DefaultEnvTexture;
        AssetHandle m_EnvTexHandle = AssetHandle(0);

        nvrhi::BindingLayoutHandle m_CompositeBindingLayout;
        Project *m_Project = nullptr;

        std::unordered_map<const nvrhi::IFramebuffer *, Ref<GraphicsPipeline>> m_GeometryPipelineCache;
        std::unordered_map<const nvrhi::IFramebuffer *, Ref<GraphicsPipeline>> m_TransparentGeometryPipelineCache;
        std::unordered_map<const nvrhi::IFramebuffer *, Ref<GraphicsPipeline>> m_EnvironmentPipelineCache;
        std::unordered_map<const nvrhi::IFramebuffer *, Ref<GraphicsPipeline>> m_CompositePipelineCache;

        std::vector<glm::mat4> m_BoneTransforms;
        Ref<ConstantBuffer> m_SkeletonGpuBuffer;
        uint32_t m_PreviewMouseX = 0;
        uint32_t m_PreviewMouseY = 0;
        bool m_PreviewMouseHovered = false;
        bool m_UseEnvironment = false;;
        bool m_EnvTextureInvalidating = false;
        bool m_EnvironmentTextureLoadAttempted = false;

        SceneBufferData m_SceneGPUData;
        CascadedShadowMapBufferData m_CSMGPUData;
    };
}
#endif
