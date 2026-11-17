// Copyright (c) 2026 Evangelion Manuhutu
#pragma once

#include "ignite/asset/asset.hpp"
#include "ignite/core/base.hpp"
#include "lua_field.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace ignite::lua
{
    class IGN_API Script : public Asset
    {
    public:
        Script() = default;
        explicit Script(const std::filesystem::path &filepath);

        virtual AssetType GetAssetType() override { return AssetType::LuaScript; }

        const std::filesystem::path &GetFilepath() const { return m_Filepath; }
        void SetFilepath(const std::filesystem::path &filepath) { m_Filepath = filepath; }

        const std::string &GetSource() const { return m_Source; }
        void SetSource(const std::string &source) { m_Source = source; }

        const std::vector<LuaFieldDesc> &GetFields() const { return m_Fields; }
        bool ExtractFields();

        bool LoadFromFile(const std::filesystem::path &filepath);
        bool Reload();

        static Ref<Script> Create(const std::filesystem::path &filepath);
        static bool ExtractFieldsFromSource(const std::string &source, std::vector<LuaFieldDesc> &outFields, const std::string &debugName = "lua_inspect");
        static std::vector<LuaFieldDesc> GetOrInspectFields(const std::filesystem::path &filepath);
        static void InvalidateFieldCache(const std::filesystem::path &filepath = {});

    private:
        std::filesystem::path m_Filepath;
        std::string m_Source;
        std::vector<LuaFieldDesc> m_Fields;
    };
}

