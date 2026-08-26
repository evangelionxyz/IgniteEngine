// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef ASSET_IMPORTER_PANEL
#define ASSET_IMPORTER_PANEL

#include "ipanel.hpp"

#include "ignite/asset/asset_importer.hpp"
#include "ignite/core/signal_bus.hpp"
#include "ignite/core/signals/asset_signal.hpp"

#include <queue>
#include <vector>

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
		virtual ~AssetImporterPanel() override;

		virtual void OnUpdate(float deltaTime) override;
		bool OnAssetImportSignal(const AssetImportSignal &signal);
		virtual void OnGuiRender() override;

	private:
		void DrawMeshImportOptions(const FileImportPayload &payload);
		void DrawTextureImportOptions(const FileImportPayload &payload);
		void DrawFontImportOptions(const FileImportPayload &payload);
		void DrawGenericImportOptions(const FileImportPayload &payload);

		bool ProcessImportRequest(const FileImportPayload &payload);
		bool AdvanceToNextAsset();
		void ResetImportState();
		void ImportCurrentAsset();
		void SkipCurrentAsset();

		std::filesystem::path PrepareAssetForImport(const FileImportPayload &payload) const;

		bool ImportStaticMesh(const std::filesystem::path &filepath);
		bool ImportSkeletalMesh(const std::filesystem::path &filepath);

		std::filesystem::path BuildUniquePath(const std::filesystem::path &directory, const std::string &baseName, const std::string &extension) const;

		std::filesystem::path m_TargetDirectory;
		std::unordered_map<AssetType, std::queue<FileImportPayload>> m_ImportQueues;
		std::vector<AssetType> m_ImportTypeOrder;
		size_t m_CurrentTypeQueueIndex = 0;
		std::optional<FileImportPayload> m_CurrentAsset;

		StaticMeshImportPayload m_StaticMeshImportPayload;
		SkeletalMeshImportPayload m_SkeletalMeshImportPayload;
		AssetType m_MeshImportType = AssetType::Invalid;

		bool m_ShowImporterWindow = false;
		bool m_SkipDialogForSameType = false;
		bool m_IsImporting = false;
		std::atomic<int> m_ActiveImportJobs = 0;
		std::atomic<int> m_TotalImportItems = 0;
		std::atomic<int> m_ImportedItems = 0;

		SignalToken m_ImportSignalToken = kInvalidSignalToken;

   };
}

#endif
