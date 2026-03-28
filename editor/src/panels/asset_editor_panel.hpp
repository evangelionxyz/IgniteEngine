//Copyright (c) 2026 Evangelion Manuhutu

#ifndef ANIMATION_PANEL_HPP
#define ANIMATION_PANEL_HPP

#include "ipanel.hpp"
#include "ignite/asset/asset.hpp"

#include "ignite/core/input/app_event.hpp"

namespace ignite
{
  class SkeletalAnimation;

	class AssetEditorPanel : public IPanel
    {
    public:
        AssetEditorPanel(const char *windowTitle, EditorLayer *editor);

        virtual void OnGuiRender() override;
        virtual void OnEvent(Event &event) override;

        bool OnAssetEditorOpenEvent(AssetEditorOpenEvent &event);

    private:
		// Current selected asset
		struct AssetEditorData
		{
			Ref<Asset> asset;
			AssetMetaData metadata;
			AssetHandle handle;
			bool isOpen = true;
            bool requestFocus = false;
			std::string windowTitle;
		};

        bool DrawAssetEditorHeader(AssetEditorData &assetData);
        void RenderSkeletalAnimationEditor(const Ref<SkeletalAnimation> &animation);
        bool SaveAsset(AssetEditorData &assetData);

        std::vector<AssetEditorData> m_Assets;
    };
}

#endif