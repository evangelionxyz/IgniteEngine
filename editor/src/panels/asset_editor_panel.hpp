//Copyright (c) 2026 Evangelion Manuhutu

#ifndef ANIMATION_PANEL_HPP
#define ANIMATION_PANEL_HPP

#include "ipanel.hpp"
#include "ignite/asset/asset.hpp"

#include "ignite/core/input/app_event.hpp"

namespace ignite
{
    class Material;
    class Texture;
    class SpriteSheet;
    class Material2D;
    class Animation2D;
    class SkeletalAnimation;
    class AnimatorController2D;

	class AssetEditorPanel : public IPanel
    {
    public:
        AssetEditorPanel(const char *windowTitle, EditorLayer *editor);

        virtual void OnGuiRender() override;
        virtual void OnEvent(Event &event) override;

        bool OnAssetEditorOpenEvent(AssetEditorOpenEvent &event);
        bool OnAssetEditorCreateEvent(AssetEditorCreateEvent &event);

    private:
		struct AssetEditorData
		{
			Ref<Asset> asset;
			AssetMetaData metadata;
			AssetHandle handle;
			bool isOpen = true;
            bool requestFocus = false;
            bool showUnsavedClosePopup = false;
			std::string windowTitle;
		};

        struct CreateAssetRequest
        {
            AssetType type = AssetType::Invalid;
            std::filesystem::path targetDirectory;
            bool open = false;
            char nameBuffer[256] = "NewAsset";
            Ref<Asset> asset;
        };

        bool DrawAssetEditorHeader(AssetEditorData &assetData);
        bool BeginAssetEditorWindow(AssetEditorData &assetData, bool &isOpen, const ImVec2 &windowSize, const ImVec2 &minWindowSize, ImGuiWindowFlags flags);
        void RenderAssetEditorClosePopup(AssetEditorData &assetData, bool &isOpen);

        void RenderMaterial2DEditor(AssetEditorData &assetData);
        void RenderSkeletalAnimationEditor(const Ref<SkeletalAnimation> &animation);
        void RenderSkeletalAnimationEditor(AssetEditorData &assetData);
        void RenderMaterial2DEditor(const Ref<Material2D> &material2D);
        void RenderTextureEditor(AssetEditorData &assetData);
        void RenderMaterialEditor(const Ref<Material> &material);
        void RenderMaterialEditor(AssetEditorData &assetData);
        void RenderTextureEditor(AssetEditorData &assetData, const Ref<Texture> &texture);
        void RenderSpriteSheetEditor(AssetEditorData &assetData);
        void RenderSpriteSheetEditor(const Ref<SpriteSheet> &spriteSheet);
        void RenderAnimation2DEditor(AssetEditorData &assetData);
        void RenderAnimation2DEditor(const Ref<Animation2D> &animation);
        void RenderAnimatorController2DEditor(AssetEditorData &assetData);
        void RenderAnimatorController2DEditor(const Ref<AnimatorController2D> &controller);
        bool SaveAsset(AssetEditorData &assetData);
        void RenderCreateAssetPopup();
        std::filesystem::path BuildUniqueAssetPath(const std::filesystem::path &baseDirectory, const std::string &baseName, const std::string &extension) const;

        std::vector<AssetEditorData> m_Assets;
        CreateAssetRequest m_CreateRequest;
    };
}

#endif