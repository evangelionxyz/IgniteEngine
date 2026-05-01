// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef ASSET_IMPORTER_PANEL
#define ASSET_IMPORTER_PANEL

#include "ipanel.hpp"

#include "ignite/core/input/asset_import_event.hpp"
#include "ignite/asset/asset_importer.hpp"

#include <queue>
#include <vector>
#include <filesystem>
#include <unordered_map>
#include <optional>
#include <atomic>

namespace ignite
{
    class Font;

	class AssetImporterPanel : public IPanel
	{
	public:
		AssetImporterPanel(const char *name, EditorLayer *editor);
		virtual ~AssetImporterPanel() override = default;

		virtual void OnEvent(Event &event) override;
		virtual void OnUpdate(float deltaTime) override;

		bool OnAssetImportEvent(AssetImportEvent &event);

		virtual void OnGuiRender() override;

	private:
		struct AssetImportData
		{
			std::filesystem::path filepath;
			AssetType assetType = AssetType::Invalid;
			MeshImportOptions meshOptions;
		};

		void DrawMeshImportOptions();
		void DrawTextureImportOptions();
		void DrawFontImportOptions();
		void DrawGenericImportOptions();
		bool ProcessImportRequest(const AssetImportData &asset);
		bool AdvanceToNextAsset();
		void ResetImportState();
		void ImportCurrentAsset();
		void SkipCurrentAsset();
		AssetImportData BuildCurrentImportData() const;
		std::filesystem::path PrepareAssetForImport(const AssetImportData &asset) const;

		void ImportFbxMesh(const std::filesystem::path &filepath, const MeshImportOptions &options);
		void ImportFbxSkeletonAndAnimations(const std::filesystem::path &filepath, const MeshImportOptions &options);

		std::filesystem::path BuildUniquePath(const std::filesystem::path &directory, const std::string &baseName, const std::string &extension) const;

		std::filesystem::path m_TargetDirectory;
		std::unordered_map<AssetType, std::queue<AssetImportData>> m_ImportQueues;
		std::vector<AssetType> m_ImportTypeOrder;
		size_t m_CurrentTypeQueueIndex = 0;
		std::optional<AssetImportData> m_CurrentAsset;

		MeshImportOptions m_MeshOptions;
		bool m_ShowImporterWindow = false;
		bool m_OpenImporterPopup = false;
		bool m_SkipDialogForSameType = false;
		bool m_IsImporting = false;
		std::atomic<int> m_ActiveImportJobs;

   };
}

#endif