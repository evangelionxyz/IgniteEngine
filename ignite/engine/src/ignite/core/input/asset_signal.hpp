// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_ASSET_SIGNAL_HPP
#define IGN_ASSET_SIGNAL_HPP

// -------------------------------------------------------------------------
// Asset Signals
//
// These are plain structs used with SignalBus for internal engine
// notifications about asset system state changes.
//
// They replace the old Event-derived asset event classes that were incorrectly
// routed through the Application::OnEvent / Layer stack pipeline.
//
// Emit via:   SignalBus::Emit(AssetChangeSignal{ handle, type });
// Subscribe:  SignalBus::Subscribe<AssetChangeSignal>([this](const auto& s){…});
// -------------------------------------------------------------------------

#include "ignite/asset/asset.hpp"
#include "ignite/core/path.hpp"

#include <vector>

namespace ignite
{
    // Fired when an already-loaded asset has been rebuilt/refreshed and
    // dependents (e.g. material binding sets) need to be re-created.
    struct AssetChangeSignal
    {
        AssetHandle handle;
        AssetType   type = AssetType::Invalid;
    };

    // Fired when the user drops files or triggers an import action that should
    // open the AssetImporter panel UI.
    struct AssetImportSignal
    {
        std::vector<ignite::Path> filepaths;
        AssetType                 assetType = AssetType::Invalid;
        ignite::Path              targetDirectory;
    };

    // Fired when the user double-clicks an asset in the content browser to
    // open it inside the AssetEditor panel.
    struct AssetEditorOpenSignal
    {
        AssetHandle   handle;
        AssetMetaData metadata;
    };

    // Fired when the user requests creation of a new asset of the given type
    // inside the AssetEditor panel.
    struct AssetEditorCreateSignal
    {
        AssetType    type = AssetType::Invalid;
        ignite::Path targetDirectory;
    };
}

#endif
