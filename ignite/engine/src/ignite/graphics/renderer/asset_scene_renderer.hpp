// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_ASSET_SCENE_RENDERER_HPP
#define IGN_ASSET_SCENE_RENDERER_HPP

#include "iscene_renderer.hpp"
#include "ignite/graphics/objects/mesh.hpp"

#include <memory>
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
        AssetSceneRenderer(const AssetSceneRenderer &) = delete;
        AssetSceneRenderer &operator=(const AssetSceneRenderer &) = delete;

        void BeginFrame();

        void SetPreviewMaterial(const Ref<Material> &material);
        void SetPreviewSkeletalMesh(const Ref<SkeletalMesh> &mesh);
        void SetPreviewStaticMesh(const Ref<StaticMesh> &mesh);
        void SetEnvironmentTexture(AssetHandle textureHandle);

        void SetBoneTransforms(const std::vector<glm::mat4> &boneTransforms);
        void SetPreviewWidget(const Ref<WidgetCanvas> &widget);
        void SetPreviewMouseState(uint32_t mouseX, uint32_t mouseY, bool hovered);
        void SetPreviewCanvasTransform(uint32_t canvasWidth, uint32_t canvasHeight, const glm::vec2 &pan, float zoom);

        void Render(ICamera *camera, FrameContext *frameContext, const Ref<RenderTarget> &sceneRT, const Ref<RenderTarget> &uiRT, const Ref<RenderTarget> &compositeRT);

        virtual Ref<Texture> GetEnvironmentMapColorTexture() override;

    private:
        struct PreviewFrameUniforms
        {
            ConstantBuffer cameraBuffer{ sizeof(CameraBufferData), true, 16, "[AssetPreview] Camera Buffer" };
            ConstantBuffer sceneBuffer{ sizeof(Scene_GPUData), true, 16, "[AssetPreview] Scene Buffer" };
            ConstantBuffer csmBuffer{ sizeof(CSM_GPUData), true, 16, "[AssetPreview] CSM Buffer" };
            ConstantBuffer pointLightBuffer{ sizeof(PointLightBufferData), true, 16, "[AssetPreview] PointLight Buffer" };
            ConstantBuffer spotLightBuffer{ sizeof(SpotLightBufferData), true, 16, "[AssetPreview] SpotLight Buffer" };
            ConstantBuffer compositePostProcessBuffer{ sizeof(CompositePostProcess_GPUData), true, 16, "[AssetPreview] Composite PostProcess Buffer" };
            Ref<BindingSet> staticMeshBindingSet;
            Ref<BindingSet> animatedBindingSet;
        };

        void SyncRuntimeMaterialFromSource();
        PreviewFrameUniforms *EnsureFrameUniforms(FrameContext *frameContext);

        void DrawEnvironment(nri::CommandBuffer *cmd, ICamera *camera, RenderTarget *rt, PreviewFrameUniforms &uniforms);
        void DrawPreviewStaticMesh(nri::CommandBuffer *cmd, RenderTarget *rt, FrameContext *frameContext, PreviewFrameUniforms &uniforms);
        void DrawPreviewSkeletalMesh(nri::CommandBuffer *cmd, RenderTarget *rt, FrameContext *frameContext, PreviewFrameUniforms &uniforms);

        template<typename MeshT>
        void DrawPreviewMeshImpl(const Ref<MeshT> &mesh, nri::CommandBuffer *cmd, RenderTarget *rt,
            FrameContext *frameContext, PreviewFrameUniforms &uniforms,
            const char *vertexShaderPath, const char *pixelShaderPath, EBindingLayout meshBindingLayout,
            std::unordered_map<const RenderTarget *, Ref<GraphicsPipeline>> &opaqueCache,
            std::unordered_map<const RenderTarget *, Ref<GraphicsPipeline>> &transparentCache);

        void CompositePass(nri::CommandBuffer *cmd, ICamera *camera, RenderTarget *rt, Ref<Texture> sceneTexture, Ref<Texture> uiTexture, ConstantBuffer &postProcessBuffer);

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

        BindingLayoutDesc m_CompositeBindingLayout;

        // Separate pipeline caches for static and skeletal meshes to avoid shader collision
        // (static uses mesh_static.hlsl; skeletal uses mesh_anim.hlsl).
        std::unordered_map<const RenderTarget *, Ref<GraphicsPipeline>> m_StaticGeometryPipelineCache;
        std::unordered_map<const RenderTarget *, Ref<GraphicsPipeline>> m_StaticTransparentPipelineCache;
        std::unordered_map<const RenderTarget *, Ref<GraphicsPipeline>> m_SkeletalGeometryPipelineCache;
        std::unordered_map<const RenderTarget *, Ref<GraphicsPipeline>> m_SkeletalTransparentPipelineCache;
        std::unordered_map<const RenderTarget *, Ref<GraphicsPipeline>> m_EnvironmentPipelineCache;
        std::unordered_map<const RenderTarget *, Ref<GraphicsPipeline>> m_CompositePipelineCache;

        std::vector<glm::mat4> m_BoneTransforms;
        uint32_t m_PreviewMouseX       = 0;
        uint32_t m_PreviewMouseY       = 0;
        bool     m_PreviewMouseHovered = false;
        uint32_t m_PreviewCanvasWidth  = 0;
        uint32_t m_PreviewCanvasHeight = 0;
        glm::vec2 m_PreviewPan         = { 0.0f, 0.0f };
        float    m_PreviewZoom         = 1.0f;
        bool     m_UseEnvironment                  = false;
        bool     m_EnvTextureInvalidating          = false;
        bool     m_EnvironmentTextureLoadAttempted = false;
        Scene_GPUData m_SceneGPUData;

        std::unordered_map<FrameContext *, Ref<PreviewFrameUniforms>> m_FrameUniforms;
    };
}
#endif
