// Copyright (c) 2026 Evangelion Manuhutu
#pragma once

#include "ignite/asset/asset.hpp"
#include "ignite/core/base.hpp"

#include <filesystem>
#include <string>

namespace ignite
{
    class IGN_API LuaScript : public Asset
    {
    public:
        LuaScript() = default;
        explicit LuaScript(const std::filesystem::path &filepath);

        virtual AssetType GetAssetType() override { return AssetType::LuaScript; }

        const std::filesystem::path &GetFilepath() const { return m_Filepath; }
        void SetFilepath(const std::filesystem::path &filepath) { m_Filepath = filepath; }

        const std::string &GetSource() const { return m_Source; }
        void SetSource(const std::string &source) { m_Source = source; }

        bool LoadFromFile(const std::filesystem::path &filepath);
        bool Reload();

        static Ref<LuaScript> Create(const std::filesystem::path &filepath);

    private:
        std::filesystem::path m_Filepath;
        std::string m_Source;
    };
}

