// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_SCENE_HPP
#define IGN_SCENE_HPP

#define GLM_ENABLE_EXPERIMENTAL
#include <entt/entt.hpp>

#include "ignite/core/base.hpp"
#include "ignite/core/types.hpp"
#include "ignite/core/uuid.hpp"
#include "ignite/asset/asset.hpp"
#include "ignite/math/aabb.hpp"

#include "ignite/animation/animator/animator_controller.hpp"

#include "ignite/graphics/buffers/constant_buffer.hpp"
#include "ignite/graphics/gpu_data.hpp"

#include <unordered_map>
#include <unordered_set>
#include <type_traits>

namespace ignite
{
    class CameraComponent;
    class Physics2D;
    class JoltScene;
    class Entity;
    class Environment;
    class WorldEnvironment;
    class SceneRenderer;
    class Project;
    class WidgetCanvas;

    enum class ESceneState : uint8_t
    {
        None = 0,
        Stop = 1 << 0, // 0001
        Play = 1 << 1, // 0010
		Simulate = 1 << 2, // 0100
		Paused = 1 << 3, // 1000

        Focus = 1 << 4, // 1 0000
    };

    inline IGN_API ESceneState operator|(ESceneState lhs, ESceneState rhs)
    {
        using UnderlyingType = std::underlying_type_t<ESceneState>;
        return static_cast<ESceneState>(static_cast<UnderlyingType>(lhs) | static_cast<UnderlyingType>(rhs));
    }
	inline IGN_API ESceneState operator&(ESceneState lhs, ESceneState rhs)
    {
        using UnderlyingType = std::underlying_type_t<ESceneState>;
        return static_cast<ESceneState>(static_cast<UnderlyingType>(lhs) & static_cast<UnderlyingType>(rhs));
    }
    inline IGN_API ESceneState operator~(ESceneState flag)
    {
        using UnderlyingType = std::underlying_type_t<ESceneState>;
        return static_cast<ESceneState>(~static_cast<UnderlyingType>(flag));
    }
    inline IGN_API ESceneState& operator|=(ESceneState& lhs, ESceneState rhs)
    {
        lhs = lhs | rhs;
        return lhs;
    }
    inline IGN_API ESceneState &operator&=(ESceneState &lhs, ESceneState rhs)
	{
		lhs = lhs & rhs;
		return lhs;
	}

    class IGN_API Scene final : public Asset
    {
    public:
        Scene();
        explicit Scene(Project *project, const std::string &name);

        ~Scene();

        void OnStart();
        void OnStop();

        void Pause();
        void Step(int frame);

        void UpdateTransforms(float deltaTime);
        void UpdateTransformRecursive(Entity entity, const glm::mat4 &parentWorldTransform);
        
        void OnUpdateRuntimeSimulate(float deltaTime);
        void OnUpdateEdit(float deltaTime);
        void SetSceneRenderer(SceneRenderer *sceneRenderer) { m_SceneRenderer = sceneRenderer; }

        inline void SetStateFlag(ESceneState state) { m_State = state; }
        inline bool IsInState(ESceneState state) const { return (m_State & state) != ESceneState::None; }
        inline ESceneState GetStateFlag() const { return m_State; }

        template<typename T>
        void OnComponentAdded(Entity entity, T &comp);

        Entity GetPrimaryCamera();
        inline Project *GetProject() { return m_Project; }
        inline AssetManager *GetAssetManager() { return m_AssetManager; }

        Ref<WidgetCanvas> GetRootWidget();

        std::string name;
        entt::registry *registry;
        Scope<Physics2D> physics2D;
        Scope<JoltScene> physics;
        std::unordered_map<UUID, entt::entity> entities; // uuid to entity
        
		inline bool IsPaused() const { return IsInState(ESceneState::Paused); }
        inline bool IsRunning() const { return IsInState(ESceneState::Play); }
        inline bool IsFocus() const { return IsInState(ESceneState::Focus); }

        void Focus();
        void Unfocus();
        
        static Ref<Scene> Create(Project *project, const std::string &name);
        
        SceneRenderer *GetSceneRenderer() { return m_SceneRenderer; }
        Environment *GetEnvironment();
        WorldEnvironment *GetActiveWorldEnvironment();

        std::unordered_set<AssetHandle> CollectReferencedAssetHandles() const;

        glm::vec3 physicsGravity{ 0.0f, -9.8f, 0.0f };
        float timeInSeconds = 0.0f;

        static AssetType GetStaticType() { return AssetType::Scene; }
        virtual AssetType GetAssetType() override { return GetStaticType(); }

    private:
        void UpdateAnimations(float deltaTime);

    private:
        SceneRenderer *m_SceneRenderer;

        Project *m_Project;
        AssetManager *m_AssetManager;
        uint32_t m_ViewportWidth;
        uint32_t m_ViewportHeight;
		uint64_t m_StepFrame = 0;
        ESceneState m_State = ESceneState::Stop;
        
        std::unordered_map<AssetHandle, Ref<AnimatorController>> m_SharedAnimatorCache;
        std::unordered_map<AssetHandle, AnimatorControllerRuntime> m_SharedAnimatorRuntime;

        friend class SceneManager;
    };
}

#endif
