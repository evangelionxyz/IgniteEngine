// Copyright (c) 2026 Evangelion Manuhutu

#ifndef ASSET_IMPORTER_PANEL
#define ASSET_IMPORTER_PANEL

#include "ipanel.hpp"

#include "ignite/core/input/asset_import_event.hpp"

#include <queue>
#include <vector>
#include <filesystem>

namespace ignite
{
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
		struct SkeletalMeshImportOptions
		{
			bool importSkeletalMesh = true;
			bool importSkeleton = true;
			bool importAnimations = true;
		};

		struct ImportRequest
		{
			std::vector<std::filesystem::path> filepaths;
			AssetType assetType = AssetType::Invalid;
			SkeletalMeshImportOptions skeletalMeshOptions;
		};

		void QueueImportRequest();
		void DrawSkeletalMeshImportOptions();
		void ProcessImportRequest(const ImportRequest &request);
		void ImportFbxAsSkeletalMesh(const std::filesystem::path &filepath);
		void ImportFbxSkeletonAndAnimations(const std::filesystem::path &filepath, const SkeletalMeshImportOptions &options);

		std::vector<std::filesystem::path> m_SelectedFilepaths;
		AssetType m_SelectedAssetType = AssetType::Invalid;
		SkeletalMeshImportOptions m_SkeletalMeshOptions;
		std::queue<ImportRequest> m_ImportRequests;
		bool m_ShowImporterWindow = false;

	};
}
#endif