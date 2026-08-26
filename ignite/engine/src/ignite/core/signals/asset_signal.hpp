// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_ASSET_SIGNAL_HPP
#define IGN_ASSET_SIGNAL_HPP

#include "ignite/asset/asset_importer.hpp"

#include <vector>

namespace ignite
{
    // Fired when an already-loaded asset has been rebuilt/refreshed and
    // dependents (e.g. material binding sets) need to be re-created.
    struct AssetChangeSignal
    {
        AssetHandle handle;
        AssetType type = AssetType::Invalid;
    };

    // Fired when the user drops files or triggers an import action that should
    // open the AssetImporter panel UI.
    struct AssetImportSignal
    {
        std::vector<FileImportPayload> payloads;
		std::filesystem::path targetDirectory;
    };

    // Fired when the user double-clicks an asset in the content browser to
    // open it inside the AssetEditor panel.
    struct AssetEditorOpenSignal
    {
        AssetHandle handle;
        AssetMetaData metadata;
    };

    // Fired when the user requests creation of a new asset of the given type
    // inside the AssetEditor panel.
    struct AssetEditorCreateSignal
    {
        AssetType type = AssetType::Invalid;
        std::filesystem::path targetDirectory;
    };
}

#endif
