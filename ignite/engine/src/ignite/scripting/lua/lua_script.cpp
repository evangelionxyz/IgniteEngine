// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"
#include "lua_script.hpp"
#include "lua.hpp"
#include "ignite/core/logger.hpp"

#include <fstream>
#include <mutex>
#include <sstream>
#include <unordered_map>

extern "C"
{
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

namespace ignite::lua
{
    namespace
    {
        struct CachedLuaSchema
        {
            std::filesystem::file_time_type lastWriteTime{};
            std::vector<LuaFieldDesc> fields;
        };

        std::mutex s_SchemaCacheMutex;
        std::unordered_map<std::filesystem::path, CachedLuaSchema> s_SchemaCache;
    }

    Script::Script(const std::filesystem::path &filepath)
        : m_Filepath(filepath)
    {
        LoadFromFile(filepath);
    }

    bool Script::ExtractFields()
    {
        return ExtractFieldsFromSource(m_Source, m_Fields, m_Filepath.generic_string());
    }

    bool Script::LoadFromFile(const std::filesystem::path &filepath)
    {
        m_Filepath = filepath;
        if (!std::filesystem::exists(filepath))
        {
            LOG_ERROR("[LuaScript] File does not exist: {}", filepath.generic_string());
            m_Ready = false;
            m_Fields.clear();
            return false;
        }

        std::ifstream file(filepath);
        if (!file.is_open())
        {
            LOG_ERROR("[LuaScript] Failed to open file: {}", filepath.generic_string());
            m_Ready = false;
            m_Fields.clear();
            return false;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        m_Source = buffer.str();
        ExtractFields();
        m_Ready = true;
        m_Dirty = false;
        return true;
    }

    bool Script::Reload()
    {
        InvalidateFieldCache(m_Filepath);
        return LoadFromFile(m_Filepath);
    }

    Ref<Script> Script::Create(const std::filesystem::path &filepath)
    {
        return CreateRef<lua::Script>(filepath);
    }

    bool Script::ExtractFieldsFromSource(const std::string &source, std::vector<LuaFieldDesc> &outFields, const std::string &debugName)
    {
        outFields.clear();
        if (source.empty())
            return true;

        lua_State *state = luaL_newstate();
        if (!state)
            return false;

        // Open safe standard libraries for top-level script metadata inspection
        luaL_requiref(state, "_G", luaopen_base, 1);   lua_pop(state, 1);
        luaL_requiref(state, LUA_MATHLIBNAME, luaopen_math, 1);     lua_pop(state, 1);
        luaL_requiref(state, LUA_TABLIBNAME, luaopen_table, 1);     lua_pop(state, 1);
        luaL_requiref(state, LUA_STRLIBNAME, luaopen_string, 1);    lua_pop(state, 1);
        luaL_requiref(state, LUA_UTF8LIBNAME, luaopen_utf8, 1);     lua_pop(state, 1);

        lua::RegisterBindings(state, nullptr);

        // Isolated environment for chunk inspection
        lua_newtable(state);
        lua_newtable(state);
        lua_pushglobaltable(state);
        lua_setfield(state, -2, "__index");
        lua_setmetatable(state, -2);
        const int envIdx = lua_gettop(state);

        const std::string chunkName = "@" + debugName;
        if (luaL_loadbuffer(state, source.data(), source.size(), chunkName.c_str()) != LUA_OK)
        {
            LOG_WARN("[LuaScript] Field inspection load failed for '{}': {}", debugName, lua::LuaErrorMessage(state));
            lua_close(state);
            return false;
        }

        lua_pushvalue(state, envIdx);
        lua_setupvalue(state, -2, 1);

        if (lua_pcall(state, 0, 1, 0) != LUA_OK)
        {
            LOG_WARN("[LuaScript] Field inspection execution failed for '{}': {}", debugName, lua::LuaErrorMessage(state));
            lua_close(state);
            return false;
        }

        bool ok = false;
        if (lua_istable(state, -1))
        {
            ok = lua::ExtractFieldDescriptorsFromTable(state, -1, outFields);
        }

        lua_close(state);
        return ok;
    }

    std::vector<LuaFieldDesc> Script::GetOrInspectFields(const std::filesystem::path &filepath)
    {
        if (filepath.empty())
            return {};

        std::error_code ec;
        const std::filesystem::path normalized = filepath.lexically_normal();
        if (!std::filesystem::exists(normalized, ec))
            return {};

        const auto writeTime = std::filesystem::last_write_time(normalized, ec);

        std::lock_guard<std::mutex> lock(s_SchemaCacheMutex);
        auto it = s_SchemaCache.find(normalized);
        if (it != s_SchemaCache.end() && it->second.lastWriteTime == writeTime)
        {
            return it->second.fields;
        }

        std::ifstream file(normalized);
        if (!file.is_open())
            return {};

        std::stringstream buffer;
        buffer << file.rdbuf();

        CachedLuaSchema entry;
        entry.lastWriteTime = writeTime;
        ExtractFieldsFromSource(buffer.str(), entry.fields, normalized.generic_string());

        auto [insertedIt, _] = s_SchemaCache.insert_or_assign(normalized, std::move(entry));
        return insertedIt->second.fields;
    }

    void Script::InvalidateFieldCache(const std::filesystem::path &filepath)
    {
        std::lock_guard<std::mutex> lock(s_SchemaCacheMutex);
        if (filepath.empty())
            s_SchemaCache.clear();
        else
            s_SchemaCache.erase(filepath.lexically_normal());
    }
}

