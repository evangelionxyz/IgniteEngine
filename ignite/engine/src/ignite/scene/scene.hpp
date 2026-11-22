// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_SCENE_HPP
#define IGN_SCENE_HPP

#define GLM_ENABLE_EXPERIMENTAL
#include <entt/entt.hpp>

#include "ignite/core/types.hpp"
#include "ignite/core/uuid.hpp"
#include "ignite/animation/animator/animator_controller.hpp"
#include "ignite/core/signal_bus.hpp"
#include "ignite/core/signals/asset_signal.hpp"
#include "ignite/asset/asset.hpp"

#include <unordered_map>
#include <unordered_set>

namespace ignite::physics
{
    class Physics2D;
    class Physics3D;
}

namespace ignite::netproto
{
    struct ComponentSyncMsg;
}

namespace ignite
{
    class CameraComponent;
    class Entity;
    class Environment;
    class WorldEnvironment;
    class SceneRenderer;
    class Project;
    class WidgetCanvas;

    namespace lua
    {
        class ScriptRuntime;
    }

    enum class ESceneState : uint8_t
    {
        None = 0,
        Stop = 1,
        Play = 2,
        Simulate = 3,
        Paused = 4,
    };

    enum class ENetMode : uint8_t
    {
        Standalone = 0,
        DedicatedServer,
        Client,
    };

    struct DebugTextWorldItem
    {
        glm::vec3 position = glm::vec3(0.0f);
        std::string text;
        glm::vec4 color = glm::vec4(1.0f);
        float scale = 0.35f;
        bool billboard = true;
        bool center = true;
        bool depthTest = true;
    };

    struct DebugTextScreenItem
    {
        glm::vec2 position = glm::vec2(0.0f);
        std::string text;
        glm::vec4 color = glm::vec4(1.0f);
        float fontSize = 20.0f;
        bool center = false;
    };

    struct DebugLineItem
    {
        glm::vec3 p0 = glm::vec3(0.0f);
        glm::vec3 p1 = glm::vec3(0.0f);
        glm::vec4 color = glm::vec4(0.2f, 0.9f, 0.4f, 1.0f);
    };

    struct SceneAssetLoadProgress
    {
        size_t totalAssets = 0;
        size_t readyAssets = 0;
        size_t pendingAssets = 0;
        bool allReady = true;
        float progress = 1.0f;
    };

    class IGN_API Scene final : public Asset
    {
    public:
        Scene();
        explicit Scene(Project *project);

        ~Scene();

        void OnStart(ESceneState playOrSimulateState);
        void OnStop();

        void Pause();
        void Resume();
        void StepFrame(int frame);

        void UpdateTransforms(float deltaTime);
        void UpdateTransformRecursive(Entity entity, const glm::mat4 &parentWorldTransform);

        void OnUpdateRuntimeSimulate(float deltaTime);
        void OnFixedUpdateRuntimeSimulate() const;

        void OnUpdateEdit(float deltaTime);
        void OnFixedUpdateEdit();

        void SetSceneRenderer(SceneRenderer *sceneRenderer) { m_SceneRenderer = sceneRenderer; }

        const ESceneState &GetState() const { return m_State; }

        void SetNetMode(ENetMode mode) { m_NetMode = mode; }
        ENetMode GetNetMode() const { return m_NetMode; }
        bool IsClient() const { return m_NetMode == ENetMode::Client; }
        bool IsServer() const { return m_NetMode == ENetMode::DedicatedServer; }

        Entity FindEntityByNetId(uint64_t netId);
        void RegisterNetEntity(uint64_t netId, Entity entity);
        void UnregisterNetEntity(uint64_t netId);

        // Network Component Synchronization
        void SerializeEntityComponents(Entity entity, uint32_t compMask, netproto::ComponentSyncMsg &outMsg) const;
        void ApplyEntityComponentSync(Entity entity, const netproto::ComponentSyncMsg &msg);
        uint32_t GetAvailableComponentMask(Entity entity) const;

        template<typename T>
        void OnComponentAdded(Entity entity, T &comp);

        Entity GetPrimaryCamera();
        inline Project *GetProject() { return m_Project; }
        lua::ScriptRuntime * GetLuaScriptRuntime() const { return m_LuaScriptRuntime.get(); }
        inline AssetManager *GetAssetManager() { return m_AssetManager; }

        Ref<WidgetCanvas> GetRootWidget() const;

        entt::registry *registry;
        std::unordered_map<UUID, entt::entity> entities; // uuid to entity
        std::vector<UUID> rootEntityOrder;

        inline bool IsPaused() const { return m_State == ESceneState::Paused; }
		inline bool IsStopped() const { return m_State == ESceneState::Stop; }
        inline bool IsSimulating() const { return m_State == ESceneState::Simulate; }
		inline bool IsPlaying() const { return m_State == ESceneState::Play; }
        inline bool IsRunning() const { return m_State == ESceneState::Play || m_State == ESceneState::Simulate || m_State == ESceneState::Play; }

        static Ref<Scene> Create(Project *project);

        SceneRenderer *GetSceneRenderer() { return m_SceneRenderer; }
        Environment *GetEnvironment() const;
        WorldEnvironment *GetActiveWorldEnvironment() const;
		physics::Physics2D *GetPhysics2D() { return m_Physics2D; }
        physics::Physics3D *GetPhysics3D() { return m_Physics3D; }

        void CreatePhysics2DBody(Entity entity) const;
        void DestroyPhysics2DBody(Entity entity) const;

        std::unordered_set<AssetHandle> CollectReferencedAssetHandles() const;
        void PreloadReferencedAssets();
        SceneAssetLoadProgress GetAssetLoadProgress() const;
        bool IsReadyToPlay() const;

        float timeInSeconds = 0.0f;
        bool showDebugEntityNames = false;

        void AddDebugTextWorld(const DebugTextWorldItem &item) { m_DebugWorldTexts.push_back(item); }
        void AddDebugTextScreen(const DebugTextScreenItem &item) { m_DebugScreenTexts.push_back(item); }
        void AddDebugLine(const DebugLineItem &item) { m_DebugLines.push_back(item); }
        void ClearDebugDraw()
        {
            m_DebugWorldTexts.clear();
            m_DebugScreenTexts.clear();
            m_DebugLines.clear();
        }
        const std::vector<DebugTextWorldItem> &GetDebugWorldTexts() const { return m_DebugWorldTexts; }
        const std::vector<DebugTextScreenItem> &GetDebugScreenTexts() const { return m_DebugScreenTexts; }
        const std::vector<DebugLineItem> &GetDebugLines() const { return m_DebugLines; }

        static AssetType GetStaticType() { return AssetType::Scene; }
        virtual AssetType GetAssetType() override { return GetStaticType(); }

    private:
        void OnAssetChangeSignal(const AssetChangeSignal &signal);
        void UpdateAnimations(float deltaTime);

    private:
        SceneRenderer *m_SceneRenderer;

        Project *m_Project;
        AssetManager *m_AssetManager;
        physics::Physics2D *m_Physics2D;
        physics::Physics3D *m_Physics3D;
        Scope<lua::ScriptRuntime> m_LuaScriptRuntime;

        uint32_t m_ViewportWidth;
        uint32_t m_ViewportHeight;
        uint64_t m_StepFrame = 0;
        SignalToken m_AssetChangeToken = kInvalidSignalToken;
        ESceneState m_State = ESceneState::Stop;
        ESceneState m_PreviousState = ESceneState::Stop;

        std::unordered_map<AssetHandle, Ref<Asset>> m_LoadedAssets;
        std::unordered_map<AssetHandle, Ref<AnimatorController>> m_SharedAnimatorCache;
        std::unordered_map<AssetHandle, AnimatorControllerRuntime> m_SharedAnimatorRuntime;
        ENetMode m_NetMode = ENetMode::Standalone;
        std::unordered_map<uint64_t, entt::entity> m_NetEntities;

        std::vector<DebugTextWorldItem> m_DebugWorldTexts;
        std::vector<DebugTextScreenItem> m_DebugScreenTexts;
        std::vector<DebugLineItem> m_DebugLines;

        friend class SceneManager;
    };
}

#endif
