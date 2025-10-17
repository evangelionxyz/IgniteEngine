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
    {
        registry = new entt::registry();
        physics2D = CreateScope<Physics2D>(this);
        physics = CreateScope<JoltScene>(this);

        m_ConstantBuffer = ConstantBuffer::Create(sizeof(SceneParameters), false, 1, "[SceneParameters]");
    }

    Scene::~Scene()
    {
        if (registry)
            delete registry;
    }

    void Scene::OnStart()
    {
        m_Playing = true;

        ScriptEngine::GetInstance()->SetSceneContext(this);

        // reset time
        timeInSeconds = 0.0f;

        // resize
        auto camView = registry->view<Camera>();
        for (entt::entity entity : camView)
        {
            Camera &cam = camView.get<Camera>(entity);
			const float aspectRatio = static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight);
			cam.camera.UpdateMatrices(aspectRatio);
        }

        // play on start audio
        auto audioView = registry->view<AudioSource>();
        for (entt::entity e : audioView)
        {
            AudioSource &as = audioView.get<AudioSource>(e);
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

        registry->view<Script>().each([this](entt::entity e, Script &script)
        {
            Entity entity{ e, this };
            ScriptEngine::GetInstance()->OnCreateEntity(entity);
        });

        physics2D->SimulationStart();
        physics->SimulationStart();
    }

    void Scene::OnStop()
    {
        m_Playing = false;

        timeInSeconds = 0.0f;

        // play on start audio
        auto audioView = registry->view<AudioSource>();
        for (entt::entity e : audioView)
        {
            AudioSource &as = audioView.get<AudioSource>(e);
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

        auto view = registry->view<ID, Transform>();
        for (auto ent : view)
        {
            const auto &[id, transform] = view.get<ID, Transform>(ent);
            if (id.parent == 0)
            {
                UpdateTransformRecursive(Entity { ent, this }, glm::mat4(1.0f));
            }
        }
    }

    void Scene::UpdateTransformRecursive(Entity entity, const glm::mat4 &parentWorldTransform)
    {
        Transform &transform = entity.GetTransform();
        ID &id = entity.GetComponent<ID>();

        glm::vec3 skew;
        glm::vec4 perspective;
        
        glm::mat4 worldMatrix = parentWorldTransform * transform.GetLocalMatrix();
        
        glm::decompose(worldMatrix,
            transform.scale,
            transform.rotation,
            transform.translation,
            skew,
            perspective);

        if (entity.HasComponent<Camera>())
        {
            Camera &cam = entity.GetComponent<Camera>();
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
        
        UpdateTransforms(deltaTime);
    }

    void Scene::Resize(uint32_t width, uint32_t height)
    {
        this->viewportWidth = width;
        this->viewportHeight = height;
        
        auto camView = registry->view<Camera>();
        for (entt::entity entity : camView)
        {
            Camera &cam = camView.get<Camera>(entity);
			const float aspectRatio = static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight);
			cam.camera.UpdateMatrices(aspectRatio);
        }
    }

    void Scene::WriteBuffer(nvrhi::ICommandList* cmd)
    {
        m_ConstantBuffer->SetData(cmd, Buffer((void*)&this->params, sizeof(SceneParameters)));
    }

    Entity Scene::GetPrimaryCamera()
    {
        auto camView = registry->view<Camera>();
        for (entt::entity entity : camView)
        {
            Camera &cam = camView.get<Camera>(entity);
            if (cam.primary)
                return Entity { entity, this };
        }
        
        return Entity{};
    }

    void Scene::OnUpdateRuntimeSimulate(f32 deltaTime)
    {
        timeInSeconds += deltaTime;

        registry->view<Script>().each([this, deltaTime](entt::entity e, Script &sc)
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

    template<typename T>
    void Scene::OnComponentAdded(Entity entity, T &comp)
    {
    }

    template<>
    void Scene::OnComponentAdded<ID>(Entity entity, ID &comp)
    {
    }

    template<>
    void Scene::OnComponentAdded<Transform>(Entity entity, Transform &comp)
    {
    }

    template<>
    void Scene::OnComponentAdded<Sprite2D>(Entity entity, Sprite2D &comp)
    {
    }

    template<>
    void Scene::OnComponentAdded<SkeletalMesh>(Entity entity, SkeletalMesh &comp)
    {
    }

    template<>
    void Scene::OnComponentAdded<Rigidbody2D>(Entity entity, Rigidbody2D &comp)
    {
    }

    template<>
    void Scene::OnComponentAdded<BoxCollider2D>(Entity entity, BoxCollider2D &comp)
    {
    }

    template<>
    void Scene::OnComponentAdded<Rigibody>(Entity entity, Rigibody &comp)
    {
    }

    template<>
    void Scene::OnComponentAdded<BoxCollider>(Entity entity, BoxCollider &comp)
    {
    }

    template<>
    void Scene::OnComponentAdded<SphereCollider>(Entity entity, SphereCollider &comp)
    {
    }

    template<>
    void Scene::OnComponentAdded<CapsuleCollider>(Entity entity, CapsuleCollider &comp)
    {
    }

    template<>
    void Scene::OnComponentAdded<MeshCollider>(Entity entity, MeshCollider &comp)
    {
    }

    template<>
    void Scene::OnComponentAdded<AudioSource>(Entity entity, AudioSource &comp)
    {
    }

    template<>
    void Scene::OnComponentAdded<Script>(Entity entity, Script &comp)
    {
    }

    template<>
    void Scene::OnComponentAdded<WorldEnvironment>(Entity entity, WorldEnvironment &comp)
    {
    }

    template<>
    void Scene::OnComponentAdded<MeshComponent>(Entity entity, MeshComponent& comp)
    {
    }

    template<>
    void Scene::OnComponentAdded<Camera>(Entity entity, Camera &comp)
    {
    }
}
