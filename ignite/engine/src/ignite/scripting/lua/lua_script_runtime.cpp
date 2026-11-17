// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "lua_script_runtime.hpp"
#include "ignite/scene/scene.hpp"

#include "ignite/scene/component.hpp"
#include "ignite/project/project.hpp"
#include "ignite/core/application.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/core/input/input_system.hpp"
#include "ignite/physics/2d/physics_2d_component.hpp"
#include "ignite/asset/asset_manager.hpp"

#include "lua.hpp"
#include "lua_script.hpp"

#include <glm/gtx/quaternion.hpp>
#include <cstring>
#include <algorithm>

extern "C"
{
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

namespace ignite::lua
{
    // ===========================================
    // Lua Script Runtime
    // ===========================================

    ScriptRuntime::ScriptRuntime(Scene *scene)
        : m_Scene(scene), m_State(luaL_newstate())
    {
        if (!m_State)
        {
            LOG_ERROR("[Lua] Failed to create Lua state");
            return;
        }
        luaL_openlibs(m_State);
        lua::RegisterBindings(m_State, m_Scene);
    }

    ScriptRuntime::~ScriptRuntime()
    {
        for (auto &[path, token] : m_WatchedFiles)
        {
            FileWatcher::Unwatch(token);
        }
        m_WatchedFiles.clear();

        if (m_State)
        {
            for (auto &[id, instance] : m_Instances)
            {
                Invoke(id, instance, "on_destroy", 0.0f, false);
                luaL_unref(m_State, LUA_REGISTRYINDEX, instance.callbacksRef);
                luaL_unref(m_State, LUA_REGISTRYINDEX, instance.environmentRef);
            }
            lua_close(m_State);
            m_State = nullptr;
        }
    }

    std::filesystem::path ScriptRuntime::ResolvePath(const std::filesystem::path &path) const
    {
        if (path.is_absolute()) return path.lexically_normal();
        Project *project = m_Scene ? m_Scene->GetProject() : nullptr;
        return project ? (project->GetDirectory() / path).lexically_normal() : path;
    }

    void ScriptRuntime::WatchScriptFile(const std::filesystem::path &absolutePath)
    {
        if (absolutePath.empty() || m_WatchedFiles.contains(absolutePath))
            return;

        if (std::filesystem::exists(absolutePath))
        {
            FileWatchToken token = FileWatcher::WatchFile(absolutePath, [this, absolutePath](const FileChangeEvent &event)
            {
                if (event.type != FileChangeType::Removed)
                {
                    std::lock_guard<std::mutex> lock(m_ReloadMutex);
                    m_PendingReloads.insert(absolutePath);
                }
            });

            if (token != kInvalidFileWatchToken)
            {
                m_WatchedFiles[absolutePath] = token;
            }
        }
    }

    void ScriptRuntime::UnwatchUnusedFiles()
    {
        std::vector<std::filesystem::path> unused;
        for (const auto &[watchedPath, token] : m_WatchedFiles)
        {
            bool inUse = false;
            for (const auto &[id, instance] : m_Instances)
            {
                if (ResolvePath(instance.path) == watchedPath)
                {
                    inUse = true;
                    break;
                }
            }
            if (!inUse)
            {
                unused.push_back(watchedPath);
            }
        }

        for (const auto &path : unused)
        {
            FileWatcher::Unwatch(m_WatchedFiles[path]);
            m_WatchedFiles.erase(path);
        }
    }

    void ScriptRuntime::CreateInstance(UUID entityId, const std::filesystem::path &scriptPath, AssetHandle scriptHandle)
    {
        if (!m_State || !m_Scene || scriptPath.empty()) return;
        DestroyInstance(entityId);

        Instance instance;
        instance.handle = scriptHandle;
        instance.path = scriptPath;
        const std::filesystem::path absolutePath = ResolvePath(scriptPath);
        const int baseTop = lua_gettop(m_State);

        lua_newtable(m_State);
        lua_newtable(m_State);
        lua_pushglobaltable(m_State);
        lua_setfield(m_State, -2, "__index");
        lua_setmetatable(m_State, -2);
        instance.environmentRef = luaL_ref(m_State, LUA_REGISTRYINDEX);

        if (luaL_loadfile(m_State, absolutePath.string().c_str()) != LUA_OK)
        {
            LOG_ERROR("[Lua] Failed loading '{}' for entity {}: {}",
                scriptPath.generic_string(), static_cast<uint64_t>(entityId), lua::LuaErrorMessage(m_State));

            lua_settop(m_State, baseTop);
            luaL_unref(m_State, LUA_REGISTRYINDEX, instance.environmentRef);
            return;
        }
        lua_rawgeti(m_State, LUA_REGISTRYINDEX, instance.environmentRef);
        if (!lua_setupvalue(m_State, -2, 1))
        {
            LOG_ERROR("[Lua] Script '{}' has no environment upvalue", scriptPath.generic_string());

            lua_settop(m_State, baseTop);
            luaL_unref(m_State, LUA_REGISTRYINDEX, instance.environmentRef);
            return;
        }
        if (lua_pcall(m_State, 0, 1, 0) != LUA_OK)
        {
            LOG_ERROR("[Lua] Failed executing '{}' for entity {}: {}",
                scriptPath.generic_string(), static_cast<uint64_t>(entityId), lua::LuaErrorMessage(m_State));

            lua_settop(m_State, baseTop);
            luaL_unref(m_State, LUA_REGISTRYINDEX, instance.environmentRef);
            return;
        }
        if (!lua_istable(m_State, -1))
        {
            LOG_ERROR("[Lua] Script '{}' must return a callback table", scriptPath.generic_string());

            lua_settop(m_State, baseTop);
            luaL_unref(m_State, LUA_REGISTRYINDEX, instance.environmentRef);
            return;
        }
        instance.callbacksRef = luaL_ref(m_State, LUA_REGISTRYINDEX);
        lua_settop(m_State, baseTop);

        m_Instances.emplace(entityId, instance);

        // Register file watch
        WatchScriptFile(absolutePath);

        auto it = m_Instances.find(entityId);
        ApplyEntityFields(entityId, it->second);
        if (!Invoke(entityId, it->second, "on_create", 0.0f, false))
            DestroyInstance(entityId);
    }

    bool ScriptRuntime::ReloadInstance(UUID entityId)
    {
        auto it = m_Instances.find(entityId);
        if (it == m_Instances.end()) return false;
        Instance &instance = it->second;

        const std::filesystem::path absolutePath = ResolvePath(instance.path);
        const int baseTop = lua_gettop(m_State);

        if (luaL_loadfile(m_State, absolutePath.string().c_str()) != LUA_OK)
        {
            LOG_ERROR("[Lua] Hot reload failed loading '{}' for entity {}: {}",
                      instance.path.generic_string(), static_cast<uint64_t>(entityId), lua::LuaErrorMessage(m_State));
            lua_settop(m_State, baseTop);
            return false;
        }

        lua_rawgeti(m_State, LUA_REGISTRYINDEX, instance.environmentRef);
        if (!lua_setupvalue(m_State, -2, 1))
        {
            LOG_ERROR("[Lua] Script '{}' has no environment upvalue during reload", instance.path.generic_string());
            lua_settop(m_State, baseTop);
            return false;
        }

        if (lua_pcall(m_State, 0, 1, 0) != LUA_OK)
        {
            LOG_ERROR("[Lua] Hot reload failed executing '{}' for entity {}: {}",
                      instance.path.generic_string(), static_cast<uint64_t>(entityId), lua::LuaErrorMessage(m_State));
            lua_settop(m_State, baseTop);
            return false;
        }

        if (!lua_istable(m_State, -1))
        {
            LOG_ERROR("[Lua] Reloaded script '{}' must return a callback table", instance.path.generic_string());
            lua_settop(m_State, baseTop);
            return false;
        }

        if (instance.callbacksRef != LUA_NOREF && instance.callbacksRef != LUA_REFNIL)
        {
            luaL_unref(m_State, LUA_REGISTRYINDEX, instance.callbacksRef);
        }

        instance.callbacksRef = luaL_ref(m_State, LUA_REGISTRYINDEX);
        lua_settop(m_State, baseTop);

        Script::InvalidateFieldCache(absolutePath);
        ApplyEntityFields(entityId, instance);

        LOG_INFO("[Lua] Successfully reloaded script '{}' for entity {}",
                 instance.path.generic_string(), static_cast<uint64_t>(entityId));

        // Re-run initialization because executing a new chunk recreates its local state.
        if (!Invoke(entityId, instance, "on_create", 0.0f, false))
            return false;

        if (!Invoke(entityId, instance, "on_reload", 0.0f, false))
            return false;

        return true;
    }

    void ScriptRuntime::ReloadScript(const std::filesystem::path &scriptPath)
    {
        const std::filesystem::path target = ResolvePath(scriptPath);
        for (auto &[entityId, instance] : m_Instances)
        {
            if (ResolvePath(instance.path) == target)
            {
                ReloadInstance(entityId);
            }
        }
    }

    void ScriptRuntime::ProcessPendingReloads()
    {
        std::unordered_set<std::filesystem::path> toReload;
        {
            std::lock_guard<std::mutex> lock(m_ReloadMutex);
            if (m_PendingReloads.empty())
                return;

            toReload = std::move(m_PendingReloads);
            m_PendingReloads.clear();
        }

        for (const auto &path : toReload)
        {
            for (auto &[entityId, instance] : m_Instances)
            {
                if (ResolvePath(instance.path) == path)
                {
                    ReloadInstance(entityId);
                }
            }
        }
    }

    bool ScriptRuntime::Invoke(UUID entityId, Instance &instance, const char *callback, float deltaTime, bool hasDeltaTime)
    {
        if (!m_State || instance.callbacksRef == LUA_NOREF) return false;
        const int baseTop = lua_gettop(m_State);
        lua_rawgeti(m_State, LUA_REGISTRYINDEX, instance.callbacksRef);
        lua_pushstring(m_State, callback);
        lua_rawget(m_State, -2);

        if (lua_isnil(m_State, -1))
        {
            lua_settop(m_State, baseTop);
            return true;
        }

        if (!lua_isfunction(m_State, -1))
        {
            LOG_ERROR("[Lua] Callback '{}' in '{}' is not a function (entity {})",
                callback, instance.path.generic_string(), static_cast<uint64_t>(entityId));

            lua_settop(m_State, baseTop);
            return false;
        }

        lua::PushEntity(m_State, m_Scene, static_cast<uint64_t>(entityId));
        int argCount = 1;
        if (hasDeltaTime)
        {
            lua_pushnumber(m_State, deltaTime);
            ++argCount;
        }

        if (lua_pcall(m_State, argCount, 0, 0) != LUA_OK)
        {
            LOG_ERROR("[Lua] Callback '{}' failed in '{}' for entity {}: {}",
                callback, instance.path.generic_string(), static_cast<uint64_t>(entityId), lua::LuaErrorMessage(m_State));

            lua_settop(m_State, baseTop);
            return false;
        }
        lua_settop(m_State, baseTop);
        return true;
    }

    void ScriptRuntime::CreatePendingInstances()
    {
        if (!m_Scene || !m_Scene->registry)
            return;

        std::vector<std::tuple<UUID, std::filesystem::path, AssetHandle>> toCreate;
        auto view = m_Scene->registry->view<LuaScriptComponent, IDComponent>();
        for (auto e : view)
        {
            const auto &idc = view.get<IDComponent>(e);
            const auto &script = view.get<LuaScriptComponent>(e);
            if (!m_Instances.contains(idc.uuid) && !script.scriptPath.empty())
                toCreate.emplace_back(idc.uuid, script.scriptPath, script.scriptHandle);
        }

        for (const auto &[id, path, handle] : toCreate)
            CreateInstance(id, path, handle);
    }

    void ScriptRuntime::InvokeNetworkEvent(const char *callback, uint32_t clientId, const std::string &clientName)
    {
        if (!m_State || !m_Scene || !callback)
            return;

        CreatePendingInstances();

        const std::vector<UUID> instanceIds = [&]()
        {
            std::vector<UUID> ids;
            ids.reserve(m_Instances.size());
            for (const auto &[id, instance] : m_Instances)
                ids.push_back(id);
            return ids;
        }();

        for (UUID entityId : instanceIds)
        {
            auto it = m_Instances.find(entityId);
            if (it == m_Instances.end())
                continue;

            Instance &instance = it->second;
            const int baseTop = lua_gettop(m_State);
            lua_rawgeti(m_State, LUA_REGISTRYINDEX, instance.callbacksRef);
            lua_pushstring(m_State, callback);
            lua_rawget(m_State, -2);
            if (lua_isnil(m_State, -1))
            {
                lua_settop(m_State, baseTop);
                continue;
            }
            if (!lua_isfunction(m_State, -1))
            {
                LOG_ERROR("[Lua] Network callback '{}' in '{}' is not a function", callback, instance.path.generic_string());
                lua_settop(m_State, baseTop);
                continue;
            }

            lua::PushEntity(m_State, m_Scene, static_cast<uint64_t>(entityId));
            lua_pushinteger(m_State, static_cast<lua_Integer>(clientId));
            lua_pushlstring(m_State, clientName.data(), clientName.size());
            if (lua_pcall(m_State, 3, 0, 0) != LUA_OK)
            {
                LOG_ERROR("[Lua] Network callback '{}' failed in '{}': {}",
                    callback, instance.path.generic_string(), lua::LuaErrorMessage(m_State));
            }
            lua_settop(m_State, baseTop);
        }
    }

    void ScriptRuntime::InvokeNetworkInput(UUID entityId, uint32_t clientId, float moveX, float moveY, uint16_t buttons, float yaw, float deltaTime)
    {
        if (!m_State || !m_Scene)
            return;

        auto it = m_Instances.find(entityId);
        if (it == m_Instances.end())
        {
            Entity entity = lua::ResolveEntity(m_Scene, static_cast<uint64_t>(entityId));
            if (!entity || !entity.HasComponent<LuaScriptComponent>())
                return;

            const auto &script = entity.GetComponent<LuaScriptComponent>();
            if (script.scriptPath.empty())
                return;

            CreateInstance(entityId, script.scriptPath, script.scriptHandle);
            it = m_Instances.find(entityId);
            if (it == m_Instances.end())
                return;
        }

        Instance &instance = it->second;
        const int baseTop = lua_gettop(m_State);
        lua_rawgeti(m_State, LUA_REGISTRYINDEX, instance.callbacksRef);
        lua_pushstring(m_State, "on_network_input");
        lua_rawget(m_State, -2);
        if (lua_isnil(m_State, -1))
        {
            lua_settop(m_State, baseTop);
            return;
        }
        if (!lua_isfunction(m_State, -1))
        {
            LOG_ERROR("[Lua] Network input callback in '{}' is not a function", instance.path.generic_string());
            lua_settop(m_State, baseTop);
            return;
        }

        lua::PushEntity(m_State, m_Scene, static_cast<uint64_t>(entityId));
        lua_pushinteger(m_State, static_cast<lua_Integer>(clientId));
        lua_pushnumber(m_State, moveX);
        lua_pushnumber(m_State, moveY);
        lua_pushinteger(m_State, static_cast<lua_Integer>(buttons));
        lua_pushnumber(m_State, yaw);
        lua_pushnumber(m_State, deltaTime);
        if (lua_pcall(m_State, 7, 0, 0) != LUA_OK)
        {
            LOG_ERROR("[Lua] Network input callback failed in '{}' for entity {}: {}",
                instance.path.generic_string(), static_cast<uint64_t>(entityId), lua::LuaErrorMessage(m_State));
        }
        lua_settop(m_State, baseTop);
    }

    void ScriptRuntime::DestroyInstance(UUID entityId)
    {
        auto it = m_Instances.find(entityId);
        if (it == m_Instances.end()) return;
        Invoke(entityId, it->second, "on_destroy", 0.0f, false);
        if (m_State)
        {
            luaL_unref(m_State, LUA_REGISTRYINDEX, it->second.callbacksRef);
            luaL_unref(m_State, LUA_REGISTRYINDEX, it->second.environmentRef);
        }
        m_Instances.erase(it);
        UnwatchUnusedFiles();
    }

    void ScriptRuntime::Update(float deltaTime)
    {
        ProcessPendingReloads();
        CreatePendingInstances();

        std::vector<UUID> failed;
        for (auto &[id, instance] : m_Instances)
        {
            Entity entity = lua::ResolveEntity(m_Scene, static_cast<uint64_t>(id));
            if (!entity || !entity.HasComponent<LuaScriptComponent>() || !Invoke(id, instance, "on_update", deltaTime, true))
                failed.push_back(id);
        }

        for (UUID id : failed)
            DestroyInstance(id);
    }

    void ScriptRuntime::FixedUpdate(float fixedDeltaTime)
    {
        std::vector<UUID> failed;
        for (auto &[id, instance] : m_Instances)
        {
            Entity entity = lua::ResolveEntity(m_Scene, static_cast<uint64_t>(id));
            if (!entity || !entity.HasComponent<LuaScriptComponent>() || !Invoke(id, instance, "on_fixed_update", fixedDeltaTime, true))
                failed.push_back(id);
        }

        for (UUID id : failed)
            DestroyInstance(id);
    }

    void ScriptRuntime::InvokeCollision(UUID entityId, UUID otherEntityId, const char *callback, const glm::vec3 &contactPoint, const glm::vec3 &contactNormal)
    {
        if (!m_State || !m_Scene || !callback)
            return;

        auto it = m_Instances.find(entityId);
        if (it == m_Instances.end())
            return;

        Instance &instance = it->second;
        if (instance.callbacksRef == LUA_NOREF)
            return;

        const int baseTop = lua_gettop(m_State);
        lua_rawgeti(m_State, LUA_REGISTRYINDEX, instance.callbacksRef);
        lua_pushstring(m_State, callback);
        lua_rawget(m_State, -2);

        if (!lua_isfunction(m_State, -1))
        {
            lua_settop(m_State, baseTop);
            return;
        }

        lua::PushEntity(m_State, m_Scene, static_cast<uint64_t>(entityId));
        if (lua::ResolveEntity(m_Scene, static_cast<uint64_t>(otherEntityId)))
            lua::PushEntity(m_State, m_Scene, static_cast<uint64_t>(otherEntityId));
        else
            lua_pushnil(m_State);

        lua::PushVector(m_State, contactPoint);
        lua::PushVector(m_State, contactNormal);

        if (lua_pcall(m_State, 4, 0, 0) != LUA_OK)
        {
            LOG_ERROR("[Lua] Collision callback '{}' failed in '{}' for entity {}: {}",
                callback, instance.path.generic_string(), static_cast<uint64_t>(entityId), lua::LuaErrorMessage(m_State));
        }

        lua_settop(m_State, baseTop);
    }

    bool ScriptRuntime::ExecuteSnippet(const std::string &code, std::string &outResult, std::string &outError)
    {
        outResult.clear();
        outError.clear();

        if (!m_State)
        {
            outError = "Lua state is not initialized";
            return false;
        }

        const int baseTop = lua_gettop(m_State);
        if (luaL_loadbuffer(m_State, code.data(), code.size(), "agent_snippet") != LUA_OK)
        {
            outError = lua::LuaErrorMessage(m_State);
            lua_settop(m_State, baseTop);
            return false;
        }

        if (lua_pcall(m_State, 0, LUA_MULTRET, 0) != LUA_OK)
        {
            outError = lua::LuaErrorMessage(m_State);
            lua_settop(m_State, baseTop);
            return false;
        }

        const int retCount = lua_gettop(m_State) - baseTop;
        for (int i = 1; i <= retCount; ++i)
        {
            const int idx = baseTop + i;
            if (i > 1)
                outResult += ", ";

            if (lua_isboolean(m_State, idx))
                outResult += lua_toboolean(m_State, idx) ? "true" : "false";
            else if (lua_isnil(m_State, idx))
                outResult += "nil";
            else if (const char *s = luaL_tolstring(m_State, idx, nullptr))
            {
                outResult += s;
                lua_pop(m_State, 1);
            }
        }

        lua_settop(m_State, baseTop);
        return true;
    }

    void ScriptRuntime::ApplyEntityFields(UUID entityId, Instance &instance)
    {
        if (!m_State || instance.callbacksRef == LUA_NOREF || instance.callbacksRef == LUA_REFNIL)
            return;

        const int baseTop = lua_gettop(m_State);
        lua_rawgeti(m_State, LUA_REGISTRYINDEX, instance.callbacksRef);
        if (!lua_istable(m_State, -1))
        {
            lua_settop(m_State, baseTop);
            return;
        }

        ExtractFieldDescriptorsFromTable(m_State, -1, instance.fields);

        Entity entity = lua::ResolveEntity(m_Scene, static_cast<uint64_t>(entityId));
        const LuaScriptComponent *comp = (entity && entity.HasComponent<LuaScriptComponent>())
            ? &entity.GetComponent<LuaScriptComponent>()
            : nullptr;

        for (const auto &desc : instance.fields)
        {
            LuaFieldValue resolved = desc.defaultValue;
            if (comp)
            {
                auto it = comp->fields.find(desc.name);
                if (it != comp->fields.end())
                {
                    const LuaFieldValue &overrideVal = it->second.value;
                    if (IsLuaFieldValueCompatible(desc.type, overrideVal))
                    {
                        resolved = ClampLuaFieldValue(desc, overrideVal);
                    }
                    else if (desc.type == ScriptFieldType::Float && std::holds_alternative<int32_t>(overrideVal))
                    {
                        resolved = ClampLuaFieldValue(desc, static_cast<float>(std::get<int32_t>(overrideVal)));
                    }
                    else if (desc.type == ScriptFieldType::Int && std::holds_alternative<float>(overrideVal))
                    {
                        resolved = ClampLuaFieldValue(desc, static_cast<int32_t>(std::get<float>(overrideVal)));
                    }
                }
            }

            PushLuaFieldValue(m_State, m_Scene, desc.type, resolved);
            lua_setfield(m_State, -2, desc.name.c_str());
        }

        lua_settop(m_State, baseTop);
    }

    bool ScriptRuntime::SetFieldValue(UUID entityId, const std::string &fieldName, const LuaFieldValue &value)
    {
        if (!m_State)
            return false;

        auto it = m_Instances.find(entityId);
        if (it == m_Instances.end() || it->second.callbacksRef == LUA_NOREF || it->second.callbacksRef == LUA_REFNIL)
            return false;

        const LuaFieldDesc *desc = nullptr;
        for (const auto &f : it->second.fields)
        {
            if (f.name == fieldName)
            {
                desc = &f;
                break;
            }
        }
        if (!desc || !IsLuaFieldValueCompatible(desc->type, value))
            return false;

        const LuaFieldValue clamped = ClampLuaFieldValue(*desc, value);
        const int baseTop = lua_gettop(m_State);
        lua_rawgeti(m_State, LUA_REGISTRYINDEX, it->second.callbacksRef);
        if (lua_istable(m_State, -1))
        {
            PushLuaFieldValue(m_State, m_Scene, desc->type, clamped);
            lua_setfield(m_State, -2, fieldName.c_str());
        }
        lua_settop(m_State, baseTop);
        return true;
    }

    bool ScriptRuntime::GetFieldValue(UUID entityId, const std::string &fieldName, LuaFieldValue &outValue) const
    {
        if (!m_State)
            return false;

        auto it = m_Instances.find(entityId);
        if (it == m_Instances.end() || it->second.callbacksRef == LUA_NOREF || it->second.callbacksRef == LUA_REFNIL)
            return false;

        const LuaFieldDesc *desc = nullptr;
        for (const auto &f : it->second.fields)
        {
            if (f.name == fieldName)
            {
                desc = &f;
                break;
            }
        }
        if (!desc)
            return false;

        const int baseTop = lua_gettop(m_State);
        lua_rawgeti(m_State, LUA_REGISTRYINDEX, it->second.callbacksRef);
        bool ok = false;
        if (lua_istable(m_State, -1))
        {
            lua_getfield(m_State, -1, fieldName.c_str());
            ok = ReadLuaFieldValue(m_State, -1, desc->type, outValue);
        }
        lua_settop(m_State, baseTop);
        return ok;
    }

    const std::vector<LuaFieldDesc> *ScriptRuntime::GetInstanceFields(UUID entityId) const
    {
        auto it = m_Instances.find(entityId);
        if (it == m_Instances.end())
            return nullptr;
        return &it->second.fields;
    }
}
