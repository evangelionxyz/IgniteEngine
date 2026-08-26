// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_ASSET_SCENE_RENDERER_HPP
#define IGN_ASSET_SCENE_RENDERER_HPP

#include "iscene_renderer.hpp"
#include "ignite/graphics/objects/mesh.hpp"

#include <type_traits>

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
        void SetPreviewSkeletalMesh(const Ref<SkeletalMesh> &mesh);
        void SetPreviewStaticMesh(const Ref<StaticMesh> &mesh);
        void SetEnvironmentTexture(AssetHandle textureHandle);

        void SetBoneTransforms(const std::vector<glm::mat4> &boneTransforms);
        void SetPreviewWidget(const Ref<WidgetCanvas> &widget);
        void SetPreviewMouseState(uint32_t mouseX, uint32_t mouseY, bool hovered);

        void Render(ICamera *camera, FrameContext *frameContext, const Ref<RenderTarget> &sceneRT, const Ref<RenderTarget> &uiRT, const Ref<RenderTarget> &compositeRT);

        virtual Ref<Texture> GetEnvironmentMapColorTexture() const override;

    private:
        void SyncRuntimeMaterialFromSource();

        void DrawEnvironment(nvrhi::ICommandList *cmd, ICamera *camera, nvrhi::IFramebuffer *framebuffer, FrameContext *frameContext);
        void DrawPreviewStaticMesh(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer, FrameContext *frameContext);
        void DrawPreviewSkeletalMesh(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer, FrameContext *frameContext);

        template<typename MeshT>
        void DrawPreviewMeshImpl(const Ref<MeshT> &mesh, nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer,
            FrameContext *frameContext, const char *vertexShaderPath, const char *pixelShaderPath, EBindingLayout meshBindingLayout,
            std::unordered_map<const nvrhi::IFramebuffer *, Ref<GraphicsPipeline>> &opaqueCache,
            std::unordered_map<const nvrhi::IFramebuffer *, Ref<GraphicsPipeline>> &transparentCache);

        void CompositePass(nvrhi::ICommandList *cmd, ICamera *camera, nvrhi::IFramebuffer *framebuffer, Ref<Texture> sceneTexture, Ref<Texture> uiTexture);

    private:
        Ref<SkeletalMesh> m_PreviewSkeletalMesh;
        Ref<StaticMesh>   m_PreviewStaticMesh;

        Ref<WidgetCanvas>   m_PreviewWidget;
        Ref<WidgetRenderer> m_WidgetRenderer;
        Ref<Material>       m_SourceMaterial;
        Ref<Material>       m_RuntimeMaterial;

        Ref<Environment>    m_Environment;
        static Ref<Texture> m_DefaultEnvTexture;
        AssetHandle         m_EnvTexHandle = AssetHandle(0);

        nvrhi::BindingLayoutHandle m_CompositeBindingLayout;

        // Separate pipeline caches for static and skeletal meshes to avoid shader collision
        // (static uses mesh_static.hlsl; skeletal uses mesh_anim.hlsl).
        std::unordered_map<const nvrhi::IFramebuffer *, Ref<GraphicsPipeline>> m_StaticGeometryPipelineCache;
        std::unordered_map<const nvrhi::IFramebuffer *, Ref<GraphicsPipeline>> m_StaticTransparentPipelineCache;
        std::unordered_map<const nvrhi::IFramebuffer *, Ref<GraphicsPipeline>> m_SkeletalGeometryPipelineCache;
        std::unordered_map<const nvrhi::IFramebuffer *, Ref<GraphicsPipeline>> m_SkeletalTransparentPipelineCache;
        std::unordered_map<const nvrhi::IFramebuffer *, Ref<GraphicsPipeline>> m_EnvironmentPipelineCache;
        std::unordered_map<const nvrhi::IFramebuffer *, Ref<GraphicsPipeline>> m_CompositePipelineCache;

        std::vector<glm::mat4> m_BoneTransforms;
        uint32_t m_PreviewMouseX       = 0;
        uint32_t m_PreviewMouseY       = 0;
        bool     m_PreviewMouseHovered = false;
        bool     m_UseEnvironment                  = false;
        bool     m_EnvTextureInvalidating          = false;
        bool     m_EnvironmentTextureLoadAttempted = false;
        Scene_GPUData m_SceneGPUData;

        void EnsureBindingSets(FrameContext *frameContext);

        std::unordered_map<FrameContext *, nvrhi::BindingSetHandle> m_StaticMeshBindingSets;
        std::unordered_map<FrameContext *, nvrhi::BindingSetHandle> m_AnimatedBindingSets;
    };
}
#endif
