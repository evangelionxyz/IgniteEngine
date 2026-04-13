// Copyright (c) 2026 Evangelion Manuhutu

#ifndef ASSET_IMPORTER_PANEL
#define ASSET_IMPORTER_PANEL

#include "ipanel.hpp"

#include "ignite/core/input/asset_import_event.hpp"
#include "ignite/asset/asset_importer.hpp"

#include <queue>
#include <vector>
#include <filesystem>

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
		struct ImportRequest
		{
			std::vector<std::filesystem::path> filepaths;
			std::filesystem::path targetDirectory;
			AssetType assetType = AssetType::Invalid;
			MeshImportOptions meshOptions;
		};

		struct FontPreviewData
		{
			std::filesystem::path sourceFilepath;
			Ref<Font> font;
		};

		void QueueImportRequest();
		void DrawFontImportPreview();
		void ProcessImportRequest(const ImportRequest &request);
		void ImportFontAsset(const std::filesystem::path &filepath);
		
		void DrawMeshImportOptions();
		void DrawStaticMeshImportOptions();
        void ImportFbxMesh(const std::filesystem::path &filepath, const MeshImportOptions &options);
		void ImportFbxSkeletonAndAnimations(const std::filesystem::path &filepath, const MeshImportOptions &options);
		
		std::filesystem::path BuildUniquePath(const std::filesystem::path &directory, const std::string &baseName, const std::string &extension) const;

		std::vector<std::filesystem::path> m_SelectedFilepaths;
		std::filesystem::path m_TargetDirectory;
		AssetType m_SelectedAssetType = AssetType::Invalid;
		MeshImportOptions m_MeshOptions;
		FontPreviewData m_FontPreview;
		std::queue<ImportRequest> m_ImportRequests;
		bool m_ShowImporterWindow = false;
	};
}

#endif