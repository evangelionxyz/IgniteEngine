// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"
#include "entity.hpp"

#include "ignite/audio/fmod_sound.hpp"
#include "ignite/audio/fmod_audio.hpp"
#include "ignite/graphics/renderer/renderer_2d.hpp"
#include "ignite/graphics/objects/environment.hpp"
#include "ignite/physics/2d/physics_2d.hpp"
#include "ignite/physics/3d/physics_3d.hpp"
#include "ignite/math/transform.hpp"
#include "ignite/scripting/script_engine.hpp"
#include "ignite/core/application.hpp"
#include "ignite/animation/skeleton.hpp"
#include "ignite/animation/animation_2d.hpp"
#include "ignite/animation/animator/animator_controller.hpp"
#include "ignite/animation/animator/animator_controller_2d.hpp"
#include "ignite/project/project.hpp"
#include "ignite/graphics/ui/widget_canvas.hpp"
#include "ignite/core/profiler/profiler.hpp"
#include "scene_manager.hpp"

namespace ignite
{
    namespace
    {
        void SyncMeshAnimatorParams(SkeletalMeshComponent &meshComp, const AnimatorController &controller)
        {
            IGN_PROFILE_FUNCTION();

            std::erase_if(meshComp.runtimeParams, [&controller](const auto &kv)
            {
                return controller.GetParam(kv.first) == nullptr;
            });

            for (const auto &[name, param] : controller.params)
            {
                AnimParam *runtimeParam = Animator::FindAnimParam(meshComp.runtimeParams, param.name);
                if (!runtimeParam)
                {
                    meshComp.runtimeParams[name] = param;
                    continue;
                }

                if (runtimeParam->type != param.type)
                {
                    *runtimeParam = param;
                }
            }
        }

        void ApplyMeshRuntimeParamsToController(const SkeletalMeshComponent &meshComp, AnimatorController &controller)
        {
			IGN_PROFILE_FUNCTION();

            for (auto &[name, param] : controller.params)
            {
                if (const AnimParam *runtimeParam = Animator::FindAnimParam(meshComp.runtimeParams, param.name))
                {
                    param.type = runtimeParam->type;
                    param.floatVal = runtimeParam->floatVal;
                    param.intVal = runtimeParam->intVal;
                    param.boolVal = runtimeParam->boolVal;
                    param.strVal = runtimeParam->strVal;
                }
            }
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
        , m_Physics2D(nullptr)
        , m_Physics3D(nullptr)
        , m_ViewportWidth(0)
        , m_ViewportHeight(0)
    {
        registry = new entt::registry();
    }

    Scene::Scene(Project *project)
        : m_Project(project)
        , m_SceneRenderer(nullptr)
        , m_ViewportWidth(1280)
        , m_ViewportHeight(720)
    {
        registry = new entt::registry();

        m_Physics2D = project->GetPhysics2D();
        m_Physics2D->SetScene(this);

        m_Physics3D = project->GetPhysics3D();

		m_AssetManager = AssetManager::GetInstance();
		m_AssetChangeToken = SignalBus::Subscribe<AssetChangeSignal>(
		[this](const AssetChangeSignal &signal)
		{
			OnAssetChangeSignal(signal);
		});
    }

    Scene::~Scene()
    {
        // Remove signal
		SignalBus::Unsubscribe<AssetChangeSignal>(m_AssetChangeToken);
        m_AssetChangeToken = kInvalidSignalToken;
        
        m_Physics3D = nullptr;
		m_Physics2D = nullptr;

        // Clear all entities from registry before deletion
        if (registry)
        {
            auto terrainView = registry->view<TerrainComponent>();
            for (auto e : terrainView)
            {
                auto &comp = terrainView.get<TerrainComponent>(e);
                comp.ReleaseGPU();
            }

            registry->clear();
            delete registry;
        }
        registry = nullptr;

        // Clear entity map
        entities.clear();
        m_AssetManager = nullptr;
        m_Project = nullptr;
    }

    void Scene::OnStart(ESceneState playOrSimulateState)
    {
		LOG_ASSERT(playOrSimulateState == ESceneState::Play || playOrSimulateState == ESceneState::Simulate, "Invalid scene state for OnStart");

        m_State = playOrSimulateState;

        PreloadReferencedAssets();

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

            Ref<FmodSound> sound = m_AssetManager->GetAsset<FmodSound>(as.handle);
			if (sound)
			{
				RebuildAudioSourceDspChain(as, sound);
                
                sound->Stop();

                if (as.playOnStart)
                    sound->Play();

				sound->SetVolume(as.volume);
				sound->SetPitch(as.pitch);
				sound->SetPan(as.pan);
				sound->SetMode(as.loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);
			}
            
        }

        m_Physics2D->SimulationStart(this);

        if (m_Physics3D)
        {
            physics::Physics3DSettings pSettings;
            if (auto assetManager = AssetManager::GetInstance())
            {
                if (auto project = assetManager->LockActiveProject())
                {
                    pSettings = project->GetInfo().physicsSettings;
                }
            }
            m_Physics3D->SimulationStart(pSettings);

            auto createCollider = [&](Entity entity, const TransformComponent &tr) -> Ref<physics::PhysicsCollider>
            {
                if (entity.HasComponent<BoxColliderComponent>())
                {
                    auto &box = entity.GetComponent<BoxColliderComponent>();
                    physics::BoxColliderDesc desc;
                    desc.center = box.center;
                    desc.halfExtents = box.scale * tr.world.scale;
                    auto col = m_Physics3D->CreateBoxCollider(desc);
                    box.collider = col;
                    return col;
                }
                else if (entity.HasComponent<SphereColliderComponent>())
                {
                    auto &sphere = entity.GetComponent<SphereColliderComponent>();
                    physics::SphereColliderDesc desc;
                    desc.center = sphere.center;
                    float maxScale = std::max({ tr.world.scale.x, tr.world.scale.y, tr.world.scale.z });
                    desc.radius = sphere.radius * maxScale;
                    auto col = m_Physics3D->CreateSphereCollider(desc);
                    sphere.collider = col;
                    return col;
                }
                else if (entity.HasComponent<CapsuleColliderComponent>())
                {
                    auto &capsule = entity.GetComponent<CapsuleColliderComponent>();
                    
					const float maxScale = glm::compMax(glm::abs(tr.world.scale));
					const float radius = capsule.radius * maxScale;

                    physics::CapsuleColliderDesc desc;
                    desc.center = capsule.center;
                    desc.radius = radius;
                    desc.halfHeight = glm::max(capsule.height * 0.5f - capsule.radius, 0.0f) * maxScale;
                    auto col = m_Physics3D->CreateCapsuleCollider(desc);
                    capsule.collider = col;
                    return col;
                }
                else if (entity.HasComponent<PlaneColliderComponent>())
                {
                    auto &plane = entity.GetComponent<PlaneColliderComponent>();
                    physics::PlaneColliderDesc desc;
                    desc.center = plane.center;
                    desc.scale = plane.scale * tr.world.scale;
                    auto col = m_Physics3D->CreatePlaneCollider(desc);
                    plane.collider = col;
                    return col;
                }
                else if (entity.HasComponent<MeshColliderComponent>())
                {
                    auto &meshComp = entity.GetComponent<MeshColliderComponent>();
                    physics::MeshColliderDesc desc;
                    desc.center = meshComp.center;
                    desc.vertices = meshComp.vertices;
                    desc.indices = meshComp.indices;
                    desc.isConvex = meshComp.convex;
                    auto col = m_Physics3D->CreateMeshCollider(desc);
                    meshComp.collider = col;
                    return col;
                }
                else if (entity.HasComponent<HeightFieldColliderComponent>())
                {
                    auto &hfComp = entity.GetComponent<HeightFieldColliderComponent>();
                    physics::HeightFieldColliderDesc desc;
                    desc.center = hfComp.center;
                    desc.scale = hfComp.scale * tr.world.scale;
                    desc.sampleCount = hfComp.sampleCount;
                    desc.heights = hfComp.heights;
                    auto col = m_Physics3D->CreateHeightFieldCollider(desc);
                    hfComp.collider = col;
                    return col;
                }
                else if (entity.HasComponent<TerrainComponent>())
                {
                    auto &tc = entity.GetComponent<TerrainComponent>();
                    if (tc.data && !tc.data->heightmap.empty() && tc.data->resolution >= 2)
                    {
                        tc.data->worldSize = tc.worldSize;
                        tc.data->maxHeight = tc.maxHeight;

                        physics::HeightFieldColliderDesc desc;
                        uint32_t res = tc.data->resolution;
                        float step = tc.worldSize / static_cast<float>(res - 1);
                        float halfSize = tc.worldSize * 0.5f;

                        desc.center = glm::vec3(-halfSize, 0.0f, -halfSize) * tr.world.scale;
                        desc.scale = glm::vec3(step, tc.maxHeight, step) * tr.world.scale;
                        desc.sampleCount = res;
                        desc.heights = tc.data->heightmap;

                        auto col = m_Physics3D->CreateHeightFieldCollider(desc);
                        tc.collider = col;
                        return col;
                    }
                }
                return nullptr;
            };

            std::unordered_set<entt::entity> processedEntities;

            for (entt::entity e : registry->view<RigidbodyComponent>())
            {
                Entity entity{ e, this };
                auto &rb = entity.GetComponent<RigidbodyComponent>();
                auto &tr = entity.GetTransform();
                uint64_t userData = static_cast<uint64_t>(entity.GetUUID());

                physics::RigidBodyDesc rbDesc;
                rbDesc.bodyType = rb.bodyType;
                rbDesc.motionQuality = rb.motionQuality;
                rbDesc.layer = rb.layer;
                rbDesc.useGravity = rb.useGravity;
                rbDesc.mass = rb.mass;
                rbDesc.friction = rb.friction;
                rbDesc.restitution = rb.restitution;
                rbDesc.rotateX = rb.rotateX;
                rbDesc.rotateY = rb.rotateY;
                rbDesc.rotateZ = rb.rotateZ;
                rbDesc.moveX = rb.moveX;
                rbDesc.moveY = rb.moveY;
                rbDesc.moveZ = rb.moveZ;

                physics::PhysicsTransformData transformData;
                transformData.position = tr.world.translation;
                transformData.rotation = tr.world.rotation;

                Ref<physics::PhysicsCollider> collider = createCollider(entity, tr);

                if (rb.bodyType == physics::BodyType::Dynamic || rb.bodyType == physics::BodyType::Kinematic)
                {
                    rb.dynamicActor = m_Physics3D->CreateDynamicBody(rbDesc, transformData, userData, collider);
                }
                else
                {
                    rb.staticActor = m_Physics3D->CreateStaticBody(rbDesc, transformData, userData, collider);
                }
                processedEntities.insert(e);
            }

            auto createStaticBodyForStandaloneCollider = [&](entt::entity e)
            {
                if (processedEntities.contains(e))
                    return;

                Entity entity{ e, this };
                auto &tr = entity.GetTransform();
                uint64_t userData = static_cast<uint64_t>(entity.GetUUID());

                physics::RigidBodyDesc rbDesc;
                rbDesc.bodyType = physics::BodyType::Static;

                physics::PhysicsTransformData transformData;
                transformData.position = tr.world.translation;
                transformData.rotation = tr.world.rotation;

                Ref<physics::PhysicsCollider> collider = createCollider(entity, tr);
                if (collider)
                {
                    auto staticActor = m_Physics3D->CreateStaticBody(rbDesc, transformData, userData, collider);
                    if (entity.HasComponent<RigidbodyComponent>())
                    {
                        entity.GetComponent<RigidbodyComponent>().staticActor = staticActor;
                    }
                    else
                    {
                        auto &rb = entity.AddComponent<RigidbodyComponent>();
                        rb.bodyType = physics::BodyType::Static;
                        rb.staticActor = staticActor;
                    }
                }
                processedEntities.insert(e);
            };

            for (entt::entity e : registry->view<BoxColliderComponent>()) createStaticBodyForStandaloneCollider(e);
            for (entt::entity e : registry->view<SphereColliderComponent>()) createStaticBodyForStandaloneCollider(e);
            for (entt::entity e : registry->view<CapsuleColliderComponent>()) createStaticBodyForStandaloneCollider(e);
            for (entt::entity e : registry->view<PlaneColliderComponent>()) createStaticBodyForStandaloneCollider(e);
            for (entt::entity e : registry->view<MeshColliderComponent>()) createStaticBodyForStandaloneCollider(e);
            for (entt::entity e : registry->view<HeightFieldColliderComponent>()) createStaticBodyForStandaloneCollider(e);
            for (entt::entity e : registry->view<TerrainComponent>()) createStaticBodyForStandaloneCollider(e);

            for (entt::entity e : registry->view<CharacterControllerComponent>())
            {
                Entity entity{ e, this };
                auto &cc = entity.GetComponent<CharacterControllerComponent>();
                auto &tr = entity.GetTransform();
                uint64_t userData = static_cast<uint64_t>(entity.GetUUID());

                const float maxScale = glm::compMax(glm::abs(tr.world.scale));
                const float radius = cc.radius * maxScale;
                const float halfHeight = glm::max(cc.height * 0.5f - cc.radius, 0.0f) * maxScale;

                physics::CharacterControllerDesc ccDesc;
                ccDesc.center = cc.center * tr.world.scale;
                ccDesc.radius = radius;
                ccDesc.halfHeight = halfHeight;
                ccDesc.mass = cc.mass;
                ccDesc.friction = cc.friction;
                ccDesc.maxStepHeight = cc.maxStepHeight;
                ccDesc.maxSlopeAngle = cc.maxSlopeAngle;
                ccDesc.up = cc.up;

                auto charActor = m_Physics3D->CreateCharacterController(ccDesc, userData);
                if (charActor)
                {
                    charActor->SetPosition(tr.world.translation);
                    charActor->SetRotation(tr.world.rotation);
                }
                cc.character = charActor;
            }
        }

        registry->view<ScriptComponent>().each([this](entt::entity e, ScriptComponent &script)
        {
            Entity entity{ e, this };
            ScriptInstanceID instanceID = entity.GetUUID();
            script.runtimeScriptInstance = ScriptEngine::GetInstance()->OnCreateEntityInstance(instanceID, script.className);
        });

        registry->view<SkeletalMeshComponent>().each([](entt::entity, SkeletalMeshComponent &smc)
        {
            smc.ResetAnimatorRuntime();
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
        if (m_Physics3D)
        {
            m_Physics3D->SimulationStop();
            for (entt::entity e : registry->view<RigidbodyComponent>())
            {
                auto &rb = registry->get<RigidbodyComponent>(e);
                rb.dynamicActor.reset();
                rb.staticActor.reset();
            }

            for (entt::entity e : registry->view<CharacterControllerComponent>())
            {
                auto &cc = registry->get<CharacterControllerComponent>(e);
                cc.character.reset();
            }
        }
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

        auto staticMeshView = registry->view<TransformComponent, RenderingComponent, StaticMeshComponent>();
        for (entt::entity e : staticMeshView)
        {
            const auto &[tr, rc, smc] = staticMeshView.get<TransformComponent, RenderingComponent, StaticMeshComponent>(e);
            if (!rc.visible || smc.handle == AssetHandle(0))
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

	void Scene::OnFixedUpdateEdit()
	{
		// {
		// 	IGN_PROFILE_SCOPE("Scene::Physics2D");
		// 	m_Physics2D->Simulate(1.0f / 60.0f);
		// }
        // 
		// {
		// 	IGN_PROFILE_SCOPE("Scene::Physics3D");
		// 	m_JoltScene->Simulate(1.0f / 60.0f);
		// }
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
                IGN_PROFILE_SCOPE("Scene::Script OnUpdate");
                registry->view<ScriptComponent>().each([this, deltaTime](entt::entity e, ScriptComponent &script)
                {
                    if (script.runtimeScriptInstance)
                        script.runtimeScriptInstance->InvokeOnUpdate(deltaTime);
                });
            }

			UpdateTransforms(deltaTime);

            // Update physics
            {
                IGN_PROFILE_SCOPE("Scene::Physics2D");
                m_Physics2D->Simulate(deltaTime);
            }

            {
                IGN_PROFILE_SCOPE("Scene::Physics3D");
                if (m_Physics3D)
                {
                    m_Physics3D->Simulate(deltaTime);

                    // Calculate Parent transform
                    auto CalculateParentTransform = [this](const IDComponent &idc, TransformComponent &trc, const glm::vec3 &worldTranslation, const glm::quat &worldRotation)
                    {
                        if (idc.parent != 0)
                        {
                            Entity parentEntity = SceneManager::GetEntity(this, idc.parent);
                            if (parentEntity && parentEntity.HasComponent<TransformComponent>())
                            {
                                const auto &parentTr = parentEntity.GetComponent<TransformComponent>();

                                const glm::mat4 childWorldMatrix = glm::translate(glm::mat4(1.0f), worldTranslation)
                                    * glm::toMat4(worldRotation) * glm::scale(glm::mat4(1.0f), trc.world.scale);
                                const glm::mat4 childLocalMatrix = glm::inverse(parentTr.world.GetMatrix()) * childWorldMatrix;
                                const glm::vec3 savedLocalScale = trc.local.scale;
                                Transform::Decompose(childLocalMatrix, trc.local);
                                trc.local.scale = savedLocalScale;
                                trc.world.translation = worldTranslation;
                                trc.world.rotation = worldRotation;
                            }
                            else
                            {
                                trc.local.translation = worldTranslation;
                                trc.local.rotation = worldRotation;
                                trc.world.translation = worldTranslation;
                                trc.world.rotation = worldRotation;
                            }
                        }
                        else
                        {
                            trc.local.translation = worldTranslation;
                            trc.local.rotation = worldRotation;
                            trc.world.translation = worldTranslation;
                            trc.world.rotation = worldRotation;
                        }
                    };

                    // Dynamic, Static, Kinematic Actors
                    for (entt::entity e : registry->view<RigidbodyComponent>())
                    {
                        auto &rb = registry->get<RigidbodyComponent>(e);
                        auto &idc = registry->get<IDComponent>(e);
                        auto &tr = registry->get<TransformComponent>(e);

                        if (auto dynActor = rb.dynamicActor.lock())
                        {
                            if (dynActor->IsActive())
                            {
                                CalculateParentTransform(idc, tr, dynActor->GetPosition(), dynActor->GetRotation());
                            }
                        }
                        else if (auto stActor = rb.staticActor.lock())
                        {
                            CalculateParentTransform(idc, tr, stActor->GetPosition(), stActor->GetRotation());
                        }
                    }

                    // Character controllers
                    for (entt::entity e : registry->view<CharacterControllerComponent>())
                    {
                        auto &cc = registry->get<CharacterControllerComponent>(e);
                        auto &idc = registry->get<IDComponent>(e);
                        auto &tr = registry->get<TransformComponent>(e);
                        if (cc.dirty)
                        {
                            uint64_t userData = static_cast<uint64_t>(Entity{ e, this }.GetUUID());
                            const float maxScale = glm::compMax(glm::abs(tr.world.scale));
                            const float radius = cc.radius * maxScale;
                            const float halfHeight = glm::max(cc.height * 0.5f - cc.radius, 0.0f) * maxScale;

                            physics::CharacterControllerDesc ccDesc;
                            ccDesc.center = cc.center * tr.world.scale;
                            ccDesc.radius = radius;
                            ccDesc.halfHeight = halfHeight;
                            ccDesc.mass = cc.mass;
                            ccDesc.friction = cc.friction;
                            ccDesc.maxStepHeight = cc.maxStepHeight;
                            ccDesc.maxSlopeAngle = cc.maxSlopeAngle;
                            ccDesc.up = cc.up;

                            glm::vec3 currentPos = tr.world.translation;
                            glm::quat currentRot = tr.world.rotation;
                            if (auto ccActor = cc.character.lock())
                            {
                                currentPos = ccActor->GetPosition();
                                currentRot = ccActor->GetRotation();
                            }

                            auto charActor = m_Physics3D->CreateCharacterController(ccDesc, userData);
                            if (charActor)
                            {
                                charActor->SetPosition(currentPos);
                                charActor->SetRotation(currentRot);
                            }
                            cc.character = charActor;
                            cc.dirty = false;
                        }

                        if (auto ccActor = cc.character.lock())
                        {
                            ccActor->Move(cc.linearVelocity, 1.0f / 60.0f);
                            CalculateParentTransform(idc, tr, ccActor->GetPosition(), ccActor->GetRotation());
                        }
                    }
                }
            }

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

                if (m_Physics3D)
                {
                    auto colEvents = m_Physics3D->DrainCollisionEvents();

                    for (const auto &ev : colEvents)
                    {
                        if (ev.userDataA == 0 || ev.userDataB == 0)
                            continue;
                       
                        auto dispatch = [&ev](Ref<ScriptInstance> scriptInstance, uint64_t otherId)
                        {
                            if (!scriptInstance)
                                return;

                            switch (ev.type)
                            {
                                case physics::CollisionEventType::Enter: scriptInstance->InvokeOnCollisionEnter(otherId); break;
                                case physics::CollisionEventType::Stay:  scriptInstance->InvokeOnCollisionStay(otherId);  break;
                                case physics::CollisionEventType::Exit:  scriptInstance->InvokeOnCollisionExit(otherId);  break;
                            }
                        };

                        dispatch(getScriptInstance(ev.userDataA), ev.userDataB);
                        dispatch(getScriptInstance(ev.userDataB), ev.userDataA);
                    }

                    auto activationEvents = m_Physics3D->DrainActivationEvents();
                    for (const auto &ev : activationEvents)
                    {
                        if (ev.userData == 0)
                            continue;
                        
                        auto scriptInstance = getScriptInstance(ev.userData);
                        if (!scriptInstance)
                            continue;

                        switch (ev.type)
                        {
                            case physics::ActivationEventType::Activated:   scriptInstance->InvokeOnBodyActivated();   break;
                            case physics::ActivationEventType::Deactivated: scriptInstance->InvokeOnBodyDeactivated(); break;
                        }
                    }
                }
            }

            // Frame synchronization point: apply pending live C# hot reload
            if (ScriptEngine *se = ScriptEngine::GetInstance())
            {
                if (se->IsHotReloadPending())
                {
                    se->HotReloadAssembly();
                }
            }
        }
    }

	void Scene::OnFixedUpdateRuntimeSimulate()
	{
		{
			IGN_PROFILE_SCOPE("Scene::Script OnFixedUpdate");
			registry->view<ScriptComponent>().each([](entt::entity e, ScriptComponent &script)
			{
				if (script.runtimeScriptInstance)
					script.runtimeScriptInstance->InvokeOnFixedUpdate();
			});
		}
	}

	Ref<Scene> Scene::Create(Project *project)
    {
        return CreateRef<Scene>(project);
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

    void Scene::PreloadReferencedAssets()
    {
        if (!m_AssetManager)
        {
            m_AssetManager = AssetManager::GetInstance();
        }

        if (!m_AssetManager)
        {
            return;
        }

        const auto handles = CollectReferencedAssetHandles();
        for (AssetHandle handle : handles)
        {
            if (handle != AssetHandle(0))
            {
                Ref<Asset> asset = m_AssetManager->GetAsset<Asset>(handle);
                if (asset)
                {
                    m_LoadedAssets[handle] = asset;
                }
            }
        }
    }

	void Scene::OnAssetChangeSignal(const AssetChangeSignal &signal)
	{
        switch (signal.type)
        {
        case AssetType::AnimatorController:
        {
			auto skeletalMeshView = registry->view<SkeletalMeshComponent>();
            for (auto ent : skeletalMeshView)
            {
                auto &smc = skeletalMeshView.get<SkeletalMeshComponent>(ent);
                
                // Reset
                if (smc.runtimeAnimatorHandle == signal.handle)
                {
                    smc.runtimeAnimatorInstance.reset();
                }
            }
            break;
        }
        }
	}

	void Scene::UpdateAnimations(float deltaTime)
    {
        IGN_PROFILE_FUNCTION();

        auto skeletalMeshView = registry->view<TransformComponent, RenderingComponent, SkeletalMeshComponent>();
        std::unordered_set<AssetHandle> updatedSharedHandles;
        for (auto ent : skeletalMeshView)
        {
            const auto &[tr, rc, smc] = skeletalMeshView.get<TransformComponent, RenderingComponent, SkeletalMeshComponent>(ent);

            if (!rc.visible || smc.handle == AssetHandle(0))
                continue;

            auto mesh = m_AssetManager->GetAsset<SkeletalMesh>(smc.handle);
            
            if (!mesh)
                continue;
            
			const auto worldMatrix = tr.world.GetMatrix();
			smc.normalMatrix = glm::transpose(glm::inverse(glm::mat3(worldMatrix)));
			smc.worldAABB = mesh->localAABB.Transform(worldMatrix);

            AssetHandle sourceAnimatorHandle = mesh->GetAnimatorHandle();
			if (smc.runtimeAnimatorHandle != AssetHandle(0))
				sourceAnimatorHandle = smc.runtimeAnimatorHandle;

            // If source animator 0, reset
            if (sourceAnimatorHandle == AssetHandle(0))
            {
                smc.runtimeAnimatorInstance.reset();
                smc.runtimeParams.clear();
                smc.finalBoneTransforms.clear();
                smc.ResetAnimatorRuntime();
                continue;
            }

            // Set the runtime animator handle
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
                        smc.runtimeAnimatorInstance = AnimatorController::Clone(sourceController);
                        if (smc.runtimeAnimatorInstance)
                        {
                            smc.runtimeParams.clear();
                            smc.ResetAnimatorRuntime();
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
				IGN_PROFILE_SCOPE("Scene::Shared Animation Runtime");

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

                    if (sharedRuntime->hasRootMotion)
                    {
                        tr.local.translation += tr.local.rotation * sharedRuntime->rootMotionDelta;
                    }
                }
            }
            else
            {
                IGN_PROFILE_SCOPE("Scene::Unique Animation Runtime");

                AnimatorControllerRuntime runtime;
                runtime.currentStateName = smc.currentStateName;
                runtime.stateElapsed = smc.stateElapsed;
                runtime.stateNormalized = smc.stateNormalized;
                runtime.blendSpaceSmoothedInput = smc.blendSpaceSmoothedInput;
                runtime.blendSpaceVelocity = smc.blendSpaceVelocity;

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
                    smc.blendSpaceSmoothedInput = runtime.blendSpaceSmoothedInput;
                    smc.blendSpaceVelocity = runtime.blendSpaceVelocity;

                    if (runtime.hasRootMotion)
                    {
                        tr.local.translation += tr.local.rotation * runtime.rootMotionDelta;
                    }
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
    IGN_API void Scene::OnComponentAdded<RenderingComponent>(Entity entity, RenderingComponent &comp)
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
    IGN_API void Scene::OnComponentAdded<CharacterControllerComponent>(Entity entity, CharacterControllerComponent &comp)
    {
    }

    template<>
    IGN_API void Scene::OnComponentAdded<MeshColliderComponent>(Entity entity, MeshColliderComponent &comp)
    {
    }

    template<>
    IGN_API void Scene::OnComponentAdded<HeightFieldColliderComponent>(Entity entity, HeightFieldColliderComponent &comp)
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
    IGN_API void Scene::OnComponentAdded<TerrainComponent>(Entity entity, TerrainComponent &comp)
    {
        if (!comp.data)
        {
            comp.data = CreateRef<TerrainData>();
            if (comp.heightmapHandle != AssetHandle(0))
            {
                auto asset = AssetManager::GetInstance()->GetAssetImmediate<Asset>(comp.heightmapHandle);
                if (asset && asset->GetAssetType() == AssetType::Terrain)
                {
                    comp.data = asset->As<TerrainData>();
                }
                else if (asset && asset->GetAssetType() == AssetType::Texture)
                {
                    comp.data->LoadFromTexture(comp.heightmapHandle);
                }
                else
                {
                    comp.data->InitFlat(comp.resolution, comp.worldSize, comp.maxHeight);
                }
            }
            else
            {
                comp.data->InitFlat(comp.resolution, comp.worldSize, comp.maxHeight);
            }
        }
        comp.gpuInitialized = false;
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

    template<>
    IGN_API void Scene::OnComponentAdded<PrefabComponent>(Entity entity, PrefabComponent &comp)
    {
    }
}
