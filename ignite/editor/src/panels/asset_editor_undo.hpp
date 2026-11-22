// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_ASSET_EDITOR_UNDO_HPP
#define IGN_ASSET_EDITOR_UNDO_HPP

#include "ignite/core/base.hpp"
#include "ignite/core/command.hpp"
#include "ignite/core/types.hpp"
#include "ignite/asset/asset.hpp"

#include <string>

namespace ignite
{
    class AssetEditorPanel;

    namespace UI
    {
        struct AssetEditorData;
    }

    class IAssetSnapshot
    {
    public:
        virtual ~IAssetSnapshot() = default;
        virtual bool Equals(const IAssetSnapshot &other) const = 0;
        virtual void Apply(UI::AssetEditorData &assetData) const = 0;
        virtual size_t GetMemoryUsage() const = 0;
    };

    Ref<IAssetSnapshot> CaptureAssetSnapshot(const UI::AssetEditorData &assetData);

    class AssetSnapshotCommand final : public ICommand
    {
    public:
        AssetSnapshotCommand(
            AssetHandle assetHandle,
            AssetEditorPanel *panel,
            std::string name,
            Ref<IAssetSnapshot> before,
            Ref<IAssetSnapshot> after);

        void Execute() override;
        void Undo() override;

        std::string GetName() const override { return m_Name.empty() ? "Edit Asset" : m_Name; }
        size_t GetMemoryUsage() const override;

    private:
        void ApplySnapshot(const Ref<IAssetSnapshot> &snapshot);

        AssetHandle m_AssetHandle = AssetHandle(0);
        AssetEditorPanel *m_Panel = nullptr;
        std::string m_Name;
        Ref<IAssetSnapshot> m_Before;
        Ref<IAssetSnapshot> m_After;
    };
}

#endif

