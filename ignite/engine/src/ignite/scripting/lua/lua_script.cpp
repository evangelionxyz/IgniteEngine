// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"
#include "lua_script.hpp"
#include "ignite/core/logger.hpp"

#include <fstream>
#include <sstream>

namespace ignite::lua
{
    Script::Script(const std::filesystem::path &filepath)
        : m_Filepath(filepath)
    {
        LoadFromFile(filepath);
    }

    bool Script::LoadFromFile(const std::filesystem::path &filepath)
    {
        m_Filepath = filepath;
        if (!std::filesystem::exists(filepath))
        {
            LOG_ERROR("[LuaScript] File does not exist: {}", filepath.generic_string());
            m_Ready = false;
            return false;
        }

        std::ifstream file(filepath);
        if (!file.is_open())
        {
            LOG_ERROR("[LuaScript] Failed to open file: {}", filepath.generic_string());
            m_Ready = false;
            return false;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        m_Source = buffer.str();
        m_Ready = true;
        m_Dirty = false;
        return true;
    }

    bool Script::Reload()
    {
        return LoadFromFile(m_Filepath);
    }

    Ref<Script> Script::Create(const std::filesystem::path &filepath)
    {
        return CreateRef<lua::Script>(filepath);
    }
}

