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

namespace ignite
{
    class CameraComponent;
    class Entity;
    class Environment;
    class WorldEnvironment;
    class SceneRenderer;
    class Project;
    class WidgetCanvas;

    enum class ESceneState : uint8_t
    {
        None = 0,
        Stop = 1,
        Play = 2,
        Simulate = 3,
        Paused = 4,
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
        void Step(int frame);

        void UpdateTransforms(float deltaTime);
        void UpdateTransformRecursive(Entity entity, const glm::mat4 &parentWorldTransform);

        void OnUpdateRuntimeSimulate(float deltaTime);
        void OnFixedUpdateRuntimeSimulate() const;

        void OnUpdateEdit(float deltaTime);
        void OnFixedUpdateEdit();

        void SetSceneRenderer(SceneRenderer *sceneRenderer) { m_SceneRenderer = sceneRenderer; }

        const ESceneState &GetState() const { return m_State; }

        template<typename T>
        void OnComponentAdded(Entity entity, T &comp);

        Entity GetPrimaryCamera();
        inline Project *GetProject() { return m_Project; }
        inline AssetManager *GetAssetManager() { return m_AssetManager; }

        Ref<WidgetCanvas> GetRootWidget() const;

        entt::registry *registry;
        std::unordered_map<UUID, entt::entity> entities; // uuid to entity

        inline bool IsPaused() const { return m_State == ESceneState::Paused; }
		inline bool IsStopped() const { return m_State == ESceneState::Stop; }
        inline bool IsSimulating() const { return m_State == ESceneState::Simulate; }
		inline bool IsPlaying() const { return m_State == ESceneState::Play; }
        inline bool IsRunning() const { return m_State == ESceneState::Play || m_State == ESceneState::Simulate; }

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

        float timeInSeconds = 0.0f;

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

        uint32_t m_ViewportWidth;
        uint32_t m_ViewportHeight;
        uint64_t m_StepFrame = 0;
        SignalToken m_AssetChangeToken = kInvalidSignalToken;
        ESceneState m_State = ESceneState::Stop;

        std::unordered_map<AssetHandle, Ref<Asset>> m_LoadedAssets;
        std::unordered_map<AssetHandle, Ref<AnimatorController>> m_SharedAnimatorCache;
        std::unordered_map<AssetHandle, AnimatorControllerRuntime> m_SharedAnimatorRuntime;

        friend class SceneManager;
    };
}

#endif
