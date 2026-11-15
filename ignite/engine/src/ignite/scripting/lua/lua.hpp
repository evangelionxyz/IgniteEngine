// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_LUA_HPP
#define IGN_LUA_HPP

#include <cstdint>
#include <string>
#include "ignite/scene/entity.hpp"

struct lua_State;
namespace ignite
{
    class Scene;
}

namespace ignite::lua
{
    constexpr const char *kEntityMetatable = "Ignite.Entity";
    constexpr const char *kComponentMetatable = "Ignite.Component";
    constexpr const char *kVecMetatable = "Ignite.Vec";

    enum class ComponentType : uint8_t
    {
        Rendering,
        Transform,
        Rigidbody2D,
        Rigidbody3D,
        Sprite2D,
        Circle2D,
        StaticMesh,
        SkeletalMesh,
        AudioSource,
        Camera,
        DirectionalLight,
        PointLight,
        SpotLight,
        CharacterController,
        BoxCollider,
        SphereCollider,
        CapsuleCollider,
        BoxCollider2D,
        CircleCollider2D,
        Text,
        Widget,
        NetworkIdentity
    };

    struct EntityRef
    {
        Scene *scene;
        uint64_t id;
    };

    struct ComponentRef
    {
        Scene *scene;
        uint64_t id;
        ComponentType type;
    };

    const char *LuaErrorMessage(lua_State *state);

    Entity ResolveEntity(Scene *scene, uint64_t id);

    void PushVector(lua_State *state, const glm::vec2 &v);
    void PushVector(lua_State *state, const glm::vec3 &v);
    void PushVector(lua_State *state, const glm::vec4 &v);
    glm::vec2 ReadVec2(lua_State *state, int index);
    glm::vec3 ReadVector(lua_State *state, int index);
    glm::vec4 ReadVec4(lua_State *state, int index);

    // Component State
    bool ParseComponentType(const std::string &name, ComponentType &type);
    bool HasComponent(Entity entity, const std::string &name, ComponentType &type);
    int EntityHasComponent(lua_State *state);
    int EntityGetComponent(lua_State *state);
    int EntityAddComponent(lua_State *state);
    int EntityRemoveComponent(lua_State *state);
    int EntityGetTransform(lua_State *state);
    int EntityIndex(lua_State *state);
    int EntityNewIndex(lua_State *state);

    // Components
    int RenderingSetVisible(lua_State *state);

    int Rigidbody2DApplyForce(lua_State *state);
    int Rigidbody2DApplyLinearImpulse(lua_State *state);
    int Rigidbody2DApplyTorque(lua_State *state);

    int ComponentIndex(lua_State *state);
    int ComponentNewIndex(lua_State *state);

    int IgniteLog(lua_State *state);
    int IgniteLogWarn(lua_State *state);
    int IgniteLogError(lua_State *state);

    void PushEntity(lua_State *state, Scene *scene, uint64_t id);
    int IgniteFindEntity(lua_State *state);
    int IgniteFindEntityByName(lua_State *state);

    int LuaInputIsKeyPressed(lua_State *state);
    int LuaInputIsMouseButtonPressed(lua_State *state);
    int LuaInputIsActionPressed(lua_State *state);
    int LuaInputGetMousePosition(lua_State *state);

    void RegisterBindings(lua_State *state, Scene *scene);
}

#endif
