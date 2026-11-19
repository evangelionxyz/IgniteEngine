// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_ASSET_EDITOR_DATA_HPP
#define IGN_ASSET_EDITOR_DATA_HPP

#include "ignite/core/types.hpp"
#include "ignite/asset/asset.hpp"
#include "ignite/graphics/texture.hpp"
#include "editor_camera.hpp"

namespace ignite
{
	class AssetSceneRenderer;
	class RenderTarget;
	class Mesh;
	class IAssetSnapshot;
}

namespace ignite::UI
{
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
		bool cameraFocused = false;
	};

	struct AssetEditorData
	{
		Ref<Asset> asset;
		AssetMetaData metadata;
		AssetHandle handle;
		EditorSceneData sceneData;
		AssetHandle previewEnvTexHandle = AssetHandle(0);
		TextureCreateInfo textureCreateInfo;
		bool textureCreateInfoInitialized = false;

		Ref<IAssetSnapshot> undoBaselineSnapshot;
		Ref<IAssetSnapshot> undoPendingBefore;
		bool undoWasInteracting = false;
		bool isWindowFocused = false;

		std::string windowTitle;
		bool isOpen = true;
		bool requestFocus = false;
		bool showUnsavedClosePopup = false;

		void DrawSceneDetails();
		void UpdateCamera(float deltaTime);
		void FocusCameraOnMesh(const Ref<Mesh> &mesh);
	};
}

#endif
