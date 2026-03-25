// Copyright (c) 2026 Evangelion Manuhutu

#include "asset_importer_panel.hpp"

namespace ignite
{
	AssetImporterPanel::AssetImporterPanel(const char *name, EditorLayer *editor)
		: IPanel(name, editor)
	{
	}

	void AssetImporterPanel::OnEvent(Event &event)
	{
		EventDispatcher dispatcher(event);
		dispatcher.Dispatch<AssetImportEvent>(BIND_CLASS_EVENT_FN(AssetImporterPanel::OnAssetImportEvent));
	}

	bool AssetImporterPanel::OnAssetImportEvent(AssetImportEvent &event)
	{
		LOG_ASSERT(false, "HERE BROOO");

		return false;
	}

	void AssetImporterPanel::OnGuiRender()
	{
		// all kind of asset importer should be rendered here
	}

}
