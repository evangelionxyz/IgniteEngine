//Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef ANIMATION_PANEL_HPP
#define ANIMATION_PANEL_HPP

#include "ipanel.hpp"
#include "editor_camera.hpp"
#include "ignite/core/path.hpp"
#include "ignite/asset/asset.hpp"
#include "ignite/graphics/render_target.hpp"
#include "ignite/graphics/renderer/asset_scene_renderer.hpp"
#include "ignite/core/input/asset_signal.hpp"
#include "ignite/core/signal_bus.hpp"
#include "ignite/core/input/mouse_event.hpp"

namespace ignite
{
    class WidgetCanvas;
    class Material;
    class Texture;
    class SpriteSheet;
    class Material2D;
    class Animation2D;
    class SkeletalAnimation;
    class AnimationMontage;
    class BlendSpace;
    class LocomotionController;
    class AnimatorController;
    class AnimatorController2D;
    class ScriptableObject;
    class Skeleton;
    class Mesh;

    struct EditorSceneData
    {
        Ref<AssetSceneRenderer> sceneRenderer;
        EditorCamera camera;
        Ref<RenderTarget> sceneRT;
        Ref<RenderTarget> uiRT;
        Ref<RenderTarget> compositeRT;
        uint32_t viewportWidth = 512;
        uint32_t viewportHeight = 512;
        bool viewportVisible = false;
        bool viewportHovered = false;
    };

    struct AssetEditorData
    {
        Ref<Asset> asset;
        AssetMetaData metadata;
        AssetHandle handle;
        EditorSceneData sceneData;
        AssetHandle previewEnvTexHandle = AssetHandle(0);
        std::string windowTitle;
        bool isOpen = true;
        bool requestFocus = false;
        bool showUnsavedClosePopup = false;
    };

	class AssetEditorPanel : public IPanel
    {
    public:
        AssetEditorPanel(const char *windowTitle, EditorLayer *editor);
        ~AssetEditorPanel();

        virtual void OnAttach() override;
        virtual void OnDetach() override;

        virtual void OnUpdate(float deltaTime) override;
        virtual void OnRender(nvrhi::IFramebuffer *framebuffer) override;

        virtual void OnGuiRender() override;
        virtual void OnEvent(Event &event) override;

        bool OnAssetEditorOpenSignal(const AssetEditorOpenSignal &signal);
        bool OnAssetEditorCreateSignal(const AssetEditorCreateSignal &signal);
        bool OnMouseScrollEvent(MouseScrolledEvent &event);

        struct CreateAssetRequest
        {
            Ref<Asset> asset;
            AssetType type = AssetType::Invalid;
            ignite::Path targetDirectory;
            char nameBuffer[256] = "NewAsset";
            bool open = false;
        };

        bool DrawAssetEditorHeader(AssetEditorData &assetData);
        bool BeginAssetEditorWindow(AssetEditorData &assetData, bool &isOpen, const ImVec2 &windowSize, const ImVec2 &minWindowSize, ImGuiWindowFlags flags);
        void UIAssetEditorClosePopup(AssetEditorData &assetData, bool &isOpen);

        void UIWidgetEditor(AssetEditorData &assetData);
        void UIMaterial2DEditor(AssetEditorData &assetData);
        void UISpriteSheet2DEditor(AssetEditorData &assetData);
        void UIAnimation2DEditor(AssetEditorData &assetData);
        void UIAnimatorController2DEditor(AssetEditorData &assetData);
        void UIMeshEditor(AssetEditorData &assetData);
        void UISkeletonEditor(AssetEditorData &assetData);
        void UISkeletalAnimationEditor(AssetEditorData &assetData);
        void UIAnimatorControllerEditor(AssetEditorData &assetData);
        void UITextureEditor(AssetEditorData &assetData);
        void UIMaterialEditor(AssetEditorData &assetData);

        void UIScriptableObjectEditor(AssetEditorData &assetData);

        bool SaveAsset(AssetEditorData &assetData);
        void UICreateAssetPopup();
        void InitializeSceneData(AssetEditorData &assetData);
        void UpdateSceneCamera(EditorSceneData &sceneData, float deltaTime);
        ignite::Path BuildUniqueAssetPath(const ignite::Path &baseDirectory, const std::string &baseName, const std::string &extension) const;

        std::vector<AssetEditorData> m_Assets;
        CreateAssetRequest m_CreateRequest;

        SignalToken m_OpenSignalToken   = kInvalidSignalToken;
        SignalToken m_CreateSignalToken = kInvalidSignalToken;
    };
}

#endif
