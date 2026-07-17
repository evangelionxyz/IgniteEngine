// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "scene.hpp"
#include <entt/entt.hpp>

#include "ignite/audio/fmod_sound.hpp"
#include "ignite/audio/fmod_audio.hpp"

#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/renderer/renderer_2d.hpp"
#include "ignite/graphics/objects/environment.hpp"
#include "ignite/physics/2d/physics_2d.hpp"
#include "ignite/physics/3d/jolt/jolt_physics.hpp"
#include "ignite/math/math.hpp"
#include "ignite/math/transform.hpp"
#include "scene_manager.hpp"
#include "ignite/scripting/script_engine.hpp"

#include "entity.hpp"

#include "ignite/core/application.hpp"
#include "ignite/animation/skeleton.hpp"
#include "ignite/animation/animation_2d.hpp"
#include "ignite/animation/animator/animator_controller.hpp"
#include "ignite/animation/animator/animator_controller_2d.hpp"
#include "ignite/project/project.hpp"
#include "ignite/graphics/ui/widget_canvas.hpp"

#include "ignite/core/profiler/profiler.hpp"

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

        void SyncMeshAnimatorParams(SkeletalMeshComponent &meshComp, const AnimatorController &controller)
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

        void ApplyMeshRuntimeParamsToController(const SkeletalMeshComponent &meshComp, AnimatorController &controller)
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

        void ResetMeshAnimatorRuntime(SkeletalMeshComponent &meshComp)
        {
            meshComp.currentStateName.clear();
            meshComp.stateElapsed = 0.0f;
            meshComp.stateNormalized = 0.0f;
        }

        AssetHandle ResolveMeshAnimatorSourceHandle(const SkeletalMeshComponent &meshComp, const Ref<SkeletalMesh> &mesh)
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

    // ===============================================
    // Scene Class
    // ===============================================
    Scene::Scene()
        : registry(nullptr)
        , m_SceneRenderer(nullptr)
        , m_Project(nullptr)
        , m_AssetManager(nullptr)
        , m_ViewportWidth(0)
        , m_ViewportHeight(0)
    {
    }

    Scene::Scene(Project *project, const std::string &_name)
        : m_Project(project), name(_name), m_SceneRenderer(nullptr)
        , m_ViewportWidth(1280), m_ViewportHeight(720)
    {
        registry = new entt::registry();

        m_Physics2D = project->GetPhysics2D();
        m_JoltScene = new JoltScene();

		m_AssetManager = AssetManager::GetInstance();
    }

    Scene::~Scene()
    {
        // Stop physics simulations first
        if (m_Physics2D)
        {
			m_Physics2D = nullptr;
        }

        if (m_JoltScene)
        {
            delete m_JoltScene;
			m_JoltScene = nullptr;
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

        m_AssetManager = nullptr;
        m_Project = nullptr;
    }

    void Scene::OnStart(ESceneState playOrSimulateState)
    {
		LOG_ASSERT(playOrSimulateState == ESceneState::Play || playOrSimulateState == ESceneState::Simulate, "Invalid scene state for OnStart");

        m_State = playOrSimulateState;

        if (auto *scriptEngine = ScriptEngine::GetInstance())
        {
            scriptEngine->SetSceneContext(this);
        }

        // reset time
        timeInSeconds = 0.0f;

        // play on start audio
        auto audioView = registry->view<AudioSourceComponent>();
        for (entt::entity e : audioView)
        {
            AudioSourceComponent &as = audioView.get<AudioSourceComponent>(e);
            if (as.handle == AssetHandle(0))
                continue;

            // Main thread
			// Load the sound asset immediately to ensure it's ready for playback
            Ref<FmodSound> sound = m_AssetManager->GetAssetImmediate<FmodSound>(as.handle);
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

        m_Physics2D->SimulationStart(this);
        m_JoltScene->SimulationStart(this);

        registry->view<ScriptComponent>().each([this](entt::entity e, ScriptComponent &script)
        {
            Entity entity{ e, this };
            ScriptInstanceID instanceID = entity.GetUUID();
            script.runtimeScriptInstance = ScriptEngine::GetInstance()->OnCreateEntityInstance(instanceID, script.className);
        });

        registry->view<SkeletalMeshComponent>().each([](entt::entity, SkeletalMeshComponent &meshComp)
        {
            ResetMeshAnimatorRuntime(meshComp);
        });

        m_SharedAnimatorRuntime.clear();
    }

    void Scene::OnStop()
    {
        if (!IsRunning())
            return;

        m_State = ESceneState::Stop;

        m_StepFrame = 0; 
        timeInSeconds = 0.0f;

		registry->view<AudioSourceComponent>().each([this](entt::entity e, AudioSourceComponent &as)
		{
			if (as.handle == AssetHandle(0))
				return;

            if (m_AssetManager->IsAssetLoaded(as.handle))
            {
			    Ref<FmodSound> sound = m_AssetManager->GetAsset<FmodSound>(as.handle);
			    if (sound)
				    sound->Stop();
            }
		});
        
        registry->view<ScriptComponent>().each([this](entt::entity e, ScriptComponent &script)
        {
            Entity entity { e, this };
            script.runtimeScriptInstance = nullptr;
            const ScriptInstanceID instanceID = entity.GetUUID();
            if (auto *scriptEngine = ScriptEngine::GetInstance())
            {
                scriptEngine->OnDestroyEntityInstance(instanceID);
            }
        });

        if (auto *scriptEngine = ScriptEngine::GetInstance())
        {
            scriptEngine->ClearSceneContext();
        }

        m_SharedAnimatorRuntime.clear();
        
        m_Physics2D->SimulationStop();
        m_JoltScene->SimulationStop();
    }

    void Scene::Pause()
    {
        m_State = ESceneState::Paused;
    }

    void Scene::Step(int frame)
    {
        m_StepFrame = frame;
    }

    void Scene::UpdateTransforms(float deltaTime)
    {
        IGN_PROFILE_FUNCTION();
        UpdateAnimations(deltaTime);

        registry->view<IDComponent, TransformComponent>().each([this](entt::entity e, const auto &id, const auto &tr)
        {
            if (id.parent == 0)
            {
                UpdateTransformRecursive(Entity{ e, this }, glm::mat4(1.0f));
            }
        });

        auto staticMeshView = registry->view<TransformComponent, StaticMeshComponent>();
        for (entt::entity e : staticMeshView)
        {
            const auto &[tr, smc] = staticMeshView.get<TransformComponent, StaticMeshComponent>(e);
            if (!tr.visible || smc.handle == AssetHandle(0))
                continue;

            if (auto mesh = m_AssetManager->GetAsset<StaticMesh>(smc.handle))
            {
                const auto worldMatrix = tr.world.GetMatrix();
                smc.normalMatrix = glm::transpose(glm::inverse(glm::mat3(worldMatrix)));
                smc.worldAABB = mesh->localAABB.Transform(worldMatrix);
            }
        }

        auto cameraView = registry->view<TransformComponent, CameraComponent>();
        for (auto entity : cameraView)
        {
            const auto &[tr, cc] = cameraView.get<TransformComponent, CameraComponent>(entity);
            cc.camera.SetTransform(tr.world.GetMatrix());
        }
    }

    void Scene::UpdateTransformRecursive(Entity entity, const glm::mat4 &parentWorldTransform)
    {
        TransformComponent &transform = entity.GetTransform();
        auto &id = entity.GetComponent<IDComponent>();

        glm::mat4 worldMatrix = parentWorldTransform * transform.local.GetMatrix();

        Transform::Decompose(worldMatrix, transform.world);
        
        transform.dirty = false;

        for (const UUID &childUUID : id.children)
        {
            Entity child = SceneManager::GetEntity(this, childUUID);
            UpdateTransformRecursive(child, worldMatrix);
        }
    }

    void Scene::OnUpdateEdit(float deltaTime)
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

    Ref<WidgetCanvas> Scene::GetRootWidget()
    {
        const auto &widgetView = registry->view<WidgetComponent>();
        for (const auto e : widgetView)
        {
            const WidgetComponent &widgetComp = widgetView.get<WidgetComponent>(e);
            if (widgetComp.widgetHandle == AssetHandle(0))
                continue;

            return m_AssetManager->GetAsset<WidgetCanvas>(widgetComp.widgetHandle);
        }

        return nullptr;
    }

    void Scene::OnUpdateRuntimeSimulate(float deltaTime)
    {
        IGN_PROFILE_FUNCTION();

        // Cap maximum delta time to prevent physics explosions/instability during large frame hitches
        if (deltaTime > 0.1f)
            deltaTime = 0.1f;

        if (!IsPaused() || m_StepFrame-- > 0)
        {
            IGN_PROFILE_SCOPE("Scene::RuntimeTick");
            timeInSeconds += deltaTime;

            // Update audio
			registry->view<AudioSourceComponent>().each([this, deltaTime](entt::entity e, AudioSourceComponent &as)
				{
					if (as.handle == AssetHandle(0))
                        return;

					Ref<FmodSound> sound = m_AssetManager->GetAsset<FmodSound>(as.handle);
                    if (sound)
                        sound->Update(deltaTime);
				});

            {
                IGN_PROFILE_SCOPE("Scene::ScriptUpdate");
                registry->view<ScriptComponent>().each([this, deltaTime](entt::entity e, ScriptComponent &script)
                {
                    if (script.runtimeScriptInstance)
                        script.runtimeScriptInstance->InvokeOnUpdate(deltaTime);
                });
            }

            {
                IGN_PROFILE_SCOPE("Scene::Physics2D");
                m_Physics2D->Simulate(deltaTime);
            }

            {
                IGN_PROFILE_SCOPE("Scene::Physics3D");
                m_JoltScene->Simulate(deltaTime);
            }

			UpdateTransforms(deltaTime);

            // Dispatch Jolt collision events to C# scripts
            {
                IGN_PROFILE_SCOPE("Scene::CollisionEvents");

				// Helper: try to find a script instance for an entity
				// Uses the ScriptComponent view to avoid error-logging for entities without scripts
				auto getScriptInstance = [&](uint64_t entityId) -> Ref<ScriptInstance>
				{
					auto it = entities.find(UUID(entityId));
					if (it == entities.end())
						return nullptr;

					Entity ent{ it->second, this };
					if (!ent.HasComponent<ScriptComponent>())
						return nullptr;

					return ent.GetComponent<ScriptComponent>().runtimeScriptInstance;
				};

                auto colEvents = m_JoltScene->DrainCollisionEvents();

                for (const auto &ev : colEvents)
                {
                    // UserData must be returns entity UUID
                    const uint64_t entityIDA = m_JoltScene->GetUserData(ev.bodyA);
                    const uint64_t entityIDB = m_JoltScene->GetUserData(ev.bodyB);
                    
                    if (entityIDA == 0 || entityIDB == 0)
                        continue;
                   
					auto dispatch = [&ev](Ref<ScriptInstance> scriptInstance, uint64_t otherId)
                    {
                        if (!scriptInstance)
                            return;

                        switch (ev.type)
                        {
                            case JoltCollisionEventType::Enter: scriptInstance->InvokeOnCollisionEnter(otherId); break;
                            case JoltCollisionEventType::Stay:  scriptInstance->InvokeOnCollisionStay(otherId);  break;
                            case JoltCollisionEventType::Exit:  scriptInstance->InvokeOnCollisionExit(otherId);  break;
                        }
                    };

                    dispatch(getScriptInstance(entityIDA), entityIDB);
                    dispatch(getScriptInstance(entityIDB), entityIDA);
                }

				auto activationEvents = m_JoltScene->DrainActivationEvents();
                for (const auto &ev : activationEvents)
                {
					const uint64_t entityID = m_JoltScene->GetUserData(ev.bodyId);
					if (entityID == 0)
						continue;
					
					auto scriptInstance = getScriptInstance(entityID);
                    if (!scriptInstance)
                        continue;

					switch (ev.type)
					{
					    case JoltActivationEventType::Activated:   scriptInstance->InvokeOnBodyActivated();   break;
					    case JoltActivationEventType::Deactivated: scriptInstance->InvokeOnBodyDeactivated(); break;
					}
                }
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
            if (!fallback)
            {
                fallback = &we;
            }
        }

        return fallback ? fallback->environment.get() : nullptr;
    }

    WorldEnvironment *Scene::GetActiveWorldEnvironment()
    {
        auto view = registry->view<WorldEnvironment>();
        WorldEnvironment *fallback = nullptr;
        for (entt::entity e : view)
        {
            WorldEnvironment &world = view.get<WorldEnvironment>(e);
            if (!fallback)
            {
                fallback = &world;
            }
        }

        return fallback;
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

        registry->view<SkeletalMeshComponent>().each([&](entt::entity, const SkeletalMeshComponent &mesh)
        {
            addHandle(mesh.handle);
            addHandle(mesh.runtimeAnimatorHandle);
            for (auto &materialHandle : mesh.overrideMaterials | std::views::values)
            {
                addHandle(materialHandle);
            }
        });

        registry->view<StaticMeshComponent>().each([&](entt::entity, const StaticMeshComponent &mesh)
        {
            addHandle(mesh.handle);
            for (auto &materialHandle : mesh.overrideMaterials | std::views::values)
            {
                addHandle(materialHandle);
            }
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
        auto skeletalMeshView = registry->view<TransformComponent, SkeletalMeshComponent>();
        std::unordered_set<AssetHandle> updatedSharedHandles;
        for (auto ent : skeletalMeshView)
        {
            const auto &[tr, smc] = skeletalMeshView.get<TransformComponent, SkeletalMeshComponent>(ent);

            if (!tr.visible || smc.handle == AssetHandle(0))
                continue;

            auto mesh = m_AssetManager->GetAsset<SkeletalMesh>(smc.handle);
            if (mesh)
            {
                const auto worldMatrix = tr.world.GetMatrix();
                smc.normalMatrix = glm::transpose(glm::inverse(glm::mat3(worldMatrix)));
                smc.worldAABB = mesh->localAABB.Transform(worldMatrix);
            }

            AssetHandle sourceAnimatorHandle = ResolveMeshAnimatorSourceHandle(smc, mesh);
            if (sourceAnimatorHandle == AssetHandle(0))
            {
                smc.runtimeAnimatorInstance.reset();
                smc.runtimeParams.clear();
                smc.finalBoneTransforms.clear();
                ResetMeshAnimatorRuntime(smc);
                continue;
            }

            smc.runtimeAnimatorHandle = sourceAnimatorHandle;

            Ref<AnimatorController> animController = nullptr;
            AnimatorControllerRuntime *sharedRuntime = nullptr;

            if (smc.uniqueAnimator)
            {
                if (!smc.runtimeAnimatorInstance)
                {
                    Ref<AnimatorController> sourceController = m_AssetManager->GetAsset<AnimatorController>(sourceAnimatorHandle);
                    if (sourceController)
                    {
                        smc.runtimeAnimatorInstance = CloneAnimatorController(sourceController);
                        if (smc.runtimeAnimatorInstance)
                        {
                            smc.runtimeParams.clear();
                            ResetMeshAnimatorRuntime(smc);
                        }
                    }
                }

                animController = smc.runtimeAnimatorInstance;
            }
            else
            {
                smc.runtimeAnimatorInstance.reset();

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

                if (sharedRuntime->currentStateName.empty() && !smc.currentStateName.empty())
                {
                    sharedRuntime->currentStateName = smc.currentStateName;
                    sharedRuntime->stateElapsed = smc.stateElapsed;
                    sharedRuntime->stateNormalized = smc.stateNormalized;
                }
            }

            if (!animController)
                continue;

            SyncMeshAnimatorParams(smc, *animController);
            ApplyMeshRuntimeParamsToController(smc, *animController);

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
                    smc.finalBoneTransforms = sharedRuntime->finalTransforms;
                    
                    // ==== Socket system: Cache global joint transforms ====
                    smc.globalJointTransforms.resize(sharedRuntime->globalPoses.size());
                    for (size_t i = 0; i < sharedRuntime->globalPoses.size(); ++i)
                    {
                        smc.globalJointTransforms[i] = sharedRuntime->globalPoses[i].GetMatrix();
                    }

                    smc.currentStateName = sharedRuntime->currentStateName;
                    smc.stateElapsed = sharedRuntime->stateElapsed;
                    smc.stateNormalized = sharedRuntime->stateNormalized;
                }
            }
            else
            {
                AnimatorControllerRuntime runtime;
                runtime.currentStateName = smc.currentStateName;
                runtime.stateElapsed = smc.stateElapsed;
                runtime.stateNormalized = smc.stateNormalized;

                if (animController->UpdateSkeleton(deltaTime, runtime, m_AssetManager))
                {
                    smc.finalBoneTransforms = std::move(runtime.finalTransforms);

                    // ==== Socket system: Cache global joint transforms ====
                    smc.globalJointTransforms.resize(runtime.globalPoses.size());
                    for (size_t i = 0; i < runtime.globalPoses.size(); ++i)
                    {
                        smc.globalJointTransforms[i] = runtime.globalPoses[i].GetMatrix();
                    }

                    smc.currentStateName = runtime.currentStateName;
                    smc.stateElapsed = runtime.stateElapsed;
                    smc.stateNormalized = runtime.stateNormalized;
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
            if (!state || state->GetAnimationAssetHandle() == AssetHandle(0))
                continue;

            Ref<Animation2D> anim = m_AssetManager->GetAsset<Animation2D>(state->GetAnimationAssetHandle());
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
    IGN_API void Scene::OnComponentAdded<IDComponent>(Entity entity, IDComponent &comp)
    {
    }

    template<>
    IGN_API void Scene::OnComponentAdded<TransformComponent>(Entity entity, TransformComponent &comp)
    {
    }

    template<>
    IGN_API void Scene::OnComponentAdded<Sprite2DComponent>(Entity entity, Sprite2DComponent &comp)
    {
    }

    template<>
    IGN_API void Scene::OnComponentAdded<Circle2DComponent>(Entity entity, Circle2DComponent &comp)
    {
    }

    template<>
    IGN_API void Scene::OnComponentAdded<PointLight2DComponent>(Entity entity, PointLight2DComponent &comp)
    {
    }

    template<>
    IGN_API void Scene::OnComponentAdded<DirectionalLightComponent>(Entity entity, DirectionalLightComponent &comp)
    {
    }

    template<>
    IGN_API void Scene::OnComponentAdded<PointLightComponent>(Entity entity, PointLightComponent &comp)
    {
    }

    template<>
    IGN_API void Scene::OnComponentAdded<SpotLightComponent>(Entity entity, SpotLightComponent &comp)
    {
    }

    template<>
    IGN_API void Scene::OnComponentAdded<Rigidbody2DComponent>(Entity entity, Rigidbody2DComponent &comp)
    {
    }

    template<>
    IGN_API void Scene::OnComponentAdded<BoxCollider2DComponent>(Entity entity, BoxCollider2DComponent &comp)
    {
    }

    template<>
    IGN_API void Scene::OnComponentAdded<CircleCollider2DComponent>(Entity entity, CircleCollider2DComponent &comp)
    {
    }

    template<>
    IGN_API void Scene::OnComponentAdded<RigidbodyComponent>(Entity entity, RigidbodyComponent &comp)
    {
    }

    template<>
    IGN_API void Scene::OnComponentAdded<BoxColliderComponent>(Entity entity, BoxColliderComponent &comp)
    {
    }

    template<>
    IGN_API void Scene::OnComponentAdded<SphereColliderComponent>(Entity entity, SphereColliderComponent &comp)
    {
    }

    template<>
    IGN_API void Scene::OnComponentAdded<CapsuleColliderComponent>(Entity entity, CapsuleColliderComponent &comp)
    {
    }

    template<>
    IGN_API void Scene::OnComponentAdded<MeshColliderComponent>(Entity entity, MeshColliderComponent &comp)
    {
    }

    template<>
    IGN_API void Scene::OnComponentAdded<AudioSourceComponent>(Entity entity, AudioSourceComponent &comp)
    {
    }

    template<>
    IGN_API void Scene::OnComponentAdded<TextComponent>(Entity entity, TextComponent &comp)
    {
    }

    template<>
    IGN_API void Scene::OnComponentAdded<WidgetComponent>(Entity entity, WidgetComponent&comp)
    {
    }

    template<>
    IGN_API void Scene::OnComponentAdded<StaticMeshComponent>(Entity entity, StaticMeshComponent &comp)
    {
    }

    template<>
    IGN_API void Scene::OnComponentAdded<SkeletalMeshComponent>(Entity entity, SkeletalMeshComponent &comp)
    {
    }

    template<>
    IGN_API void Scene::OnComponentAdded<ScriptComponent>(Entity entity, ScriptComponent &comp)
    {
    }

    template<>
    IGN_API void Scene::OnComponentAdded<Animator2DComponent>(Entity entity, Animator2DComponent &comp)
    {
        // Will initialize from controller's default state on first update tick
        comp.currentStateName = "";
        comp.currentFrame     = 0;
        comp.elapsed          = 0.0f;
        comp.stateElapsed     = 0.0f;
        comp.stateNormalized  = 0.0f;
    }

    template<>
    IGN_API void Scene::OnComponentAdded<WorldEnvironment>(Entity entity, WorldEnvironment &comp)
    {
        if (!comp.environment)
        {
            comp.environment = Environment::Create();
        }

        comp.dirtyEnvironment = true;
        comp.gpuInitialized = false;
    }

    template<>
    IGN_API void Scene::OnComponentAdded<CameraComponent>(Entity entity, CameraComponent &comp)
    {
        comp.camera.UpdateProjection(m_ViewportWidth, m_ViewportHeight);
    }
}
