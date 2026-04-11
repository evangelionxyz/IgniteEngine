// Copyright (c) 2025 Evangelion Manuhutu

#include "scene.hpp"
#include <entt/entt.hpp>

#include "ignite/audio/fmod_sound.hpp"

#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/renderer/renderer_2d.hpp"
#include "ignite/graphics/objects/environment.hpp"
#include "ignite/physics/2d/physics_2d.hpp"
#include "ignite/physics/jolt/jolt_physics.hpp"
#include "ignite/math/math.hpp"
#include "scene_manager.hpp"
#include "ignite/scripting/script_engine.hpp"

#include "entity.hpp"

#include "ignite/core/application.hpp"
#include "ignite/animation/skeleton.hpp"
#include "ignite/animation/animation_2d.hpp"
#include "ignite/animation/animator/animator_controller.hpp"
#include "ignite/animation/animator/animator_controller_2d.hpp"

#include "ignite/project/project.hpp"
#include "ignite/core/profiler/profiler.hpp"

#include <ranges>
#include <cmath>

namespace ignite
{
    Scene::Scene(Project *project, const std::string &_name)
		: m_Project(project), name(_name)
        , m_ViewportWidth(1280), m_ViewportHeight(720)
    {
        LOG_TRACE("Scene::Scene() - Creating scene: {0}", name);
        registry = new entt::registry();
        physics2D = CreateScope<Physics2D>(this);
        physics = CreateScope<JoltScene>(this);

        m_SceneGPUDataBuffer = ConstantBuffer::Create(sizeof(Scene_GPUData), false, 1, "[Scene GPU Data]");
		m_CSMGPUDataBuffer = ConstantBuffer::Create(sizeof(CascadedShadowMap_GPUData), false, 1, "[CSM GPU Data]");

		ScriptEngine::GetInstance()->SetSceneContext(this);
    }

    Scene::~Scene()
    {
		// Stop physics simulations first
		if (physics2D)
		{
			physics2D->SimulationStop();
		}
		if (physics)
		{
			physics->SimulationStop();
		}

		// Clear all entities from registry before deletion
        if (registry)
        {
			registry->clear();
            delete registry;
			registry = nullptr;
        }

		// Clear entity map
		entities.clear();

		// Release GPU buffers explicitly
		m_SceneGPUDataBuffer.reset();
		m_CSMGPUDataBuffer.reset();

		// Release physics systems
		physics2D.reset();
		physics.reset();
    }

    void Scene::OnStart()
    {
        m_IsPlaying = true;
        m_IsPaused = false;

		ScriptEngine::GetInstance()->SetSceneContext(this);

        // reset time
        timeInSeconds = 0.0f;

        // resize
        auto camView = registry->view<CameraComponent>();
        for (entt::entity entity : camView)
        {
            CameraComponent &cc = camView.get<CameraComponent>(entity);
            cc.camera.UpdateProjection(static_cast<float>(m_ViewportWidth), static_cast<float>(m_ViewportHeight));
        }

        // play on start audio
        auto audioView = registry->view<AudioSourceComponent>();
        for (entt::entity e : audioView)
        {
            AudioSourceComponent &as = audioView.get<AudioSourceComponent>(e);
            if (as.playOnStart)
            {
                Ref<FmodSound> sound = m_Project->GetAsset<FmodSound>(as.handle);
                if (sound)
                {
                    sound->Play();
                    sound->SetVolume(as.volume);
                    sound->SetPitch(as.pitch);
                    sound->SetPan(as.pan);
                }
            }
        }

        registry->view<ScriptComponent>().each([this](entt::entity e, ScriptComponent &script)
        {
            Entity entity{ e, this };
            ScriptEngine::GetInstance()->OnCreateEntity(entity);
        });

        physics2D->SimulationStart();
        physics->SimulationStart();
    }

    void Scene::OnStop()
    {
        m_IsPlaying = false;
        m_IsPaused = false;
        m_StepFrame = 0;

        timeInSeconds = 0.0f;

        // play on start audio
        auto audioView = registry->view<AudioSourceComponent>();
        for (entt::entity e : audioView)
        {
            AudioSourceComponent &as = audioView.get<AudioSourceComponent>(e);
            Ref<FmodSound> sound = m_Project->GetAsset<FmodSound>(as.handle);
            if (sound)
            {
                sound->Stop();
            }
        }

        ScriptEngine::GetInstance()->ClearSceneContext();
        
        physics2D->SimulationStop();
        physics->SimulationStop();
    }

	void Scene::Pause()
	{
        m_IsPaused = !m_IsPaused;
	}

	void Scene::Step(int frame)
	{
        m_StepFrame = frame;
	}

	void Scene::UpdateTransforms(float deltaTime)
    {
        IGN_PROFILE_FUNCTION();

        UpdateAnimations(deltaTime);

        auto view = registry->view<IDComponent, TransformComponent>();
        for (auto ent : view)
        {
            const auto &[id, transform] = view.get<IDComponent, TransformComponent>(ent);
            if (id.parent == 0)
            {
                UpdateTransformRecursive(Entity { ent, this }, glm::mat4(1.0f));
            }
        }

        auto cameraView = registry->view<TransformComponent, CameraComponent>();
        for (auto entity : cameraView)
        {
            auto &tr = cameraView.get<TransformComponent>(entity);
            auto &cc = cameraView.get<CameraComponent>(entity);
            cc.camera.SetTransform(tr.GetWorldMatrix());
        }
    }

    void Scene::UpdateTransformRecursive(Entity entity, const glm::mat4 &parentWorldTransform)
    {
        TransformComponent &transform = entity.GetTransform();
        IDComponent &id = entity.GetComponent<IDComponent>();

        glm::vec3 skew;
        glm::vec4 perspective;
        
        glm::mat4 worldMatrix = parentWorldTransform * transform.GetLocalMatrix();
        
        glm::decompose(worldMatrix,
            transform.scale,
            transform.rotation,
            transform.translation,
            skew,
            perspective);

        transform.dirty = false;

        for (const UUID &childUUID : id.children)
        {
            Entity child = SceneManager::GetEntity(this, childUUID);
            UpdateTransformRecursive(child, worldMatrix);
        }
    }

    void Scene::OnUpdateEdit(f32 deltaTime)
    {
        IGN_PROFILE_FUNCTION();
        timeInSeconds += deltaTime;
        m_StepFrame++;
    
        UpdateTransforms(deltaTime);
    }

    void Scene::Resize(uint32_t width, uint32_t height)
    {
        this->m_ViewportWidth = width;
        this->m_ViewportHeight = height;
    }

    void Scene::WriteBuffer(nvrhi::ICommandList *cmd)
    {
        Scene_GPUData mergedGPUData = gpuData;

        bool hasDirectionalLight = false;
        auto dirLightView = registry->view<TransformComponent, DirectionalLightComponent>();
        for (entt::entity e : dirLightView)
        {
            const TransformComponent &tr = dirLightView.get<TransformComponent>(e);
            const DirectionalLightComponent &light = dirLightView.get<DirectionalLightComponent>(e);

            const glm::vec3 forward = glm::normalize(tr.rotation * glm::vec3(0.0f, 0.0f, 1.0f));

            mergedGPUData.sunColor = glm::vec4(light.color.r, light.color.g, light.color.b, light.intensity);
            mergedGPUData.sungAngles.x = std::atan2(forward.x, forward.z);
            mergedGPUData.sungAngles.y = std::asin(glm::clamp(forward.y, -1.0f, 1.0f));
            mergedGPUData.sunAngularRadius = glm::radians(light.angularRadius);
            mergedGPUData.exposure = light.exposure;
            mergedGPUData.gamma = light.gamma;
            mergedGPUData.ambient = light.ambient;

            hasDirectionalLight = true;
            break;
        }

        if (!hasDirectionalLight)
        {
            const Scene_GPUData *sceneGPUData = &gpuData;

            auto view = registry->view<WorldEnvironment>();
            WorldEnvironment *fallback = nullptr;
            for (entt::entity e : view)
            {
                WorldEnvironment &world = view.get<WorldEnvironment>(e);
                if (!world.enabled)
                    continue;

                if (world.primary)
                {
                    sceneGPUData = &world.sceneGPUData;
                    fallback = nullptr;
                    break;
                }

                if (!fallback)
                {
                    fallback = &world;
                }
            }

            if (fallback)
            {
                sceneGPUData = &fallback->sceneGPUData;
            }

            mergedGPUData.sunColor = sceneGPUData->sunColor;
            mergedGPUData.sungAngles = sceneGPUData->sungAngles;
            mergedGPUData.sunAngularRadius = sceneGPUData->sunAngularRadius;
            mergedGPUData.exposure = sceneGPUData->exposure;
            mergedGPUData.gamma = sceneGPUData->gamma;
            mergedGPUData.ambient = sceneGPUData->ambient;
        }

        gpuData = mergedGPUData;
        m_SceneGPUDataBuffer->SetData(cmd, Buffer((void *)&gpuData, sizeof(Scene_GPUData)));
    }

    Entity Scene::GetPrimaryCamera()
    {
        auto camView = registry->view<CameraComponent>();
        for (entt::entity entity : camView)
        {
            CameraComponent &cam = camView.get<CameraComponent>(entity);
            if (cam.primary)
                return Entity { entity, this };
        }
        
        return Entity{};
    }

    void Scene::OnUpdateRuntimeSimulate(f32 deltaTime)
    {
        IGN_PROFILE_FUNCTION();
        if (!m_IsPaused || m_StepFrame-- > 0)
        {
            IGN_PROFILE_SCOPE("Scene::RuntimeTick");
            timeInSeconds += deltaTime;

            {
                IGN_PROFILE_SCOPE("Scene::ScriptUpdate");
                registry->view<ScriptComponent>().each([this, deltaTime](entt::entity e, ScriptComponent &sc)
                {
                    IGN_PROFILE_SCOPE("Scene::ScriptUpdateEntity");
                    Entity entity { e, this };
                    ScriptEngine::GetInstance()->OnUpdateEntity(entity, deltaTime);
                });
            }

            UpdateTransforms(deltaTime);

            {
                IGN_PROFILE_SCOPE("Scene::Physics2D");
                physics2D->Simulate(deltaTime);
            }

            {
                IGN_PROFILE_SCOPE("Scene::Physics3D");
                physics->Simulate(deltaTime);
            }
        }
    }

    Ref<Scene> Scene::Create(Project *project, const std::string &name)
    {
        return CreateRef<Scene>(project, name);
    }

    Environment *Scene::GetEnvironment()
    {
        auto view = registry->view<WorldEnvironment>();
        WorldEnvironment *fallback = nullptr;
        for (entt::entity e : view)
        {
            WorldEnvironment &we = registry->get<WorldEnvironment>(e);
            if (!we.enabled || !we.environment)
                continue;

            if (we.primary)
            {
                return we.environment.get();
            }

            if (!fallback)
            {
                fallback = &we;
            }
        }

        return fallback ? fallback->environment.get() : nullptr;
    }

    void Scene::UpdateAnimations(float deltaTime)
    {
        // ---------------------------------------------------------------
        // Animator update: state machine driven skeletal animation
        // ---------------------------------------------------------------
        auto assetManager = m_Project->GetAssetManager();
        
        auto skeletalMeshView = registry->view<SkeletalMeshComponent>();
        for (auto ent : skeletalMeshView)
        {
            SkeletalMeshComponent &sm = skeletalMeshView.get<SkeletalMeshComponent>(ent);
            if (sm.handle == AssetHandle(0) || sm.animatorHandle == AssetHandle(0))
                continue;

            Ref<SkeletalMesh> skeletalMesh = std::dynamic_pointer_cast<SkeletalMesh>(assetManager->GetAsset(sm.handle, AssetType::SkeletalMesh));
            if (!skeletalMesh)
                continue;

            Ref<AnimatorController> animController = std::dynamic_pointer_cast<AnimatorController>(assetManager->GetAsset(sm.animatorHandle));
            if (!animController)
                continue;

            AnimatorControllerRuntime runtime;
            runtime.currentStateName = sm.currentStateName;
            runtime.stateElapsed = sm.stateElapsed;
            runtime.stateNormalized = 0.0f;

            if (animController->UpdateSkeleton(deltaTime, runtime, assetManager, skeletalMesh->boneTransforms))
            {
                sm.currentStateName = runtime.currentStateName;
                sm.stateElapsed = runtime.stateElapsed;
                sm.stateNormalized = runtime.stateNormalized;
            }
        }

        // ---------------------------------------------------------------
        // Animator2D update: state machine driven 2D animation
        // ---------------------------------------------------------------
        if (m_Project)
        {
            auto animator2dView = registry->view<Sprite2DComponent, Animator2DComponent>();
            for (entt::entity e : animator2dView)
            {
                auto &sprite = animator2dView.get<Sprite2DComponent>(e);
                auto &animComp = animator2dView.get<Animator2DComponent>(e);

                if (animComp.controllerHandle == AssetHandle(0))
                    continue;

                Ref<AnimatorController2D> ctrl = m_Project->GetAsset<AnimatorController2D>(animComp.controllerHandle);
                if (!ctrl)
                {
                    continue;
                }

                // Initialize state if empty
                if (animComp.currentStateName.empty())
                {
                    animComp.currentStateName = ctrl->defaultState;
                    animComp.currentFrame = 0;
                    animComp.elapsed = 0.0f;
                    animComp.stateElapsed = 0.0f;
                    animComp.stateNormalized = 0.0f;
                }

                const AnimState2D *state = ctrl->FindState(animComp.currentStateName);
                if (!state || state->animHandle == AssetHandle(0))
                    continue;

                Ref<Animation2D> anim = m_Project->GetAsset<Animation2D>(state->animHandle);
                if (!anim || anim->frames.empty())
                {
                    continue;
                }

                // Advance per-entity frame counter
                const float frameDuration = (anim->fps > 0.0f) ? (1.0f / anim->fps) : 1.0f;
                animComp.elapsed += deltaTime;
                animComp.stateElapsed += deltaTime;

                while (animComp.elapsed >= frameDuration)
                {
                    animComp.elapsed -= frameDuration;
                    animComp.currentFrame++;
                    if (animComp.currentFrame >= static_cast<int>(anim->frames.size()))
                    {
                        if (anim->loop)
                        {
                            animComp.currentFrame = 0;
                        }
                        else
                        {
                            animComp.currentFrame = static_cast<int>(anim->frames.size()) - 1;
                            animComp.elapsed = 0.0f;
                            break;
                        }
                    }
                }

                // Update normalized time
                const float totalDur = static_cast<float>(anim->frames.size()) * frameDuration;
                animComp.stateNormalized = (totalDur > 0.0f) ? std::min(animComp.stateElapsed / totalDur, 1.0f) : 0.0f;

                // Push UV to sprite
                const int clamped = std::max(0, std::min(animComp.currentFrame, static_cast<int>(anim->frames.size()) - 1));
                sprite.uv0 = anim->frames[static_cast<size_t>(clamped)].uv0;
                sprite.uv1 = anim->frames[static_cast<size_t>(clamped)].uv1;
                if (anim->textureHandle != AssetHandle(0))
                {
                    sprite.handle = anim->textureHandle;
                }

                // Evaluate transitions
                std::string nextState = ctrl->EvaluateTransitions(animComp.currentStateName, animComp.stateNormalized);
                if (!nextState.empty() && nextState != animComp.currentStateName)
                {
                    animComp.currentStateName = nextState;
                    animComp.currentFrame = 0;
                    animComp.elapsed = 0.0f;
                    animComp.stateElapsed = 0.0f;
                    animComp.stateNormalized = 0.0f;
                }
            }
        }
    }

    template<typename T>
    void Scene::OnComponentAdded(Entity entity, T &comp)
    {
    }

    template<>
    void Scene::OnComponentAdded<IDComponent>(Entity entity, IDComponent &comp)
    {
    }

    template<>
    void Scene::OnComponentAdded<TransformComponent>(Entity entity, TransformComponent &comp)
    {
    }

    template<>
    void Scene::OnComponentAdded<Sprite2DComponent>(Entity entity, Sprite2DComponent &comp)
    {
    }

	template<>
	void Scene::OnComponentAdded<Circle2DComponent>(Entity entity, Circle2DComponent &comp)
	{
	}

    template<>
    void Scene::OnComponentAdded<PointLight2DComponent>(Entity entity, PointLight2DComponent &comp)
    {
    }

    template<>
    void Scene::OnComponentAdded<DirectionalLightComponent>(Entity entity, DirectionalLightComponent &comp)
    {
    }

    template<>
    void Scene::OnComponentAdded<Rigidbody2DComponent>(Entity entity, Rigidbody2DComponent &comp)
    {
    }

    template<>
    void Scene::OnComponentAdded<BoxCollider2DComponent>(Entity entity, BoxCollider2DComponent &comp)
    {
    }

	template<>
	void Scene::OnComponentAdded<CircleCollider2DComponent>(Entity entity, CircleCollider2DComponent &comp)
	{
	}

    template<>
    void Scene::OnComponentAdded<RigibodyComponent>(Entity entity, RigibodyComponent &comp)
    {
    }

    template<>
    void Scene::OnComponentAdded<BoxColliderComponent>(Entity entity, BoxColliderComponent &comp)
    {
    }

    template<>
    void Scene::OnComponentAdded<SphereColliderComponent>(Entity entity, SphereColliderComponent &comp)
    {
    }

    template<>
    void Scene::OnComponentAdded<CapsuleColliderComponent>(Entity entity, CapsuleColliderComponent &comp)
    {
    }

    template<>
    void Scene::OnComponentAdded<MeshColliderComponent>(Entity entity, MeshColliderComponent &comp)
    {
    }

    template<>
    void Scene::OnComponentAdded<AudioSourceComponent>(Entity entity, AudioSourceComponent &comp)
    {
    }

	template<>
	void Scene::OnComponentAdded<TextComponent>(Entity entity, TextComponent &comp)
	{
	}

    template<>
    void Scene::OnComponentAdded<ScriptComponent>(Entity entity, ScriptComponent &comp)
    {
    }

    template<>
    void Scene::OnComponentAdded<Animator2DComponent>(Entity entity, Animator2DComponent &comp)
    {
        // Will initialize from controller's default state on first update tick
        comp.currentStateName = "";
        comp.currentFrame     = 0;
        comp.elapsed          = 0.0f;
        comp.stateElapsed     = 0.0f;
        comp.stateNormalized  = 0.0f;
    }

    template<>
    void Scene::OnComponentAdded<WorldEnvironment>(Entity entity, WorldEnvironment &comp)
    {
        if (!comp.environment)
        {
            comp.environment = Environment::Create(this);
        }

        comp.sceneGPUData = gpuData;
        comp.dirtyEnvironment = true;
        comp.gpuInitialized = false;
        comp.loadedHDRHandle = AssetHandle(0);
    }

    template<>
    void Scene::OnComponentAdded<StaticMeshComponent>(Entity entity, StaticMeshComponent& comp)
    {
    }

	template<>
	void Scene::OnComponentAdded<SkeletalMeshComponent>(Entity entity, SkeletalMeshComponent& comp)
	{
       comp.currentStateName.clear();
        comp.stateElapsed = 0.0f;
        comp.stateNormalized = 0.0f;
	}

    template<>
    void Scene::OnComponentAdded<CameraComponent>(Entity entity, CameraComponent &comp)
    {
        comp.camera.UpdateProjection(static_cast<float>(m_ViewportWidth), static_cast<float>(m_ViewportHeight));
    }
}
