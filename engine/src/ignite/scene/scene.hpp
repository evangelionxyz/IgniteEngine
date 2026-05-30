// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef SCENE_HPP
#define SCENE_HPP

#define GLM_ENABLE_EXPERIMENTAL
#include <entt/entt.hpp>

#include "ignite/core/types.hpp"
#include "ignite/core/uuid.hpp"
#include "ignite/asset/asset.hpp"
#include "ignite/math/aabb.hpp"

#include "ignite/animation/animator/animator_controller.hpp"

#include "ignite/graphics/buffers/constant_buffer.hpp"
#include "ignite/graphics/gpu_data.hpp"

#include <unordered_map>
#include <unordered_set>

namespace ignite
{
    class CameraComponent;
    class Physics2D;
    class JoltScene;
    class Entity;
    class Environment;
    class SceneRenderer;
    class Project;
    class WidgetCanvas;

    class Scene : public Asset
    {
    public:
        Scene() = default;
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

        template<typename T>
        void OnComponentAdded(Entity entity, T &comp);

        Entity GetPrimaryCamera();
        Project *GetProject() { return m_Project; }
        AssetManager *GetAssetManager() { return m_AssetManager; }

        Ref<WidgetCanvas> GetRootWidget();

        std::string name;
        entt::registry *registry;
        Scope<Physics2D> physics2D;
        Scope<JoltScene> physics;
        std::unordered_map<UUID, entt::entity> entities; // uuid to entity
        
		inline bool IsPaused() const { return m_IsPaused; }
        inline bool IsRunning() const { return m_IsPlaying; }
        inline bool IsFocusing() const { return m_IsFocusing; }

        void Focus();
        void Unfocus();
        
        static Ref<Scene> Create(Project *project, const std::string &name);
        
        SceneRenderer *GetSceneRenderer() { return m_SceneRenderer; }
        Environment *GetEnvironment();
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
        
        bool m_IsPaused = false;
        bool m_IsPlaying = false;
        bool m_IsFocusing = false;

        std::unordered_map<AssetHandle, Ref<AnimatorController>> m_SharedAnimatorCache;
        std::unordered_map<AssetHandle, AnimatorControllerRuntime> m_SharedAnimatorRuntime;

        friend class SceneManager;
    };
}

#endif
