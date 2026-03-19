/* MIT License
* 
* Copyright (c) 2025 Evangelion Manuhutu | IGNITE STUDIO
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#include "scene.hpp"
#include <entt/entt.hpp>

#include "ignite/audio/fmod_sound.hpp"

#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/renderer_2d.hpp"
#include "ignite/graphics/objects/environment.hpp"
#include "ignite/physics/2d/physics_2d.hpp"
#include "ignite/physics/jolt/jolt_physics.hpp"
#include "ignite/math/math.hpp"
#include "scene_manager.hpp"
#include "ignite/scripting/script_engine.hpp"

#include "entity.hpp"

#include "ignite/core/application.hpp"
#include "ignite/animation/skeleton.hpp"
#include "ignite/animation/animation_system.hpp"

#include "ignite/project/project.hpp"

#include <ranges>

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
            CameraComponent &cam = camView.get<CameraComponent>(entity);
			cam.camera.UpdateMatrices(static_cast<float>(m_ViewportWidth), static_cast<float>(m_ViewportHeight));
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
        m_IsPaused = true;
	}

	void Scene::Step(int frame)
	{

	}

	void Scene::UpdateTransforms(float deltaTime)
    {
#if 0
        auto skeletalMeshView = registry->view<SkeletalMesh>();
        for (auto entity : skeletalMeshView)
        {
            SkeletalMesh &sm = skeletalMeshView.get<SkeletalMesh>(entity);
            Ref<Skeleton> skeleton = m_Project->GetAsset<Skeleton>(sm.skeletonHandle);
            Ref<SkeletalAnimation> anim = m_Project->GetAsset<SkeletalAnimation>(sm.activeAnimationHandle);

            if (skeleton && anim && anim->isPlaying)
            {
                if (AnimationSystem::UpdateSkeleton(skeleton, anim, timeInSeconds))
                {
                    AnimationSystem::ApplySkeletonToEntities(this, skeleton);
                    sm.boneTransforms = AnimationSystem::GetFinalJointTransforms(skeleton);
                }
            }
            
            const size_t numBones = std::min(sm.boneTransforms.size(), static_cast<size_t>(MAX_BONES));
            for (auto &mesh : sm.meshes)
            {
                for (size_t i = 0; i < numBones; ++i)
                {
                    mesh->skinBuffer.boneTransforms[i] = sm.boneTransforms[i];
                }

                for (size_t i = numBones; i < MAX_BONES; ++i)
                {
                    mesh->skinBuffer.boneTransforms[i] = glm::mat4(1.0f);
                }
            }
        }
#endif

        auto view = registry->view<IDComponent, TransformComponent>();
        for (auto ent : view)
        {
            const auto &[id, transform] = view.get<IDComponent, TransformComponent>(ent);
            if (id.parent == 0)
            {
                UpdateTransformRecursive(Entity { ent, this }, glm::mat4(1.0f));
            }
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

        if (entity.HasComponent<CameraComponent>())
        {
            CameraComponent &cam = entity.GetComponent<CameraComponent>();
            if (cam.primary)
            {
                cam.camera.position = transform.translation;
                cam.camera.view = glm::translate(glm::mat4(1.0f), transform.translation) * glm::toMat4(transform.rotation);
                cam.camera.view = glm::inverse(cam.camera.view);
            }
        }
        
        transform.dirty = false;

        for (const UUID &childUUID : id.children)
        {
            Entity child = SceneManager::GetEntity(this, childUUID);
            UpdateTransformRecursive(child, worldMatrix);
        }
    }

    void Scene::OnUpdateEdit(f32 deltaTime)
    {
        timeInSeconds += deltaTime;
        m_StepFrame++;
        UpdateTransforms(deltaTime);
    }

    void Scene::Resize(uint32_t width, uint32_t height)
    {
        this->m_ViewportWidth = width;
        this->m_ViewportHeight = height;
        
        const auto &camView = registry->view<CameraComponent>();
        for (entt::entity entity : camView)
        {
            CameraComponent &cam = camView.get<CameraComponent>(entity);
			cam.camera.UpdateMatrices(static_cast<float>(width), static_cast<float>(height));
        }
    }

    void Scene::WriteBuffer(nvrhi::ICommandList* cmd)
    {
        m_SceneGPUDataBuffer->SetData(cmd, Buffer((void*)&this->gpuData, sizeof(Scene_GPUData)));
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
        timeInSeconds += deltaTime;

        registry->view<ScriptComponent>().each([this, deltaTime](entt::entity e, ScriptComponent &sc)
        {
            Entity entity{ e, this };
            ScriptEngine::GetInstance()->OnUpdateEntity(entity, deltaTime);
        });

        UpdateTransforms(deltaTime);

        physics2D->Simulate(deltaTime);
        physics->Simulate(deltaTime);
    }

    Ref<Scene> Scene::Create(Project *project, const std::string &name)
    {
        return CreateRef<Scene>(project, name);
    }

	Environment *Scene::GetEnvironment()
	{
        auto view = registry->view<WorldEnvironment>();
        for (entt::entity e : view)
        {
            WorldEnvironment &we = registry->get<WorldEnvironment>(e);
            if (we.environment)
                return we.environment.get();
        }

        return nullptr;
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
    void Scene::OnComponentAdded<Rigidbody2DComponent>(Entity entity, Rigidbody2DComponent &comp)
    {
    }

    template<>
    void Scene::OnComponentAdded<BoxCollider2DComponent>(Entity entity, BoxCollider2DComponent &comp)
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
    void Scene::OnComponentAdded<ScriptComponent>(Entity entity, ScriptComponent &comp)
    {
    }

    template<>
    void Scene::OnComponentAdded<WorldEnvironment>(Entity entity, WorldEnvironment &comp)
    {
    }

    template<>
    void Scene::OnComponentAdded<StaticMeshComponent>(Entity entity, StaticMeshComponent& comp)
    {
    }

	template<>
	void Scene::OnComponentAdded<SkeletalMeshComponent>(Entity entity, SkeletalMeshComponent& comp)
	{
	}

    template<>
    void Scene::OnComponentAdded<CameraComponent>(Entity entity, CameraComponent &comp)
    {
		auto camView = registry->view<CameraComponent>();
		for (entt::entity entity : camView)
		{
			CameraComponent &cam = camView.get<CameraComponent>(entity);
			cam.camera.UpdateMatrices(static_cast<float>(m_ViewportWidth), static_cast<float>(m_ViewportHeight));
		}
    }
}
