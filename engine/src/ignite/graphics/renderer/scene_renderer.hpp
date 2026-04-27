// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef SCENE_RENDERER_HPP
#define SCENE_RENDERER_HPP

#include "iscene_renderer.hpp"
#include <SDL3/SDL_events.h>

namespace ignite
{
    class WidgetRenderer;
    class NuklearRenderer;

    class SceneRenderer : public ISceneRenderer
    {
    public:
        SceneRenderer();
        ~SceneRenderer();

        virtual void OnUpdate(float deltaTime) override;

        void BeginFrame();
        void SetActiveScene(const Ref<Scene> &scene);
        
        void RenderEditorTo(ICamera *camera);
        void RenderGameplayTo(ICamera *camera);

        virtual void ResizeFramebuffer(uint32_t width, uint32_t height) override;
        void ResizeGameplayFramebuffer(uint32_t width, uint32_t height);
        void HandleNuklearEvent(SDL_Event *evt);
        void SetFillMode(nvrhi::RasterFillMode mode);

        void SetSelectedEntity(const Entity& entity);
        void UnselectEntity(const Entity& entity);
        void ClearSelectedEntities();

		virtual Ref<Texture> GetEnvironmentMapColorTexture() const override;
        virtual Ref<Texture> GetCascadedShadowMapDepthTexture() const override;

        virtual Ref<CascadedShadowMap> GetCascadedShadowMap() override;
        Ref<Renderer2D> &GetRenderer2D() { return m_Renderer2D; }

        DebugGridSettings &GetDebugGridSettings() { return m_DebugGridSettings; }
        const DebugGridSettings &GetDebugGridSettings() const { return m_DebugGridSettings; }
        void SetDebugGridSettings(const DebugGridSettings &settings) { m_DebugGridSettings = settings; }

        const Ref<RenderTarget> &GetCompositeRT() { return m_CompositeRT; }
        const Ref<RenderTarget> &GetSceneRT() { return m_SceneRT; }
        const Ref<RenderTarget> &GetWidgetRT() { return m_WidgetRT; }

        const Ref<RenderTarget> &GetGameplayCompositeRT() { return m_GameplayCompositeRT; }
        const Ref<RenderTarget> &GetGameplaySceneRT() { return m_GameplaySceneRT; }
        const Ref<RenderTarget> &GetGameplayWidgetRT() { return m_GameplayWidgetRT; }
    private:
        Ref<Mesh> ResolveMesh(Project *project, AssetHandle handle);
        Ref<Material> ResolveMaterial(Project *project, AssetHandle handle);

        void ShadowPass(nvrhi::ICommandList *cmd, ICamera *camera);
        void ColorPass(nvrhi::ICommandList *cmd, ICamera *camera, nvrhi::IFramebuffer *framebuffer);
        void UIPass(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer);
        void CompositePass(nvrhi::ICommandList *cmd, ICamera *camera, const PostProcessing &postProcessing, 
            nvrhi::IFramebuffer *framebuffer, Ref<Texture> sceneTexture, Ref<Texture> uiTexture, Ref<Texture> edgeTexture = nullptr,
            Ref<Texture> bloomTexture = nullptr, Ref<Texture> ssaoTexture = nullptr);

        void DrawIcons(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer, ICamera *camera);
        void DrawDebugGrid(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer, const DebugGridStyle &style, bool is2D);
        void DrawDebug2D(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer);
        void DrawDebug3D(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer);

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
        Scope<NuklearRenderer> m_Nuklear;

        Ref<RenderTarget> m_SceneRT;
        Ref<RenderTarget> m_WidgetRT;
        Ref<RenderTarget> m_CompositeRT;

        Ref<RenderTarget> m_GameplaySceneRT;
        Ref<RenderTarget> m_GameplayWidgetRT;
        Ref<RenderTarget> m_GameplayCompositeRT;

        std::unordered_map<std::string, Ref<Texture>> m_Icons;
        std::unordered_map<AssetResolveKey, Ref<Mesh>, AssetResolveKeyHash> m_MeshResolveCache;
        std::unordered_map<AssetResolveKey, Ref<Material>, AssetResolveKeyHash> m_MaterialResolveCache;

        uint32_t m_EditorWidgetMouseX = 0;
        uint32_t m_EditorWidgetMouseY = 0;
        bool m_EditorWidgetMouseHovered = false;
        bool m_UseEditorWidgetMouseOverride = false;

        uint32_t m_GameplayWidgetMouseX = 0;
        uint32_t m_GameplayWidgetMouseY = 0;
        bool m_GameplayWidgetMouseHovered = false;
        bool m_UseGameplayWidgetMouseOverride = false;
    };
}

#endif
