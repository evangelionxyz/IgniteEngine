// Copyright (c) 2026 Evangelion Manuhutu

#ifndef ASSET_IMPORTER_PANEL
#define ASSET_IMPORTER_PANEL

#include "ipanel.hpp"

#include "ignite/core/input/asset_import_event.hpp"

namespace ignite
{
	class AssetImporterPanel : public IPanel
	{
	public:
		AssetImporterPanel(const char *name, EditorLayer *editor);

		virtual void OnEvent(Event &event);

		bool OnAssetImportEvent(AssetImportEvent &event);

		virtual void OnGuiRender() override;

		// Each asset type ImGui panel functions here
		// .....

	private:

	};
}
#endif