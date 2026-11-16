// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"
#include "lua.hpp"

#include "ignite/core/application.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/core/input/input_system.hpp"
#include "ignite/asset/asset_manager.hpp"
#include "ignite/audio/fmod_sound.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/net/network_manager.hpp"
#include "ignite/physics/2d/physics_2d.hpp"
#include "ignite/physics/2d/physics_2d_component.hpp"
#include "ignite/physics/3d/physics_3d.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/scene/prefab.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/scene/scene_manager.hpp"

#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>

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
        Scene *GetSceneUpvalue(lua_State *state)
        {
            auto **scenePtr = static_cast<Scene **>(lua_touserdata(state, lua_upvalueindex(1)));
            return scenePtr ? *scenePtr : nullptr;
        }

        template<typename T>
        bool PushNewComponentRef(lua_State *state, Scene *scene, uint64_t id, ComponentType type, Entity entity)
        {
            if (!entity.HasComponent<T>())
            {
                entity.AddComponent<T>();
            }
            auto *component = static_cast<ComponentRef *>(lua_newuserdatauv(state, sizeof(ComponentRef), 0));
            *component = { scene, id, type };
            luaL_getmetatable(state, kComponentMetatable);
            lua_setmetatable(state, -2);
            return true;
        }

        // Vector helpers & metatable
        int DetectVecDim(lua_State *state, int index)
        {
            if (!lua_istable(state, index))
                return 0;
            lua_getfield(state, index, "w");
            const bool hasW = !lua_isnil(state, -1);
            lua_pop(state, 1);
            if (hasW) return 4;

            lua_getfield(state, index, "z");
            const bool hasZ = !lua_isnil(state, -1);
            lua_pop(state, 1);
            if (hasZ) return 3;

            lua_getfield(state, index, "y");
            const bool hasY = !lua_isnil(state, -1);
            lua_pop(state, 1);
            if (hasY) return 2;

            return static_cast<int>(lua_rawlen(state, index));
        }

        int PushVecResult(lua_State *state, const glm::vec4 &v, int dim)
        {
            if (dim <= 2)
                PushVector(state, glm::vec2(v.x, v.y));
            else if (dim == 3)
                PushVector(state, glm::vec3(v.x, v.y, v.z));
            else
                PushVector(state, v);
            return 1;
        }

        int VecAdd(lua_State *state)
        {
            const int dim = std::max(DetectVecDim(state, 1), DetectVecDim(state, 2));
            const glm::vec4 a = ReadVec4(state, 1);
            const glm::vec4 b = ReadVec4(state, 2);
            return PushVecResult(state, a + b, dim == 0 ? 3 : dim);
        }

        int VecSub(lua_State *state)
        {
            const int dim = std::max(DetectVecDim(state, 1), DetectVecDim(state, 2));
            const glm::vec4 a = ReadVec4(state, 1);
            const glm::vec4 b = ReadVec4(state, 2);
            return PushVecResult(state, a - b, dim == 0 ? 3 : dim);
        }

        int VecMul(lua_State *state)
        {
            if (lua_isnumber(state, 1))
            {
                const float s = static_cast<float>(lua_tonumber(state, 1));
                const int dim = DetectVecDim(state, 2);
                const glm::vec4 v = ReadVec4(state, 2);
                return PushVecResult(state, v * s, dim == 0 ? 3 : dim);
            }
            if (lua_isnumber(state, 2))
            {
                const int dim = DetectVecDim(state, 1);
                const glm::vec4 v = ReadVec4(state, 1);
                const float s = static_cast<float>(lua_tonumber(state, 2));
                return PushVecResult(state, v * s, dim == 0 ? 3 : dim);
            }
            const int dim = std::max(DetectVecDim(state, 1), DetectVecDim(state, 2));
            const glm::vec4 a = ReadVec4(state, 1);
            const glm::vec4 b = ReadVec4(state, 2);
            return PushVecResult(state, a * b, dim == 0 ? 3 : dim);
        }

        int VecDiv(lua_State *state)
        {
            const int dim = DetectVecDim(state, 1);
            const glm::vec4 a = ReadVec4(state, 1);
            if (lua_isnumber(state, 2))
            {
                const float s = static_cast<float>(lua_tonumber(state, 2));
                return PushVecResult(state, s != 0.0f ? (a / s) : glm::vec4(0.0f), dim == 0 ? 3 : dim);
            }
            const glm::vec4 b = ReadVec4(state, 2);
            glm::vec4 r(0.0f);
            r.x = b.x != 0.0f ? a.x / b.x : 0.0f;
            r.y = b.y != 0.0f ? a.y / b.y : 0.0f;
            r.z = b.z != 0.0f ? a.z / b.z : 0.0f;
            r.w = b.w != 0.0f ? a.w / b.w : 0.0f;
            return PushVecResult(state, r, dim == 0 ? 3 : dim);
        }

        int VecUnm(lua_State *state)
        {
            const int dim = DetectVecDim(state, 1);
            const glm::vec4 a = ReadVec4(state, 1);
            return PushVecResult(state, -a, dim == 0 ? 3 : dim);
        }

        int VecLength(lua_State *state)
        {
            const int dim = DetectVecDim(state, 1);
            const glm::vec4 v = ReadVec4(state, 1);
            float len = 0.0f;
            if (dim <= 2) len = glm::length(glm::vec2(v));
            else if (dim == 3) len = glm::length(glm::vec3(v));
            else len = glm::length(v);
            lua_pushnumber(state, len);
            return 1;
        }

        int VecNormalized(lua_State *state)
        {
            const int dim = DetectVecDim(state, 1);
            const glm::vec4 v = ReadVec4(state, 1);
            if (dim <= 2)
            {
                const float len = glm::length(glm::vec2(v));
                PushVector(state, len > 1e-6f ? glm::vec2(v) / len : glm::vec2(0.0f));
                return 1;
            }
            if (dim == 3)
            {
                const float len = glm::length(glm::vec3(v));
                PushVector(state, len > 1e-6f ? glm::vec3(v) / len : glm::vec3(0.0f));
                return 1;
            }
            const float len = glm::length(v);
            PushVector(state, len > 1e-6f ? v / len : glm::vec4(0.0f));
            return 1;
        }

        int VecDot(lua_State *state)
        {
            const glm::vec4 a = ReadVec4(state, 1);
            const glm::vec4 b = ReadVec4(state, 2);
            lua_pushnumber(state, a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w);
            return 1;
        }

        int VecCross(lua_State *state)
        {
            const glm::vec3 a = ReadVector(state, 1);
            const glm::vec3 b = ReadVector(state, 2);
            PushVector(state, glm::cross(a, b));
            return 1;
        }

        int VecDistance(lua_State *state)
        {
            const int dim = std::max(DetectVecDim(state, 1), DetectVecDim(state, 2));
            const glm::vec4 a = ReadVec4(state, 1);
            const glm::vec4 b = ReadVec4(state, 2);
            if (dim <= 2)
                lua_pushnumber(state, glm::distance(glm::vec2(a), glm::vec2(b)));
            else
                lua_pushnumber(state, glm::distance(glm::vec3(a), glm::vec3(b)));
            return 1;
        }

        int VecLerp(lua_State *state)
        {
            const int dim = std::max(DetectVecDim(state, 1), DetectVecDim(state, 2));
            const glm::vec4 a = ReadVec4(state, 1);
            const glm::vec4 b = ReadVec4(state, 2);
            const float t = static_cast<float>(luaL_checknumber(state, 3));
            return PushVecResult(state, glm::mix(a, b, t), dim == 0 ? 3 : dim);
        }

        int VecIndex(lua_State *state)
        {
            const char *key = lua_tostring(state, 2);
            if (!key)
            {
                lua_pushnil(state);
                return 1;
            }
            if (strcmp(key, "length") == 0) { lua_pushcfunction(state, VecLength); return 1; }
            if (strcmp(key, "normalized") == 0) { lua_pushcfunction(state, VecNormalized); return 1; }
            if (strcmp(key, "dot") == 0) { lua_pushcfunction(state, VecDot); return 1; }
            if (strcmp(key, "cross") == 0) { lua_pushcfunction(state, VecCross); return 1; }
            if (strcmp(key, "distance") == 0) { lua_pushcfunction(state, VecDistance); return 1; }
            if (strcmp(key, "lerp") == 0) { lua_pushcfunction(state, VecLerp); return 1; }
            lua_pushnil(state);
            return 1;
        }

        int LuaVec2(lua_State *state)
        {
            if (lua_istable(state, 1))
            {
                PushVector(state, ReadVec2(state, 1));
                return 1;
            }
            const float x = static_cast<float>(luaL_optnumber(state, 1, 0.0));
            const float y = static_cast<float>(luaL_optnumber(state, 2, x));
            PushVector(state, glm::vec2(x, y));
            return 1;
        }

        int LuaVec3(lua_State *state)
        {
            if (lua_istable(state, 1))
            {
                PushVector(state, ReadVector(state, 1));
                return 1;
            }
            const float x = static_cast<float>(luaL_optnumber(state, 1, 0.0));
            const float y = static_cast<float>(luaL_optnumber(state, 2, lua_gettop(state) == 1 ? x : 0.0));
            const float z = static_cast<float>(luaL_optnumber(state, 3, lua_gettop(state) == 1 ? x : 0.0));
            PushVector(state, glm::vec3(x, y, z));
            return 1;
        }

        int LuaVec4(lua_State *state)
        {
            if (lua_istable(state, 1))
            {
                PushVector(state, ReadVec4(state, 1));
                return 1;
            }
            const float x = static_cast<float>(luaL_optnumber(state, 1, 0.0));
            const float y = static_cast<float>(luaL_optnumber(state, 2, lua_gettop(state) == 1 ? x : 0.0));
            const float z = static_cast<float>(luaL_optnumber(state, 3, lua_gettop(state) == 1 ? x : 0.0));
            const float w = static_cast<float>(luaL_optnumber(state, 4, 1.0));
            PushVector(state, glm::vec4(x, y, z, w));
            return 1;
        }

        int MathLerp(lua_State *state)
        {
            if (lua_istable(state, 1))
                return VecLerp(state);
            const float a = static_cast<float>(luaL_checknumber(state, 1));
            const float b = static_cast<float>(luaL_checknumber(state, 2));
            const float t = static_cast<float>(luaL_checknumber(state, 3));
            lua_pushnumber(state, glm::mix(a, b, t));
            return 1;
        }

        int MathClamp(lua_State *state)
        {
            const float v = static_cast<float>(luaL_checknumber(state, 1));
            const float lo = static_cast<float>(luaL_checknumber(state, 2));
            const float hi = static_cast<float>(luaL_checknumber(state, 3));
            lua_pushnumber(state, std::clamp(v, lo, hi));
            return 1;
        }

        int MathSmoothstep(lua_State *state)
        {
            const float e0 = static_cast<float>(luaL_checknumber(state, 1));
            const float e1 = static_cast<float>(luaL_checknumber(state, 2));
            const float x = static_cast<float>(luaL_checknumber(state, 3));
            lua_pushnumber(state, glm::smoothstep(e0, e1, x));
            return 1;
        }

        int MathDeg2Rad(lua_State *state)
        {
            lua_pushnumber(state, glm::radians(static_cast<float>(luaL_checknumber(state, 1))));
            return 1;
        }

        int MathRad2Deg(lua_State *state)
        {
            lua_pushnumber(state, glm::degrees(static_cast<float>(luaL_checknumber(state, 1))));
            return 1;
        }

        // Entity methods
        int EntityDestroy(lua_State *state)
        {
            auto *ref = static_cast<EntityRef *>(luaL_checkudata(state, 1, kEntityMetatable));
            Entity entity = ResolveEntity(ref->scene, ref->id);
            if (entity)
            {
                SceneManager::DestroyEntity(ref->scene, entity);
            }
            return 0;
        }

        int EntityIsValid(lua_State *state)
        {
            auto *ref = static_cast<EntityRef *>(luaL_checkudata(state, 1, kEntityMetatable));
            lua_pushboolean(state, static_cast<bool>(ResolveEntity(ref->scene, ref->id)));
            return 1;
        }

        int EntityGetName(lua_State *state)
        {
            auto *ref = static_cast<EntityRef *>(luaL_checkudata(state, 1, kEntityMetatable));
            Entity entity = ResolveEntity(ref->scene, ref->id);
            if (entity)
                lua_pushstring(state, entity.GetName().c_str());
            else
                lua_pushnil(state);
            return 1;
        }

        int EntityGetParent(lua_State *state)
        {
            auto *ref = static_cast<EntityRef *>(luaL_checkudata(state, 1, kEntityMetatable));
            Entity entity = ResolveEntity(ref->scene, ref->id);
            if (!entity || entity.GetParentUUID() == UUID(0))
            {
                lua_pushnil(state);
                return 1;
            }
            Entity parent = SceneManager::GetEntity(ref->scene, entity.GetParentUUID());
            if (!parent)
            {
                lua_pushnil(state);
                return 1;
            }
            PushEntity(state, ref->scene, static_cast<uint64_t>(parent.GetUUID()));
            return 1;
        }

        int EntitySetParent(lua_State *state)
        {
            auto *ref = static_cast<EntityRef *>(luaL_checkudata(state, 1, kEntityMetatable));
            Entity child = ResolveEntity(ref->scene, ref->id);
            if (!child)
                return 0;

            if (lua_isnil(state, 2))
            {
                auto &childId = child.GetComponent<IDComponent>();
                if (childId.parent != UUID(0))
                {
                    if (Entity oldParent = SceneManager::GetEntity(ref->scene, childId.parent))
                    {
                        oldParent.GetComponent<IDComponent>().RemoveChild(childId.uuid);
                    }
                    childId.parent = UUID(0);
                }
                return 0;
            }

            auto *parentRef = static_cast<EntityRef *>(luaL_checkudata(state, 2, kEntityMetatable));
            Entity parent = ResolveEntity(parentRef->scene, parentRef->id);
            if (parent && parentRef->scene == ref->scene)
            {
                SceneManager::AddChild(ref->scene, parent, child);
            }
            return 0;
        }

        int EntityGetChildren(lua_State *state)
        {
            auto *ref = static_cast<EntityRef *>(luaL_checkudata(state, 1, kEntityMetatable));
            Entity entity = ResolveEntity(ref->scene, ref->id);
            lua_newtable(state);
            if (!entity)
                return 1;

            int idx = 1;
            for (const UUID childUuid : entity.GetComponent<IDComponent>().children)
            {
                Entity child = SceneManager::GetEntity(ref->scene, childUuid);
                if (child)
                {
                    PushEntity(state, ref->scene, static_cast<uint64_t>(childUuid));
                    lua_rawseti(state, -2, idx++);
                }
            }
            return 1;
        }

        // Transform methods
        int TransformTranslate(lua_State *state)
        {
            auto *ref = static_cast<ComponentRef *>(luaL_checkudata(state, 1, kComponentMetatable));
            Entity entity = ResolveEntity(ref->scene, ref->id);
            if (entity && entity.HasComponent<TransformComponent>())
            {
                auto &tc = entity.GetComponent<TransformComponent>();
                if (lua_istable(state, 2))
                {
                    tc.local.translation += ReadVector(state, 2);
                }
                else
                {
                    const float x = static_cast<float>(luaL_checknumber(state, 2));
                    const float y = static_cast<float>(luaL_checknumber(state, 3));
                    const float z = static_cast<float>(luaL_optnumber(state, 4, 0.0));
                    tc.local.translation += glm::vec3(x, y, z);
                }
                tc.dirty = true;
            }
            return 0;
        }

        int TransformRotate(lua_State *state)
        {
            auto *ref = static_cast<ComponentRef *>(luaL_checkudata(state, 1, kComponentMetatable));
            Entity entity = ResolveEntity(ref->scene, ref->id);
            if (entity && entity.HasComponent<TransformComponent>())
            {
                auto &tc = entity.GetComponent<TransformComponent>();
                glm::vec3 eulerDeg(0.0f);
                if (lua_istable(state, 2))
                {
                    eulerDeg = ReadVector(state, 2);
                }
                else
                {
                    eulerDeg.x = static_cast<float>(luaL_checknumber(state, 2));
                    eulerDeg.y = static_cast<float>(luaL_checknumber(state, 3));
                    eulerDeg.z = static_cast<float>(luaL_optnumber(state, 4, 0.0));
                }
                const glm::quat delta = glm::quat(glm::radians(eulerDeg));
                tc.local.rotation = glm::normalize(delta * tc.local.rotation);
                tc.dirty = true;
            }
            return 0;
        }

        int TransformLookAt(lua_State *state)
        {
            auto *ref = static_cast<ComponentRef *>(luaL_checkudata(state, 1, kComponentMetatable));
            Entity entity = ResolveEntity(ref->scene, ref->id);
            if (entity && entity.HasComponent<TransformComponent>())
            {
                auto &tc = entity.GetComponent<TransformComponent>();
                const glm::vec3 target = ReadVector(state, 2);
                const glm::vec3 up = lua_istable(state, 3) ? ReadVector(state, 3) : glm::vec3(0.0f, 1.0f, 0.0f);
                const glm::vec3 dir = target - tc.local.translation;
                if (glm::length2(dir) > 1e-8f)
                {
                    tc.local.rotation = glm::quatLookAt(glm::normalize(dir), glm::normalize(up));
                    tc.dirty = true;
                }
            }
            return 0;
        }

        // Rigidbody3D methods
        int Rigidbody3DApplyForce(lua_State *state)
        {
            auto *ref = static_cast<ComponentRef *>(luaL_checkudata(state, 1, kComponentMetatable));
            const glm::vec3 force = lua_istable(state, 2)
                ? ReadVector(state, 2)
                : glm::vec3(luaL_checknumber(state, 2), luaL_checknumber(state, 3), luaL_optnumber(state, 4, 0.0));
            Entity entity = ResolveEntity(ref->scene, ref->id);
            if (entity && entity.HasComponent<RigidbodyComponent>())
            {
                if (auto actor = entity.GetComponent<RigidbodyComponent>().dynamicActor.lock())
                {
                    actor->ActivateBody();
                    actor->AddForce(force);
                }
            }
            return 0;
        }

        int Rigidbody3DApplyForceAt(lua_State *state)
        {
            auto *ref = static_cast<ComponentRef *>(luaL_checkudata(state, 1, kComponentMetatable));
            const glm::vec3 force = ReadVector(state, 2);
            const glm::vec3 point = ReadVector(state, 3);
            Entity entity = ResolveEntity(ref->scene, ref->id);
            if (entity && entity.HasComponent<RigidbodyComponent>())
            {
                if (auto actor = entity.GetComponent<RigidbodyComponent>().dynamicActor.lock())
                {
                    actor->ActivateBody();
                    actor->AddForceAtPosition(force, point);
                }
            }
            return 0;
        }

        int Rigidbody3DApplyImpulse(lua_State *state)
        {
            auto *ref = static_cast<ComponentRef *>(luaL_checkudata(state, 1, kComponentMetatable));
            const glm::vec3 impulse = lua_istable(state, 2)
                ? ReadVector(state, 2)
                : glm::vec3(luaL_checknumber(state, 2), luaL_checknumber(state, 3), luaL_optnumber(state, 4, 0.0));
            Entity entity = ResolveEntity(ref->scene, ref->id);
            if (entity && entity.HasComponent<RigidbodyComponent>())
            {
                if (auto actor = entity.GetComponent<RigidbodyComponent>().dynamicActor.lock())
                {
                    actor->ActivateBody();
                    actor->AddImpulse(impulse);
                }
            }
            return 0;
        }

        int Rigidbody3DApplyImpulseAt(lua_State *state)
        {
            auto *ref = static_cast<ComponentRef *>(luaL_checkudata(state, 1, kComponentMetatable));
            const glm::vec3 impulse = ReadVector(state, 2);
            const glm::vec3 point = ReadVector(state, 3);
            Entity entity = ResolveEntity(ref->scene, ref->id);
            if (entity && entity.HasComponent<RigidbodyComponent>())
            {
                if (auto actor = entity.GetComponent<RigidbodyComponent>().dynamicActor.lock())
                {
                    actor->ActivateBody();
                    actor->AddImpulseAtPosition(impulse, point);
                }
            }
            return 0;
        }

        int Rigidbody3DApplyTorque(lua_State *state)
        {
            auto *ref = static_cast<ComponentRef *>(luaL_checkudata(state, 1, kComponentMetatable));
            const glm::vec3 torque = lua_istable(state, 2)
                ? ReadVector(state, 2)
                : glm::vec3(luaL_checknumber(state, 2), luaL_checknumber(state, 3), luaL_optnumber(state, 4, 0.0));
            Entity entity = ResolveEntity(ref->scene, ref->id);
            if (entity && entity.HasComponent<RigidbodyComponent>())
            {
                if (auto actor = entity.GetComponent<RigidbodyComponent>().dynamicActor.lock())
                {
                    actor->ActivateBody();
                    actor->AddTorque(torque);
                }
            }
            return 0;
        }

        int Rigidbody3DActivate(lua_State *state)
        {
            auto *ref = static_cast<ComponentRef *>(luaL_checkudata(state, 1, kComponentMetatable));
            Entity entity = ResolveEntity(ref->scene, ref->id);
            if (entity && entity.HasComponent<RigidbodyComponent>())
            {
                if (auto actor = entity.GetComponent<RigidbodyComponent>().dynamicActor.lock())
                    actor->ActivateBody();
            }
            return 0;
        }

        int Rigidbody3DDeactivate(lua_State *state)
        {
            auto *ref = static_cast<ComponentRef *>(luaL_checkudata(state, 1, kComponentMetatable));
            Entity entity = ResolveEntity(ref->scene, ref->id);
            if (entity && entity.HasComponent<RigidbodyComponent>())
            {
                if (auto actor = entity.GetComponent<RigidbodyComponent>().dynamicActor.lock())
                    actor->DeactivateBody();
            }
            return 0;
        }

        // CharacterController methods
        int CharacterMove(lua_State *state)
        {
            auto *ref = static_cast<ComponentRef *>(luaL_checkudata(state, 1, kComponentMetatable));
            const glm::vec3 disp = ReadVector(state, 2);
            const float dt = static_cast<float>(luaL_optnumber(state, 3, 1.0 / 60.0));
            Entity entity = ResolveEntity(ref->scene, ref->id);
            if (entity && entity.HasComponent<CharacterControllerComponent>())
            {
                auto &cc = entity.GetComponent<CharacterControllerComponent>();
                if (auto actor = cc.character.lock())
                {
                    actor->Move(disp, dt);
                }
            }
            return 0;
        }

        int CharacterSetPosition(lua_State *state)
        {
            auto *ref = static_cast<ComponentRef *>(luaL_checkudata(state, 1, kComponentMetatable));
            const glm::vec3 pos = ReadVector(state, 2);
            Entity entity = ResolveEntity(ref->scene, ref->id);
            if (entity && entity.HasComponent<CharacterControllerComponent>())
            {
                auto &cc = entity.GetComponent<CharacterControllerComponent>();
                if (auto actor = cc.character.lock())
                {
                    actor->SetPosition(pos);
                }
                if (entity.HasComponent<TransformComponent>())
                {
                    entity.GetComponent<TransformComponent>().local.translation = pos;
                    entity.GetComponent<TransformComponent>().dirty = true;
                }
            }
            return 0;
        }

        int CharacterSetRotation(lua_State *state)
        {
            auto *ref = static_cast<ComponentRef *>(luaL_checkudata(state, 1, kComponentMetatable));
            const glm::vec3 eulerDeg = lua_istable(state, 2)
                ? ReadVector(state, 2)
                : glm::vec3(luaL_optnumber(state, 2, 0.0), luaL_optnumber(state, 3, 0.0), luaL_optnumber(state, 4, 0.0));
            const glm::quat q(glm::radians(eulerDeg));
            Entity entity = ResolveEntity(ref->scene, ref->id);
            if (entity && entity.HasComponent<CharacterControllerComponent>())
            {
                auto &cc = entity.GetComponent<CharacterControllerComponent>();
                if (auto actor = cc.character.lock())
                {
                    actor->SetRotation(q);
                }
                if (entity.HasComponent<TransformComponent>())
                {
                    entity.GetComponent<TransformComponent>().local.rotation = q;
                    entity.GetComponent<TransformComponent>().dirty = true;
                }
            }
            return 0;
        }

        // SkeletalMesh / Animator methods
        void EnsureSkeletalRuntimeParam(SkeletalMeshComponent &c, const char *name, AnimParam::Type type)
        {
            if (AnimParam *existing = Animator::FindAnimParam(c.runtimeParams, name))
            {
                existing->type = type;
                return;
            }
            AnimParam param;
            param.name = name;
            param.type = type;
            c.runtimeParams[name] = std::move(param);
        }

        int SkeletalSetFloat(lua_State *state)
        {
            auto *ref = static_cast<ComponentRef *>(luaL_checkudata(state, 1, kComponentMetatable));
            const char *name = luaL_checkstring(state, 2);
            const float val = static_cast<float>(luaL_checknumber(state, 3));
            Entity entity = ResolveEntity(ref->scene, ref->id);
            if (entity && entity.HasComponent<SkeletalMeshComponent>())
            {
                auto &c = entity.GetComponent<SkeletalMeshComponent>();
                EnsureSkeletalRuntimeParam(c, name, AnimParam::Type::Float);
                c.runtimeParams[name].floatVal = val;
            }
            return 0;
        }

        int SkeletalSetBool(lua_State *state)
        {
            auto *ref = static_cast<ComponentRef *>(luaL_checkudata(state, 1, kComponentMetatable));
            const char *name = luaL_checkstring(state, 2);
            const bool val = lua_toboolean(state, 3) != 0;
            Entity entity = ResolveEntity(ref->scene, ref->id);
            if (entity && entity.HasComponent<SkeletalMeshComponent>())
            {
                auto &c = entity.GetComponent<SkeletalMeshComponent>();
                EnsureSkeletalRuntimeParam(c, name, AnimParam::Type::Bool);
                c.runtimeParams[name].boolVal = val;
            }
            return 0;
        }

        int SkeletalSetInt(lua_State *state)
        {
            auto *ref = static_cast<ComponentRef *>(luaL_checkudata(state, 1, kComponentMetatable));
            const char *name = luaL_checkstring(state, 2);
            const int val = static_cast<int>(luaL_checkinteger(state, 3));
            Entity entity = ResolveEntity(ref->scene, ref->id);
            if (entity && entity.HasComponent<SkeletalMeshComponent>())
            {
                auto &c = entity.GetComponent<SkeletalMeshComponent>();
                EnsureSkeletalRuntimeParam(c, name, AnimParam::Type::Int);
                c.runtimeParams[name].intVal = val;
            }
            return 0;
        }

        int SkeletalSetString(lua_State *state)
        {
            auto *ref = static_cast<ComponentRef *>(luaL_checkudata(state, 1, kComponentMetatable));
            const char *name = luaL_checkstring(state, 2);
            const char *val = luaL_checkstring(state, 3);
            Entity entity = ResolveEntity(ref->scene, ref->id);
            if (entity && entity.HasComponent<SkeletalMeshComponent>())
            {
                auto &c = entity.GetComponent<SkeletalMeshComponent>();
                EnsureSkeletalRuntimeParam(c, name, AnimParam::Type::String);
                c.runtimeParams[name].strVal = val;
            }
            return 0;
        }

        int SkeletalGetFloat(lua_State *state)
        {
            auto *ref = static_cast<ComponentRef *>(luaL_checkudata(state, 1, kComponentMetatable));
            const char *name = luaL_checkstring(state, 2);
            Entity entity = ResolveEntity(ref->scene, ref->id);
            float out = 0.0f;
            if (entity && entity.HasComponent<SkeletalMeshComponent>())
            {
                if (const AnimParam *p = Animator::FindAnimParam(entity.GetComponent<SkeletalMeshComponent>().runtimeParams, name))
                    out = p->floatVal;
            }
            lua_pushnumber(state, out);
            return 1;
        }

        int SkeletalGetBool(lua_State *state)
        {
            auto *ref = static_cast<ComponentRef *>(luaL_checkudata(state, 1, kComponentMetatable));
            const char *name = luaL_checkstring(state, 2);
            Entity entity = ResolveEntity(ref->scene, ref->id);
            bool out = false;
            if (entity && entity.HasComponent<SkeletalMeshComponent>())
            {
                if (const AnimParam *p = Animator::FindAnimParam(entity.GetComponent<SkeletalMeshComponent>().runtimeParams, name))
                    out = p->boolVal;
            }
            lua_pushboolean(state, out);
            return 1;
        }

        int SkeletalGetInt(lua_State *state)
        {
            auto *ref = static_cast<ComponentRef *>(luaL_checkudata(state, 1, kComponentMetatable));
            const char *name = luaL_checkstring(state, 2);
            Entity entity = ResolveEntity(ref->scene, ref->id);
            int out = 0;
            if (entity && entity.HasComponent<SkeletalMeshComponent>())
            {
                if (const AnimParam *p = Animator::FindAnimParam(entity.GetComponent<SkeletalMeshComponent>().runtimeParams, name))
                    out = p->intVal;
            }
            lua_pushinteger(state, out);
            return 1;
        }

        int SkeletalPlayState(lua_State *state)
        {
            auto *ref = static_cast<ComponentRef *>(luaL_checkudata(state, 1, kComponentMetatable));
            const char *stateName = luaL_checkstring(state, 2);
            Entity entity = ResolveEntity(ref->scene, ref->id);
            if (entity && entity.HasComponent<SkeletalMeshComponent>())
            {
                auto &c = entity.GetComponent<SkeletalMeshComponent>();
                c.currentStateName = stateName;
                c.stateElapsed = 0.0f;
                c.stateNormalized = 0.0f;
                c.isTransitioning = false;
                c.transitionTargetState.clear();
                c.transitionElapsed = 0.0f;
            }
            return 0;
        }

        // AudioSource helpers & methods
        Ref<FmodSound> GetAudioSound(Scene *scene, const AudioSourceComponent &c)
        {
            if (!scene || !scene->GetAssetManager() || c.handle == AssetHandle(0))
                return nullptr;
            return scene->GetAssetManager()->GetAsset<FmodSound>(c.handle);
        }

        int AudioSourcePlay(lua_State *state)
        {
            auto *ref = static_cast<ComponentRef *>(luaL_checkudata(state, 1, kComponentMetatable));
            Entity entity = ResolveEntity(ref->scene, ref->id);
            if (entity && entity.HasComponent<AudioSourceComponent>())
            {
                const auto &c = entity.GetComponent<AudioSourceComponent>();
                if (auto sound = GetAudioSound(ref->scene, c))
                {
                    FMOD_MODE mode = sound->GetMode();
                    mode = c.loop ? ((mode & ~FMOD_LOOP_OFF) | FMOD_LOOP_NORMAL) : ((mode & ~FMOD_LOOP_NORMAL) | FMOD_LOOP_OFF);
                    sound->SetMode(mode);
                    sound->Play();
                    sound->SetVolume(c.volume);
                    sound->SetPitch(c.pitch);
                    sound->SetPan(c.pan);
                }
            }
            return 0;
        }

        int AudioSourceStop(lua_State *state)
        {
            auto *ref = static_cast<ComponentRef *>(luaL_checkudata(state, 1, kComponentMetatable));
            Entity entity = ResolveEntity(ref->scene, ref->id);
            if (entity && entity.HasComponent<AudioSourceComponent>())
            {
                if (auto sound = GetAudioSound(ref->scene, entity.GetComponent<AudioSourceComponent>()))
                    sound->Stop();
            }
            return 0;
        }

        int AudioSourcePause(lua_State *state)
        {
            auto *ref = static_cast<ComponentRef *>(luaL_checkudata(state, 1, kComponentMetatable));
            Entity entity = ResolveEntity(ref->scene, ref->id);
            if (entity && entity.HasComponent<AudioSourceComponent>())
            {
                if (auto sound = GetAudioSound(ref->scene, entity.GetComponent<AudioSourceComponent>()))
                    sound->Pause();
            }
            return 0;
        }

        int AudioSourceResume(lua_State *state)
        {
            auto *ref = static_cast<ComponentRef *>(luaL_checkudata(state, 1, kComponentMetatable));
            Entity entity = ResolveEntity(ref->scene, ref->id);
            if (entity && entity.HasComponent<AudioSourceComponent>())
            {
                if (auto sound = GetAudioSound(ref->scene, entity.GetComponent<AudioSourceComponent>()))
                    sound->Resume();
            }
            return 0;
        }

        int MeshSetPrimitive(lua_State *state)
        {
            auto *ref = static_cast<ComponentRef *>(luaL_checkudata(state, 1, kComponentMetatable));
            const char *typeStr = luaL_checkstring(state, 2);
            Entity entity = ResolveEntity(ref->scene, ref->id);
            if (entity && entity.HasComponent<StaticMeshComponent>())
            {
                EMeshType meshType = EMeshType::CUBE;
                if (Renderer::ParseMeshType(typeStr, meshType))
                {
                    entity.GetComponent<StaticMeshComponent>().handle = Renderer::GetDefaultMeshHandle(meshType);
                }
            }
            return 0;
        }

        int NetMarkDirty(lua_State *state)
        {
            auto *ref = static_cast<ComponentRef *>(luaL_checkudata(state, 1, kComponentMetatable));
            const uint32_t mask = static_cast<uint32_t>(luaL_optinteger(state, 2, NetComp_All));
            Entity entity = ResolveEntity(ref->scene, ref->id);
            if (entity && entity.HasComponent<NetworkIdentityComponent>())
            {
                entity.GetComponent<NetworkIdentityComponent>().MarkDirty(mask);
            }
            return 0;
        }

        int ParticlePlay(lua_State *state)
        {
            auto *ref = static_cast<ComponentRef *>(luaL_checkudata(state, 1, kComponentMetatable));
            Entity entity = ResolveEntity(ref->scene, ref->id);
            if (entity && entity.HasComponent<ParticleSystemComponent>())
            {
                entity.GetComponent<ParticleSystemComponent>().isPlaying = true;
            }
            return 0;
        }

        int ParticleStop(lua_State *state)
        {
            auto *ref = static_cast<ComponentRef *>(luaL_checkudata(state, 1, kComponentMetatable));
            Entity entity = ResolveEntity(ref->scene, ref->id);
            if (entity && entity.HasComponent<ParticleSystemComponent>())
            {
                entity.GetComponent<ParticleSystemComponent>().isPlaying = false;
            }
            return 0;
        }

        int ParticleRestart(lua_State *state)
        {
            auto *ref = static_cast<ComponentRef *>(luaL_checkudata(state, 1, kComponentMetatable));
            Entity entity = ResolveEntity(ref->scene, ref->id);
            if (entity && entity.HasComponent<ParticleSystemComponent>())
            {
                auto &pc = entity.GetComponent<ParticleSystemComponent>();
                pc.isPlaying = true;
                pc.system = nullptr; // forces recreation & buffer reset on next frame
            }
            return 0;
        }

        int ParticleSetPreset(lua_State *state)
        {
            auto *ref = static_cast<ComponentRef *>(luaL_checkudata(state, 1, kComponentMetatable));
            const char *preset = luaL_checkstring(state, 2);
            Entity entity = ResolveEntity(ref->scene, ref->id);
            if (entity && entity.HasComponent<ParticleSystemComponent>())
            {
                auto &pc = entity.GetComponent<ParticleSystemComponent>();
                if (strcmp(preset, "Fire") == 0) pc.settings = ParticleEmitterSettings::CreateFire();
                else if (strcmp(preset, "Smoke") == 0) pc.settings = ParticleEmitterSettings::CreateSmoke();
                else if (strcmp(preset, "Spark") == 0) pc.settings = ParticleEmitterSettings::CreateSpark();
                pc.system = nullptr;
            }
            return 0;
        }

        // Top-level Scene / Entity / Physics / Network bindings
        int IgniteCreateEntity(lua_State *state)
        {
            Scene *scene = GetSceneUpvalue(state);
            if (!scene)
            {
                lua_pushnil(state);
                return 1;
            }
            const char *name = luaL_optstring(state, 1, "Entity");
            Entity entity = SceneManager::CreateEmptyEntity(scene, name);
            if (!entity)
            {
                lua_pushnil(state);
                return 1;
            }
            PushEntity(state, scene, static_cast<uint64_t>(entity.GetUUID()));
            return 1;
        }

        int IgniteCreatePrimitive(lua_State *state)
        {
            Scene *scene = GetSceneUpvalue(state);
            if (!scene)
            {
                lua_pushnil(state);
                return 1;
            }
            EMeshType meshType = EMeshType::CUBE;
            if (lua_isinteger(state, 1))
                meshType = static_cast<EMeshType>(lua_tointeger(state, 1));
            else
                Renderer::ParseMeshType(luaL_optstring(state, 1, "cube"), meshType);

            const char *name = luaL_optstring(state, 2, "");
            Entity entity = SceneManager::CreatePrimitive(scene, meshType, name);
            if (!entity)
            {
                lua_pushnil(state);
                return 1;
            }
            PushEntity(state, scene, static_cast<uint64_t>(entity.GetUUID()));
            return 1;
        }

        int IgniteDestroyEntity(lua_State *state)
        {
            Scene *scene = GetSceneUpvalue(state);
            if (!scene)
                return 0;
            if (lua_isuserdata(state, 1))
            {
                auto *ref = static_cast<EntityRef *>(luaL_checkudata(state, 1, kEntityMetatable));
                if (Entity entity = ResolveEntity(scene, ref->id))
                    SceneManager::DestroyEntity(scene, entity);
            }
            else if (lua_isinteger(state, 1))
            {
                const UUID uuid(static_cast<uint64_t>(lua_tointeger(state, 1)));
                SceneManager::DestroyEntity(scene, uuid);
            }
            return 0;
        }

        int IgniteDuplicateEntity(lua_State *state)
        {
            Scene *scene = GetSceneUpvalue(state);
            if (!scene)
            {
                lua_pushnil(state);
                return 1;
            }
            Entity src;
            if (lua_isuserdata(state, 1))
            {
                auto *ref = static_cast<EntityRef *>(luaL_checkudata(state, 1, kEntityMetatable));
                src = ResolveEntity(scene, ref->id);
            }
            else if (lua_isinteger(state, 1))
            {
                src = ResolveEntity(scene, static_cast<uint64_t>(lua_tointeger(state, 1)));
            }
            if (!src)
            {
                lua_pushnil(state);
                return 1;
            }
            Entity dup = SceneManager::DuplicateEntity(scene, src);
            if (!dup)
            {
                lua_pushnil(state);
                return 1;
            }
            PushEntity(state, scene, static_cast<uint64_t>(dup.GetUUID()));
            return 1;
        }

        int IgniteGetEntitiesWith(lua_State *state)
        {
            Scene *scene = GetSceneUpvalue(state);
            const char *compName = luaL_checkstring(state, 1);
            lua_newtable(state);
            if (!scene || !scene->registry)
                return 1;

            int idx = 1;
            ComponentType unused{};
            for (const auto &[uuid, enttId] : scene->entities)
            {
                if (!scene->registry->valid(enttId))
                    continue;
                Entity e(enttId, scene);
                if (HasComponent(e, compName, unused))
                {
                    PushEntity(state, scene, static_cast<uint64_t>(uuid));
                    lua_rawseti(state, -2, idx++);
                }
            }
            return 1;
        }

        int IgniteInstantiatePrefab(lua_State *state)
        {
            Scene *scene = GetSceneUpvalue(state);
            if (!scene || !scene->GetAssetManager())
            {
                lua_pushnil(state);
                return 1;
            }
            const AssetHandle handle(static_cast<uint64_t>(luaL_checkinteger(state, 1)));
            Ref<Prefab> prefab = scene->GetAssetManager()->GetAsset<Prefab>(handle);
            if (!prefab)
            {
                lua_pushnil(state);
                return 1;
            }
            Entity instance = SceneManager::InstantiatePrefab(scene, prefab);
            if (!instance)
            {
                lua_pushnil(state);
                return 1;
            }
            PushEntity(state, scene, static_cast<uint64_t>(instance.GetUUID()));
            return 1;
        }

        int LuaSceneLoad(lua_State *state)
        {
            Scene *scene = GetSceneUpvalue(state);
            if (lua_isinteger(state, 1))
            {
                SceneManager::Transition(AssetHandle(static_cast<uint64_t>(lua_tointeger(state, 1))));
                return 0;
            }
            if (lua_isstring(state, 1) && scene && scene->GetAssetManager())
            {
                const std::string path = lua_tostring(state, 1);
                const AssetHandle handle = scene->GetAssetManager()->GetAssetHandle(path);
                if (handle != AssetHandle(0))
                {
                    SceneManager::Transition(handle);
                }
            }
            return 0;
        }

        int LuaSceneGetTime(lua_State *state)
        {
            Scene *scene = GetSceneUpvalue(state);
            lua_pushnumber(state, scene ? scene->timeInSeconds : 0.0f);
            return 1;
        }

        int LuaSceneIsPlaying(lua_State *state)
        {
            Scene *scene = GetSceneUpvalue(state);
            lua_pushboolean(state, scene && scene->IsPlaying());
            return 1;
        }

        int LuaSceneIsSimulating(lua_State *state)
        {
            Scene *scene = GetSceneUpvalue(state);
            lua_pushboolean(state, scene && scene->IsSimulating());
            return 1;
        }

        int LuaSceneIsPaused(lua_State *state)
        {
            Scene *scene = GetSceneUpvalue(state);
            lua_pushboolean(state, scene && scene->IsPaused());
            return 1;
        }

        int LuaSceneGetPrimaryCamera(lua_State *state)
        {
            Scene *scene = GetSceneUpvalue(state);
            if (!scene)
            {
                lua_pushnil(state);
                return 1;
            }
            Entity cam = scene->GetPrimaryCamera();
            if (!cam)
            {
                lua_pushnil(state);
                return 1;
            }
            PushEntity(state, scene, static_cast<uint64_t>(cam.GetUUID()));
            return 1;
        }

        int LuaDebugDrawTextWorld(lua_State *state)
        {
            Scene *scene = GetSceneUpvalue(state);
            if (!scene) return 0;

            DebugTextWorldItem item;
            item.position = ReadVector(state, 1);
            item.text = luaL_checkstring(state, 2);
            if (lua_gettop(state) >= 3 && lua_istable(state, 3))
                item.color = ReadVec4(state, 3);
            if (lua_gettop(state) >= 4 && lua_isnumber(state, 4))
                item.scale = static_cast<float>(lua_tonumber(state, 4));
            if (lua_gettop(state) >= 5 && !lua_isnil(state, 5))
                item.billboard = lua_toboolean(state, 5) != 0;
            if (lua_gettop(state) >= 6 && !lua_isnil(state, 6))
                item.center = lua_toboolean(state, 6) != 0;

            scene->AddDebugTextWorld(item);
            return 0;
        }

        int LuaDebugDrawTextScreen(lua_State *state)
        {
            Scene *scene = GetSceneUpvalue(state);
            if (!scene) return 0;

            DebugTextScreenItem item;
            int argOffset = 0;
            if (lua_istable(state, 1))
            {
                item.position = ReadVec2(state, 1);
                argOffset = 0;
            }
            else
            {
                item.position.x = static_cast<float>(luaL_checknumber(state, 1));
                item.position.y = static_cast<float>(luaL_checknumber(state, 2));
                argOffset = 1;
            }

            item.text = luaL_checkstring(state, 2 + argOffset);
            if (lua_gettop(state) >= 3 + argOffset && lua_istable(state, 3 + argOffset))
                item.color = ReadVec4(state, 3 + argOffset);
            if (lua_gettop(state) >= 4 + argOffset && lua_isnumber(state, 4 + argOffset))
                item.fontSize = static_cast<float>(lua_tonumber(state, 4 + argOffset));
            if (lua_gettop(state) >= 5 + argOffset && !lua_isnil(state, 5 + argOffset))
                item.center = lua_toboolean(state, 5 + argOffset) != 0;

            scene->AddDebugTextScreen(item);
            return 0;
        }

        int LuaDebugDrawLine(lua_State *state)
        {
            Scene *scene = GetSceneUpvalue(state);
            if (!scene) return 0;

            DebugLineItem item;
            item.p0 = ReadVector(state, 1);
            item.p1 = ReadVector(state, 2);
            if (lua_gettop(state) >= 3 && lua_istable(state, 3))
                item.color = ReadVec4(state, 3);

            scene->AddDebugLine(item);
            return 0;
        }

        int LuaDebugSetShowEntityNames(lua_State *state)
        {
            Scene *scene = GetSceneUpvalue(state);
            if (scene)
                scene->showDebugEntityNames = lua_toboolean(state, 1) != 0;
            return 0;
        }

        int LuaDebugGetShowEntityNames(lua_State *state)
        {
            Scene *scene = GetSceneUpvalue(state);
            lua_pushboolean(state, scene && scene->showDebugEntityNames);
            return 1;
        }

        int LuaPhysicsRaycast(lua_State *state)
        {
            Scene *scene = GetSceneUpvalue(state);
            if (!scene || !scene->GetPhysics3D())
            {
                lua_pushnil(state);
                return 1;
            }

            Ray ray;
            ray.origin = ReadVector(state, 1);
            glm::vec3 dir = ReadVector(state, 2);
            const float len = glm::length(dir);
            if (len < 1e-6f)
            {
                lua_pushnil(state);
                return 1;
            }
            ray.direction = dir / len;

            const float maxDistance = static_cast<float>(luaL_optnumber(state, 3, 1000.0));
            const uint32_t layerMask = static_cast<uint32_t>(luaL_optinteger(state, 4, 0xFFFFFFFF));

            RaycastHit hit;
            if (!scene->GetPhysics3D()->Raycast(ray, hit, maxDistance, layerMask))
            {
                lua_pushnil(state);
                return 1;
            }

            lua_newtable(state);
            lua_pushboolean(state, true);
            lua_setfield(state, -2, "hit");
            PushVector(state, hit.hitPoint);
            lua_setfield(state, -2, "point");
            PushVector(state, hit.hitNormal);
            lua_setfield(state, -2, "normal");
            lua_pushnumber(state, hit.fraction);
            lua_setfield(state, -2, "fraction");
            lua_pushnumber(state, hit.fraction * maxDistance);
            lua_setfield(state, -2, "distance");

            if (hit.userData != 0 && ResolveEntity(scene, hit.userData))
            {
                PushEntity(state, scene, hit.userData);
            }
            else
            {
                lua_pushnil(state);
            }
            lua_setfield(state, -2, "entity");
            return 1;
        }

        int LuaNetIsConnected(lua_State *state)
        {
            NetworkManager *nm = NetworkManager::GetInstance();
            lua_pushboolean(state, nm && nm->IsConnected());
            return 1;
        }

        int LuaNetIsServerRunning(lua_State *state)
        {
            NetworkManager *nm = NetworkManager::GetInstance();
            lua_pushboolean(state, nm && nm->IsServerRunning());
            return 1;
        }

        int LuaNetIsConnecting(lua_State *state)
        {
            NetworkManager *nm = NetworkManager::GetInstance();
            lua_pushboolean(state, nm && nm->IsConnecting());
            return 1;
        }

        int LuaNetGetClientId(lua_State *state)
        {
            NetworkManager *nm = NetworkManager::GetInstance();
            lua_pushinteger(state, nm ? static_cast<lua_Integer>(nm->GetClientId()) : 0);
            return 1;
        }

        int LuaNetStartServer(lua_State *state)
        {
            NetworkManager *nm = NetworkManager::GetInstance();
            if (!nm)
            {
                lua_pushboolean(state, false);
                return 1;
            }
            const bool ok = lua_gettop(state) >= 1
                ? nm->StartServer(static_cast<uint16_t>(luaL_checkinteger(state, 1)),
                    static_cast<uint16_t>(luaL_optinteger(state, 2, 60)),
                    static_cast<uint16_t>(luaL_optinteger(state, 3, 20)))
                : nm->StartServer();
            lua_pushboolean(state, ok);
            return 1;
        }

        int LuaNetStopServer(lua_State *state)
        {
            if (NetworkManager *nm = NetworkManager::GetInstance())
                nm->StopServer();
            return 0;
        }

        int LuaNetConnect(lua_State *state)
        {
            NetworkManager *nm = NetworkManager::GetInstance();
            if (!nm)
            {
                lua_pushboolean(state, false);
                return 1;
            }
            const char *addr = luaL_checkstring(state, 1);
            const uint16_t port = static_cast<uint16_t>(luaL_optinteger(state, 2, nm->GetPort()));
            const char *name = luaL_optstring(state, 3, "Player");
            lua_pushboolean(state, nm->Connect(addr, port, name));
            return 1;
        }

        int LuaNetDisconnect(lua_State *state)
        {
            if (NetworkManager *nm = NetworkManager::GetInstance())
                nm->Disconnect();
            return 0;
        }

        int LuaNetSpawnEntity(lua_State *state)
        {
            NetworkManager *nm = NetworkManager::GetInstance();
            if (!nm || !nm->IsServerRunning() || !nm->GetServerReplication())
            {
                lua_pushnil(state);
                return 1;
            }

            // Retrieve Lua Ignite.SpawnEntity parameters
            const char *templateOrPrefab = luaL_checkstring(state, 1);
            const glm::vec3 position   = ReadVector(state, 2);
            const auto yawDegrees     = static_cast<float>(luaL_optnumber(state, 3, 0.0));
            const auto ownerClient = static_cast<uint32_t>(luaL_checkinteger(state, 5));

            const glm::quat rotation = glm::angleAxis(glm::radians(yawDegrees), glm::vec3(0.0f, 1.0f, 0.0f));

            Entity entity = nm->GetServerReplication()->SpawnEntity(templateOrPrefab, position, rotation, ownerClient);
            if (!entity.IsValid())
            {
                lua_pushnil(state);
                return 1;
            }

            PushEntity(state, nm->GetActiveScene(), static_cast<uint64_t>(entity.GetUUID()));
            return 1;
        }

        int LuaNetSendInput(lua_State *state)
        {
            NetworkManager *nm = NetworkManager::GetInstance();
            if (!nm)
                return 0;
            if (lua_istable(state, 1))
            {
                const glm::vec2 move = ReadVec2(state, 1);
                const uint16_t buttons = static_cast<uint16_t>(luaL_optinteger(state, 2, 0));
                const float yaw = static_cast<float>(luaL_optnumber(state, 3, 0.0));
                nm->SendInput(move, buttons, yaw);
            }
            else
            {
                const float mx = static_cast<float>(luaL_optnumber(state, 1, 0.0));
                const float my = static_cast<float>(luaL_optnumber(state, 2, 0.0));
                const uint16_t buttons = static_cast<uint16_t>(luaL_optinteger(state, 3, 0));
                const float yaw = static_cast<float>(luaL_optnumber(state, 4, 0.0));
                nm->SendInput(glm::vec2(mx, my), buttons, yaw);
            }
            return 0;
        }

        int LuaNetSendChat(lua_State *state)
        {
            if (NetworkManager *nm = NetworkManager::GetInstance())
                nm->SendChat(luaL_checkstring(state, 1));
            return 0;
        }

        int LuaNetGetStats(lua_State *state)
        {
            NetworkManager *nm = NetworkManager::GetInstance();
            lua_newtable(state);
            if (!nm) return 1;
            NetStats s = nm->GetStats();
            lua_pushinteger(state, s.rttMs); lua_setfield(state, -2, "rtt");
            lua_pushnumber(state, s.packetLossPercent); lua_setfield(state, -2, "packet_loss");
            lua_pushnumber(state, s.inKBytesPerSec); lua_setfield(state, -2, "in_kbps");
            lua_pushnumber(state, s.outKBytesPerSec); lua_setfield(state, -2, "out_kbps");
            lua_pushinteger(state, s.clientCount); lua_setfield(state, -2, "client_count");
            return 1;
        }

        int LuaNetGetServerTick(lua_State *state)
        {
            NetworkManager *nm = NetworkManager::GetInstance();
            lua_pushinteger(state, nm ? static_cast<lua_Integer>(nm->GetServerTick()) : 0);
            return 1;
        }

        int LuaNetGetClientServerTick(lua_State *state)
        {
            NetworkManager *nm = NetworkManager::GetInstance();
            lua_pushinteger(state, nm ? static_cast<lua_Integer>(nm->GetClientServerTick()) : 0);
            return 1;
        }

        int LuaNetGetClientCount(lua_State *state)
        {
            NetworkManager *nm = NetworkManager::GetInstance();
            lua_pushinteger(state, nm ? static_cast<lua_Integer>(nm->GetClientCount()) : 0);
            return 1;
        }

        int LuaInputGetMouseDelta(lua_State *state)
        {
            PushVector(state, InputSystem::GetMouseDelta());
            return 1;
        }

        int LuaInputSetMousePosition(lua_State *state)
        {
            const int x = static_cast<int>(luaL_checkinteger(state, 1));
            const int y = static_cast<int>(luaL_checkinteger(state, 2));
            if (auto *sys = InputSystem::GetActiveSystem())
                sys->SetMousePosition(x, y);
            return 0;
        }
    }

    const char *LuaErrorMessage(lua_State *state)
    {
        const char *message = lua_tostring(state, -1);
        return message ? message : "(non-string Lua error)";
    }

    Entity ResolveEntity(Scene *scene, uint64_t id)
    {
        if (!scene || !scene->registry)
            return {};
        const auto it = scene->entities.find(UUID(id));
        if (it == scene->entities.end() || !scene->registry->valid(it->second))
            return {};
        return Entity(it->second, scene);
    }

    void PushVector(lua_State *state, const glm::vec2 &v)
    {
        lua_createtable(state, 2, 2);
        lua_pushnumber(state, v.x); lua_setfield(state, -2, "x");
        lua_pushnumber(state, v.y); lua_setfield(state, -2, "y");
        lua_pushnumber(state, v.x); lua_rawseti(state, -2, 1);
        lua_pushnumber(state, v.y); lua_rawseti(state, -2, 2);
        luaL_getmetatable(state, kVecMetatable);
        lua_setmetatable(state, -2);
    }

    void PushVector(lua_State *state, const glm::vec3 &v)
    {
        lua_createtable(state, 3, 3);
        lua_pushnumber(state, v.x); lua_setfield(state, -2, "x");
        lua_pushnumber(state, v.y); lua_setfield(state, -2, "y");
        lua_pushnumber(state, v.z); lua_setfield(state, -2, "z");
        lua_pushnumber(state, v.x); lua_rawseti(state, -2, 1);
        lua_pushnumber(state, v.y); lua_rawseti(state, -2, 2);
        lua_pushnumber(state, v.z); lua_rawseti(state, -2, 3);
        luaL_getmetatable(state, kVecMetatable);
        lua_setmetatable(state, -2);
    }

    void PushVector(lua_State *state, const glm::vec4 &v)
    {
        lua_createtable(state, 4, 4);
        lua_pushnumber(state, v.x); lua_setfield(state, -2, "x");
        lua_pushnumber(state, v.y); lua_setfield(state, -2, "y");
        lua_pushnumber(state, v.z); lua_setfield(state, -2, "z");
        lua_pushnumber(state, v.w); lua_setfield(state, -2, "w");
        lua_pushnumber(state, v.x); lua_rawseti(state, -2, 1);
        lua_pushnumber(state, v.y); lua_rawseti(state, -2, 2);
        lua_pushnumber(state, v.z); lua_rawseti(state, -2, 3);
        lua_pushnumber(state, v.w); lua_rawseti(state, -2, 4);
        luaL_getmetatable(state, kVecMetatable);
        lua_setmetatable(state, -2);
    }

    glm::vec4 ReadVec4(lua_State *state, int index)
    {
        glm::vec4 v(0.0f);
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
                if (lua_isnumber(state, -1)) v.y = static_cast<float>(lua_tonumber(state, -1));
                lua_pop(state, 1);

                lua_getfield(state, index, "z");
                if (lua_isnumber(state, -1)) v.z = static_cast<float>(lua_tonumber(state, -1));
                lua_pop(state, 1);

                lua_getfield(state, index, "w");
                if (lua_isnumber(state, -1)) v.w = static_cast<float>(lua_tonumber(state, -1));
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

                lua_rawgeti(state, index, 4);
                v.w = static_cast<float>(luaL_optnumber(state, -1, 0));
                lua_pop(state, 1);
            }
        }
        return v;
    }

    glm::vec2 ReadVec2(lua_State *state, int index)
    {
        const glm::vec4 v = ReadVec4(state, index);
        return { v.x, v.y };
    }

    glm::vec3 ReadVector(lua_State *state, int index)
    {
        const glm::vec4 v = ReadVec4(state, index);
        return { v.x, v.y, v.z };
    }

    bool ParseComponentType(const std::string &name, ComponentType &type)
    {
        if (name == "Rendering") { type = ComponentType::Rendering; return true; }
        if (name == "Transform") { type = ComponentType::Transform; return true; }
        if (name == "Rigidbody2D") { type = ComponentType::Rigidbody2D; return true; }
        if (name == "Rigidbody" || name == "Rigidbody3D") { type = ComponentType::Rigidbody3D; return true; }
        if (name == "Sprite2D") { type = ComponentType::Sprite2D; return true; }
        if (name == "Circle2D") { type = ComponentType::Circle2D; return true; }
        if (name == "StaticMesh") { type = ComponentType::StaticMesh; return true; }
        if (name == "SkeletalMesh") { type = ComponentType::SkeletalMesh; return true; }
        if (name == "AudioSource") { type = ComponentType::AudioSource; return true; }
        if (name == "Camera") { type = ComponentType::Camera; return true; }
        if (name == "DirectionalLight") { type = ComponentType::DirectionalLight; return true; }
        if (name == "PointLight") { type = ComponentType::PointLight; return true; }
        if (name == "SpotLight") { type = ComponentType::SpotLight; return true; }
        if (name == "CharacterController") { type = ComponentType::CharacterController; return true; }
        if (name == "BoxCollider") { type = ComponentType::BoxCollider; return true; }
        if (name == "SphereCollider") { type = ComponentType::SphereCollider; return true; }
        if (name == "CapsuleCollider") { type = ComponentType::CapsuleCollider; return true; }
        if (name == "BoxCollider2D") { type = ComponentType::BoxCollider2D; return true; }
        if (name == "CircleCollider2D") { type = ComponentType::CircleCollider2D; return true; }
        if (name == "Text") { type = ComponentType::Text; return true; }
        if (name == "Widget") { type = ComponentType::Widget; return true; }
        if (name == "NetworkIdentity") { type = ComponentType::NetworkIdentity; return true; }
        if (name == "PhysicsJoint" || name == "Joint") { type = ComponentType::PhysicsJoint; return true; }
        if (name == "ParticleSystem" || name == "Particle") { type = ComponentType::ParticleSystem; return true; }
        return false;
    }

    bool HasComponent(Entity entity, const std::string &name, ComponentType &type)
    {
        if (!entity || !ParseComponentType(name, type))
            return false;

        switch (type)
        {
        case ComponentType::Rendering:           return entity.HasComponent<RenderingComponent>();
        case ComponentType::Transform:           return entity.HasComponent<TransformComponent>();
        case ComponentType::Rigidbody2D:         return entity.HasComponent<Rigidbody2DComponent>();
        case ComponentType::Rigidbody3D:         return entity.HasComponent<RigidbodyComponent>();
        case ComponentType::Sprite2D:            return entity.HasComponent<Sprite2DComponent>();
        case ComponentType::Circle2D:            return entity.HasComponent<Circle2DComponent>();
        case ComponentType::StaticMesh:          return entity.HasComponent<StaticMeshComponent>();
        case ComponentType::SkeletalMesh:        return entity.HasComponent<SkeletalMeshComponent>();
        case ComponentType::AudioSource:         return entity.HasComponent<AudioSourceComponent>();
        case ComponentType::Camera:              return entity.HasComponent<CameraComponent>();
        case ComponentType::DirectionalLight:    return entity.HasComponent<DirectionalLightComponent>();
        case ComponentType::PointLight:          return entity.HasComponent<PointLightComponent>();
        case ComponentType::SpotLight:           return entity.HasComponent<SpotLightComponent>();
        case ComponentType::CharacterController: return entity.HasComponent<CharacterControllerComponent>();
        case ComponentType::BoxCollider:         return entity.HasComponent<BoxColliderComponent>();
        case ComponentType::SphereCollider:      return entity.HasComponent<SphereColliderComponent>();
        case ComponentType::CapsuleCollider:     return entity.HasComponent<CapsuleColliderComponent>();
        case ComponentType::BoxCollider2D:       return entity.HasComponent<BoxCollider2DComponent>();
        case ComponentType::CircleCollider2D:    return entity.HasComponent<CircleCollider2DComponent>();
        case ComponentType::Text:                return entity.HasComponent<TextComponent>();
        case ComponentType::Widget:              return entity.HasComponent<WidgetComponent>();
        case ComponentType::NetworkIdentity:     return entity.HasComponent<NetworkIdentityComponent>();
        case ComponentType::PhysicsJoint:        return entity.HasComponent<PhysicsJointComponent>();
        case ComponentType::ParticleSystem:      return entity.HasComponent<ParticleSystemComponent>();
        }
        return false;
    }

    int EntityHasComponent(lua_State *state)
    {
        auto *ref = static_cast<EntityRef *>(luaL_checkudata(state, 1, kEntityMetatable));
        ComponentType type{};
        const bool found = HasComponent(ResolveEntity(ref->scene, ref->id), luaL_checkstring(state, 2), type);
        lua_pushboolean(state, found);
        return 1;
    }

    int EntityGetComponent(lua_State *state)
    {
        auto *ref = static_cast<EntityRef *>(luaL_checkudata(state, 1, kEntityMetatable));
        ComponentType type{};
        const bool found = HasComponent(ResolveEntity(ref->scene, ref->id), luaL_checkstring(state, 2), type);
        if (!found)
        {
            lua_pushnil(state);
            return 1;
        }

        auto *component = static_cast<ComponentRef *>(lua_newuserdatauv(state, sizeof(ComponentRef), 0));
        *component = { ref->scene, ref->id, type };
        luaL_getmetatable(state, kComponentMetatable);
        lua_setmetatable(state, -2);
        return 1;
    }

    int EntityAddComponent(lua_State *state)
    {
        auto *ref = static_cast<EntityRef *>(luaL_checkudata(state, 1, kEntityMetatable));
        const char *name = luaL_checkstring(state, 2);
        Entity entity = ResolveEntity(ref->scene, ref->id);
        ComponentType type{};
        if (!entity || !ParseComponentType(name, type))
        {
            lua_pushnil(state);
            return 1;
        }

        switch (type)
        {
        case ComponentType::Rendering:           PushNewComponentRef<RenderingComponent>(state, ref->scene, ref->id, type, entity); return 1;
        case ComponentType::Transform:           PushNewComponentRef<TransformComponent>(state, ref->scene, ref->id, type, entity); return 1;
        case ComponentType::Rigidbody2D:
            PushNewComponentRef<Rigidbody2DComponent>(state, ref->scene, ref->id, type, entity);
            if (ref->scene && ref->scene->IsRunning()) ref->scene->CreatePhysics2DBody(entity);
            return 1;
        case ComponentType::Rigidbody3D:         PushNewComponentRef<RigidbodyComponent>(state, ref->scene, ref->id, type, entity); return 1;
        case ComponentType::Sprite2D:            PushNewComponentRef<Sprite2DComponent>(state, ref->scene, ref->id, type, entity); return 1;
        case ComponentType::Circle2D:            PushNewComponentRef<Circle2DComponent>(state, ref->scene, ref->id, type, entity); return 1;
        case ComponentType::StaticMesh:          PushNewComponentRef<StaticMeshComponent>(state, ref->scene, ref->id, type, entity); return 1;
        case ComponentType::SkeletalMesh:        PushNewComponentRef<SkeletalMeshComponent>(state, ref->scene, ref->id, type, entity); return 1;
        case ComponentType::AudioSource:         PushNewComponentRef<AudioSourceComponent>(state, ref->scene, ref->id, type, entity); return 1;
        case ComponentType::Camera:              PushNewComponentRef<CameraComponent>(state, ref->scene, ref->id, type, entity); return 1;
        case ComponentType::DirectionalLight:    PushNewComponentRef<DirectionalLightComponent>(state, ref->scene, ref->id, type, entity); return 1;
        case ComponentType::PointLight:          PushNewComponentRef<PointLightComponent>(state, ref->scene, ref->id, type, entity); return 1;
        case ComponentType::SpotLight:           PushNewComponentRef<SpotLightComponent>(state, ref->scene, ref->id, type, entity); return 1;
        case ComponentType::CharacterController: PushNewComponentRef<CharacterControllerComponent>(state, ref->scene, ref->id, type, entity); return 1;
        case ComponentType::BoxCollider:         PushNewComponentRef<BoxColliderComponent>(state, ref->scene, ref->id, type, entity); return 1;
        case ComponentType::SphereCollider:      PushNewComponentRef<SphereColliderComponent>(state, ref->scene, ref->id, type, entity); return 1;
        case ComponentType::CapsuleCollider:     PushNewComponentRef<CapsuleColliderComponent>(state, ref->scene, ref->id, type, entity); return 1;
        case ComponentType::BoxCollider2D:       PushNewComponentRef<BoxCollider2DComponent>(state, ref->scene, ref->id, type, entity); return 1;
        case ComponentType::CircleCollider2D:    PushNewComponentRef<CircleCollider2DComponent>(state, ref->scene, ref->id, type, entity); return 1;
        case ComponentType::Text:                PushNewComponentRef<TextComponent>(state, ref->scene, ref->id, type, entity); return 1;
        case ComponentType::Widget:              PushNewComponentRef<WidgetComponent>(state, ref->scene, ref->id, type, entity); return 1;
        case ComponentType::NetworkIdentity:     PushNewComponentRef<NetworkIdentityComponent>(state, ref->scene, ref->id, type, entity); return 1;
        case ComponentType::PhysicsJoint:
        {
            PushNewComponentRef<PhysicsJointComponent>(state, ref->scene, ref->id, type, entity);
            if (entity.HasComponent<PhysicsJointComponent>()) entity.GetComponent<PhysicsJointComponent>().dirty = true;
            return 1;
        }
        case ComponentType::ParticleSystem:
        {
            if (!entity.HasComponent<RenderingComponent>()) entity.AddComponent<RenderingComponent>();
            PushNewComponentRef<ParticleSystemComponent>(state, ref->scene, ref->id, type, entity);
            return 1;
        }
        }
        lua_pushnil(state);
        return 1;
    }

    int EntityRemoveComponent(lua_State *state)
    {
        auto *ref = static_cast<EntityRef *>(luaL_checkudata(state, 1, kEntityMetatable));
        const char *name = luaL_checkstring(state, 2);
        Entity entity = ResolveEntity(ref->scene, ref->id);
        ComponentType type{};
        if (!entity || !ParseComponentType(name, type))
            return 0;

        switch (type)
        {
        case ComponentType::Rendering:           if (entity.HasComponent<RenderingComponent>()) entity.RemoveComponent<RenderingComponent>(); break;
        case ComponentType::Transform:           break; // Never strip required TransformComponent
        case ComponentType::Rigidbody2D:
            if (entity.HasComponent<Rigidbody2DComponent>())
            {
                if (ref->scene) ref->scene->DestroyPhysics2DBody(entity);
                entity.RemoveComponent<Rigidbody2DComponent>();
            }
            break;
        case ComponentType::Rigidbody3D:         if (entity.HasComponent<RigidbodyComponent>()) entity.RemoveComponent<RigidbodyComponent>(); break;
        case ComponentType::Sprite2D:            if (entity.HasComponent<Sprite2DComponent>()) entity.RemoveComponent<Sprite2DComponent>(); break;
        case ComponentType::Circle2D:            if (entity.HasComponent<Circle2DComponent>()) entity.RemoveComponent<Circle2DComponent>(); break;
        case ComponentType::StaticMesh:          if (entity.HasComponent<StaticMeshComponent>()) entity.RemoveComponent<StaticMeshComponent>(); break;
        case ComponentType::SkeletalMesh:        if (entity.HasComponent<SkeletalMeshComponent>()) entity.RemoveComponent<SkeletalMeshComponent>(); break;
        case ComponentType::AudioSource:         if (entity.HasComponent<AudioSourceComponent>()) entity.RemoveComponent<AudioSourceComponent>(); break;
        case ComponentType::Camera:              if (entity.HasComponent<CameraComponent>()) entity.RemoveComponent<CameraComponent>(); break;
        case ComponentType::DirectionalLight:    if (entity.HasComponent<DirectionalLightComponent>()) entity.RemoveComponent<DirectionalLightComponent>(); break;
        case ComponentType::PointLight:          if (entity.HasComponent<PointLightComponent>()) entity.RemoveComponent<PointLightComponent>(); break;
        case ComponentType::SpotLight:           if (entity.HasComponent<SpotLightComponent>()) entity.RemoveComponent<SpotLightComponent>(); break;
        case ComponentType::CharacterController: if (entity.HasComponent<CharacterControllerComponent>()) entity.RemoveComponent<CharacterControllerComponent>(); break;
        case ComponentType::BoxCollider:         if (entity.HasComponent<BoxColliderComponent>()) entity.RemoveComponent<BoxColliderComponent>(); break;
        case ComponentType::SphereCollider:      if (entity.HasComponent<SphereColliderComponent>()) entity.RemoveComponent<SphereColliderComponent>(); break;
        case ComponentType::CapsuleCollider:     if (entity.HasComponent<CapsuleColliderComponent>()) entity.RemoveComponent<CapsuleColliderComponent>(); break;
        case ComponentType::BoxCollider2D:       if (entity.HasComponent<BoxCollider2DComponent>()) entity.RemoveComponent<BoxCollider2DComponent>(); break;
        case ComponentType::CircleCollider2D:    if (entity.HasComponent<CircleCollider2DComponent>()) entity.RemoveComponent<CircleCollider2DComponent>(); break;
        case ComponentType::Text:                if (entity.HasComponent<TextComponent>()) entity.RemoveComponent<TextComponent>(); break;
        case ComponentType::Widget:              if (entity.HasComponent<WidgetComponent>()) entity.RemoveComponent<WidgetComponent>(); break;
        case ComponentType::NetworkIdentity:     if (entity.HasComponent<NetworkIdentityComponent>()) entity.RemoveComponent<NetworkIdentityComponent>(); break;
        case ComponentType::PhysicsJoint:        if (entity.HasComponent<PhysicsJointComponent>()) entity.RemoveComponent<PhysicsJointComponent>(); break;
        case ComponentType::ParticleSystem:      if (entity.HasComponent<ParticleSystemComponent>()) entity.RemoveComponent<ParticleSystemComponent>(); break;
        }
        return 0;
    }

    int EntityGetTransform(lua_State *state)
    {
        auto *ref = static_cast<EntityRef *>(luaL_checkudata(state, 1, kEntityMetatable));
        Entity entity = ResolveEntity(ref->scene, ref->id);

        if (!entity || !entity.HasComponent<TransformComponent>())
        {
            lua_pushnil(state);
            return 1;
        }

        auto *component = static_cast<ComponentRef *>(lua_newuserdatauv(state, sizeof(ComponentRef), 0));
        *component = { ref->scene, ref->id, ComponentType::Transform };
        luaL_getmetatable(state, kComponentMetatable);
        lua_setmetatable(state, -2);
        return 1;
    }

    int EntityIndex(lua_State *state)
    {
        auto *ref = static_cast<EntityRef *>(luaL_checkudata(state, 1, kEntityMetatable));
        const char *key = luaL_checkstring(state, 2);
        Entity entity = ResolveEntity(ref->scene, ref->id);

        if (strcmp(key, "id") == 0)
        {
            lua_pushinteger(state, static_cast<lua_Integer>(ref->id));
            return 1;
        }
        if (strcmp(key, "valid") == 0)
        {
            lua_pushboolean(state, static_cast<bool>(entity));
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
        if (strcmp(key, "has_component") == 0) { lua_pushcfunction(state, EntityHasComponent); return 1; }
        if (strcmp(key, "get_component") == 0) { lua_pushcfunction(state, EntityGetComponent); return 1; }
        if (strcmp(key, "add_component") == 0) { lua_pushcfunction(state, EntityAddComponent); return 1; }
        if (strcmp(key, "remove_component") == 0) { lua_pushcfunction(state, EntityRemoveComponent); return 1; }
        if (strcmp(key, "get_transform") == 0) { lua_pushcfunction(state, EntityGetTransform); return 1; }
        if (strcmp(key, "destroy") == 0) { lua_pushcfunction(state, EntityDestroy); return 1; }
        if (strcmp(key, "is_valid") == 0) { lua_pushcfunction(state, EntityIsValid); return 1; }
        if (strcmp(key, "get_name") == 0) { lua_pushcfunction(state, EntityGetName); return 1; }
        if (strcmp(key, "get_parent") == 0) { lua_pushcfunction(state, EntityGetParent); return 1; }
        if (strcmp(key, "set_parent") == 0) { lua_pushcfunction(state, EntitySetParent); return 1; }
        if (strcmp(key, "get_children") == 0) { lua_pushcfunction(state, EntityGetChildren); return 1; }

        lua_pushnil(state);
        return 1;
    }

    int EntityNewIndex(lua_State *state)
    {
        auto *ref = static_cast<EntityRef *>(luaL_checkudata(state, 1, kEntityMetatable));
        const char *key = luaL_checkstring(state, 2);
        Entity entity = ResolveEntity(ref->scene, ref->id);
        if (!entity)
            return 0;

        if (strcmp(key, "name") == 0)
        {
            const char *newName = luaL_checkstring(state, 3);
            entity.GetComponent<IDComponent>().name = newName;
            return 0;
        }
        return luaL_error(state, "unsupported Entity property '%s'", key);
    }

    int RenderingSetVisible(lua_State *state)
    {
        auto *ref = static_cast<ComponentRef *>(luaL_checkudata(state, 1, kComponentMetatable));
        const bool visible = lua_isboolean(state, 2)
            ? (lua_toboolean(state, 2) != 0)
            : (luaL_checkinteger(state, 2) != 0);
        Entity entity = ResolveEntity(ref->scene, ref->id);
        if (entity && entity.HasComponent<RenderingComponent>())
        {
            entity.GetComponent<RenderingComponent>().visible = visible;
        }
        return 0;
    }

    int Rigidbody2DApplyForce(lua_State *state)
    {
        auto *ref = static_cast<ComponentRef *>(luaL_checkudata(state, 1, kComponentMetatable));
        const glm::vec2 force = lua_istable(state, 2)
            ? ReadVec2(state, 2)
            : glm::vec2(static_cast<float>(luaL_checknumber(state, 2)), static_cast<float>(luaL_checknumber(state, 3)));

        Entity entity = ResolveEntity(ref->scene, ref->id);
        if (entity && entity.HasComponent<Rigidbody2DComponent>())
        {
            auto &c = entity.GetComponent<Rigidbody2DComponent>();
            if (b2Body_IsValid(c.bodyId))
                b2Body_ApplyForceToCenter(c.bodyId, b2Vec2{ force.x, force.y }, true);
            else
                c.linearVelocity += force;
        }
        return 0;
    }

    int Rigidbody2DApplyLinearImpulse(lua_State *state)
    {
        auto *ref = static_cast<ComponentRef *>(luaL_checkudata(state, 1, kComponentMetatable));
        const glm::vec2 impulse = lua_istable(state, 2)
            ? ReadVec2(state, 2)
            : glm::vec2(static_cast<float>(luaL_checknumber(state, 2)), static_cast<float>(luaL_checknumber(state, 3)));
        Entity entity = ResolveEntity(ref->scene, ref->id);
        if (entity && entity.HasComponent<Rigidbody2DComponent>())
        {
            auto &c = entity.GetComponent<Rigidbody2DComponent>();
            if (b2Body_IsValid(c.bodyId))
                b2Body_ApplyLinearImpulseToCenter(c.bodyId, b2Vec2{ impulse.x, impulse.y }, true);
            else
                c.linearVelocity += impulse;
        }
        return 0;
    }

    int Rigidbody2DApplyTorque(lua_State *state)
    {
        auto *ref = static_cast<ComponentRef *>(luaL_checkudata(state, 1, kComponentMetatable));
        const auto torque = static_cast<float>(luaL_checknumber(state, 2));
        Entity entity = ResolveEntity(ref->scene, ref->id);
        if (entity && entity.HasComponent<Rigidbody2DComponent>())
        {
            auto &c = entity.GetComponent<Rigidbody2DComponent>();
            if (b2Body_IsValid(c.bodyId))
                b2Body_ApplyTorque(c.bodyId, torque, true);
            else
                c.angularVelocity += torque;
        }
        return 0;
    }

    int ComponentIndex(lua_State *state)
    {
        auto *ref = static_cast<ComponentRef *>(luaL_checkudata(state, 1, kComponentMetatable));
        const char *key = luaL_checkstring(state, 2);
        Entity entity = ResolveEntity(ref->scene, ref->id);

        if (!entity)
        {
            lua_pushnil(state);
            return 1;
        }

        switch (ref->type)
        {
        case ComponentType::Rendering:
        {
            auto &c = entity.GetComponent<RenderingComponent>();
            if (strcmp(key, "visible") == 0) { lua_pushboolean(state, c.visible); return 1; }
            if (strcmp(key, "set_visible") == 0) { lua_pushcfunction(state, RenderingSetVisible); return 1; }
            break;
        }
        case ComponentType::Transform:
        {
            auto &tc = entity.GetComponent<TransformComponent>();
            if (strcmp(key, "position") == 0) { PushVector(state, tc.local.translation); return 1; }
            if (strcmp(key, "rotation") == 0) { PushVector(state, glm::degrees(glm::eulerAngles(tc.local.rotation))); return 1; }
            if (strcmp(key, "scale") == 0) { PushVector(state, tc.local.scale); return 1; }
            if (strcmp(key, "world_position") == 0) { PushVector(state, tc.world.translation); return 1; }
            if (strcmp(key, "world_scale") == 0) { PushVector(state, tc.world.scale); return 1; }
            if (strcmp(key, "forward") == 0) { PushVector(state, glm::normalize(tc.local.rotation * glm::vec3(0.0f, 0.0f, -1.0f))); return 1; }
            if (strcmp(key, "right") == 0) { PushVector(state, glm::normalize(tc.local.rotation * glm::vec3(1.0f, 0.0f, 0.0f))); return 1; }
            if (strcmp(key, "up") == 0) { PushVector(state, glm::normalize(tc.local.rotation * glm::vec3(0.0f, 1.0f, 0.0f))); return 1; }
            if (strcmp(key, "translate") == 0) { lua_pushcfunction(state, TransformTranslate); return 1; }
            if (strcmp(key, "rotate") == 0) { lua_pushcfunction(state, TransformRotate); return 1; }
            if (strcmp(key, "look_at") == 0) { lua_pushcfunction(state, TransformLookAt); return 1; }
            break;
        }
        case ComponentType::Rigidbody2D:
        {
            auto &c = entity.GetComponent<Rigidbody2DComponent>();
            if (strcmp(key, "linear_velocity") == 0) { PushVector(state, c.linearVelocity); return 1; }
            if (strcmp(key, "angular_velocity") == 0) { lua_pushnumber(state, c.angularVelocity); return 1; }
            if (strcmp(key, "gravity_scale") == 0) { lua_pushnumber(state, c.gravityScale); return 1; }
            if (strcmp(key, "linear_damping") == 0) { lua_pushnumber(state, c.linearDamping); return 1; }
            if (strcmp(key, "angular_damping") == 0) { lua_pushnumber(state, c.angularDamping); return 1; }
            if (strcmp(key, "fixed_rotation") == 0) { lua_pushboolean(state, c.fixedRotation); return 1; }
            if (strcmp(key, "is_awake") == 0) { lua_pushboolean(state, c.isAwake); return 1; }
            if (strcmp(key, "apply_force") == 0) { lua_pushcfunction(state, Rigidbody2DApplyForce); return 1; }
            if (strcmp(key, "apply_linear_impulse") == 0) { lua_pushcfunction(state, Rigidbody2DApplyLinearImpulse); return 1; }
            if (strcmp(key, "apply_torque") == 0) { lua_pushcfunction(state, Rigidbody2DApplyTorque); return 1; }
            break;
        }
        case ComponentType::Rigidbody3D:
        {
            auto &c = entity.GetComponent<RigidbodyComponent>();
            if (strcmp(key, "linear_velocity") == 0) { PushVector(state, c.linearVelocity); return 1; }
            if (strcmp(key, "angular_velocity") == 0) { PushVector(state, c.angularVelocity); return 1; }
            if (strcmp(key, "use_gravity") == 0) { lua_pushboolean(state, c.useGravity); return 1; }
            if (strcmp(key, "gravity_factor") == 0) { lua_pushnumber(state, c.gravityFactor); return 1; }
            if (strcmp(key, "friction") == 0) { lua_pushnumber(state, c.friction); return 1; }
            if (strcmp(key, "restitution") == 0) { lua_pushnumber(state, c.restitution); return 1; }
            if (strcmp(key, "mass") == 0) { lua_pushnumber(state, c.mass); return 1; }
            if (strcmp(key, "linear_damping") == 0) { lua_pushnumber(state, c.linearDamping); return 1; }
            if (strcmp(key, "angular_damping") == 0) { lua_pushnumber(state, c.angularDamping); return 1; }
            if (strcmp(key, "apply_force") == 0) { lua_pushcfunction(state, Rigidbody3DApplyForce); return 1; }
            if (strcmp(key, "apply_force_at") == 0) { lua_pushcfunction(state, Rigidbody3DApplyForceAt); return 1; }
            if (strcmp(key, "apply_impulse") == 0) { lua_pushcfunction(state, Rigidbody3DApplyImpulse); return 1; }
            if (strcmp(key, "apply_impulse_at") == 0) { lua_pushcfunction(state, Rigidbody3DApplyImpulseAt); return 1; }
            if (strcmp(key, "apply_torque") == 0) { lua_pushcfunction(state, Rigidbody3DApplyTorque); return 1; }
            if (strcmp(key, "activate") == 0) { lua_pushcfunction(state, Rigidbody3DActivate); return 1; }
            if (strcmp(key, "deactivate") == 0) { lua_pushcfunction(state, Rigidbody3DDeactivate); return 1; }
            break;
        }
        case ComponentType::CharacterController:
        {
            auto &c = entity.GetComponent<CharacterControllerComponent>();
            if (strcmp(key, "linear_velocity") == 0) { PushVector(state, c.linearVelocity); return 1; }
            if (strcmp(key, "radius") == 0) { lua_pushnumber(state, c.radius); return 1; }
            if (strcmp(key, "height") == 0) { lua_pushnumber(state, c.height); return 1; }
            if (strcmp(key, "max_step_height") == 0) { lua_pushnumber(state, c.maxStepHeight); return 1; }
            if (strcmp(key, "max_slope_angle") == 0) { lua_pushnumber(state, c.maxSlopeAngle); return 1; }
            if (strcmp(key, "gravity_factor") == 0) { lua_pushnumber(state, c.gravityFactor); return 1; }
            if (strcmp(key, "is_grounded") == 0)
            {
                auto actor = c.character.lock();
                lua_pushboolean(state, actor && actor->IsOnGround());
                return 1;
            }
            if (strcmp(key, "ground_velocity") == 0)
            {
                auto actor = c.character.lock();
                PushVector(state, actor ? actor->GetGroundVelocity() : glm::vec3(0.0f));
                return 1;
            }
            if (strcmp(key, "ground_normal") == 0)
            {
                auto actor = c.character.lock();
                PushVector(state, actor ? actor->GetGroundNormal() : glm::vec3(0.0f, 1.0f, 0.0f));
                return 1;
            }
            if (strcmp(key, "move") == 0) { lua_pushcfunction(state, CharacterMove); return 1; }
            if (strcmp(key, "set_position") == 0) { lua_pushcfunction(state, CharacterSetPosition); return 1; }
            if (strcmp(key, "set_rotation") == 0) { lua_pushcfunction(state, CharacterSetRotation); return 1; }
            break;
        }
        case ComponentType::Sprite2D:
        {
            auto &c = entity.GetComponent<Sprite2DComponent>();
            if (strcmp(key, "flip_x") == 0) { lua_pushboolean(state, c.flipX); return 1; }
            if (strcmp(key, "flip_y") == 0) { lua_pushboolean(state, c.flipY); return 1; }
            if (strcmp(key, "color") == 0) { PushVector(state, c.color); return 1; }
            if (strcmp(key, "tiling_factor") == 0) { PushVector(state, c.tilingFactor); return 1; }
            break;
        }
        case ComponentType::Circle2D:
        {
            auto &c = entity.GetComponent<Circle2DComponent>();
            if (strcmp(key, "color") == 0) { PushVector(state, c.color); return 1; }
            if (strcmp(key, "thickness") == 0) { lua_pushnumber(state, c.thickness); return 1; }
            if (strcmp(key, "fade") == 0) { lua_pushnumber(state, c.fade); return 1; }
            break;
        }
        case ComponentType::StaticMesh:
        {
            auto &c = entity.GetComponent<StaticMeshComponent>();
            if (strcmp(key, "asset_handle") == 0) { lua_pushinteger(state, static_cast<lua_Integer>(static_cast<uint64_t>(c.handle))); return 1; }
            if (strcmp(key, "set_primitive") == 0) { lua_pushcfunction(state, MeshSetPrimitive); return 1; }
            break;
        }
        case ComponentType::SkeletalMesh:
        {
            auto &c = entity.GetComponent<SkeletalMeshComponent>();
            if (strcmp(key, "asset_handle") == 0) { lua_pushinteger(state, static_cast<lua_Integer>(static_cast<uint64_t>(c.handle))); return 1; }
            if (strcmp(key, "animator_handle") == 0) { lua_pushinteger(state, static_cast<lua_Integer>(static_cast<uint64_t>(c.runtimeAnimatorHandle))); return 1; }
            if (strcmp(key, "current_state") == 0) { lua_pushstring(state, c.currentStateName.c_str()); return 1; }
            if (strcmp(key, "state_elapsed") == 0) { lua_pushnumber(state, c.stateElapsed); return 1; }
            if (strcmp(key, "state_normalized") == 0) { lua_pushnumber(state, c.stateNormalized); return 1; }
            if (strcmp(key, "unique_animator") == 0) { lua_pushboolean(state, c.uniqueAnimator); return 1; }
            if (strcmp(key, "set_float") == 0) { lua_pushcfunction(state, SkeletalSetFloat); return 1; }
            if (strcmp(key, "set_bool") == 0) { lua_pushcfunction(state, SkeletalSetBool); return 1; }
            if (strcmp(key, "set_int") == 0) { lua_pushcfunction(state, SkeletalSetInt); return 1; }
            if (strcmp(key, "set_string") == 0) { lua_pushcfunction(state, SkeletalSetString); return 1; }
            if (strcmp(key, "get_float") == 0) { lua_pushcfunction(state, SkeletalGetFloat); return 1; }
            if (strcmp(key, "get_bool") == 0) { lua_pushcfunction(state, SkeletalGetBool); return 1; }
            if (strcmp(key, "get_int") == 0) { lua_pushcfunction(state, SkeletalGetInt); return 1; }
            if (strcmp(key, "play_state") == 0) { lua_pushcfunction(state, SkeletalPlayState); return 1; }
            break;
        }
        case ComponentType::AudioSource:
        {
            auto &c = entity.GetComponent<AudioSourceComponent>();
            if (strcmp(key, "volume") == 0) { lua_pushnumber(state, c.volume); return 1; }
            if (strcmp(key, "pitch") == 0) { lua_pushnumber(state, c.pitch); return 1; }
            if (strcmp(key, "pan") == 0) { lua_pushnumber(state, c.pan); return 1; }
            if (strcmp(key, "loop") == 0) { lua_pushboolean(state, c.loop); return 1; }
            if (strcmp(key, "play_on_start") == 0) { lua_pushboolean(state, c.playOnStart); return 1; }
            if (strcmp(key, "is_playing") == 0)
            {
                auto sound = GetAudioSound(ref->scene, c);
                lua_pushboolean(state, sound && sound->IsPlaying());
                return 1;
            }
            if (strcmp(key, "is_paused") == 0)
            {
                auto sound = GetAudioSound(ref->scene, c);
                lua_pushboolean(state, sound && sound->IsPaused());
                return 1;
            }
            if (strcmp(key, "play") == 0) { lua_pushcfunction(state, AudioSourcePlay); return 1; }
            if (strcmp(key, "stop") == 0) { lua_pushcfunction(state, AudioSourceStop); return 1; }
            if (strcmp(key, "pause") == 0) { lua_pushcfunction(state, AudioSourcePause); return 1; }
            if (strcmp(key, "resume") == 0) { lua_pushcfunction(state, AudioSourceResume); return 1; }
            break;
        }
        case ComponentType::Camera:
        {
            auto &c = entity.GetComponent<CameraComponent>();
            if (strcmp(key, "primary") == 0) { lua_pushboolean(state, c.primary); return 1; }
            if (strcmp(key, "fov") == 0) { lua_pushnumber(state, c.camera.fov); return 1; }
            if (strcmp(key, "near_plane") == 0 || strcmp(key, "near_clip") == 0) { lua_pushnumber(state, c.camera.nearPlane); return 1; }
            if (strcmp(key, "far_plane") == 0 || strcmp(key, "far_clip") == 0) { lua_pushnumber(state, c.camera.farPlane); return 1; }
            if (strcmp(key, "ortho_size") == 0) { lua_pushnumber(state, c.camera.orthoSize); return 1; }
            if (strcmp(key, "projection_type") == 0)
            {
                lua_pushstring(state, c.camera.projectionType == ProjectionType::Perspective ? "perspective" : "orthographic");
                return 1;
            }
            break;
        }
        case ComponentType::DirectionalLight:
        {
            auto &c = entity.GetComponent<DirectionalLightComponent>();
            if (strcmp(key, "color") == 0) { PushVector(state, c.color); return 1; }
            if (strcmp(key, "intensity") == 0) { lua_pushnumber(state, c.intensity); return 1; }
            if (strcmp(key, "shadow_distance") == 0) { lua_pushnumber(state, c.shadowDistance); return 1; }
            if (strcmp(key, "cascade_shadow") == 0) { lua_pushboolean(state, c.cascadeShadow); return 1; }
            break;
        }
        case ComponentType::PointLight:
        {
            auto &c = entity.GetComponent<PointLightComponent>();
            if (strcmp(key, "color") == 0) { PushVector(state, c.color); return 1; }
            if (strcmp(key, "intensity") == 0) { lua_pushnumber(state, c.intensity); return 1; }
            if (strcmp(key, "range") == 0) { lua_pushnumber(state, c.range); return 1; }
            if (strcmp(key, "enabled") == 0) { lua_pushboolean(state, c.enabled); return 1; }
            break;
        }
        case ComponentType::SpotLight:
        {
            auto &c = entity.GetComponent<SpotLightComponent>();
            if (strcmp(key, "color") == 0) { PushVector(state, c.color); return 1; }
            if (strcmp(key, "intensity") == 0) { lua_pushnumber(state, c.intensity); return 1; }
            if (strcmp(key, "range") == 0) { lua_pushnumber(state, c.range); return 1; }
            if (strcmp(key, "enabled") == 0) { lua_pushboolean(state, c.enabled); return 1; }
            if (strcmp(key, "inner_cone_angle") == 0) { lua_pushnumber(state, c.innerConeAngle); return 1; }
            if (strcmp(key, "outer_cone_angle") == 0) { lua_pushnumber(state, c.outerConeAngle); return 1; }
            break;
        }
        case ComponentType::BoxCollider:
        {
            auto &c = entity.GetComponent<BoxColliderComponent>();
            if (strcmp(key, "center") == 0) { PushVector(state, c.center); return 1; }
            if (strcmp(key, "scale") == 0 || strcmp(key, "size") == 0) { PushVector(state, c.scale); return 1; }
            break;
        }
        case ComponentType::SphereCollider:
        {
            auto &c = entity.GetComponent<SphereColliderComponent>();
            if (strcmp(key, "center") == 0) { PushVector(state, c.center); return 1; }
            if (strcmp(key, "radius") == 0) { lua_pushnumber(state, c.radius); return 1; }
            break;
        }
        case ComponentType::CapsuleCollider:
        {
            auto &c = entity.GetComponent<CapsuleColliderComponent>();
            if (strcmp(key, "center") == 0) { PushVector(state, c.center); return 1; }
            if (strcmp(key, "radius") == 0) { lua_pushnumber(state, c.radius); return 1; }
            if (strcmp(key, "height") == 0) { lua_pushnumber(state, c.height); return 1; }
            break;
        }
        case ComponentType::BoxCollider2D:
        {
            auto &c = entity.GetComponent<BoxCollider2DComponent>();
            if (strcmp(key, "size") == 0) { PushVector(state, c.size); return 1; }
            if (strcmp(key, "offset") == 0) { PushVector(state, c.offset); return 1; }
            if (strcmp(key, "friction") == 0) { lua_pushnumber(state, c.friction); return 1; }
            if (strcmp(key, "restitution") == 0) { lua_pushnumber(state, c.restitution); return 1; }
            if (strcmp(key, "density") == 0) { lua_pushnumber(state, c.density); return 1; }
            if (strcmp(key, "is_sensor") == 0) { lua_pushboolean(state, c.isSensor); return 1; }
            break;
        }
        case ComponentType::CircleCollider2D:
        {
            auto &c = entity.GetComponent<CircleCollider2DComponent>();
            if (strcmp(key, "center") == 0) { PushVector(state, c.center); return 1; }
            if (strcmp(key, "radius") == 0) { lua_pushnumber(state, c.radius); return 1; }
            if (strcmp(key, "friction") == 0) { lua_pushnumber(state, c.friction); return 1; }
            if (strcmp(key, "restitution") == 0) { lua_pushnumber(state, c.restitution); return 1; }
            if (strcmp(key, "density") == 0) { lua_pushnumber(state, c.density); return 1; }
            if (strcmp(key, "is_sensor") == 0) { lua_pushboolean(state, c.isSensor); return 1; }
            break;
        }
        case ComponentType::Text:
        {
            auto &c = entity.GetComponent<TextComponent>();
            if (strcmp(key, "text") == 0) { lua_pushstring(state, c.text.c_str()); return 1; }
            if (strcmp(key, "color") == 0) { PushVector(state, c.color); return 1; }
            if (strcmp(key, "kerning") == 0) { lua_pushnumber(state, c.kerning); return 1; }
            if (strcmp(key, "line_spacing") == 0) { lua_pushnumber(state, c.lineSpacing); return 1; }
            if (strcmp(key, "font_size") == 0) { lua_pushnumber(state, c.fontSize); return 1; }
            if (strcmp(key, "screen_space") == 0) { lua_pushboolean(state, c.screenSpace); return 1; }
            if (strcmp(key, "billboard") == 0) { lua_pushboolean(state, c.billboard); return 1; }
            if (strcmp(key, "center_align") == 0) { lua_pushboolean(state, c.centerAlign); return 1; }
            break;
        }
        case ComponentType::Widget:
        {
            auto &c = entity.GetComponent<WidgetComponent>();
            if (strcmp(key, "widget_handle") == 0) { lua_pushinteger(state, static_cast<lua_Integer>(static_cast<uint64_t>(c.widgetHandle))); return 1; }
            break;
        }
        case ComponentType::NetworkIdentity:
        {
            auto &c = entity.GetComponent<NetworkIdentityComponent>();
            if (strcmp(key, "net_id") == 0) { lua_pushinteger(state, static_cast<lua_Integer>(c.netId)); return 1; }
            if (strcmp(key, "owner_client") == 0) { lua_pushinteger(state, static_cast<lua_Integer>(c.ownerClient)); return 1; }
            if (strcmp(key, "is_local_player") == 0) { lua_pushboolean(state, c.isLocalPlayer); return 1; }
            if (strcmp(key, "replicate") == 0) { lua_pushboolean(state, c.replicate); return 1; }
            if (strcmp(key, "sync_mask") == 0) { lua_pushinteger(state, static_cast<lua_Integer>(c.syncComponentsMask)); return 1; }
            if (strcmp(key, "replicated_movement_speed") == 0) { lua_pushnumber(state, c.replicatedMovementSpeed); return 1; }
            if (strcmp(key, "replicated_is_grounded") == 0) { lua_pushboolean(state, c.replicatedIsGrounded); return 1; }
            if (strcmp(key, "mark_dirty") == 0) { lua_pushcfunction(state, NetMarkDirty); return 1; }
            break;
        }
        case ComponentType::PhysicsJoint:
        {
            auto &c = entity.GetComponent<PhysicsJointComponent>();
            if (strcmp(key, "connected_entity") == 0) { lua_pushinteger(state, static_cast<lua_Integer>(static_cast<uint64_t>(c.connectedEntity))); return 1; }
            if (strcmp(key, "type") == 0 || strcmp(key, "joint_type") == 0) { lua_pushstring(state, physics::JointTypeToString(c.desc.type)); return 1; }
            if (strcmp(key, "space") == 0) { lua_pushstring(state, c.desc.space == physics::JointSpace::World ? "World" : "Local"); return 1; }
            if (strcmp(key, "anchor_a") == 0) { PushVector(state, c.desc.localAnchorA); return 1; }
            if (strcmp(key, "anchor_b") == 0) { PushVector(state, c.desc.localAnchorB); return 1; }
            if (strcmp(key, "axis_a") == 0) { PushVector(state, c.desc.localAxisA); return 1; }
            if (strcmp(key, "axis_b") == 0) { PushVector(state, c.desc.localAxisB); return 1; }
            if (strcmp(key, "min_distance") == 0) { lua_pushnumber(state, c.desc.minDistance); return 1; }
            if (strcmp(key, "max_distance") == 0) { lua_pushnumber(state, c.desc.maxDistance); return 1; }
            if (strcmp(key, "enable_limits") == 0) { lua_pushboolean(state, c.desc.enableLimits); return 1; }
            if (strcmp(key, "limits_min") == 0) { lua_pushnumber(state, c.desc.limitsMin); return 1; }
            if (strcmp(key, "limits_max") == 0) { lua_pushnumber(state, c.desc.limitsMax); return 1; }
            if (strcmp(key, "enable_spring") == 0) { lua_pushboolean(state, c.desc.enableSpring); return 1; }
            if (strcmp(key, "spring_frequency") == 0) { lua_pushnumber(state, c.desc.springFrequency); return 1; }
            if (strcmp(key, "spring_damping") == 0) { lua_pushnumber(state, c.desc.springDamping); return 1; }
            if (strcmp(key, "motor_state") == 0) { lua_pushstring(state, physics::JointMotorStateToString(c.desc.motorState)); return 1; }
            if (strcmp(key, "motor_velocity") == 0) { lua_pushnumber(state, c.desc.motorTargetVelocity); return 1; }
            if (strcmp(key, "max_motor_force") == 0) { lua_pushnumber(state, c.desc.maxMotorForce); return 1; }
            if (strcmp(key, "break_force") == 0) { lua_pushnumber(state, c.desc.breakForce); return 1; }
            break;
        }
        case ComponentType::ParticleSystem:
        {
            auto &c = entity.GetComponent<ParticleSystemComponent>();
            if (strcmp(key, "is_playing") == 0) { lua_pushboolean(state, c.isPlaying); return 1; }
            if (strcmp(key, "type") == 0 || strcmp(key, "emitter_type") == 0)
            {
                switch (c.settings.type)
                {
                case ParticleEmitterType::Smoke: lua_pushstring(state, "Smoke"); return 1;
                case ParticleEmitterType::Fire: lua_pushstring(state, "Fire"); return 1;
                case ParticleEmitterType::Spark: lua_pushstring(state, "Spark"); return 1;
                default: lua_pushstring(state, "Custom"); return 1;
                }
            }
            if (strcmp(key, "blend_mode") == 0) { lua_pushstring(state, c.settings.blendMode == ParticleBlendMode::Additive ? "Additive" : "Alpha"); return 1; }
            if (strcmp(key, "max_particles") == 0) { lua_pushinteger(state, c.settings.maxParticles); return 1; }
            if (strcmp(key, "emission_rate") == 0) { lua_pushnumber(state, c.settings.emissionRate); return 1; }
            if (strcmp(key, "lifetime_min") == 0) { lua_pushnumber(state, c.settings.lifetimeMin); return 1; }
            if (strcmp(key, "lifetime_max") == 0) { lua_pushnumber(state, c.settings.lifetimeMax); return 1; }
            if (strcmp(key, "velocity_min") == 0) { PushVector(state, c.settings.velocityMin); return 1; }
            if (strcmp(key, "velocity_max") == 0) { PushVector(state, c.settings.velocityMax); return 1; }
            if (strcmp(key, "acceleration") == 0) { PushVector(state, c.settings.acceleration); return 1; }
            if (strcmp(key, "drag") == 0) { lua_pushnumber(state, c.settings.drag); return 1; }
            if (strcmp(key, "start_color") == 0) { PushVector(state, c.settings.startColor); return 1; }
            if (strcmp(key, "end_color") == 0) { PushVector(state, c.settings.endColor); return 1; }
            if (strcmp(key, "start_size") == 0) { PushVector(state, c.settings.startSize); return 1; }
            if (strcmp(key, "end_size") == 0) { PushVector(state, c.settings.endSize); return 1; }
            if (strcmp(key, "spawn_radius") == 0) { lua_pushnumber(state, c.settings.spawnRadius); return 1; }
            if (strcmp(key, "velocity_alignment") == 0) { lua_pushnumber(state, c.settings.velocityAlignment); return 1; }
            if (strcmp(key, "play") == 0) { lua_pushcfunction(state, ParticlePlay); return 1; }
            if (strcmp(key, "stop") == 0) { lua_pushcfunction(state, ParticleStop); return 1; }
            if (strcmp(key, "restart") == 0) { lua_pushcfunction(state, ParticleRestart); return 1; }
            if (strcmp(key, "set_preset") == 0) { lua_pushcfunction(state, ParticleSetPreset); return 1; }
            break;
        }
        }
        lua_pushnil(state);
        return 1;
    }

    int ComponentNewIndex(lua_State *state)
    {
        auto *ref = static_cast<ComponentRef *>(luaL_checkudata(state, 1, kComponentMetatable));
        const char *key = luaL_checkstring(state, 2);

        Entity entity = ResolveEntity(ref->scene, ref->id);
        if (!entity)
            return 0;

        switch (ref->type)
        {
        case ComponentType::Rendering:
        {
            auto &c = entity.GetComponent<RenderingComponent>();
            if (strcmp(key, "visible") == 0) { c.visible = lua_toboolean(state, 3) != 0; return 0; }
            break;
        }
        case ComponentType::Transform:
        {
            auto &tc = entity.GetComponent<TransformComponent>();
            if (strcmp(key, "position") == 0) { tc.local.translation = ReadVector(state, 3); tc.dirty = true; return 0; }
            if (strcmp(key, "rotation") == 0) { tc.local.rotation = glm::quat(glm::radians(ReadVector(state, 3))); tc.dirty = true; return 0; }
            if (strcmp(key, "scale") == 0) { tc.local.scale = ReadVector(state, 3); tc.dirty = true; return 0; }
            break;
        }
        case ComponentType::Rigidbody2D:
        {
            auto &c = entity.GetComponent<Rigidbody2DComponent>();
            physics::Physics2D *p2d = ref->scene ? ref->scene->GetPhysics2D() : nullptr;
            const bool validBody = p2d && p2d->IsValidBody(c.bodyId);
            if (strcmp(key, "linear_velocity") == 0)
            {
                c.linearVelocity = ReadVec2(state, 3);
                if (validBody) p2d->SetLinearVelocity(c.bodyId, c.linearVelocity);
                return 0;
            }
            if (strcmp(key, "angular_velocity") == 0)
            {
                c.angularVelocity = static_cast<float>(luaL_checknumber(state, 3));
                if (validBody) p2d->SetAngularVelocity(c.bodyId, c.angularVelocity);
                return 0;
            }
            if (strcmp(key, "gravity_scale") == 0)
            {
                c.gravityScale = static_cast<float>(luaL_checknumber(state, 3));
                if (validBody) p2d->SetGravityScale(c.bodyId, c.gravityScale);
                return 0;
            }
            if (strcmp(key, "linear_damping") == 0)
            {
                c.linearDamping = static_cast<float>(luaL_checknumber(state, 3));
                if (validBody) p2d->SetLinearDamping(c.bodyId, c.linearDamping);
                return 0;
            }
            if (strcmp(key, "angular_damping") == 0)
            {
                c.angularDamping = static_cast<float>(luaL_checknumber(state, 3));
                if (validBody) p2d->SetAngularDamping(c.bodyId, c.angularDamping);
                return 0;
            }
            if (strcmp(key, "fixed_rotation") == 0)
            {
                c.fixedRotation = lua_toboolean(state, 3) != 0;
                if (validBody) p2d->SetMotionLock(c.bodyId, false, false, c.fixedRotation);
                return 0;
            }
            break;
        }
        case ComponentType::Rigidbody3D:
        {
            auto &c = entity.GetComponent<RigidbodyComponent>();
            auto dynActor = c.dynamicActor.lock();
            if (strcmp(key, "linear_velocity") == 0)
            {
                c.linearVelocity = ReadVector(state, 3);
                if (dynActor) dynActor->SetLinearVelocity(c.linearVelocity);
                return 0;
            }
            if (strcmp(key, "angular_velocity") == 0)
            {
                c.angularVelocity = ReadVector(state, 3);
                if (dynActor) dynActor->SetAngularVelocity(c.angularVelocity);
                return 0;
            }
            if (strcmp(key, "use_gravity") == 0)
            {
                c.useGravity = lua_toboolean(state, 3) != 0;
                if (dynActor) dynActor->SetGravityFactor(c.useGravity ? c.gravityFactor : 0.0f);
                return 0;
            }
            if (strcmp(key, "gravity_factor") == 0)
            {
                c.gravityFactor = static_cast<float>(luaL_checknumber(state, 3));
                if (dynActor) dynActor->SetGravityFactor(c.useGravity ? c.gravityFactor : 0.0f);
                return 0;
            }
            if (strcmp(key, "friction") == 0)
            {
                c.friction = static_cast<float>(luaL_checknumber(state, 3));
                if (dynActor) dynActor->SetFriction(c.friction);
                if (auto stActor = c.staticActor.lock()) stActor->SetFriction(c.friction);
                return 0;
            }
            if (strcmp(key, "restitution") == 0)
            {
                c.restitution = static_cast<float>(luaL_checknumber(state, 3));
                if (dynActor) dynActor->SetRestitution(c.restitution);
                if (auto stActor = c.staticActor.lock()) stActor->SetRestitution(c.restitution);
                return 0;
            }
            if (strcmp(key, "linear_damping") == 0)
            {
                c.linearDamping = static_cast<float>(luaL_checknumber(state, 3));
                if (dynActor) dynActor->SetLinearDamping(c.linearDamping);
                return 0;
            }
            if (strcmp(key, "angular_damping") == 0)
            {
                c.angularDamping = static_cast<float>(luaL_checknumber(state, 3));
                if (dynActor) dynActor->SetAngularDamping(c.angularDamping);
                return 0;
            }
            if (strcmp(key, "mass") == 0)
            {
                c.mass = static_cast<float>(luaL_checknumber(state, 3));
                c.dirty = true;
                return 0;
            }
            break;
        }
        case ComponentType::CharacterController:
        {
            auto &c = entity.GetComponent<CharacterControllerComponent>();
            if (strcmp(key, "linear_velocity") == 0)
            {
                c.desiredVelocity = ReadVector(state, 3);
                c.linearVelocity = c.desiredVelocity;
                if (auto actor = c.character.lock())
                    actor->SetLinearVelocity(c.desiredVelocity);
                return 0;
            }
            if (strcmp(key, "radius") == 0) { c.radius = static_cast<float>(luaL_checknumber(state, 3)); c.dirty = true; return 0; }
            if (strcmp(key, "height") == 0) { c.height = static_cast<float>(luaL_checknumber(state, 3)); c.dirty = true; return 0; }
            if (strcmp(key, "max_step_height") == 0) { c.maxStepHeight = static_cast<float>(luaL_checknumber(state, 3)); c.dirty = true; return 0; }
            if (strcmp(key, "max_slope_angle") == 0) { c.maxSlopeAngle = static_cast<float>(luaL_checknumber(state, 3)); c.dirty = true; return 0; }
            if (strcmp(key, "gravity_factor") == 0) { c.gravityFactor = static_cast<float>(luaL_checknumber(state, 3)); c.dirty = true; return 0; }
            break;
        }
        case ComponentType::Sprite2D:
        {
            auto &c = entity.GetComponent<Sprite2DComponent>();
            if (strcmp(key, "flip_x") == 0) { c.flipX = lua_toboolean(state, 3) != 0; return 0; }
            if (strcmp(key, "flip_y") == 0) { c.flipY = lua_toboolean(state, 3) != 0; return 0; }
            if (strcmp(key, "color") == 0 && lua_istable(state, 3)) { c.color = ReadVec4(state, 3); return 0; }
            if (strcmp(key, "tiling_factor") == 0 && lua_istable(state, 3)) { c.tilingFactor = ReadVec2(state, 3); return 0; }
            break;
        }
        case ComponentType::Circle2D:
        {
            auto &c = entity.GetComponent<Circle2DComponent>();
            if (strcmp(key, "color") == 0) { c.color = ReadVec4(state, 3); return 0; }
            if (strcmp(key, "thickness") == 0) { c.thickness = static_cast<float>(luaL_checknumber(state, 3)); return 0; }
            if (strcmp(key, "fade") == 0) { c.fade = static_cast<float>(luaL_checknumber(state, 3)); return 0; }
            break;
        }
        case ComponentType::AudioSource:
        {
            auto &c = entity.GetComponent<AudioSourceComponent>();
            auto sound = GetAudioSound(ref->scene, c);
            if (strcmp(key, "volume") == 0)
            {
                c.volume = static_cast<float>(luaL_checknumber(state, 3));
                if (sound) sound->SetVolume(c.volume);
                return 0;
            }
            if (strcmp(key, "pitch") == 0)
            {
                c.pitch = static_cast<float>(luaL_checknumber(state, 3));
                if (sound) sound->SetPitch(c.pitch);
                return 0;
            }
            if (strcmp(key, "pan") == 0)
            {
                c.pan = static_cast<float>(luaL_checknumber(state, 3));
                if (sound) sound->SetPan(c.pan);
                return 0;
            }
            if (strcmp(key, "loop") == 0)
            {
                c.loop = lua_toboolean(state, 3) != 0;
                if (sound)
                {
                    FMOD_MODE mode = sound->GetMode();
                    mode = c.loop ? ((mode & ~FMOD_LOOP_OFF) | FMOD_LOOP_NORMAL) : ((mode & ~FMOD_LOOP_NORMAL) | FMOD_LOOP_OFF);
                    sound->SetMode(mode);
                }
                return 0;
            }
            if (strcmp(key, "play_on_start") == 0) { c.playOnStart = lua_toboolean(state, 3) != 0; return 0; }
            break;
        }
        case ComponentType::StaticMesh:
        {
            auto &c = entity.GetComponent<StaticMeshComponent>();
            if (strcmp(key, "asset_handle") == 0) { c.handle = AssetHandle(static_cast<uint64_t>(luaL_checkinteger(state, 3))); return 0; }
            break;
        }
        case ComponentType::SkeletalMesh:
        {
            auto &c = entity.GetComponent<SkeletalMeshComponent>();
            if (strcmp(key, "asset_handle") == 0) { c.handle = AssetHandle(static_cast<uint64_t>(luaL_checkinteger(state, 3))); return 0; }
            if (strcmp(key, "animator_handle") == 0) { c.runtimeAnimatorHandle = AssetHandle(static_cast<uint64_t>(luaL_checkinteger(state, 3))); return 0; }
            if (strcmp(key, "current_state") == 0) { c.currentStateName = luaL_checkstring(state, 3); return 0; }
            if (strcmp(key, "unique_animator") == 0) { c.uniqueAnimator = lua_toboolean(state, 3) != 0; return 0; }
            break;
        }
        case ComponentType::Camera:
        {
            auto &c = entity.GetComponent<CameraComponent>();
            if (strcmp(key, "primary") == 0) { c.primary = lua_toboolean(state, 3) != 0; return 0; }
            if (strcmp(key, "fov") == 0) { c.camera.fov = static_cast<float>(luaL_checknumber(state, 3)); return 0; }
            if (strcmp(key, "near_plane") == 0 || strcmp(key, "near_clip") == 0) { c.camera.nearPlane = static_cast<float>(luaL_checknumber(state, 3)); return 0; }
            if (strcmp(key, "far_plane") == 0 || strcmp(key, "far_clip") == 0) { c.camera.farPlane = static_cast<float>(luaL_checknumber(state, 3)); return 0; }
            if (strcmp(key, "ortho_size") == 0) { c.camera.orthoSize = static_cast<float>(luaL_checknumber(state, 3)); return 0; }
            if (strcmp(key, "projection_type") == 0)
            {
                const std::string proj = luaL_checkstring(state, 3);
                c.camera.projectionType = (proj == "orthographic" || proj == "ortho") ? ProjectionType::Orthographic : ProjectionType::Perspective;
                return 0;
            }
            break;
        }
        case ComponentType::DirectionalLight:
        {
            auto &c = entity.GetComponent<DirectionalLightComponent>();
            if (strcmp(key, "color") == 0) { c.color = ReadVec4(state, 3); return 0; }
            if (strcmp(key, "intensity") == 0) { c.intensity = static_cast<float>(luaL_checknumber(state, 3)); return 0; }
            if (strcmp(key, "shadow_distance") == 0) { c.shadowDistance = static_cast<float>(luaL_checknumber(state, 3)); return 0; }
            if (strcmp(key, "cascade_shadow") == 0) { c.cascadeShadow = lua_toboolean(state, 3) != 0; return 0; }
            break;
        }
        case ComponentType::PointLight:
        {
            auto &c = entity.GetComponent<PointLightComponent>();
            if (strcmp(key, "color") == 0) { c.color = ReadVec4(state, 3); return 0; }
            if (strcmp(key, "intensity") == 0) { c.intensity = static_cast<float>(luaL_checknumber(state, 3)); return 0; }
            if (strcmp(key, "range") == 0) { c.range = static_cast<float>(luaL_checknumber(state, 3)); return 0; }
            if (strcmp(key, "enabled") == 0) { c.enabled = lua_toboolean(state, 3) != 0; return 0; }
            break;
        }
        case ComponentType::SpotLight:
        {
            auto &c = entity.GetComponent<SpotLightComponent>();
            if (strcmp(key, "color") == 0) { c.color = ReadVec4(state, 3); return 0; }
            if (strcmp(key, "intensity") == 0) { c.intensity = static_cast<float>(luaL_checknumber(state, 3)); return 0; }
            if (strcmp(key, "range") == 0) { c.range = static_cast<float>(luaL_checknumber(state, 3)); return 0; }
            if (strcmp(key, "enabled") == 0) { c.enabled = lua_toboolean(state, 3) != 0; return 0; }
            if (strcmp(key, "inner_cone_angle") == 0) { c.innerConeAngle = static_cast<float>(luaL_checknumber(state, 3)); return 0; }
            if (strcmp(key, "outer_cone_angle") == 0) { c.outerConeAngle = static_cast<float>(luaL_checknumber(state, 3)); return 0; }
            break;
        }
        case ComponentType::BoxCollider:
        {
            auto &c = entity.GetComponent<BoxColliderComponent>();
            if (strcmp(key, "center") == 0) { c.center = ReadVector(state, 3); c.dirty = true; return 0; }
            if (strcmp(key, "scale") == 0 || strcmp(key, "size") == 0) { c.scale = ReadVector(state, 3); c.dirty = true; return 0; }
            break;
        }
        case ComponentType::SphereCollider:
        {
            auto &c = entity.GetComponent<SphereColliderComponent>();
            if (strcmp(key, "center") == 0) { c.center = ReadVector(state, 3); c.dirty = true; return 0; }
            if (strcmp(key, "radius") == 0) { c.radius = static_cast<float>(luaL_checknumber(state, 3)); c.dirty = true; return 0; }
            break;
        }
        case ComponentType::CapsuleCollider:
        {
            auto &c = entity.GetComponent<CapsuleColliderComponent>();
            if (strcmp(key, "center") == 0) { c.center = ReadVector(state, 3); c.dirty = true; return 0; }
            if (strcmp(key, "radius") == 0) { c.radius = static_cast<float>(luaL_checknumber(state, 3)); c.dirty = true; return 0; }
            if (strcmp(key, "height") == 0) { c.height = static_cast<float>(luaL_checknumber(state, 3)); c.dirty = true; return 0; }
            break;
        }
        case ComponentType::BoxCollider2D:
        {
            auto &c = entity.GetComponent<BoxCollider2DComponent>();
            if (strcmp(key, "size") == 0) { c.size = ReadVec2(state, 3); c.dirty = true; return 0; }
            if (strcmp(key, "offset") == 0) { c.offset = ReadVec2(state, 3); c.dirty = true; return 0; }
            if (strcmp(key, "friction") == 0) { c.friction = static_cast<float>(luaL_checknumber(state, 3)); c.dirty = true; return 0; }
            if (strcmp(key, "restitution") == 0) { c.restitution = static_cast<float>(luaL_checknumber(state, 3)); c.dirty = true; return 0; }
            if (strcmp(key, "density") == 0) { c.density = static_cast<float>(luaL_checknumber(state, 3)); c.dirty = true; return 0; }
            if (strcmp(key, "is_sensor") == 0) { c.isSensor = lua_toboolean(state, 3) != 0; c.dirty = true; return 0; }
            break;
        }
        case ComponentType::CircleCollider2D:
        {
            auto &c = entity.GetComponent<CircleCollider2DComponent>();
            if (strcmp(key, "center") == 0) { c.center = ReadVec2(state, 3); c.dirty = true; return 0; }
            if (strcmp(key, "radius") == 0) { c.radius = static_cast<float>(luaL_checknumber(state, 3)); c.dirty = true; return 0; }
            if (strcmp(key, "friction") == 0) { c.friction = static_cast<float>(luaL_checknumber(state, 3)); c.dirty = true; return 0; }
            if (strcmp(key, "restitution") == 0) { c.restitution = static_cast<float>(luaL_checknumber(state, 3)); c.dirty = true; return 0; }
            if (strcmp(key, "density") == 0) { c.density = static_cast<float>(luaL_checknumber(state, 3)); c.dirty = true; return 0; }
            if (strcmp(key, "is_sensor") == 0) { c.isSensor = lua_toboolean(state, 3) != 0; c.dirty = true; return 0; }
            break;
        }
        case ComponentType::Text:
        {
            auto &c = entity.GetComponent<TextComponent>();
            if (strcmp(key, "text") == 0) { c.text = luaL_checkstring(state, 3); return 0; }
            if (strcmp(key, "color") == 0) { c.color = ReadVec4(state, 3); return 0; }
            if (strcmp(key, "kerning") == 0) { c.kerning = static_cast<float>(luaL_checknumber(state, 3)); return 0; }
            if (strcmp(key, "line_spacing") == 0) { c.lineSpacing = static_cast<float>(luaL_checknumber(state, 3)); return 0; }
            if (strcmp(key, "font_size") == 0) { c.fontSize = static_cast<float>(luaL_checknumber(state, 3)); return 0; }
            if (strcmp(key, "screen_space") == 0) { c.screenSpace = lua_toboolean(state, 3) != 0; return 0; }
            if (strcmp(key, "billboard") == 0) { c.billboard = lua_toboolean(state, 3) != 0; return 0; }
            if (strcmp(key, "center_align") == 0) { c.centerAlign = lua_toboolean(state, 3) != 0; return 0; }
            break;
        }
        case ComponentType::Widget:
        {
            auto &c = entity.GetComponent<WidgetComponent>();
            if (strcmp(key, "widget_handle") == 0) { c.widgetHandle = AssetHandle(static_cast<uint64_t>(luaL_checkinteger(state, 3))); return 0; }
            break;
        }
        case ComponentType::NetworkIdentity:
        {
            auto &c = entity.GetComponent<NetworkIdentityComponent>();
            if (strcmp(key, "replicate") == 0) { c.replicate = lua_toboolean(state, 3) != 0; return 0; }
            if (strcmp(key, "is_local_player") == 0) { c.isLocalPlayer = lua_toboolean(state, 3) != 0; return 0; }
            if (strcmp(key, "sync_mask") == 0) { c.syncComponentsMask = static_cast<uint32_t>(luaL_checkinteger(state, 3)); return 0; }
            break;
        }
        case ComponentType::PhysicsJoint:
        {
            auto &c = entity.GetComponent<PhysicsJointComponent>();
            if (strcmp(key, "connected_entity") == 0)
            {
                if (lua_isinteger(state, 3)) c.connectedEntity = UUID(static_cast<uint64_t>(lua_tointeger(state, 3)));
                else if (auto *other = static_cast<EntityRef *>(luaL_testudata(state, 3, kEntityMetatable)))
                {
                    Entity otherEnt = ResolveEntity(other->scene, other->id);
                    if (otherEnt) c.connectedEntity = otherEnt.GetUUID();
                }
                c.dirty = true;
                return 0;
            }
            if (strcmp(key, "type") == 0 || strcmp(key, "joint_type") == 0)
            {
                c.desc.type = physics::JointTypeFromString(luaL_checkstring(state, 3));
                c.dirty = true;
                return 0;
            }
            if (strcmp(key, "space") == 0)
            {
                c.desc.space = (strcmp(luaL_checkstring(state, 3), "World") == 0) ? physics::JointSpace::World : physics::JointSpace::Local;
                c.dirty = true;
                return 0;
            }
            if (strcmp(key, "anchor_a") == 0) { c.desc.localAnchorA = ReadVector(state, 3); c.dirty = true; return 0; }
            if (strcmp(key, "anchor_b") == 0) { c.desc.localAnchorB = ReadVector(state, 3); c.dirty = true; return 0; }
            if (strcmp(key, "axis_a") == 0) { c.desc.localAxisA = ReadVector(state, 3); c.dirty = true; return 0; }
            if (strcmp(key, "axis_b") == 0) { c.desc.localAxisB = ReadVector(state, 3); c.dirty = true; return 0; }
            if (strcmp(key, "min_distance") == 0) { c.desc.minDistance = static_cast<float>(luaL_checknumber(state, 3)); c.dirty = true; return 0; }
            if (strcmp(key, "max_distance") == 0) { c.desc.maxDistance = static_cast<float>(luaL_checknumber(state, 3)); c.dirty = true; return 0; }
            if (strcmp(key, "enable_limits") == 0) { c.desc.enableLimits = lua_toboolean(state, 3) != 0; c.dirty = true; return 0; }
            if (strcmp(key, "limits_min") == 0) { c.desc.limitsMin = static_cast<float>(luaL_checknumber(state, 3)); c.dirty = true; return 0; }
            if (strcmp(key, "limits_max") == 0) { c.desc.limitsMax = static_cast<float>(luaL_checknumber(state, 3)); c.dirty = true; return 0; }
            if (strcmp(key, "enable_spring") == 0) { c.desc.enableSpring = lua_toboolean(state, 3) != 0; c.dirty = true; return 0; }
            if (strcmp(key, "spring_frequency") == 0) { c.desc.springFrequency = static_cast<float>(luaL_checknumber(state, 3)); c.dirty = true; return 0; }
            if (strcmp(key, "spring_damping") == 0) { c.desc.springDamping = static_cast<float>(luaL_checknumber(state, 3)); c.dirty = true; return 0; }
            if (strcmp(key, "motor_state") == 0) { c.desc.motorState = physics::JointMotorStateFromString(luaL_checkstring(state, 3)); c.dirty = true; return 0; }
            if (strcmp(key, "motor_velocity") == 0) { c.desc.motorTargetVelocity = static_cast<float>(luaL_checknumber(state, 3)); c.dirty = true; return 0; }
            if (strcmp(key, "max_motor_force") == 0) { c.desc.maxMotorForce = static_cast<float>(luaL_checknumber(state, 3)); c.dirty = true; return 0; }
            if (strcmp(key, "break_force") == 0) { c.desc.breakForce = static_cast<float>(luaL_checknumber(state, 3)); c.dirty = true; return 0; }
            break;
        }
        case ComponentType::ParticleSystem:
        {
            auto &c = entity.GetComponent<ParticleSystemComponent>();
            if (strcmp(key, "is_playing") == 0) { c.isPlaying = lua_toboolean(state, 3) != 0; return 0; }
            if (strcmp(key, "type") == 0 || strcmp(key, "emitter_type") == 0)
            {
                const char *s = luaL_checkstring(state, 3);
                if (strcmp(s, "Fire") == 0) c.settings.type = ParticleEmitterType::Fire;
                else if (strcmp(s, "Smoke") == 0) c.settings.type = ParticleEmitterType::Smoke;
                else if (strcmp(s, "Spark") == 0) c.settings.type = ParticleEmitterType::Spark;
                else c.settings.type = ParticleEmitterType::Custom;
                return 0;
            }
            if (strcmp(key, "blend_mode") == 0)
            {
                c.settings.blendMode = (strcmp(luaL_checkstring(state, 3), "Additive") == 0) ? ParticleBlendMode::Additive : ParticleBlendMode::Alpha;
                return 0;
            }
            if (strcmp(key, "max_particles") == 0) { c.settings.maxParticles = static_cast<uint32_t>(luaL_checkinteger(state, 3)); return 0; }
            if (strcmp(key, "emission_rate") == 0) { c.settings.emissionRate = static_cast<float>(luaL_checknumber(state, 3)); return 0; }
            if (strcmp(key, "lifetime_min") == 0) { c.settings.lifetimeMin = static_cast<float>(luaL_checknumber(state, 3)); return 0; }
            if (strcmp(key, "lifetime_max") == 0) { c.settings.lifetimeMax = static_cast<float>(luaL_checknumber(state, 3)); return 0; }
            if (strcmp(key, "velocity_min") == 0) { c.settings.velocityMin = ReadVector(state, 3); return 0; }
            if (strcmp(key, "velocity_max") == 0) { c.settings.velocityMax = ReadVector(state, 3); return 0; }
            if (strcmp(key, "acceleration") == 0) { c.settings.acceleration = ReadVector(state, 3); return 0; }
            if (strcmp(key, "drag") == 0) { c.settings.drag = static_cast<float>(luaL_checknumber(state, 3)); return 0; }
            if (strcmp(key, "start_color") == 0) { c.settings.startColor = ReadVec4(state, 3); return 0; }
            if (strcmp(key, "end_color") == 0) { c.settings.endColor = ReadVec4(state, 3); return 0; }
            if (strcmp(key, "start_size") == 0) { c.settings.startSize = ReadVec2(state, 3); return 0; }
            if (strcmp(key, "end_size") == 0) { c.settings.endSize = ReadVec2(state, 3); return 0; }
            if (strcmp(key, "spawn_radius") == 0) { c.settings.spawnRadius = static_cast<float>(luaL_checknumber(state, 3)); return 0; }
            if (strcmp(key, "velocity_alignment") == 0) { c.settings.velocityAlignment = static_cast<float>(luaL_checknumber(state, 3)); return 0; }
            break;
        }
        }
        return luaL_error(state, "unsupported property '%s' for component", key);
    }

    int IgniteLog(lua_State *state)
    {
        LOG_INFO("[Lua] {}", luaL_checkstring(state, 1));
        return 0;
    }

    int IgniteLogWarn(lua_State *state)
    {
        LOG_WARN("[Lua] {}", luaL_checkstring(state, 1));
        return 0;
    }

    int IgniteLogError(lua_State *state)
    {
        LOG_ERROR("[Lua] {}", luaL_checkstring(state, 1));
        return 0;
    }

    void PushEntity(lua_State *state, Scene *scene, uint64_t id)
    {
        auto *ref = static_cast<EntityRef *>(lua_newuserdatauv(state, sizeof(EntityRef), 0));
        *ref = { scene, id };
        luaL_getmetatable(state, kEntityMetatable);
        lua_setmetatable(state, -2);
    }

    int IgniteFindEntity(lua_State *state)
    {
        Scene *scene = GetSceneUpvalue(state);
        const uint64_t id = static_cast<uint64_t>(luaL_checkinteger(state, 1));
        if (!ResolveEntity(scene, id))
        {
            lua_pushnil(state);
            return 1;
        }

        PushEntity(state, scene, id);
        return 1;
    }

    int IgniteFindEntityByName(lua_State *state)
    {
        Scene *scene = GetSceneUpvalue(state);
        const char *name = luaL_checkstring(state, 1);
        if (!scene || !scene->registry) { lua_pushnil(state); return 1; }
        for (const auto &[uuid, enttId] : scene->entities)
        {
            if (!scene->registry->valid(enttId)) continue;
            Entity e(enttId, scene);
            if (e.GetName() == name)
            {
                PushEntity(state, scene, static_cast<uint64_t>(uuid));
                return 1;
            }
        }
        lua_pushnil(state);
        return 1;
    }

    int LuaInputIsKeyPressed(lua_State *state)
    {
        const int key = static_cast<int>(luaL_checkinteger(state, 1));
        lua_pushboolean(state, InputSystem::IsKeyPressed(static_cast<KeyCode>(key)));
        return 1;
    }

    int LuaInputIsMouseButtonPressed(lua_State *state)
    {
        const int button = static_cast<int>(luaL_checkinteger(state, 1));
        lua_pushboolean(state, InputSystem::IsMouseButtonPressed(static_cast<MouseCode>(button)));
        return 1;
    }

    int LuaInputIsActionPressed(lua_State *state)
    {
        const char *action = luaL_checkstring(state, 1);
        lua_pushboolean(state, InputSystem::IsActionPressed(action));
        return 1;
    }

    int LuaInputGetMousePosition(lua_State *state)
    {
        const glm::ivec2 pos = InputSystem::GetMousePosition();
        PushVector(state, glm::vec2(static_cast<float>(pos.x), static_cast<float>(pos.y)));
        return 1;
    }

    static void RegisterSceneClosure(lua_State *state, Scene *scene, const char *name, lua_CFunction fn)
    {
        auto **sceneRef = static_cast<Scene **>(lua_newuserdatauv(state, sizeof(Scene *), 0));
        *sceneRef = scene;
        lua_pushcclosure(state, fn, 1);
        lua_setfield(state, -2, name);
    }

    void RegisterBindings(lua_State *state, Scene *scene)
    {
        // Vec metatable
        luaL_newmetatable(state, kVecMetatable);
        lua_pushcfunction(state, VecAdd); lua_setfield(state, -2, "__add");
        lua_pushcfunction(state, VecSub); lua_setfield(state, -2, "__sub");
        lua_pushcfunction(state, VecMul); lua_setfield(state, -2, "__mul");
        lua_pushcfunction(state, VecDiv); lua_setfield(state, -2, "__div");
        lua_pushcfunction(state, VecUnm); lua_setfield(state, -2, "__unm");
        lua_pushcfunction(state, VecIndex); lua_setfield(state, -2, "__index");
        lua_pop(state, 1);

        // Entity metatable
        luaL_newmetatable(state, kEntityMetatable);
        lua_pushcfunction(state, EntityIndex);
        lua_setfield(state, -2, "__index");
        lua_pushcfunction(state, EntityNewIndex);
        lua_setfield(state, -2, "__newindex");
        lua_pop(state, 1);

        // Component metatable
        luaL_newmetatable(state, kComponentMetatable);
        lua_pushcfunction(state, ComponentIndex);
        lua_setfield(state, -2, "__index");
        lua_pushcfunction(state, ComponentNewIndex);
        lua_setfield(state, -2, "__newindex");
        lua_pop(state, 1);

        // Root Ignite table
        lua_newtable(state);

        lua_pushcfunction(state, IgniteLog); lua_setfield(state, -2, "Log");
        lua_pushcfunction(state, IgniteLogWarn); lua_setfield(state, -2, "LogWarn");
        lua_pushcfunction(state, IgniteLogError); lua_setfield(state, -2, "LogError");

        lua_pushcfunction(state, LuaVec2); lua_setfield(state, -2, "Vec2");
        lua_pushcfunction(state, LuaVec3); lua_setfield(state, -2, "Vec3");
        lua_pushcfunction(state, LuaVec4); lua_setfield(state, -2, "Vec4");

        RegisterSceneClosure(state, scene, "FindEntity", IgniteFindEntity);
        RegisterSceneClosure(state, scene, "FindEntityByName", IgniteFindEntityByName);
        RegisterSceneClosure(state, scene, "CreateEntity", IgniteCreateEntity);
        RegisterSceneClosure(state, scene, "CreatePrimitive", IgniteCreatePrimitive);
        RegisterSceneClosure(state, scene, "DestroyEntity", IgniteDestroyEntity);
        RegisterSceneClosure(state, scene, "DuplicateEntity", IgniteDuplicateEntity);
        RegisterSceneClosure(state, scene, "GetEntitiesWith", IgniteGetEntitiesWith);
        RegisterSceneClosure(state, scene, "InstantiatePrefab", IgniteInstantiatePrefab);

        // Ignite.Math
        lua_newtable(state);
        lua_pushcfunction(state, MathLerp); lua_setfield(state, -2, "Lerp");
        lua_pushcfunction(state, MathClamp); lua_setfield(state, -2, "Clamp");
        lua_pushcfunction(state, MathSmoothstep); lua_setfield(state, -2, "Smoothstep");
        lua_pushcfunction(state, MathDeg2Rad); lua_setfield(state, -2, "Deg2Rad");
        lua_pushcfunction(state, MathRad2Deg); lua_setfield(state, -2, "Rad2Deg");
        lua_pushcfunction(state, VecDot); lua_setfield(state, -2, "Dot");
        lua_pushcfunction(state, VecCross); lua_setfield(state, -2, "Cross");
        lua_pushcfunction(state, VecDistance); lua_setfield(state, -2, "Distance");
        lua_pushcfunction(state, LuaVec2); lua_setfield(state, -2, "Vec2");
        lua_pushcfunction(state, LuaVec3); lua_setfield(state, -2, "Vec3");
        lua_pushcfunction(state, LuaVec4); lua_setfield(state, -2, "Vec4");
        lua_setfield(state, -2, "Math");

        // Ignite.Scene
        lua_newtable(state);
        RegisterSceneClosure(state, scene, "Load", LuaSceneLoad);
        RegisterSceneClosure(state, scene, "GetTime", LuaSceneGetTime);
        RegisterSceneClosure(state, scene, "IsPlaying", LuaSceneIsPlaying);
        RegisterSceneClosure(state, scene, "IsSimulating", LuaSceneIsSimulating);
        RegisterSceneClosure(state, scene, "IsPaused", LuaSceneIsPaused);
        RegisterSceneClosure(state, scene, "GetPrimaryCamera", LuaSceneGetPrimaryCamera);
        lua_setfield(state, -2, "Scene");

        // Ignite.Physics
        lua_newtable(state);
        RegisterSceneClosure(state, scene, "Raycast", LuaPhysicsRaycast);
        lua_setfield(state, -2, "Physics");

        // Ignite.Debug
        lua_newtable(state);
        RegisterSceneClosure(state, scene, "DrawTextWorld", LuaDebugDrawTextWorld);
        RegisterSceneClosure(state, scene, "DrawTextScreen", LuaDebugDrawTextScreen);
        RegisterSceneClosure(state, scene, "DrawLine", LuaDebugDrawLine);
        RegisterSceneClosure(state, scene, "SetShowEntityNames", LuaDebugSetShowEntityNames);
        RegisterSceneClosure(state, scene, "GetShowEntityNames", LuaDebugGetShowEntityNames);
        lua_setfield(state, -2, "Debug");

        // Ignite.Net
        lua_newtable(state);
        lua_pushcfunction(state, LuaNetIsConnected); lua_setfield(state, -2, "IsConnected");
        lua_pushcfunction(state, LuaNetIsServerRunning); lua_setfield(state, -2, "IsServerRunning");
        lua_pushcfunction(state, LuaNetIsConnecting); lua_setfield(state, -2, "IsConnecting");
        lua_pushcfunction(state, LuaNetGetClientId); lua_setfield(state, -2, "GetClientId");
        lua_pushcfunction(state, LuaNetStartServer); lua_setfield(state, -2, "StartServer");
        lua_pushcfunction(state, LuaNetStopServer); lua_setfield(state, -2, "StopServer");
        lua_pushcfunction(state, LuaNetConnect); lua_setfield(state, -2, "Connect");
        lua_pushcfunction(state, LuaNetDisconnect); lua_setfield(state, -2, "Disconnect");
        lua_pushcfunction(state, LuaNetSpawnEntity); lua_setfield(state, -2, "SpawnEntity");
        lua_pushcfunction(state, LuaNetSendInput); lua_setfield(state, -2, "SendInput");
        lua_pushcfunction(state, LuaNetSendChat); lua_setfield(state, -2, "SendChat");
        lua_pushcfunction(state, LuaNetGetStats); lua_setfield(state, -2, "GetStats");
        lua_pushcfunction(state, LuaNetGetServerTick); lua_setfield(state, -2, "GetServerTick");
        lua_pushcfunction(state, LuaNetGetClientServerTick); lua_setfield(state, -2, "GetClientServerTick");
        lua_pushcfunction(state, LuaNetGetClientCount); lua_setfield(state, -2, "GetClientCount");
        lua_setfield(state, -2, "Net");

        // Ignite.Input
        lua_newtable(state);
        lua_pushcfunction(state, LuaInputIsKeyPressed); lua_setfield(state, -2, "IsKeyPressed");
        lua_pushcfunction(state, LuaInputIsMouseButtonPressed); lua_setfield(state, -2, "IsMouseButtonPressed");
        lua_pushcfunction(state, LuaInputIsActionPressed); lua_setfield(state, -2, "IsActionPressed");
        lua_pushcfunction(state, LuaInputGetMousePosition); lua_setfield(state, -2, "GetMousePosition");
        lua_pushcfunction(state, LuaInputGetMouseDelta); lua_setfield(state, -2, "GetMouseDelta");
        lua_pushcfunction(state, LuaInputSetMousePosition); lua_setfield(state, -2, "SetMousePosition");
        lua_setfield(state, -2, "Input");

        // Ignite.Key
        lua_newtable(state);
        auto setKey = [&](const char *k, KeyCode v) { lua_pushinteger(state, static_cast<lua_Integer>(v)); lua_setfield(state, -2, k); };
        setKey("Space", Key::Space); setKey("Enter", Key::Enter); setKey("Escape", Key::Escape);
        setKey("Tab", Key::Tab); setKey("Backspace", Key::Backspace);
        setKey("Left", Key::Left); setKey("Right", Key::Right); setKey("Up", Key::Up); setKey("Down", Key::Down);
        setKey("LeftShift", IGNITE_KEY_LSHIFT); setKey("RightShift", IGNITE_KEY_RSHIFT);
        setKey("LeftControl", IGNITE_KEY_LCTRL); setKey("RightControl", IGNITE_KEY_RCTRL);
        setKey("LeftAlt", IGNITE_KEY_LALT); setKey("RightAlt", IGNITE_KEY_RALT);
        setKey("A", Key::A); setKey("B", Key::B); setKey("C", Key::C); setKey("D", Key::D);
        setKey("E", Key::E); setKey("F", Key::F); setKey("G", Key::G); setKey("H", Key::H);
        setKey("I", Key::I); setKey("J", Key::J); setKey("K", Key::K); setKey("L", Key::L);
        setKey("M", Key::M); setKey("N", Key::N); setKey("O", Key::O); setKey("P", Key::P);
        setKey("Q", Key::Q); setKey("R", Key::R); setKey("S", Key::S); setKey("T", Key::T);
        setKey("U", Key::U); setKey("V", Key::V); setKey("W", Key::W); setKey("X", Key::X);
        setKey("Y", Key::Y); setKey("Z", Key::Z);
        setKey("D0", Key::D0); setKey("D1", Key::D1); setKey("D2", Key::D2); setKey("D3", Key::D3);
        setKey("D4", Key::D4); setKey("D5", Key::D5); setKey("D6", Key::D6); setKey("D7", Key::D7);
        setKey("D8", Key::D8); setKey("D9", Key::D9);
        setKey("F1", Key::F1); setKey("F2", Key::F2); setKey("F3", Key::F3); setKey("F4", Key::F4);
        setKey("F5", Key::F5); setKey("F6", Key::F6); setKey("F7", Key::F7); setKey("F8", Key::F8);
        setKey("F9", Key::F9); setKey("F10", Key::F10); setKey("F11", Key::F11); setKey("F12", Key::F12);
        lua_setfield(state, -2, "Key");

        // Ignite.Mouse
        lua_newtable(state);
        lua_pushinteger(state, static_cast<lua_Integer>(Mouse::ButtonLeft)); lua_setfield(state, -2, "Left");
        lua_pushinteger(state, static_cast<lua_Integer>(Mouse::ButtonRight)); lua_setfield(state, -2, "Right");
        lua_pushinteger(state, static_cast<lua_Integer>(Mouse::ButtonMiddle)); lua_setfield(state, -2, "Middle");
        lua_setfield(state, -2, "Mouse");

        lua_setglobal(state, "Ignite");
    }
}
