// Copyright (c) 2026 Evangelion Manuhutu
#pragma once

#include "ignite/core/base.hpp"
#include "ignite/core/uuid.hpp"

#include <filesystem>
#include <memory>
#include <unordered_map>

struct lua_State;

namespace ignite
{
    class Scene;

    class IGN_API LuaScriptRuntime
    {
    public:
        explicit LuaScriptRuntime(Scene *scene);
        ~LuaScriptRuntime();

        LuaScriptRuntime(const LuaScriptRuntime &) = delete;
        LuaScriptRuntime &operator=(const LuaScriptRuntime &) = delete;

        bool IsInitialized() const { return m_State != nullptr; }
        void CreateInstance(UUID entityId, const std::filesystem::path &scriptPath);
        void DestroyInstance(UUID entityId);
        void Update(float deltaTime);
        void FixedUpdate();

    private:
        struct Instance
        {
            std::filesystem::path path;
            int environmentRef = -2;
            int callbacksRef = -2;
        };

        bool Invoke(UUID entityId, Instance &instance, const char *callback, float deltaTime, bool hasDeltaTime);
        std::filesystem::path ResolvePath(const std::filesystem::path &path) const;

        Scene *m_Scene = nullptr;
        lua_State *m_State = nullptr;
        std::unordered_map<UUID, Instance> m_Instances;
    };
}
