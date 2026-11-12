// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"
#include "lua.hpp"

#include "ignite/core/application.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/core/input/input_system.hpp"
#include "ignite/physics/2d/physics_2d_component.hpp"

extern "C"
{
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

namespace ignite::lua
{
    const char* LuaErrorMessage(lua_State* state)
    {
        const char* message = lua_tostring(state, -1);
        return message ? message : "(non-string Lua error)";
    }

    Entity ResolveEntity(Scene* scene, uint64_t id)
    {
        if (!scene)
            return {};
        const auto it = scene->entities.find(UUID(id));
        if (it == scene->entities.end() || !scene->registry->valid(it->second))
            return {};
        return Entity(it->second, scene);
    }

    void PushVector(lua_State* state, const glm::vec3& v)
    {
        lua_createtable(state, 0, 3);
        lua_pushnumber(state, v.x); lua_setfield(state, -2, "x");
        lua_pushnumber(state, v.y); lua_setfield(state, -2, "y");
        lua_pushnumber(state, v.z); lua_setfield(state, -2, "z");
    }

    glm::vec3 ReadVector(lua_State* state, int index)
    {
        glm::vec3 v(0.0f);
        if (lua_istable(state, index))
        {
            lua_getfield(state, index, "x");
            const bool named = !lua_isnil(state, -1);
            if (lua_isnumber(state, -1))
                v.x = static_cast<float>(lua_tonumber(state, -1));
            lua_pop(state, 1);

            if (named)
            {
                lua_getfield(state, index, "y");
                if (lua_isnumber(state, -1))
                    v.y = static_cast<float>(lua_tonumber(state, -1));
                lua_pop(state, 1);

                lua_getfield(state, index, "z");
                if (lua_isnumber(state, -1))
                    v.z = static_cast<float>(lua_tonumber(state, -1));
                lua_pop(state, 1);
            }
            else
            {
                lua_rawgeti(state, index, 1);
                v.x = static_cast<float>(luaL_optnumber(state, -1, 0));
                lua_pop(state, 1);

                lua_rawgeti(state, index, 2);
                v.y = static_cast<float>(luaL_optnumber(state, -1, 0));
                lua_pop(state, 1);

                lua_rawgeti(state, index, 3);
                v.z = static_cast<float>(luaL_optnumber(state, -1, 0));
                lua_pop(state, 1);
            }
        }
        return v;
    }

    bool HasComponent(Entity entity, const std::string& name, ComponentType& type)
    {
        if (name == "Rendering") { type = ComponentType::Rendering; return entity && entity.HasComponent<RenderingComponent>(); }
        if (name == "Transform") { type = ComponentType::Transform; return entity && entity.HasComponent<TransformComponent>(); }
        if (name == "Rigidbody2D") { type = ComponentType::Rigidbody2D; return entity && entity.HasComponent<Rigidbody2DComponent>(); }
        if (name == "Rigidbody") { type = ComponentType::Rigidbody3D; return entity && entity.HasComponent<RigidbodyComponent>(); }
        if (name == "Sprite2D") { type = ComponentType::Sprite2D; return entity && entity.HasComponent<Sprite2DComponent>(); }
        if (name == "StaticMesh") { type = ComponentType::StaticMesh; return entity && entity.HasComponent<StaticMeshComponent>(); }
        if (name == "SkeletalMesh") { type = ComponentType::SkeletalMesh; return entity && entity.HasComponent<SkeletalMeshComponent>(); }
        if (name == "AudioSource") { type = ComponentType::AudioSource; return entity && entity.HasComponent<AudioSourceComponent>(); }
        return false;
    }

    int EntityHasComponent(lua_State* state)
    {
        auto* ref = static_cast<EntityRef*>(luaL_checkudata(state, 1, kEntityMetatable));
        ComponentType type{};

        const bool found = HasComponent(ResolveEntity(ref->scene, ref->id), luaL_checkstring(state, 2), type);
        lua_pushboolean(state, found);

        return 1;
    }

    int EntityGetComponent(lua_State* state)
    {
        auto* ref = static_cast<EntityRef*>(luaL_checkudata(state, 1, kEntityMetatable));
        ComponentType type{};

        const bool found = HasComponent(ResolveEntity(ref->scene, ref->id), luaL_checkstring(state, 2), type);
        if (!found)
        {
            lua_pushnil(state);
            return 1;
        }

        auto* component = static_cast<ComponentRef*>(lua_newuserdatauv(state, sizeof(ComponentRef), 0));
        *component =
        {
            ref->scene,
            ref->id,
            type
        };

        luaL_getmetatable(state, kComponentMetatable);
        lua_setmetatable(state, -2);

        return 1;
    }

    int EntityGetTransform(lua_State* state)
    {
        auto* ref = static_cast<EntityRef*>(luaL_checkudata(state, 1, kEntityMetatable));
        Entity entity = ResolveEntity(ref->scene, ref->id);

        if (!entity || !entity.HasComponent<TransformComponent>())
        {
            lua_pushnil(state);
            return 1;
        }

        auto* component = static_cast<ComponentRef*>(lua_newuserdatauv(state, sizeof(ComponentRef), 0));
        *component =
        {
            ref->scene,
            ref->id,
            ComponentType::Transform
        };

        luaL_getmetatable(state, kComponentMetatable);
        lua_setmetatable(state, -2);

        return 1;
    }

    int EntityIndex(lua_State* state)
    {
        auto* ref = static_cast<EntityRef*>(luaL_checkudata(state, 1, kEntityMetatable));
        const char* key = luaL_checkstring(state, 2);
        Entity entity = ResolveEntity(ref->scene, ref->id);

        if (strcmp(key, "id") == 0)
        {
            lua_pushinteger(state, static_cast<lua_Integer>(ref->id));
            return 1;
        }

        if (strcmp(key, "name") == 0)
        {
            if (entity)
                lua_pushstring(state, entity.GetName().c_str());
            else
                lua_pushnil(state);
            return 1;
        }

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

        if (strcmp(key, "get_transform") == 0)
        {
            lua_pushcfunction(state, EntityGetTransform);
            return 1;
        }

        lua_pushnil(state);
        return 1;
    }

    int RenderingSetVisible(lua_State* state)
    {
        auto* ref = static_cast<ComponentRef*>(luaL_checkudata(state, 1, kComponentMetatable));
        const auto visible = static_cast<bool>(luaL_checkinteger(state, 2));
        Entity entity = ResolveEntity(ref->scene, ref->id);
        if (entity && entity.HasComponent<RenderingComponent>())
        {
            auto& c = entity.GetComponent<RenderingComponent>();
            c.visible = visible;
        }
        return 0;
    }

    int Rigidbody2DApplyForce(lua_State* state)
    {
        auto* ref = static_cast<ComponentRef*>(luaL_checkudata(state, 1, kComponentMetatable));
        const auto fx = static_cast<float>(luaL_checknumber(state, 2));
        const auto fy = static_cast<float>(luaL_checknumber(state, 3));

        Entity entity = ResolveEntity(ref->scene, ref->id);
        if (entity && entity.HasComponent<Rigidbody2DComponent>())
        {
            auto& c = entity.GetComponent<Rigidbody2DComponent>();
            if (b2Body_IsValid(c.bodyId))
            {
                b2Body_ApplyForceToCenter(c.bodyId, b2Vec2{ fx, fy }, true);
            }
            else
            {
                c.linearVelocity += glm::vec2(fx, fy);
            }
        }
        return 0;
    }

    int Rigidbody2DApplyLinearImpulse(lua_State* state)
    {
        auto* ref = static_cast<ComponentRef*>(luaL_checkudata(state, 1, kComponentMetatable));
        const auto ix = static_cast<float>(luaL_checknumber(state, 2));
        const auto iy = static_cast<float>(luaL_checknumber(state, 3));
        Entity entity = ResolveEntity(ref->scene, ref->id);
        if (entity && entity.HasComponent<Rigidbody2DComponent>())
        {
            auto& c = entity.GetComponent<Rigidbody2DComponent>();
            if (b2Body_IsValid(c.bodyId))
            {
                b2Body_ApplyLinearImpulseToCenter(c.bodyId, b2Vec2{ ix, iy }, true);
            }
            else
            {
                c.linearVelocity += glm::vec2(ix, iy);
            }
        }
        return 0;
    }

    int Rigidbody2DApplyTorque(lua_State* state)
    {
        auto* ref = static_cast<ComponentRef*>(luaL_checkudata(state, 1, kComponentMetatable));
        const auto torque = static_cast<float>(luaL_checknumber(state, 2));
        Entity entity = ResolveEntity(ref->scene, ref->id);
        if (entity && entity.HasComponent<Rigidbody2DComponent>())
        {
            auto& c = entity.GetComponent<Rigidbody2DComponent>();
            if (b2Body_IsValid(c.bodyId))
            {
                b2Body_ApplyTorque(c.bodyId, torque, true);
            }
            else
            {
                c.angularVelocity += torque;
            }
        }
        return 0;
    }

    int ComponentIndex(lua_State* state)
    {
        auto* ref = static_cast<ComponentRef*>(luaL_checkudata(state, 1, kComponentMetatable));
        const char* key = luaL_checkstring(state, 2);
        Entity entity = ResolveEntity(ref->scene, ref->id);

        if (!entity)
        {
            lua_pushnil(state);
            return 1;
        }

        switch (ref->type)
        {
        case ComponentType::Transform:
        {
            auto& t = entity.GetComponent<TransformComponent>().local;
            if (strcmp(key, "position") == 0) { PushVector(state, t.translation); return 1; }
            if (strcmp(key, "rotation") == 0) { PushVector(state, glm::degrees(glm::eulerAngles(t.rotation))); return 1; }
            if (strcmp(key, "scale") == 0) { PushVector(state, t.scale); return 1; }
            break;
        }
        case ComponentType::Rigidbody2D:
        {
            auto& c = entity.GetComponent<Rigidbody2DComponent>();
            if (strcmp(key, "linear_velocity") == 0) { PushVector(state, glm::vec3(c.linearVelocity, 0.0f)); return 1; }
            if (strcmp(key, "angular_velocity") == 0) { lua_pushnumber(state, c.angularVelocity); return 1; }
            if (strcmp(key, "gravity_scale") == 0) { lua_pushnumber(state, c.gravityScale); return 1; }
            if (strcmp(key, "apply_force") == 0) { lua_pushcfunction(state, Rigidbody2DApplyForce); return 1; }
            if (strcmp(key, "apply_linear_impulse") == 0) { lua_pushcfunction(state, Rigidbody2DApplyLinearImpulse); return 1; }
            if (strcmp(key, "apply_torque") == 0) { lua_pushcfunction(state, Rigidbody2DApplyTorque); return 1; }
            break;
        }
        case ComponentType::Rigidbody3D:
        {
            auto& c = entity.GetComponent<RigidbodyComponent>();
            if (strcmp(key, "linear_velocity") == 0) { PushVector(state, c.linearVelocity); return 1; }
            if (strcmp(key, "angular_velocity") == 0) { PushVector(state, c.angularVelocity); return 1; }
            if (strcmp(key, "use_gravity") == 0) { lua_pushboolean(state, c.useGravity); return 1; }
            break;
        }
        case ComponentType::Sprite2D:
        {
            auto& c = entity.GetComponent<Sprite2DComponent>();
            if (strcmp(key, "flip_x") == 0) { lua_pushboolean(state, c.flipX); return 1; }
            if (strcmp(key, "flip_y") == 0) { lua_pushboolean(state, c.flipY); return 1; }
            if (strcmp(key, "color") == 0)
            {
                lua_createtable(state, 4, 0);
                for (int i = 0; i < 4; i++)
                {
                    lua_pushnumber(state, c.color[i]);
                    lua_rawseti(state, -2, i + 1);
                }
                return 1;
            }

            break;
        }
        case ComponentType::StaticMesh:
        {
            if (strcmp(key, "asset_handle") == 0)
            {
                lua_pushinteger(state, static_cast<lua_Integer>(static_cast<uint64_t>(entity.GetComponent<StaticMeshComponent>().handle)));
                return 1;
            }
            break;
        }
        case ComponentType::SkeletalMesh:
        {
            if (strcmp(key, "asset_handle") == 0)
            {
                lua_pushinteger(state, static_cast<lua_Integer>(static_cast<uint64_t>(entity.GetComponent<SkeletalMeshComponent>().handle)));
                return 1;
            }
            break;
        }
        case ComponentType::AudioSource:
        {
            auto& c = entity.GetComponent<AudioSourceComponent>();
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

    int ComponentNewIndex(lua_State* state)
    {
        auto* ref = static_cast<ComponentRef*>(luaL_checkudata(state, 1, kComponentMetatable));
        const char* key = luaL_checkstring(state, 2);

        Entity entity = ResolveEntity(ref->scene, ref->id);
        if (!entity)
            return 0;

        switch (ref->type)
        {
        case ComponentType::Transform:
        {
            auto& t = entity.GetComponent<TransformComponent>().local;
            if (strcmp(key, "position") == 0) { t.translation = ReadVector(state, 3); return 0; }
            if (strcmp(key, "rotation") == 0) { t.rotation = glm::quat(glm::radians(ReadVector(state, 3))); return 0; }
            if (strcmp(key, "scale") == 0) { t.scale = ReadVector(state, 3); return 0; }
            break;
        }
        case ComponentType::Rigidbody2D:
        {
            auto& c = entity.GetComponent<Rigidbody2DComponent>();
            if (strcmp(key, "linear_velocity") == 0) { const auto v = ReadVector(state, 3); c.linearVelocity = { v.x,v.y }; return 0; }
            if (strcmp(key, "angular_velocity") == 0) { c.angularVelocity = static_cast<float>(luaL_checknumber(state, 3)); return 0; }
            if (strcmp(key, "gravity_scale") == 0) { c.gravityScale = static_cast<float>(luaL_checknumber(state, 3)); return 0; }
            break;
        }
        case ComponentType::Rigidbody3D:
        {
            auto& c = entity.GetComponent<RigidbodyComponent>();
            if (strcmp(key, "linear_velocity") == 0) { c.linearVelocity = ReadVector(state, 3); return 0; }
            if (strcmp(key, "angular_velocity") == 0) { c.angularVelocity = ReadVector(state, 3); return 0; }
            if (strcmp(key, "use_gravity") == 0) { c.useGravity = lua_toboolean(state, 3) != 0; return 0; }
            break;
        }
        case ComponentType::Sprite2D:
        {
            auto& c = entity.GetComponent<Sprite2DComponent>();
            if (strcmp(key, "flip_x") == 0)
            {
                c.flipX = lua_toboolean(state, 3) != 0;
                return 0;
            }
            if (strcmp(key, "flip_y") == 0)
            {
                c.flipY = lua_toboolean(state, 3) != 0;
                return 0;
            }
            if (strcmp(key, "color") == 0 && lua_istable(state, 3))
            {
                for (int i = 0; i < 4; i++)
                {
                    lua_rawgeti(state, 3, i + 1);
                    c.color[i] = static_cast<float>(luaL_optnumber(state, -1, 1));
                    lua_pop(state, 1);
                }
                return 0;
            }
            break;
        }
        case ComponentType::AudioSource:
        {
            auto& c = entity.GetComponent<AudioSourceComponent>();
            if (strcmp(key, "volume") == 0) { c.volume = static_cast<float>(luaL_checknumber(state, 3)); return 0; }
            if (strcmp(key, "pitch") == 0) { c.pitch = static_cast<float>(luaL_checknumber(state, 3)); return 0; }
            if (strcmp(key, "pan") == 0) { c.pan = static_cast<float>(luaL_checknumber(state, 3)); return 0; }
            if (strcmp(key, "loop") == 0) { c.loop = lua_toboolean(state, 3) != 0; return 0; }
            if (strcmp(key, "play_on_start") == 0) { c.playOnStart = lua_toboolean(state, 3) != 0; return 0; }
            break;
        }
        case ComponentType::StaticMesh:
        case ComponentType::SkeletalMesh:
            break;
        }
        return luaL_error(state, "unsupported property '%s' for component", key);
    }

    int IgniteLog(lua_State* state)
    {
        LOG_INFO("[Lua] {}", luaL_checkstring(state, 1));
        return 0;
    }

    int IgniteLogWarn(lua_State* state)
    {
        LOG_WARN("[Lua] {}", luaL_checkstring(state, 1));
        return 0;
    }

    int IgniteLogError(lua_State* state)
    {
        LOG_ERROR("[Lua] {}", luaL_checkstring(state, 1));
        return 0;
    }

    void PushEntity(lua_State* state, Scene* scene, uint64_t id)
    {
        auto* ref = static_cast<EntityRef*>(lua_newuserdatauv(state, sizeof(EntityRef), 0));
        *ref = { scene, id };

        luaL_getmetatable(state, kEntityMetatable);
        lua_setmetatable(state, -2);
    }

    int IgniteFindEntity(lua_State* state)
    {
        auto** scene = static_cast<Scene**>(lua_touserdata(state, lua_upvalueindex(1)));
        const uint64_t id = static_cast<uint64_t>(luaL_checkinteger(state, 1));
        if (!ResolveEntity(*scene, id))
        {
            lua_pushnil(state);
            return 1;
        }

        PushEntity(state, *scene, id);
        return 1;
    }

    int IgniteFindEntityByName(lua_State* state)
    {
        auto** scene = static_cast<Scene**>(lua_touserdata(state, lua_upvalueindex(1)));
        const char* name = luaL_checkstring(state, 1);
        if (!*scene) { lua_pushnil(state); return 1; }
        for (const auto& [uuid, enttId] : (*scene)->entities)
        {
            if (!(*scene)->registry->valid(enttId)) continue;
            Entity e(enttId, *scene);
            if (e.GetName() == name)
            {
                PushEntity(state, *scene, static_cast<uint64_t>(uuid));
                return 1;
            }
        }
        lua_pushnil(state);
        return 1;
    }

    int LuaInputIsKeyPressed(lua_State* state)
    {
        const int key = static_cast<int>(luaL_checkinteger(state, 1));
        lua_pushboolean(state, InputSystem::IsKeyPressed(static_cast<KeyCode>(key)));
        return 1;
    }

    int LuaInputIsMouseButtonPressed(lua_State* state)
    {
        const int button = static_cast<int>(luaL_checkinteger(state, 1));
        lua_pushboolean(state, InputSystem::IsMouseButtonPressed(static_cast<MouseCode>(button)));
        return 1;
    }

    int LuaInputIsActionPressed(lua_State* state)
    {
        const char* action = luaL_checkstring(state, 1);
        lua_pushboolean(state, InputSystem::IsActionPressed(action));
        return 1;
    }

    int LuaInputGetMousePosition(lua_State* state)
    {
        const glm::ivec2 pos = InputSystem::GetMousePosition();
        lua_createtable(state, 0, 2);
        lua_pushnumber(state, pos.x); lua_setfield(state, -2, "x");
        lua_pushnumber(state, pos.y); lua_setfield(state, -2, "y");
        return 1;
    }

    void RegisterBindings(lua_State* state, Scene* scene)
    {
        luaL_newmetatable(state, kEntityMetatable);
        lua_pushcfunction(state, EntityIndex);
        lua_setfield(state, -2, "__index");
        lua_pop(state, 1);

        luaL_newmetatable(state, kComponentMetatable);
        lua_pushcfunction(state, ComponentIndex);
        lua_setfield(state, -2, "__index");

        lua_pushcfunction(state, ComponentNewIndex);
        lua_setfield(state, -2, "__newindex");
        lua_pop(state, 1);

        lua_newtable(state);
        lua_pushcfunction(state, IgniteLog);
        lua_setfield(state, -2, "Log");

        lua_pushcfunction(state, IgniteLogWarn);
        lua_setfield(state, -2, "LogWarn");

        lua_pushcfunction(state, IgniteLogError);
        lua_setfield(state, -2, "LogError");

        auto** sceneRef = static_cast<Scene**>(lua_newuserdatauv(state, sizeof(Scene*), 0));
        *sceneRef = scene;

        lua_pushcclosure(state, IgniteFindEntity, 1);
        lua_setfield(state, -2, "FindEntity");

        auto** sceneRef2 = static_cast<Scene**>(lua_newuserdatauv(state, sizeof(Scene*), 0));
        *sceneRef2 = scene;
        lua_pushcclosure(state, IgniteFindEntityByName, 1);
        lua_setfield(state, -2, "FindEntityByName");

        // Input sub-table
        lua_newtable(state);
        lua_pushcfunction(state, LuaInputIsKeyPressed);
        lua_setfield(state, -2, "IsKeyPressed");

        lua_pushcfunction(state, LuaInputIsMouseButtonPressed);
        lua_setfield(state, -2, "IsMouseButtonPressed");

        lua_pushcfunction(state, LuaInputIsActionPressed);
        lua_setfield(state, -2, "IsActionPressed");

        lua_pushcfunction(state, LuaInputGetMousePosition);
        lua_setfield(state, -2, "GetMousePosition");
        lua_setfield(state, -2, "Input");

        lua_setglobal(state, "Ignite");
    }
}
