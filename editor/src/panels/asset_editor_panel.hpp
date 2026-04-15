//Copyright (c) 2026 Evangelion Manuhutu

#ifndef ANIMATION_PANEL_HPP
#define ANIMATION_PANEL_HPP

#include "ipanel.hpp"
#include "editor_camera.hpp"
#include "ignite/asset/asset.hpp"
#include "ignite/graphics/render_target.hpp"
#include "ignite/graphics/renderer/asset_scene_renderer.hpp"
#include "ignite/core/input/app_event.hpp"
#include "ignite/core/input/mouse_event.hpp"

namespace ignite
{
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
    class Skeleton;
    class Mesh;

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

        bool OnAssetEditorOpenEvent(AssetEditorOpenEvent &event);
        bool OnAssetEditorCreateEvent(AssetEditorCreateEvent &event);
        bool OnMouseScrollEvent(MouseScrolledEvent &event);

    private:
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
			std::string windowTitle;
			bool isOpen = true;
            bool requestFocus = false;
            bool showUnsavedClosePopup = false;
		};

        struct CreateAssetRequest
        {
            Ref<Asset> asset;
            AssetType type = AssetType::Invalid;
            std::filesystem::path targetDirectory;
            char nameBuffer[256] = "NewAsset";
            bool open = false;
        };

        bool DrawAssetEditorHeader(AssetEditorData &assetData);
        bool BeginAssetEditorWindow(AssetEditorData &assetData, bool &isOpen, const ImVec2 &windowSize, const ImVec2 &minWindowSize, ImGuiWindowFlags flags);
        void UIAssetEditorClosePopup(AssetEditorData &assetData, bool &isOpen);

        void UIMaterial2DEditor(AssetEditorData &assetData);
        void UIMaterial2DEditor(const Ref<Material2D> &material2D);

        void UISpriteSheet2DEditor(AssetEditorData &assetData);
        void UISpriteSheet2DEditor(const Ref<SpriteSheet> &spriteSheet);

        void UIAnimation2DEditor(AssetEditorData &assetData);
        void UIAnimation2DEditor(const Ref<Animation2D> &animation);

        void UIAnimatorController2DEditor(AssetEditorData &assetData);
        void UIAnimatorController2DEditor(const Ref<AnimatorController2D> &controller);

        void UIMeshEditor(const Ref<Mesh> &mesh, EditorSceneData &sceneData);
        void UIMeshEditor(AssetEditorData &assetData);

        void UISkeletonEditor(const Ref<Skeleton> &skeleton, EditorSceneData &sceneData);
        void UISkeletonEditor(AssetEditorData &assetData);
        
        void UISkeletalAnimationEditor(const Ref<SkeletalAnimation> &animation);
        void UISkeletalAnimationEditor(AssetEditorData &assetData);
        
        void UIAnimatorControllerEditor(AssetEditorData &assetData);
        void UIAnimatorControllerEditor(const Ref<AnimatorController> &animator);

        void UITextureEditor(AssetEditorData &assetData);
        void UITextureEditor(AssetEditorData &assetData, const Ref<Texture> &texture);
        
        void UIMaterialEditor(const Ref<Material> &material);
        void UIMaterialEditor(AssetEditorData &assetData);
        
        bool SaveAsset(AssetEditorData &assetData);
        void UICreateAssetPopup();
        void InitializeSceneData(AssetEditorData &assetData);
        void UpdateSceneCamera(EditorSceneData &sceneData, float deltaTime);
        std::filesystem::path BuildUniqueAssetPath(const std::filesystem::path &baseDirectory, const std::string &baseName, const std::string &extension) const;

        std::vector<AssetEditorData> m_Assets;
        CreateAssetRequest m_CreateRequest;
    };
}

#endif