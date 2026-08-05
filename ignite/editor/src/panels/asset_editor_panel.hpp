// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef ANIMATION_PANEL_HPP
#define ANIMATION_PANEL_HPP

#include "ipanel.hpp"
#include "asset_editor_data.hpp"

#include "ignite/core/path.hpp"
#include "ignite/asset/asset.hpp"
#include "ignite/core/input/mouse_event.hpp"
#include "ignite/core/signal_bus.hpp"
#include "ignite/core/signals/asset_signal.hpp"

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

	struct CreateAssetRequest
	{
		Ref<Asset> asset;
		AssetType type = AssetType::Invalid;
		ignite::Path targetDirectory;
		char nameBuffer[256] = "NewAsset";
		bool open = false;
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
        void CloseAllAssetEditors();

        bool DrawAssetEditorHeader(UI::AssetEditorData &assetData);
        bool BeginAssetEditorWindow(UI::AssetEditorData &assetData, bool &isOpen, const ImVec2 &windowSize, const ImVec2 &minWindowSize, ImGuiWindowFlags flags);

        void UIAssetEditorClosePopup(UI::AssetEditorData &assetData, bool &isOpen);

        void UIWidgetEditor(UI::AssetEditorData &assetData);
        void UIMaterial2DEditor(UI::AssetEditorData &assetData);
        void UISpriteSheet2DEditor(UI::AssetEditorData &assetData);
        void UIAnimation2DEditor(UI::AssetEditorData &assetData);
        void UIAnimatorController2DEditor(UI::AssetEditorData &assetData);
        void UIStaticMeshEditor(UI::AssetEditorData &assetData);
        void UISkeletalMeshEditor(UI::AssetEditorData &assetData);
        void UISkeletonEditor(UI::AssetEditorData &assetData);
        void UISkeletalAnimationEditor(UI::AssetEditorData &assetData);
        void UIAnimationMontageEditor(UI::AssetEditorData &assetData);
        void UIAnimatorControllerEditor(UI::AssetEditorData &assetData);
        void UIBlendSpaceEditor(UI::AssetEditorData &assetData);
        void UITextureEditor(UI::AssetEditorData &assetData);
        void UIMaterialEditor(UI::AssetEditorData &assetData);
        void UITerrainDataEditor(UI::AssetEditorData &assetData);

        void UIScriptableObjectEditor(UI::AssetEditorData &assetData);

        bool SaveAsset(UI::AssetEditorData &assetData);
        void UICreateAssetPopup();
        void InitializeSceneData(UI::AssetEditorData &assetData);
        ignite::Path BuildUniqueAssetPath(const ignite::Path &baseDirectory, const std::string &baseName, const std::string &extension) const;

        std::vector<UI::AssetEditorData> m_Assets;
        CreateAssetRequest m_CreateRequest;

        SignalToken m_OpenSignalToken   = kInvalidSignalToken;
        SignalToken m_CreateSignalToken = kInvalidSignalToken;
    };
}

#endif
