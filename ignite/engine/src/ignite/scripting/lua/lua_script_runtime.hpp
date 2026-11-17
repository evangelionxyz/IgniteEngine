// Copyright (c) 2026 Evangelion Manuhutu
#pragma once

#include "ignite/core/base.hpp"
#include "ignite/core/uuid.hpp"
#include "ignite/asset/asset_types.hpp"
#include "ignite/core/file_watcher.hpp"
#include "lua_field.hpp"

#include <glm/glm.hpp>
#include <filesystem>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct lua_State;

namespace ignite
{
    class Scene;
}

namespace ignite::lua
{
    class IGN_API ScriptRuntime
    {
    public:
        explicit ScriptRuntime(Scene *scene);
        ~ScriptRuntime();

        ScriptRuntime(const ScriptRuntime &) = delete;
        ScriptRuntime &operator=(const ScriptRuntime &) = delete;

        bool IsInitialized() const { return m_State != nullptr; }
        void CreateInstance(UUID entityId, const std::filesystem::path &scriptPath, AssetHandle scriptHandle = AssetHandle(0));
        void DestroyInstance(UUID entityId);
        bool ReloadInstance(UUID entityId);
        void ReloadScript(const std::filesystem::path &scriptPath);

        void Update(float deltaTime);
        void FixedUpdate(float fixedDeltaTime = 1.0f / 60.0f);
        void InvokeCollision(UUID entityId, UUID otherEntityId, const char *callback, const glm::vec3 &contactPoint, const glm::vec3 &contactNormal);
        void InvokeNetworkEvent(const char *callback, uint32_t clientId, const std::string &clientName);
        void InvokeNetworkInput(UUID entityId, uint32_t clientId, float moveX, float moveY, uint16_t buttons, float yaw, float deltaTime);

        bool ExecuteSnippet(const std::string &code, std::string &outResult, std::string &outError);

        bool SetFieldValue(UUID entityId, const std::string &fieldName, const LuaFieldValue &value);
        bool GetFieldValue(UUID entityId, const std::string &fieldName, LuaFieldValue &outValue) const;
        const std::vector<LuaFieldDesc> *GetInstanceFields(UUID entityId) const;

        void ProcessPendingReloads();

    private:
        struct Instance
        {
            AssetHandle handle = AssetHandle(0);
            std::filesystem::path path;
            int environmentRef = -2;
            int callbacksRef = -2;
            std::vector<LuaFieldDesc> fields;
        };

        void ApplyEntityFields(UUID entityId, Instance &instance);

        bool Invoke(UUID entityId, Instance &instance, const char *callback, float deltaTime, bool hasDeltaTime);
        void CreatePendingInstances();
        std::filesystem::path ResolvePath(const std::filesystem::path &path) const;

        void WatchScriptFile(const std::filesystem::path &absolutePath);
        void UnwatchUnusedFiles();

        Scene *m_Scene = nullptr;
        lua_State *m_State = nullptr;
        std::unordered_map<UUID, Instance> m_Instances;

        std::unordered_map<std::filesystem::path, FileWatchToken> m_WatchedFiles;
        std::unordered_set<std::filesystem::path> m_PendingReloads;
        std::mutex m_ReloadMutex;
    };
}
