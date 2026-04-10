// Copyright (c) 2026 Evangelion Manuhutu

#ifndef SCENE_RENDERER_HPP
#define SCENE_RENDERER_HPP

#include "iscene_renderer.hpp"

namespace ignite
{
    class SceneRenderer : public ISceneRenderer
    {
    public:
        SceneRenderer();
        ~SceneRenderer();

        void BeginFrame();
        void SetActiveScene(const Ref<Scene> &scene);
        
        void RenderEditorTo(ICamera *camera, const Ref<RenderTarget> &sceneRT, const Ref<RenderTarget> &uiRT, const Ref<RenderTarget> &compositeRT, bool renderEnvironment = true);
        void RenderGameplayTo(ICamera *camera, const Ref<RenderTarget> &sceneRT, const Ref<RenderTarget> &uiRT, const Ref<RenderTarget> &compositeRT, bool renderEnvironment = true);
        
        void SetFillMode(nvrhi::RasterFillMode mode);

        void SetSelectedEntity(const Entity& entity);
        void UnselectEntity(const Entity& entity);
        void ClearSelectedEntities();

        // UI Input handling
        void UpdateUIInput(const glm::vec2& viewportMousePos, const glm::vec2& viewportPos, const glm::vec2& viewportSize, bool mousePressed);

		Ref<Texture> GetEnvironmentMapColorTexture() const;
		Ref<Texture> GetCascadedShadowMapDepthTexture() const;

        Ref<CascadedShadowMap> GetCascadedShadowMap();
        Ref<UIRenderer> &GetUIRenderer() { return m_UIRenderer; }
        Ref<Renderer2D> &GetRenderer2D() { return m_Renderer2D; }

        DebugGridSettings &GetDebugGridSettings() { return m_DebugGridSettings; }
        const DebugGridSettings &GetDebugGridSettings() const { return m_DebugGridSettings; }
        void SetDebugGridSettings(const DebugGridSettings &settings) { m_DebugGridSettings = settings; }

    private:
        Ref<StaticMesh> ResolveStaticMesh(Project *project, AssetHandle handle);
        Ref<SkeletalMesh> ResolveSkeletalMesh(Project *project, AssetHandle handle);
        Ref<Material> ResolveMaterial(Project *project, AssetHandle handle);

        void ShadowPass(nvrhi::ICommandList *cmd, ICamera *camera);
        void ColorPass(nvrhi::ICommandList *cmd, ICamera *camera, nvrhi::IFramebuffer *framebuffer);
        void CompositePass(nvrhi::ICommandList *cmd, ICamera *camera, nvrhi::IFramebuffer *framebuffer,
            Ref<Texture> sceneTexture, Ref<Texture> uiTexture, Ref<Texture> edgeTexture = nullptr,
            Ref<Texture> bloomTexture = nullptr, Ref<Texture> ssaoTexture = nullptr);

        void DrawDebugGrid(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer, const DebugGridStyle &style, bool is2D);
        void DrawDebug2DPhysics(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer);
        void DrawDebug3DPhysics(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer);

    private:
        void Clear3DAssetResolveCache();

        struct AssetResolveKey
        {
            Project *project = nullptr;
            AssetHandle handle = AssetHandle(0);

            bool operator==(const AssetResolveKey &other) const noexcept
            {
                return project == other.project && handle == other.handle;
            }
        };

        struct AssetResolveKeyHash
        {
            size_t operator()(const AssetResolveKey &key) const noexcept
            {
                size_t h = std::hash<const void *>{}(key.project);
                h ^= (std::hash<AssetHandle>{}(key.handle) + 0x9e3779b9 + (h << 6) + (h >> 2));
                return h;
            }
        };
        
    private:
        std::unordered_map<AssetResolveKey, Ref<StaticMesh>, AssetResolveKeyHash> m_StaticMeshResolveCache;
        std::unordered_map<AssetResolveKey, Ref<SkeletalMesh>, AssetResolveKeyHash> m_SkeletalMeshResolveCache;
        std::unordered_map<AssetResolveKey, Ref<Material>, AssetResolveKeyHash> m_MaterialResolveCache;
    };
}

#endif
