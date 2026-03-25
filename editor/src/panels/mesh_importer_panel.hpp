// Copyright (c) 2026 Evangelion Manuhutu

#ifndef MESH_IMPORTER_PANEL
#define MESH_IMPORTER_PANEL

#include "ipanel.hpp"

namespace ignite
{
	class MeshImporterPanel : public IPanel
	{
	public:
		MeshImporterPanel(const char *windowTitle);

		virtual void OnGuiRender() override;

	};
}
#endif