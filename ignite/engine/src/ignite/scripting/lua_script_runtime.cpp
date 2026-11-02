// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "lua_script_runtime.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/scene/entity.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/project/project.hpp"
#include "ignite/core/application.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/physics/2d/physics_2d_component.hpp"

#include <cstring>

extern "C"
{
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

namespace ignite
{
    namespace
    {
        constexpr const char *kEntityMetatable = "Ignite.Entity";
        constexpr const char *kComponentMetatable = "Ignite.Component";

        const char *LuaErrorMessage(lua_State *state)
        {
            const char *message = lua_tostring(state, -1);
            return message ? message : "(non-string Lua error)";
        }

        struct LuaEntityRef { Scene *scene; uint64_t id; };
        enum class LuaComponentType : uint8_t { Transform, Rigidbody2D, Rigidbody3D, Sprite2D, StaticMesh, SkeletalMesh, AudioSource };
        struct LuaComponentRef { Scene *scene; uint64_t id; LuaComponentType type; };

        Entity ResolveEntity(Scene *scene, uint64_t id)
        {
            if (!scene)
                return {};
            const auto it = scene->entities.find(UUID(id));
            if (it == scene->entities.end() || !scene->registry->valid(it->second))
                return {};
            return Entity(it->second, scene);
        }

        void PushVector(lua_State *state, const glm::vec3 &v)
        {
            lua_createtable(state, 0, 3);
            lua_pushnumber(state, v.x); lua_setfield(state, -2, "x");
            lua_pushnumber(state, v.y); lua_setfield(state, -2, "y");
            lua_pushnumber(state, v.z); lua_setfield(state, -2, "z");
        }

        glm::vec3 ReadVector(lua_State *state, int index)
        {
            glm::vec3 v(0.0f);
            if (lua_istable(state, index))
            {
                lua_getfield(state, index, "x");
                const bool named = !lua_isnil(state, -1);
                if (lua_isnumber(state, -1)) v.x = static_cast<float>(lua_tonumber(state, -1));
                lua_pop(state, 1);
                if (named)
                {
                    lua_getfield(state, index, "y"); if (lua_isnumber(state, -1)) v.y = static_cast<float>(lua_tonumber(state, -1)); lua_pop(state, 1);
                    lua_getfield(state, index, "z"); if (lua_isnumber(state, -1)) v.z = static_cast<float>(lua_tonumber(state, -1)); lua_pop(state, 1);
                }
                else
                {
                    lua_rawgeti(state, index, 1); v.x = static_cast<float>(luaL_optnumber(state, -1, 0)); lua_pop(state, 1);
                    lua_rawgeti(state, index, 2); v.y = static_cast<float>(luaL_optnumber(state, -1, 0)); lua_pop(state, 1);
                    lua_rawgeti(state, index, 3); v.z = static_cast<float>(luaL_optnumber(state, -1, 0)); lua_pop(state, 1);
                }
            }
            return v;
        }

        bool HasComponent(Entity entity, const std::string &name, LuaComponentType &type)
        {
            if (name == "Transform") { type = LuaComponentType::Transform; return entity && entity.HasComponent<TransformComponent>(); }
            if (name == "Rigidbody2D") { type = LuaComponentType::Rigidbody2D; return entity && entity.HasComponent<Rigidbody2DComponent>(); }
            if (name == "Rigidbody") { type = LuaComponentType::Rigidbody3D; return entity && entity.HasComponent<RigidbodyComponent>(); }
            if (name == "Sprite2D") { type = LuaComponentType::Sprite2D; return entity && entity.HasComponent<Sprite2DComponent>(); }
            if (name == "StaticMesh") { type = LuaComponentType::StaticMesh; return entity && entity.HasComponent<StaticMeshComponent>(); }
            if (name == "SkeletalMesh") { type = LuaComponentType::SkeletalMesh; return entity && entity.HasComponent<SkeletalMeshComponent>(); }
            if (name == "AudioSource") { type = LuaComponentType::AudioSource; return entity && entity.HasComponent<AudioSourceComponent>(); }
            return false;
        }

        int EntityHasComponent(lua_State *state)
        {
            auto *ref = static_cast<LuaEntityRef *>(luaL_checkudata(state, 1, kEntityMetatable));
            LuaComponentType type{};
            const bool found = HasComponent(ResolveEntity(ref->scene, ref->id), luaL_checkstring(state, 2), type);
            lua_pushboolean(state, found);
            return 1;
        }

        int EntityGetComponent(lua_State *state)
        {
            auto *ref = static_cast<LuaEntityRef *>(luaL_checkudata(state, 1, kEntityMetatable));
            LuaComponentType type{};
            const bool found = HasComponent(ResolveEntity(ref->scene, ref->id), luaL_checkstring(state, 2), type);
            if (!found) { lua_pushnil(state); return 1; }
            auto *component = static_cast<LuaComponentRef *>(lua_newuserdatauv(state, sizeof(LuaComponentRef), 0));
            *component = { ref->scene, ref->id, type };
            luaL_getmetatable(state, kComponentMetatable); lua_setmetatable(state, -2);
            return 1;
        }

        int EntityIndex(lua_State *state)
        {
            auto *ref = static_cast<LuaEntityRef *>(luaL_checkudata(state, 1, kEntityMetatable));
            const char *key = luaL_checkstring(state, 2);
            Entity entity = ResolveEntity(ref->scene, ref->id);
            if (strcmp(key, "id") == 0) { lua_pushinteger(state, static_cast<lua_Integer>(ref->id)); return 1; }
            if (strcmp(key, "name") == 0) { if (entity) lua_pushstring(state, entity.GetName().c_str()); else lua_pushnil(state); return 1; }
            if (strcmp(key, "has_component") == 0)
            {
                lua_pushcfunction(state, EntityHasComponent);
                return 1;
            }
            if (strcmp(key, "get_component") == 0)
            {
                lua_pushcfunction(state, EntityGetComponent);
                return 1;
            }
            lua_pushnil(state);
            return 1;
        }

        int ComponentIndex(lua_State *state)
        {
            auto *ref = static_cast<LuaComponentRef *>(luaL_checkudata(state, 1, kComponentMetatable));
            const char *key = luaL_checkstring(state, 2);
            Entity entity = ResolveEntity(ref->scene, ref->id);
            if (!entity) { lua_pushnil(state); return 1; }
            switch (ref->type)
            {
                case LuaComponentType::Transform:
                {
                    auto &t = entity.GetComponent<TransformComponent>().local;
                    if (strcmp(key, "position") == 0) { PushVector(state, t.translation); return 1; }
                    if (strcmp(key, "scale") == 0) { PushVector(state, t.scale); return 1; }
                    break;
                }
                case LuaComponentType::Rigidbody2D:
                {
                    auto &c = entity.GetComponent<Rigidbody2DComponent>();
                    if (strcmp(key, "linear_velocity") == 0) { PushVector(state, glm::vec3(c.linearVelocity, 0.0f)); return 1; }
                    if (strcmp(key, "angular_velocity") == 0) { lua_pushnumber(state, c.angularVelocity); return 1; }
                    if (strcmp(key, "gravity_scale") == 0) { lua_pushnumber(state, c.gravityScale); return 1; }
                    break;
                }
                case LuaComponentType::Rigidbody3D:
                {
                    auto &c = entity.GetComponent<RigidbodyComponent>();
                    if (strcmp(key, "linear_velocity") == 0) { PushVector(state, c.linearVelocity); return 1; }
                    if (strcmp(key, "angular_velocity") == 0) { PushVector(state, c.angularVelocity); return 1; }
                    if (strcmp(key, "use_gravity") == 0) { lua_pushboolean(state, c.useGravity); return 1; }
                    break;
                }
                case LuaComponentType::Sprite2D:
                {
                    auto &c = entity.GetComponent<Sprite2DComponent>();
                    if (strcmp(key, "flip_x") == 0) { lua_pushboolean(state, c.flipX); return 1; }
                    if (strcmp(key, "flip_y") == 0) { lua_pushboolean(state, c.flipY); return 1; }
                    if (strcmp(key, "color") == 0) { lua_createtable(state, 4, 0); for (int i=0;i<4;i++){ lua_pushnumber(state,c.color[i]); lua_rawseti(state,-2,i+1); } return 1; }
                    break;
                }
                case LuaComponentType::StaticMesh:
                    if (strcmp(key, "asset_handle") == 0) { lua_pushinteger(state, static_cast<lua_Integer>(static_cast<uint64_t>(entity.GetComponent<StaticMeshComponent>().handle))); return 1; }
                    break;
                case LuaComponentType::SkeletalMesh:
                    if (strcmp(key, "asset_handle") == 0) { lua_pushinteger(state, static_cast<lua_Integer>(static_cast<uint64_t>(entity.GetComponent<SkeletalMeshComponent>().handle))); return 1; }
                    break;
                case LuaComponentType::AudioSource:
                {
                    auto &c = entity.GetComponent<AudioSourceComponent>();
                    if (strcmp(key, "volume") == 0) { lua_pushnumber(state, c.volume); return 1; }
                    if (strcmp(key, "pitch") == 0) { lua_pushnumber(state, c.pitch); return 1; }
                    if (strcmp(key, "pan") == 0) { lua_pushnumber(state, c.pan); return 1; }
                    if (strcmp(key, "loop") == 0) { lua_pushboolean(state, c.loop); return 1; }
                    if (strcmp(key, "play_on_start") == 0) { lua_pushboolean(state, c.playOnStart); return 1; }
                    break;
                }
            }
            lua_pushnil(state);
            return 1;
        }

        int ComponentNewIndex(lua_State *state)
        {
            auto *ref = static_cast<LuaComponentRef *>(luaL_checkudata(state, 1, kComponentMetatable));
            const char *key = luaL_checkstring(state, 2);
            Entity entity = ResolveEntity(ref->scene, ref->id);
            if (!entity) return 0;
            switch (ref->type)
            {
                case LuaComponentType::Transform:
                {
                    auto &t = entity.GetComponent<TransformComponent>().local;
                    if (strcmp(key, "position") == 0) { t.translation = ReadVector(state, 3); return 0; }
                    if (strcmp(key, "scale") == 0) { t.scale = ReadVector(state, 3); return 0; }
                    break;
                }
                case LuaComponentType::Rigidbody2D:
                {
                    auto &c = entity.GetComponent<Rigidbody2DComponent>();
                    if (strcmp(key, "linear_velocity") == 0) { const auto v = ReadVector(state, 3); c.linearVelocity = {v.x,v.y}; return 0; }
                    if (strcmp(key, "angular_velocity") == 0) { c.angularVelocity = static_cast<float>(luaL_checknumber(state, 3)); return 0; }
                    if (strcmp(key, "gravity_scale") == 0) { c.gravityScale = static_cast<float>(luaL_checknumber(state, 3)); return 0; }
                    break;
                }
                case LuaComponentType::Rigidbody3D:
                {
                    auto &c = entity.GetComponent<RigidbodyComponent>();
                    if (strcmp(key, "linear_velocity") == 0) { c.linearVelocity = ReadVector(state, 3); return 0; }
                    if (strcmp(key, "angular_velocity") == 0) { c.angularVelocity = ReadVector(state, 3); return 0; }
                    if (strcmp(key, "use_gravity") == 0) { c.useGravity = lua_toboolean(state, 3) != 0; return 0; }
                    break;
                }
                case LuaComponentType::Sprite2D:
                {
                    auto &c = entity.GetComponent<Sprite2DComponent>();
                    if (strcmp(key, "flip_x") == 0) { c.flipX = lua_toboolean(state, 3) != 0; return 0; }
                    if (strcmp(key, "flip_y") == 0) { c.flipY = lua_toboolean(state, 3) != 0; return 0; }
                    if (strcmp(key, "color") == 0 && lua_istable(state, 3)) { for(int i=0;i<4;i++){ lua_rawgeti(state,3,i+1); c.color[i]=static_cast<float>(luaL_optnumber(state,-1,1)); lua_pop(state,1); } return 0; }
                    break;
                }
                case LuaComponentType::AudioSource:
                {
                    auto &c = entity.GetComponent<AudioSourceComponent>();
                    if (strcmp(key, "volume") == 0) { c.volume = static_cast<float>(luaL_checknumber(state, 3)); return 0; }
                    if (strcmp(key, "pitch") == 0) { c.pitch = static_cast<float>(luaL_checknumber(state, 3)); return 0; }
                    if (strcmp(key, "pan") == 0) { c.pan = static_cast<float>(luaL_checknumber(state, 3)); return 0; }
                    if (strcmp(key, "loop") == 0) { c.loop = lua_toboolean(state, 3) != 0; return 0; }
                    if (strcmp(key, "play_on_start") == 0) { c.playOnStart = lua_toboolean(state, 3) != 0; return 0; }
                    break;
                }
                case LuaComponentType::StaticMesh:
                case LuaComponentType::SkeletalMesh:
                    break;
            }
            return luaL_error(state, "unsupported property '%s' for component", key);
        }

        int IgniteLog(lua_State *state)
        {
            LOG_INFO("[Lua] {}", luaL_checkstring(state, 1));
            return 0;
        }

        int IgniteFindEntity(lua_State *state)
        {
            auto **scene = static_cast<Scene **>(lua_touserdata(state, lua_upvalueindex(1)));
            const uint64_t id = static_cast<uint64_t>(luaL_checkinteger(state, 1));
            if (!ResolveEntity(*scene, id)) { lua_pushnil(state); return 1; }
            auto *ref = static_cast<LuaEntityRef *>(lua_newuserdatauv(state, sizeof(LuaEntityRef), 0));
            *ref = { *scene, id };
            luaL_getmetatable(state, kEntityMetatable); lua_setmetatable(state, -2);
            return 1;
        }

        void PushEntity(lua_State *state, Scene *scene, uint64_t id)
        {
            auto *ref = static_cast<LuaEntityRef *>(lua_newuserdatauv(state, sizeof(LuaEntityRef), 0));
            *ref = { scene, id };
            luaL_getmetatable(state, kEntityMetatable); lua_setmetatable(state, -2);
        }

        void RegisterBindings(lua_State *state, Scene *scene)
        {
            luaL_newmetatable(state, kEntityMetatable);
            lua_pushcfunction(state, EntityIndex); lua_setfield(state, -2, "__index");
            lua_pop(state, 1);
            luaL_newmetatable(state, kComponentMetatable);
            lua_pushcfunction(state, ComponentIndex); lua_setfield(state, -2, "__index");
            lua_pushcfunction(state, ComponentNewIndex); lua_setfield(state, -2, "__newindex");
            lua_pop(state, 1);

            lua_newtable(state);
            lua_pushcfunction(state, IgniteLog); lua_setfield(state, -2, "Log");
            auto **sceneRef = static_cast<Scene **>(lua_newuserdatauv(state, sizeof(Scene *), 0));
            *sceneRef = scene;
            lua_pushcclosure(state, IgniteFindEntity, 1); lua_setfield(state, -2, "FindEntity");
            lua_setglobal(state, "Ignite");
        }
    }

    LuaScriptRuntime::LuaScriptRuntime(Scene *scene)
        : m_Scene(scene), m_State(luaL_newstate())
    {
        if (!m_State)
        {
            LOG_ERROR("[Lua] Failed to create Lua state");
            return;
        }
        luaL_openlibs(m_State);
        RegisterBindings(m_State, m_Scene);
    }

    LuaScriptRuntime::~LuaScriptRuntime()
    {
        if (m_State)
        {
            for (auto &[id, instance] : m_Instances)
            {
                Invoke(id, instance, "on_destroy", 0.0f, false);
                luaL_unref(m_State, LUA_REGISTRYINDEX, instance.callbacksRef);
                luaL_unref(m_State, LUA_REGISTRYINDEX, instance.environmentRef);
            }
            lua_close(m_State);
        }
    }

    std::filesystem::path LuaScriptRuntime::ResolvePath(const std::filesystem::path &path) const
    {
        if (path.is_absolute()) return path.lexically_normal();
        Project *project = m_Scene ? m_Scene->GetProject() : nullptr;
        return project ? (project->GetDirectory() / path).lexically_normal() : path;
    }

    void LuaScriptRuntime::CreateInstance(UUID entityId, const std::filesystem::path &scriptPath)
    {
        if (!m_State || !m_Scene || scriptPath.empty()) return;
        DestroyInstance(entityId);
        Instance instance;
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
            LOG_ERROR("[Lua] Failed loading '{}' for entity {}: {}", scriptPath.generic_string(), static_cast<uint64_t>(entityId), LuaErrorMessage(m_State));
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
            LOG_ERROR("[Lua] Failed executing '{}' for entity {}: {}", scriptPath.generic_string(), static_cast<uint64_t>(entityId), LuaErrorMessage(m_State));
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
        auto it = m_Instances.find(entityId);
        if (!Invoke(entityId, it->second, "on_create", 0.0f, false))
            DestroyInstance(entityId);
    }

    bool LuaScriptRuntime::Invoke(UUID entityId, Instance &instance, const char *callback, float deltaTime, bool hasDeltaTime)
    {
        if (!m_State || instance.callbacksRef == LUA_NOREF) return false;
        const int baseTop = lua_gettop(m_State);
        lua_rawgeti(m_State, LUA_REGISTRYINDEX, instance.callbacksRef);
        lua_pushstring(m_State, callback);
        lua_rawget(m_State, -2);
        if (lua_isnil(m_State, -1)) { lua_settop(m_State, baseTop); return true; }
        if (!lua_isfunction(m_State, -1))
        {
            LOG_ERROR("[Lua] Callback '{}' in '{}' is not a function (entity {})", callback, instance.path.generic_string(), static_cast<uint64_t>(entityId));
            lua_settop(m_State, baseTop);
            return false;
        }
        PushEntity(m_State, m_Scene, static_cast<uint64_t>(entityId));
        int argCount = 1;
        if (hasDeltaTime) { lua_pushnumber(m_State, deltaTime); ++argCount; }
        if (lua_pcall(m_State, argCount, 0, 0) != LUA_OK)
        {
            LOG_ERROR("[Lua] Callback '{}' failed in '{}' for entity {}: {}", callback, instance.path.generic_string(), static_cast<uint64_t>(entityId), LuaErrorMessage(m_State));
            lua_settop(m_State, baseTop);
            return false;
        }
        lua_settop(m_State, baseTop);
        return true;
    }

    void LuaScriptRuntime::DestroyInstance(UUID entityId)
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
    }

    void LuaScriptRuntime::Update(float deltaTime)
    {
        std::vector<UUID> failed;
        for (auto &[id, instance] : m_Instances)
        {
            Entity entity = ResolveEntity(m_Scene, static_cast<uint64_t>(id));
            if (!entity || !entity.HasComponent<LuaScriptComponent>() || !Invoke(id, instance, "on_update", deltaTime, true))
                failed.push_back(id);
        }
        for (UUID id : failed) DestroyInstance(id);
    }

    void LuaScriptRuntime::FixedUpdate()
    {
        std::vector<UUID> failed;
        for (auto &[id, instance] : m_Instances)
        {
            Entity entity = ResolveEntity(m_Scene, static_cast<uint64_t>(id));
            if (!entity || !entity.HasComponent<LuaScriptComponent>() || !Invoke(id, instance, "on_fixed_update", 0.0f, false))
                failed.push_back(id);
        }
        for (UUID id : failed) DestroyInstance(id);
    }
}
