// Copyright (c) 2026 Evangelion Manuhutu

#ifndef ASSET_SCENE_RENDERER_HPP
#define ASSET_SCENE_RENDERER_HPP

#include "iscene_renderer.hpp"
#include "ignite/graphics/objects/mesh.hpp"

namespace ignite
{
    class Project;

    class AssetSceneRenderer : public ISceneRenderer
    {
    public:
        AssetSceneRenderer();
        ~AssetSceneRenderer() override;

        void BeginFrame();
        void SetMaterial(const Ref<Material> &material);
        void SetPreviewMesh(const Ref<StaticMesh> &mesh);
        void SetEnvironmentTexture(const Ref<Texture> &texture);
        void SetProject(Project *project);

        void Render(ICamera *camera, const Ref<RenderTarget> &sceneRT, const Ref<RenderTarget> &uiRT, const Ref<RenderTarget> &compositeRT);

        Ref<Texture> GetEnvironmentMapColorTexture() const override;

    private:
        void SyncRuntimeMaterialFromSource();
        void DrawPreviewMesh(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer);
        void CompositePass(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer, Ref<Texture> sceneTexture, Ref<Texture> uiTexture);

    private:
        Ref<StaticMesh> m_PreviewMesh;
        Ref<Material> m_SourceMaterial;
        Ref<Material> m_RuntimeMaterial;
        Ref<Texture> m_EnvironmentTexture;

        Ref<ConstantBuffer> m_SceneGPUDataBuffer;
        Ref<ConstantBuffer> m_CSMGPUDataBuffer;
        Ref<ConstantBuffer> m_PerEntityBuffer;

        nvrhi::BindingSetHandle m_MeshBindingSet;
        nvrhi::BindingLayoutHandle m_CompositeBindingLayout;
        nvrhi::ITexture *m_LastBoundEnvironmentTexture = nullptr;
        Project *m_Project = nullptr;

        std::unordered_map<const nvrhi::IFramebuffer *, Ref<GraphicsPipeline>> m_GeometryPipelineCache;
        std::unordered_map<const nvrhi::IFramebuffer *, Ref<GraphicsPipeline>> m_CompositePipelineCache;

        Scene_GPUData m_SceneGPUData;
        CascadedShadowMap_GPUData m_CSMGPUData;
    };
}
#endif