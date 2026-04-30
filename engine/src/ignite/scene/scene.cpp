// Copyright (c) 2025 Evangelion Manuhutu

#include "scene.hpp"
#include <entt/entt.hpp>

#include "ignite/audio/fmod_sound.hpp"
#include "ignite/audio/fmod_audio.hpp"

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
#include <algorithm>
#include <unordered_set>

namespace ignite
{
    namespace
    {
        Ref<AnimatorController> CloneAnimatorController(const Ref<AnimatorController> &source)
        {
            if (!source)
                return nullptr;

            return CreateRef<AnimatorController>(*source);
        }

        AnimParam *FindAnimParam(std::vector<AnimParam> &params, const std::string &name)
        {
            auto it = std::find_if(params.begin(), params.end(), [&name](const AnimParam &p) { return p.name == name; });
            return it != params.end() ? &(*it) : nullptr;
        }

        const AnimParam *FindAnimParam(const std::vector<AnimParam> &params, const std::string &name)
        {
            auto it = std::find_if(params.begin(), params.end(), [&name](const AnimParam &p) { return p.name == name; });
            return it != params.end() ? &(*it) : nullptr;
        }

        void SyncMeshAnimatorParams(MeshComponent &meshComp, const AnimatorController &controller)
        {
            std::erase_if(meshComp.runtimeParams, [&controller](const AnimParam &param)
            {
                return controller.GetParam(param.name) == nullptr;
            });

            for (const AnimParam &controllerParam : controller.params)
            {
                AnimParam *runtimeParam = FindAnimParam(meshComp.runtimeParams, controllerParam.name);
                if (!runtimeParam)
                {
                    meshComp.runtimeParams.push_back(controllerParam);
                    continue;
                }

                if (runtimeParam->type != controllerParam.type)
                {
                    *runtimeParam = controllerParam;
                }
            }
        }

        void ApplyMeshRuntimeParamsToController(const MeshComponent &meshComp, AnimatorController &controller)
        {
            for (AnimParam &controllerParam : controller.params)
            {
                if (const AnimParam *runtimeParam = FindAnimParam(meshComp.runtimeParams, controllerParam.name))
                {
                    controllerParam.type = runtimeParam->type;
                    controllerParam.floatVal = runtimeParam->floatVal;
                    controllerParam.intVal = runtimeParam->intVal;
                    controllerParam.boolVal = runtimeParam->boolVal;
                    controllerParam.strVal = runtimeParam->strVal;
                }
            }
        }

        void ResetMeshAnimatorRuntime(MeshComponent &meshComp)
        {
            meshComp.currentStateName.clear();
            meshComp.stateElapsed = 0.0f;
            meshComp.stateNormalized = 0.0f;
        }

        AssetHandle ResolveMeshAnimatorSourceHandle(const MeshComponent &meshComp, const Ref<Mesh> &mesh)
        {
            if (meshComp.runtimeAnimatorHandle != AssetHandle(0))
                return meshComp.runtimeAnimatorHandle;

            return mesh ? mesh->GetAnimatorHandle() : AssetHandle(0);
        }

        FMOD::DSP *CreateAudioSourceDsp(const AudioSourceComponent::DspSettings &settings)
        {
            FMOD::DSP *dsp = nullptr;
            FMOD_DSP_TYPE dspType = FMOD_DSP_TYPE_UNKNOWN;

            switch (settings.type)
            {
                case AudioSourceComponent::DspType::Reverb: dspType = FMOD_DSP_TYPE_SFXREVERB; break;
                case AudioSourceComponent::DspType::Distortion: dspType = FMOD_DSP_TYPE_DISTORTION; break;
                case AudioSourceComponent::DspType::Chorus: dspType = FMOD_DSP_TYPE_CHORUS; break;
                case AudioSourceComponent::DspType::Compressor: dspType = FMOD_DSP_TYPE_COMPRESSOR; break;
                case AudioSourceComponent::DspType::Delay: dspType = FMOD_DSP_TYPE_ECHO; break;
            }

            if (dspType == FMOD_DSP_TYPE_UNKNOWN)
            {
                return nullptr;
            }

            const FMOD_RESULT createResult = FmodAudio::GetFmodSystem()->createDSPByType(dspType, &dsp);
            if (createResult != FMOD_OK || !dsp)
            {
                return nullptr;
            }

            switch (settings.type)
            {
                case AudioSourceComponent::DspType::Reverb:
                    dsp->setParameterFloat(FMOD_DSP_SFXREVERB_DECAYTIME, settings.reverbDecayTime);
                    dsp->setParameterFloat(FMOD_DSP_SFXREVERB_EARLYDELAY, settings.reverbEarlyDelay);
                    dsp->setParameterFloat(FMOD_DSP_SFXREVERB_LATEDELAY, settings.reverbLateDelay);
                    dsp->setParameterFloat(FMOD_DSP_SFXREVERB_HFREFERENCE, settings.reverbHighFrequencyReference);
                    dsp->setParameterFloat(FMOD_DSP_SFXREVERB_DIFFUSION, settings.reverbDiffusion);
                    dsp->setParameterFloat(FMOD_DSP_SFXREVERB_DENSITY, settings.reverbDensity);
                    dsp->setParameterFloat(FMOD_DSP_SFXREVERB_LOWSHELFFREQUENCY, settings.reverbLowShelfGain);
                    dsp->setParameterFloat(FMOD_DSP_SFXREVERB_HIGHCUT, settings.reverbHighCut);
                    dsp->setParameterFloat(FMOD_DSP_SFXREVERB_DRYLEVEL, settings.reverbDryLevel);
                    dsp->setParameterFloat(FMOD_DSP_SFXREVERB_WETLEVEL, settings.reverbWetLevel);
                    break;
                case AudioSourceComponent::DspType::Distortion:
                    dsp->setParameterFloat(FMOD_DSP_DISTORTION_LEVEL, settings.distortionLevel);
                    break;
                case AudioSourceComponent::DspType::Chorus:
                    dsp->setParameterFloat(FMOD_DSP_CHORUS_MIX, settings.chorusMix);
                    dsp->setParameterFloat(FMOD_DSP_CHORUS_RATE, settings.chorusRate);
                    dsp->setParameterFloat(FMOD_DSP_CHORUS_DEPTH, settings.chorusDepth);
                    break;
                case AudioSourceComponent::DspType::Compressor:
                    dsp->setParameterFloat(FMOD_DSP_COMPRESSOR_THRESHOLD, settings.compressorThreshold);
                    dsp->setParameterFloat(FMOD_DSP_COMPRESSOR_RATIO, settings.compressorRatio);
                    dsp->setParameterFloat(FMOD_DSP_COMPRESSOR_RELEASE, settings.compressorRelease);
                    dsp->setParameterFloat(FMOD_DSP_COMPRESSOR_GAINMAKEUP, settings.compressorGainMakeup);
                    dsp->setParameterBool(FMOD_DSP_COMPRESSOR_USESIDECHAIN, settings.compressorUseSidechain);
                    break;
                case AudioSourceComponent::DspType::Delay:
                    dsp->setParameterFloat(FMOD_DSP_ECHO_DELAY, settings.delayMs);
                    dsp->setParameterFloat(FMOD_DSP_ECHO_FEEDBACK, settings.delayFeedback);
                    break;
            }

            dsp->setActive(settings.enabled);
            return dsp;
        }

        void RebuildAudioSourceDspChain(const AudioSourceComponent &audioSource, const Ref<FmodSound> &sound)
        {
            if (!sound)
            {
                return;
            }

            sound->ClearDsps(true);
            for (const auto &dspSettings : audioSource.dsps)
            {
                if (FMOD::DSP *dsp = CreateAudioSourceDsp(dspSettings))
                {
                    sound->AddDsp(dsp);
                }
            }
        }
    }

    Scene::Scene(Project *project, const std::string &_name)
		: m_Project(project), name(_name)
        , m_ViewportWidth(1280), m_ViewportHeight(720)
    {
        LOG_TRACE("Scene::Scene() - Creating scene: {0}", name);

        registry = new entt::registry();
        physics2D = CreateScope<Physics2D>(this);
        physics = CreateScope<JoltScene>(this);

		ScriptEngine::GetInstance()->SetSceneContext(this);

        m_AssetManager = m_Project->GetAssetManager();
    }

    Scene::~Scene()
    {
        m_AssetManager = nullptr;
        m_Project = nullptr;

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
            if (as.handle == AssetHandle(0))
                continue;

            Ref<FmodSound> sound = m_AssetManager->GetAsset<FmodSound>(as.handle);
            if (as.playOnStart)
            {
                if (sound)
                {
                    RebuildAudioSourceDspChain(as, sound);
                    sound->Play();
                    sound->SetVolume(as.volume);
                    sound->SetPitch(as.pitch);
                    sound->SetPan(as.pan);
                    sound->SetMode(as.loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);
                }
            }
        }

        registry->view<ScriptComponent>().each([this](entt::entity e, ScriptComponent &script)
        {
            Entity entity{ e, this };
            ScriptInstanceID instanceID = entity.GetUUID();
            script.runtimeScriptInstance = ScriptEngine::GetInstance()->OnCreateEntityInstance(instanceID, script.className);
        });

        registry->view<MeshComponent>().each([](entt::entity, MeshComponent &meshComp)
        {
            ResetMeshAnimatorRuntime(meshComp);
        });

        m_SharedAnimatorRuntime.clear();

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
            Ref<FmodSound> sound = m_AssetManager->GetAsset<FmodSound>(as.handle);
            if (sound)
            {
                sound->Stop();
            }
        }

        registry->view<ScriptComponent>().each([this](entt::entity e, ScriptComponent &script)
        {
            Entity entity { e, this };
            script.runtimeScriptInstance = nullptr;
            const ScriptInstanceID instanceID = entity.GetUUID();
            ScriptEngine::GetInstance()->OnDestroyEntityInstance(instanceID);
        });

        ScriptEngine::GetInstance()->ClearSceneContext();

        m_SharedAnimatorRuntime.clear();
        
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
                registry->view<ScriptComponent>().each([this, deltaTime](entt::entity e, ScriptComponent &script)
                {
                    if (script.runtimeScriptInstance)
                        script.runtimeScriptInstance->InvokeOnUpdate(deltaTime);
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

    std::unordered_set<AssetHandle> Scene::CollectReferencedAssetHandles() const
    {
        std::unordered_set<AssetHandle> handles;
        if (!registry)
        {
            return handles;
        }

        auto addHandle = [&handles](AssetHandle handle)
        {
            if (handle != AssetHandle(0))
            {
                handles.insert(handle);
            }
        };

        registry->view<WorldEnvironment>().each([&](entt::entity, const WorldEnvironment &env)
        {
            addHandle(env.hdrHandle);
            addHandle(env.loadedHDRHandle);
        });

        registry->view<Sprite2DComponent>().each([&](entt::entity, const Sprite2DComponent &sprite)
        {
            addHandle(sprite.handle);
            addHandle(sprite.materialHandle);
        });

        registry->view<TextComponent>().each([&](entt::entity, const TextComponent &text)
        {
            addHandle(text.fontHandle);
            addHandle(text.material2dHandle);
        });

        registry->view<WidgetComponent>().each([&](entt::entity, const WidgetComponent &widget)
        {
            addHandle(widget.widgetHandle);
        });

        registry->view<MeshComponent>().each([&](entt::entity, const MeshComponent &mesh)
        {
            addHandle(mesh.handle);
            addHandle(mesh.runtimeAnimatorHandle);
        });

        registry->view<AudioSourceComponent>().each([&](entt::entity, const AudioSourceComponent &audio)
        {
            addHandle(audio.handle);
        });

        registry->view<Animator2DComponent>().each([&](entt::entity, const Animator2DComponent &animator)
        {
            addHandle(animator.controllerHandle);
        });

        return handles;
    }

    void Scene::UpdateAnimations(float deltaTime)
    {
        auto skeletalMeshView = registry->view<TransformComponent, MeshComponent>();
        std::unordered_set<AssetHandle> updatedSharedHandles;
        for (auto ent : skeletalMeshView)
        {
            TransformComponent &tr = skeletalMeshView.get<TransformComponent>(ent);
            MeshComponent &sm = skeletalMeshView.get<MeshComponent>(ent);
            if (!tr.visible || sm.handle == AssetHandle(0))
                continue;

            Ref<Mesh> mesh = m_AssetManager->GetAsset<Mesh>(sm.handle);
            if (!mesh)
                continue;

            sm.worldMatrix = tr.GetWorldMatrix();
            sm.normalMatrix = glm::transpose(glm::inverse(glm::mat3(sm.worldMatrix)));

            if (mesh)
            {
                // Transform the mesh AABB by the entity's world matrix so it reflects runtime transforms
                const AABB &localAabb = mesh->aabb;
                const glm::vec3 corners[8] =
                {
                    { localAabb.min.x, localAabb.min.y, localAabb.min.z },
                    { localAabb.max.x, localAabb.min.y, localAabb.min.z },
                    { localAabb.min.x, localAabb.max.y, localAabb.min.z },
                    { localAabb.max.x, localAabb.max.y, localAabb.min.z },
                    { localAabb.min.x, localAabb.min.y, localAabb.max.z },
                    { localAabb.max.x, localAabb.min.y, localAabb.max.z },
                    { localAabb.min.x, localAabb.max.y, localAabb.max.z },
                    { localAabb.max.x, localAabb.max.y, localAabb.max.z },
                };

                sm.worldAABB.min = glm::vec3(std::numeric_limits<float>::max());
                sm.worldAABB.max = glm::vec3(std::numeric_limits<float>::lowest());

                const glm::mat4 &worldMat = sm.worldMatrix;
                for (const glm::vec3 &corner : corners)
                {
                    const glm::vec4 wc = worldMat * glm::vec4(corner, 1.0f);
                    sm.worldAABB.min = glm::min(sm.worldAABB.min, glm::vec3(wc));
                    sm.worldAABB.max = glm::max(sm.worldAABB.max, glm::vec3(wc));
                }
            }

            AssetHandle sourceAnimatorHandle = ResolveMeshAnimatorSourceHandle(sm, mesh);
            if (sourceAnimatorHandle == AssetHandle(0))
            {
                sm.runtimeAnimatorInstance.reset();
                sm.runtimeParams.clear();
                sm.skeletonGpuBuffer.reset();
                sm.finalBoneTransforms.clear();
                ResetMeshAnimatorRuntime(sm);
                continue;
            }

            sm.runtimeAnimatorHandle = sourceAnimatorHandle;

            Ref<AnimatorController> animController = nullptr;
            AnimatorControllerRuntime *sharedRuntime = nullptr;

            if (sm.uniqueAnimator)
            {
                if (!sm.runtimeAnimatorInstance)
                {
                    Ref<AnimatorController> sourceController = m_AssetManager->GetAsset<AnimatorController>(sourceAnimatorHandle);
                    if (sourceController)
                    {
                        sm.runtimeAnimatorInstance = CloneAnimatorController(sourceController);
                        if (sm.runtimeAnimatorInstance)
                        {
                            sm.runtimeParams.clear();
                            ResetMeshAnimatorRuntime(sm);
                        }
                    }
                }

                animController = sm.runtimeAnimatorInstance;
            }
            else
            {
                sm.runtimeAnimatorInstance.reset();

                auto sharedIt = m_SharedAnimatorCache.find(sourceAnimatorHandle);
                if (sharedIt == m_SharedAnimatorCache.end())
                {
                    Ref<AnimatorController> controller = m_AssetManager->GetAsset<AnimatorController>(sourceAnimatorHandle);
                    if (controller)
                    {
                        sharedIt = m_SharedAnimatorCache.emplace(sourceAnimatorHandle, controller).first;
                    }

                }

                animController = sharedIt->second;
                sharedRuntime = &m_SharedAnimatorRuntime[sourceAnimatorHandle];

                if (sharedRuntime->currentStateName.empty() && !sm.currentStateName.empty())
                {
                    sharedRuntime->currentStateName = sm.currentStateName;
                    sharedRuntime->stateElapsed = sm.stateElapsed;
                    sharedRuntime->stateNormalized = sm.stateNormalized;
                }
            }

            if (!animController)
                continue;

            SyncMeshAnimatorParams(sm, *animController);
            ApplyMeshRuntimeParamsToController(sm, *animController);

            if (sharedRuntime)
            {
                // Only advance time once per shared controller per frame
                if (updatedSharedHandles.find(sourceAnimatorHandle) == updatedSharedHandles.end())
                {
                    AnimatorControllerRuntime tmpRuntime = *sharedRuntime;
                    if (animController->UpdateSkeleton(deltaTime, tmpRuntime, m_AssetManager))
                    {
                        *sharedRuntime = std::move(tmpRuntime);
                        updatedSharedHandles.insert(sourceAnimatorHandle);
                    }
                }

                if (!sharedRuntime->finalTransforms.empty())
                {
                    sm.finalBoneTransforms = sharedRuntime->finalTransforms;
                    sm.currentStateName = sharedRuntime->currentStateName;
                    sm.stateElapsed = sharedRuntime->stateElapsed;
                    sm.stateNormalized = sharedRuntime->stateNormalized;
                }
            }
            else
            {
                AnimatorControllerRuntime runtime;
                runtime.currentStateName = sm.currentStateName;
                runtime.stateElapsed = sm.stateElapsed;
                runtime.stateNormalized = sm.stateNormalized;

                if (animController->UpdateSkeleton(deltaTime, runtime, m_AssetManager))
                {
                    sm.finalBoneTransforms = std::move(runtime.finalTransforms);
                    sm.currentStateName = runtime.currentStateName;
                    sm.stateElapsed = runtime.stateElapsed;
                    sm.stateNormalized = runtime.stateNormalized;
                }
            }
        }

        auto animator2dView = registry->view<Sprite2DComponent, Animator2DComponent>();
        for (entt::entity e : animator2dView)
        {
            auto &sprite = animator2dView.get<Sprite2DComponent>(e);
            auto &animComp = animator2dView.get<Animator2DComponent>(e);

            if (animComp.controllerHandle == AssetHandle(0))
                continue;

            Ref<AnimatorController2D> ctrl = m_AssetManager->GetAsset<AnimatorController2D>(animComp.controllerHandle);
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

            Ref<Animation2D> anim = m_AssetManager->GetAsset<Animation2D>(state->animHandle);
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
    void Scene::OnComponentAdded<WidgetComponent>(Entity entity, WidgetComponent&comp)
    {
    }

    template<>
    void Scene::OnComponentAdded<MeshComponent>(Entity entity, MeshComponent &comp)
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

        comp.dirtyEnvironment = true;
        comp.gpuInitialized = false;
        comp.loadedHDRHandle = AssetHandle(0);
    }

    template<>
    void Scene::OnComponentAdded<CameraComponent>(Entity entity, CameraComponent &comp)
    {
        comp.camera.UpdateProjection(static_cast<float>(m_ViewportWidth), static_cast<float>(m_ViewportHeight));
    }
}
