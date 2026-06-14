// Copyright (c) 2026 Evangelion Manuhutu

#include "pch.hpp"

#include "component_script_glue.hpp"
#include "ignite/core/input/input_system.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/scene/component_group.hpp"
#include "ignite/scene/scene_manager.hpp"
#include "ignite/scripting/script_engine.hpp"
#include "ignite/project/project.hpp"
#include "ignite/globals/globals.hpp"

#include "ignite/graphics/ui/widget.hpp"
#include "ignite/graphics/ui/widget_canvas.hpp"
#include "ignite/graphics/ui/widget_button.hpp"
#include "ignite/graphics/ui/widget_label.hpp"
#include "ignite/graphics/ui/widget_image.hpp"

#include "ignite/audio/fmod_sound.hpp"
#include "ignite/audio/fmod_audio.hpp"
#include "ignite/core/application.hpp"

#include "ignite/physics/jolt/jolt_physics.hpp"
#include "ignite/physics/2d/physics_2d.hpp"

#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <cctype>
#include <functional>
#include <string>
#include <string_view>
#include <typeinfo>
#include <unordered_map>
#include <vector>
#include <cstring>
#include <limits>
#include <objbase.h>

namespace ignite
{
    namespace
    {
        static Scene *GetSceneContext()
        {
            if (auto *engine = ScriptEngine::GetInstance())
            {
                return engine->GetSceneContext();
            }
            return nullptr;
        }


        // Get Ray Origin from Primary Camera Position
        // Get Ray Direction from Camera Position towards to -Z Axis
        static void Scene_GetScreenToWorldRay(float x, float y, glm::vec3 *outOrigin, glm::vec3 *outDirection)
        {
            if (outOrigin) *outOrigin = glm::vec3(0.0f);
            if (outDirection) *outDirection = glm::vec3(0.0f, 0.0f, -1.0f);

            Scene *scene = GetSceneContext();
            if (!scene)
                return;

            Entity cameraEntity = scene->GetPrimaryCamera();
            if (!cameraEntity.IsValid())
                return;

            auto &cc = cameraEntity.GetComponent<CameraComponent>();
            
            glm::vec2 viewportSize = globals::GEditor::GameViewport.max;
            if (viewportSize.x <= 0.0f || viewportSize.y <= 0.0f)
                return;

            // x, y are already viewport-relative coords (see Input_GetMousePosition)
            glm::vec2 coord = glm::vec2(x, viewportSize.y - y);

            // Ensure the pick is within viewport bounds
            if (coord.x < 0.0f || coord.y < 0.0f || coord.x > viewportSize.x || coord.y > viewportSize.y)
                return;

            glm::vec3 rayOrigin;
            glm::vec3 rayDir = Math::GetRayFromScreenCoords(coord, viewportSize, &cc.camera, &rayOrigin);

            if (outOrigin) *outOrigin = rayOrigin;
            if (outDirection) *outDirection = rayDir;
        }

        static uint64_t Scene_Raycast(const glm::vec3 *origin, const glm::vec3 *direction)
        {
            Scene *scene = GetSceneContext();
            if (!scene || !origin || !direction)
                return 0;

            float minDistance = std::numeric_limits<float>::max();
            uint64_t resultID = 0;

            // --- 3D Meshes ---
            auto meshView = scene->registry->view<MeshComponent, TransformComponent>();
            for (auto entityID : meshView)
            {
                Entity entity(entityID, scene);
                auto &transform = entity.GetComponent<TransformComponent>();
                if (!transform.visible) continue;

                auto &mesh = entity.GetComponent<MeshComponent>();
                float t;
                if (mesh.worldAABB.IntersectRay(*origin, *direction, t))
                {
                    if (t < minDistance)
                    {
                        minDistance = t;
                        resultID = (uint64_t)entity.GetUUID();
                    }
                }
            }

            // Helper for quad intersection (2D components)
            auto CheckQuad = [&](Entity entity, glm::vec2 size)
            {
                auto &transform = entity.GetComponent<TransformComponent>();
                if (!transform.visible) return;

                glm::vec3 pos = transform.translation;
                glm::quat rot = transform.rotation;
                glm::vec3 scale = transform.scale;
                
                glm::vec3 halfSize = glm::vec3(size.x * 0.5f, size.y * 0.5f, 0.0f) * scale;

                glm::vec3 v0 = pos + rot * glm::vec3(-halfSize.x, -halfSize.y, 0.0f);
                glm::vec3 v1 = pos + rot * glm::vec3( halfSize.x, -halfSize.y, 0.0f);
                glm::vec3 v2 = pos + rot * glm::vec3( halfSize.x,  halfSize.y, 0.0f);
                glm::vec3 v3 = pos + rot * glm::vec3(-halfSize.x,  halfSize.y, 0.0f);

                float t;
                if (Math::RayQuadIntersection(*origin, *direction, v0, v1, v2, v3, t))
                {
                    if (t < minDistance)
                    {
                        minDistance = t;
                        resultID = (uint64_t)entity.GetUUID();
                    }
                }
            };

            auto spriteView = scene->registry->view<Sprite2DComponent, TransformComponent>();
            for (auto entityID : spriteView)
            {
                CheckQuad(Entity(entityID, scene), { 1.0f, 1.0f });
            }

            auto circleView = scene->registry->view<Circle2DComponent, TransformComponent>();
            for (auto entityID : circleView)
            {
                CheckQuad(Entity(entityID, scene), { 1.0f, 1.0f });
            }

            auto textView = scene->registry->view<TextComponent, TransformComponent>();
            for (auto entityID : textView)
            {
                CheckQuad(Entity(entityID, scene), { 1.0f, 1.0f });
            }

            return resultID;
        }

        // Physics (Jolt narrow-phase) raycast - returns first solid-body hit
        // outHitEntityID: UUID of the hit entity (0 if none)
        // outHitPoint, outHitNormal: world-space hit info
        static uint64_t Scene_PhysicsRaycast(const glm::vec3 *origin, const glm::vec3 *direction, float maxDistance, glm::vec3 *outHitPoint, glm::vec3 *outHitNormal)
        {
            if (outHitPoint) *outHitPoint = glm::vec3(0.f);
            if (outHitNormal) *outHitNormal = glm::vec3(0.f, 1.f, 0.f);

            Scene *scene = GetSceneContext();
            if (!scene || !scene->physics || !origin || !direction)
                return 0;

            JoltRaycastHit hit = scene->physics->Raycast(*origin, *direction, maxDistance);
            if (!hit.hit)
                return 0;

            if (outHitPoint)
                *outHitPoint = hit.hitPoint;
            
            if (outHitNormal)
                *outHitNormal = hit.hitNormal;

            // Resolve body ID to entity UUID directly from the Jolt body user data.
            const uint64_t entityID = static_cast<uint64_t>(scene->physics->GetUserData(hit.bodyId));
            return entityID;
        }

        static uint64_t Scene_GetPrimaryCamera()
        {
            Scene *scene = GetSceneContext();
            if (!scene)
                return 0;

            Entity cameraEntity = scene->GetPrimaryCamera();
            return cameraEntity.IsValid() ? static_cast<uint64_t>(cameraEntity.GetUUID()) : 0;
        }

        static Entity GetEntityByID(uint64_t entityID)
        {
            Scene *scene = GetSceneContext();
            if (!scene)
            {
                return {};
            }

            Entity entity = SceneManager::GetEntity(scene, UUID(entityID));
            if (entity.IsValid())
            {
                return entity;
            }

            auto view = scene->registry->view<IDComponent>();
            for (entt::entity e : view)
            {
                const IDComponent &id = view.get<IDComponent>(e);
                if (static_cast<uint64_t>(id.uuid) == entityID)
                {
                    return Entity { e, scene };
                }
            }

            return {};
        }

        static Ref<FmodSound> GetAudioSourceSound(Entity entity)
        {
            if (!entity.IsValid() || !entity.HasComponent<AudioSourceComponent>())
            {
                return nullptr;
            }

            Scene *scene = GetSceneContext();
            if (!scene)
            {
                return nullptr;
            }

            AssetManager *assetManager = scene->GetAssetManager();
            if (!assetManager)
            {
                return nullptr;
            }

            const auto &audioSource = entity.GetComponent<AudioSourceComponent>();
            if (audioSource.handle == AssetHandle(0))
            {
                return nullptr;
            }

            return assetManager->GetAsset<FmodSound>(audioSource.handle);
        }

        static FMOD::DSP *CreateAudioSourceDsp(const AudioSourceComponent::DspSettings &settings)
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

            if (FmodAudio::GetFmodSystem()->createDSPByType(dspType, &dsp) != FMOD_OK || !dsp)
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

        static void RebuildAudioSourceDspChain(Entity entity, const Ref<FmodSound> &sound)
        {
            if (!entity.IsValid() || !entity.HasComponent<AudioSourceComponent>() || !sound)
            {
                return;
            }

            const auto &audioSource = entity.GetComponent<AudioSourceComponent>();
            sound->ClearDsps(true);
            for (const auto &dspSettings : audioSource.dsps)
            {
                if (FMOD::DSP *dsp = CreateAudioSourceDsp(dspSettings))
                {
                    sound->AddDsp(dsp);
                }
            }
        }

        static std::unordered_map<std::string, std::function<bool(Entity)>> s_EntityHasComponentFuncs;
        static std::unordered_map<std::string, std::function<void(Entity)>> s_EntityAddComponentFuncs;

        enum class WidgetButtonEventType : int32_t
        {
            Click = 0,
            Pressed = 1,
            Released = 2,
            HoverEnter = 3,
            HoverExit = 4,
        };

        struct WidgetButtonEventKey
        {
            uint64_t entityID = 0;
            std::string buttonName;
            WidgetButtonEventType eventType = WidgetButtonEventType::Click;

            bool operator==(const WidgetButtonEventKey &other) const
            {
                return entityID == other.entityID && buttonName == other.buttonName && eventType == other.eventType;
            }
        };

        struct WidgetButtonEventKeyHash
        {
            size_t operator()(const WidgetButtonEventKey &key) const
            {
                size_t hash = std::hash<uint64_t> {}(key.entityID);
                hash ^= std::hash<std::string> {}(key.buttonName) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
                hash ^= std::hash<int32_t> {}(static_cast<int32_t>(key.eventType)) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
                return hash;
            }
        };

        struct ScriptWidgetCallbackBinding
        {
            std::string methodName;
            int methodId = 0;
        };

        static std::unordered_map<WidgetButtonEventKey, std::vector<ScriptWidgetCallbackBinding>, WidgetButtonEventKeyHash> s_WidgetButtonEventBindings;

        static std::string TrimString(std::string value)
        {
            value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char ch) { return !std::isspace(ch); }));
            value.erase(std::find_if(value.rbegin(), value.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), value.end());
            return value;
        }

        static std::string NormalizeManagedTypeName(const char *componentTypeName)
        {
            if (!componentTypeName)
            {
                return {};
            }

            std::string typeName(componentTypeName);

            if (const size_t comma = typeName.find(','); comma != std::string::npos)
            {
                typeName = typeName.substr(0, comma);
            }

            typeName = TrimString(typeName);

            const size_t plus = typeName.find_last_of('+');
            if (plus != std::string::npos)
            {
                typeName = typeName.substr(plus + 1);
            }

            const size_t dot = typeName.find_last_of('.');
            if (dot != std::string::npos)
            {
                typeName = typeName.substr(dot + 1);
            }

            if (const size_t genericMarker = typeName.find('`'); genericMarker != std::string::npos)
            {
                typeName = typeName.substr(0, genericMarker);
            }

            return typeName;
        }

        static std::string GetNativeComponentName(std::string typeName)
        {
            const size_t separator = typeName.find_last_of(':');
            if (separator != std::string::npos)
            {
                typeName = typeName.substr(separator + 1);
            }

            typeName = TrimString(typeName);

            constexpr std::string_view classPrefix = "class ";
            if (typeName.rfind(classPrefix, 0) == 0)
            {
                typeName = typeName.substr(classPrefix.size());
            }

            constexpr std::string_view structPrefix = "struct ";
            if (typeName.rfind(structPrefix, 0) == 0)
            {
                typeName = typeName.substr(structPrefix.size());
            }

            return TrimString(typeName);
        }

        static std::string GetManagedComponentName(std::string nativeComponentName)
        {
            constexpr std::string_view suffix = "Component";
            if (nativeComponentName.size() > suffix.size() && nativeComponentName.ends_with(suffix))
            {
                nativeComponentName = nativeComponentName.substr(0, nativeComponentName.size() - suffix.size());
            }

            return nativeComponentName;
        }

        template<typename... Component>
        static void RegisterComponent()
        {
            (([]()
            {
                std::string nativeTypeName = GetNativeComponentName(typeid(Component).name());
                std::string managedTypeName = GetManagedComponentName(nativeTypeName);

                const auto hasComponentFunc = [](Entity entity) { return entity.HasComponent<Component>(); };
                const auto addComponentFunc = [](Entity entity) { entity.AddOrReplaceComponent<Component>(); };

                s_EntityHasComponentFuncs[managedTypeName] = hasComponentFunc;
                s_EntityAddComponentFuncs[managedTypeName] = addComponentFunc;

                s_EntityHasComponentFuncs[nativeTypeName] = hasComponentFunc;
                s_EntityAddComponentFuncs[nativeTypeName] = addComponentFunc;
            }()), ...);
        }

        template<typename... Component>
        static void RegisterComponent(ComponentGroup<Component...>)
        {
            RegisterComponent<Component...>();
        }

        static bool TryParseWidgetButtonEventType(int32_t eventType, WidgetButtonEventType &outEventType)
        {
            switch (eventType)
            {
                case static_cast<int32_t>(WidgetButtonEventType::Click):
                case static_cast<int32_t>(WidgetButtonEventType::Pressed):
                case static_cast<int32_t>(WidgetButtonEventType::Released):
                case static_cast<int32_t>(WidgetButtonEventType::HoverEnter):
                case static_cast<int32_t>(WidgetButtonEventType::HoverExit):
                    outEventType = static_cast<WidgetButtonEventType>(eventType);
                    return true;
                default:
                    return false;
            }
        }

        static Ref<WidgetCanvas> GetEntityWidgetCanvas(Entity entity)
        {
            if (!entity.IsValid() || !entity.HasComponent<WidgetComponent>())
            {
                return nullptr;
            }

            Scene *scene = GetSceneContext();
            if (!scene)
            {
                return nullptr;
            }

            AssetManager *assetManager = scene->GetAssetManager();
            if (!assetManager)
            {
                return nullptr;
            }

            const auto &widgetComponent = entity.GetComponent<WidgetComponent>();
            if (widgetComponent.widgetHandle == AssetHandle(0))
            {
                return nullptr;
            }

            return assetManager->GetAsset<WidgetCanvas>(widgetComponent.widgetHandle);
        }

        static Ref<WidgetButton> FindWidgetButton(Entity entity, const std::string &buttonName)
        {
            if (buttonName.empty())
            {
                return nullptr;
            }

            Ref<WidgetCanvas> widgetCanvas = GetEntityWidgetCanvas(entity);
            if (!widgetCanvas)
            {
                return nullptr;
            }

            for (const auto &[_, item] : widgetCanvas->GetItems())
            {
                if (!item || item->GetWidgetType() != WidgetType::Button)
                {
                    continue;
                }

                if (item->name != buttonName)
                {
                    continue;
                }

                return item->As<WidgetButton>();
            }

            return nullptr;
        }

        static void InvokeWidgetButtonCallbacks(const WidgetButtonEventKey &key)
        {
            const auto bindingsIt = s_WidgetButtonEventBindings.find(key);
            if (bindingsIt == s_WidgetButtonEventBindings.end() || bindingsIt->second.empty())
            {
                return;
            }

            ScriptEngine *scriptEngine = ScriptEngine::GetInstance();
            if (!scriptEngine)
            {
                return;
            }

            ScriptHost *scriptHost = scriptEngine->GetScriptHost();
            if (!scriptHost)
            {
                return;
            }

            for (const ScriptWidgetCallbackBinding &binding : bindingsIt->second)
            {
                if (binding.methodId == 0)
                {
                    continue;
                }

                scriptHost->Invoke(binding.methodId, nullptr, 0, nullptr);
            }
        }

        static void ApplyWidgetButtonCallback(Ref<WidgetButton> button, const WidgetButtonEventKey &key)
        {
            if (!button)
            {
                return;
            }

            const auto bindingsIt = s_WidgetButtonEventBindings.find(key);
            const bool hasBindings = bindingsIt != s_WidgetButtonEventBindings.end() && !bindingsIt->second.empty();

            const auto callback = hasBindings
                ? std::function<void()>([key]() { InvokeWidgetButtonCallbacks(key); })
                : std::function<void()> {};

            switch (key.eventType)
            {
                case WidgetButtonEventType::Click:
                    button->SetOnClick(callback);
                    break;
                case WidgetButtonEventType::Pressed:
                    button->SetOnPressed(callback);
                    break;
                case WidgetButtonEventType::Released:
                    button->SetOnReleased(callback);
                    break;
                case WidgetButtonEventType::HoverEnter:
                    button->SetOnHoverEnter(callback);
                    break;
                case WidgetButtonEventType::HoverExit:
                    button->SetOnHoverExit(callback);
                    break;
            }
        }

        static bool Entity_HasComponent(uint64_t entityID, const char *componentTypeName)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid())
            {
                return false;
            }

            const std::string typeName = NormalizeManagedTypeName(componentTypeName);
            if (typeName.empty())
            {
                return false;
            }

            const auto hasComponentIt = s_EntityHasComponentFuncs.find(typeName);
            if (hasComponentIt == s_EntityHasComponentFuncs.end())
            {
                return false;
            }

            return hasComponentIt->second(entity);
        }

        static void Entity_AddComponent(uint64_t entityID, const char *componentTypeName)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid())
            {
                return;
            }

            const std::string typeName = NormalizeManagedTypeName(componentTypeName);
            if (typeName.empty())
            {
                return;
            }

            const auto addComponentIt = s_EntityAddComponentFuncs.find(typeName);
            if (addComponentIt == s_EntityAddComponentFuncs.end())
            {
                return;
            }

            addComponentIt->second(entity);
        }

        static uint64_t Entity_FindEntityByName(const char *name)
        {
            Scene *scene = GetSceneContext();
            if (!scene || !name)
            {
                return 0;
            }

            Entity entity = SceneManager::GetEntity(scene, std::string(name));
            return entity.IsValid() ? static_cast<uint64_t>(entity.GetUUID()) : 0;
        }

        static uint64_t Entity_FindChildEntityByName(uint64_t entityID, const char *childName)
        {
            Scene *scene = GetSceneContext();
            if (!scene || entityID == 0 || !childName)
                return 0;

            Entity entity = SceneManager::GetEntity(scene, UUID(entityID));
            Entity childEntity = SceneManager::GetEntity(scene, childName);
            SceneManager::FindChild(scene, entity, childEntity.GetUUID());

            return childEntity.IsValid() ? static_cast<uint64_t>(childEntity.GetUUID()) : 0;
        }

        static bool Entity_IsParent(uint64_t entityID, uint64_t parentID)
        {
            Scene *scene = GetSceneContext();
            if (!scene || entityID == 0)
                return false;

            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid())
                return false;

            return entity.GetParentUUID() == UUID(parentID);
        }

        static uint64_t Entity_GetParent(uint64_t entityID)
        {
            Scene *scene = GetSceneContext();
            if (!scene || entityID == 0)
                return 0;

            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid())
                return 0;

            return static_cast<uint64_t>(entity.GetParentUUID());
        }

        static uint64_t Entity_InstantiateWithName(const char *name, const glm::vec3 *value)
        {
            Scene *scene = GetSceneContext();
            if (!scene || !value)
                return 0;

            Entity entity = SceneManager::CreateEmptyEntity(scene, name);
            entity.GetComponent<TransformComponent>().translation = *value;

            if (scene->IsRunning())
            {
                auto *scriptEngine = ScriptEngine::GetInstance();
                if (entity.HasComponent<ScriptComponent>())
                {
                    const auto &sc = entity.GetComponent<ScriptComponent>();
                    const ScriptInstanceID instanceID = entity.GetUUID();
                    scriptEngine->OnCreateEntityInstance(instanceID, sc.className);
                }
            }

            return static_cast<uint64_t>(entity.GetUUID());
        }

        static uint64_t Entity_Instantiate(uint64_t entityID, const glm::vec3 *value)
        {
            Scene *scene = GetSceneContext();
            if (!scene || !value)
                return 0;

            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid())
                return 0;

            Entity copyEntity = SceneManager::DuplicateEntity(scene, entity);
            if (!copyEntity.IsValid())
                return 0;

            // If physics is running, ensure we set transform before physics instantiation happens.
            // DuplicateEntity will call scene->physics->InstantiateEntity() when scene->IsRunning().
            // Move the entity to the requested world position and clear any existing runtime physics body
            // on the copied entity before returning.
            copyEntity.GetComponent<TransformComponent>().SetWorldTranslation(*value);

            // If duplicated entity has a RigidbodyComponent, ensure no stale body pointer is present
            // if (copyEntity.HasComponent<RigidbodyComponent>())
            // {
            //     auto &rb = copyEntity.GetComponent<RigidbodyComponent>();
            //     if (rb.body)
            //     {
            //         // Remove and destroy any stale body attached to this component to avoid duplicates
            //         if (scene->physics && scene->physics->GetBodyInterface())
            //         {
            //             scene->physics->GetBodyInterface()->RemoveBody(rb.body->GetID());
            //             scene->physics->GetBodyInterface()->DestroyBody(rb.body->GetID());
            //         }
            //         rb.body = nullptr;
            //     }
            // }

            if (scene->IsRunning())
            {
                auto *scriptEngine = ScriptEngine::GetInstance();
                if (copyEntity.HasComponent<ScriptComponent>())
                {
                    auto &sc = copyEntity.GetComponent<ScriptComponent>();
                    const ScriptInstanceID instanceID = copyEntity.GetUUID();
                    sc.runtimeScriptInstance = scriptEngine->OnCreateEntityInstance(instanceID, sc.className);
                }
            }

            return static_cast<uint64_t>(copyEntity.GetUUID());
        }

        static void Entity_Destroy(uint64_t entityID)
        {
            Scene *scene = GetSceneContext();
            if (!scene)
                return;

            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid())
                return;

            if (scene->IsRunning())
            {
                auto *scriptEngine = ScriptEngine::GetInstance();
                if (entity.HasComponent<ScriptComponent>())
                {
                    auto &sc = entity.GetComponent<ScriptComponent>();
                    sc.runtimeScriptInstance = nullptr;
                    const ScriptInstanceID instanceID = entity.GetUUID();
                    ScriptEngine::GetInstance()->OnDestroyEntityInstance(instanceID);
                }
            }

            std::erase_if(s_WidgetButtonEventBindings, [entityID](const auto &entry)
            {
                return entry.first.entityID == entityID;
            });

            SceneManager::DestroyEntity(scene, entity);
        }

        static void Entity_SetVisibility(uint64_t entityID, bool value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TransformComponent>())
                return;

            entity.GetComponent<TransformComponent>().visible = value;
        }

        static void Entity_GetVisibility(uint64_t entityID, bool *result)
        {
            if (!result)
                return;

            *result = false;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TransformComponent>())
                return;

            *result = entity.GetComponent<TransformComponent>().visible;
        }

        static const char *Entity_GetName(uint64_t entityID)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid())
                return nullptr;
            return entity.GetName().c_str();
        }

        static bool WidgetComponent_HasButton(uint64_t entityID, const char *buttonName)
        {
            if (!buttonName)
                return false;

            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid())
                return false;

            return static_cast<bool>(FindWidgetButton(entity, std::string(buttonName)));
        }

        static bool WidgetComponent_AddButtonEventCallback(uint64_t entityID, const char *buttonName, int32_t eventType, const char *methodName)
        {
            if (!buttonName || !methodName)
            {
                return false;
            }

            const std::string resolvedButtonName = TrimString(buttonName);
            const std::string resolvedMethodName = TrimString(methodName);
            if (resolvedButtonName.empty() || resolvedMethodName.empty())
            {
                return false;
            }

            WidgetButtonEventType resolvedEventType;
            if (!TryParseWidgetButtonEventType(eventType, resolvedEventType))
            {
                return false;
            }

            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid())
            {
                return false;
            }

            Ref<WidgetButton> button = FindWidgetButton(entity, resolvedButtonName);
            if (!button)
            {
                return false;
            }

            ScriptEngine *scriptEngine = ScriptEngine::GetInstance();
            if (!scriptEngine)
            {
                return false;
            }

            Ref<ScriptInstance> scriptInstance = scriptEngine->GetEntityScriptInstance(entity.GetUUID());
            if (!scriptInstance || !scriptInstance->GetScriptClass())
            {
                return false;
            }

            const int methodId = scriptInstance->GetScriptClass()->BindInstanceMethod(scriptInstance->GetInstanceID(), resolvedMethodName);
            if (methodId == 0)
            {
                LOG_WARN("[ScriptGlue] Failed to bind widget callback '{}.{}'", entityID, resolvedMethodName);
                return false;
            }

            const WidgetButtonEventKey key { entityID, resolvedButtonName, resolvedEventType };
            s_WidgetButtonEventBindings[key].push_back({ resolvedMethodName, methodId });
            ApplyWidgetButtonCallback(button, key);
            return true;
        }

        static bool WidgetComponent_RemoveButtonEventCallback(uint64_t entityID, const char *buttonName, int32_t eventType, const char *methodName)
        {
            if (!buttonName || !methodName)
            {
                return false;
            }

            const std::string resolvedButtonName = TrimString(buttonName);
            const std::string resolvedMethodName = TrimString(methodName);
            if (resolvedButtonName.empty() || resolvedMethodName.empty())
            {
                return false;
            }

            WidgetButtonEventType resolvedEventType;
            if (!TryParseWidgetButtonEventType(eventType, resolvedEventType))
            {
                return false;
            }

            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid())
            {
                return false;
            }

            Ref<WidgetButton> button = FindWidgetButton(entity, resolvedButtonName);
            if (!button)
            {
                return false;
            }

            const WidgetButtonEventKey key { entityID, resolvedButtonName, resolvedEventType };
            const auto bindingsIt = s_WidgetButtonEventBindings.find(key);
            if (bindingsIt == s_WidgetButtonEventBindings.end())
            {
                return false;
            }

            auto &bindings = bindingsIt->second;
            const auto removeIt = std::find_if(bindings.begin(), bindings.end(), [&](const ScriptWidgetCallbackBinding &binding)
            {
                return binding.methodName == resolvedMethodName;
            });

            if (removeIt == bindings.end())
            {
                return false;
            }

            bindings.erase(removeIt);
            if (bindings.empty())
            {
                s_WidgetButtonEventBindings.erase(bindingsIt);
            }

            ApplyWidgetButtonCallback(button, key);
            return true;
        }

        // =====================================================================
        // Widget Label / Image helper finders
        // =====================================================================

        static Ref<WidgetLabel> FindWidgetLabel(Entity entity, const std::string &name)
        {
            Ref<WidgetCanvas> canvas = GetEntityWidgetCanvas(entity);
            if (!canvas) return nullptr;
            for (const auto &[_, item] : canvas->GetItems())
                if (item && item->name == name && item->GetWidgetType() == WidgetType::Label)
                    return item->As<WidgetLabel>();
            return nullptr;
        }

        static Ref<WidgetImage> FindWidgetImage(Entity entity, const std::string &name)
        {
            Ref<WidgetCanvas> canvas = GetEntityWidgetCanvas(entity);
            if (!canvas) return nullptr;
            for (const auto &[_, item] : canvas->GetItems())
                if (item && item->name == name && item->GetWidgetType() == WidgetType::Image)
                    return item->As<WidgetImage>();
            return nullptr;
        }

        // --- Label ---
        static bool WidgetComponent_HasLabel(uint64_t entityID, const char *labelName)
        {
            if (!labelName) return false;
            return static_cast<bool>(FindWidgetLabel(GetEntityByID(entityID), std::string(labelName)));
        }

        static void WidgetComponent_GetLabelText(uint64_t entityID, const char *labelName, const char **result)
        {
            if (!labelName || !result) return;
            if (Ref<WidgetLabel> lbl = FindWidgetLabel(GetEntityByID(entityID), std::string(labelName)))
                *result = lbl->text.c_str();
        }

        static void WidgetComponent_SetLabelText(uint64_t entityID, const char *labelName, const char *text)
        {
            if (!labelName || !text) return;
            Entity entity = GetEntityByID(entityID);
            if (Ref<WidgetLabel> lbl = FindWidgetLabel(entity, std::string(labelName)))
            {
                lbl->text = text;
                if (Ref<WidgetCanvas> c = GetEntityWidgetCanvas(entity)) c->SetDirtyFlag(true);
            }
        }

        static void WidgetComponent_GetLabelColor(uint64_t entityID, const char *labelName, glm::vec4 *result)
        {
            if (!labelName || !result) return;
            if (Ref<WidgetLabel> lbl = FindWidgetLabel(GetEntityByID(entityID), std::string(labelName)))
                *result = lbl->style.color;
        }

        static void WidgetComponent_SetLabelColor(uint64_t entityID, const char *labelName, glm::vec4 *color)
        {
            if (!labelName || !color) return;
            Entity entity = GetEntityByID(entityID);
            if (Ref<WidgetLabel> lbl = FindWidgetLabel(entity, std::string(labelName)))
            {
                lbl->style.color = *color;
                if (Ref<WidgetCanvas> c = GetEntityWidgetCanvas(entity)) c->SetDirtyFlag(true);
            }
        }

        static void WidgetComponent_GetLabelFontSize(uint64_t entityID, const char *labelName, float *result)
        {
            if (!labelName || !result) return;
            if (Ref<WidgetLabel> lbl = FindWidgetLabel(GetEntityByID(entityID), std::string(labelName)))
                *result = lbl->style.fontSize;
        }

        static void WidgetComponent_SetLabelFontSize(uint64_t entityID, const char *labelName, float size)
        {
            if (!labelName) return;
            Entity entity = GetEntityByID(entityID);
            if (Ref<WidgetLabel> lbl = FindWidgetLabel(entity, std::string(labelName)))
            {
                lbl->style.fontSize = size;
                if (Ref<WidgetCanvas> c = GetEntityWidgetCanvas(entity)) c->SetDirtyFlag(true);
            }
        }

        // --- Image ---
        static bool WidgetComponent_HasImage(uint64_t entityID, const char *imageName)
        {
            if (!imageName) return false;
            return static_cast<bool>(FindWidgetImage(GetEntityByID(entityID), std::string(imageName)));
        }

        static void WidgetComponent_GetImageHandle(uint64_t entityID, const char *imageName, uint64_t *result)
        {
            if (!imageName || !result) return;
            if (Ref<WidgetImage> img = FindWidgetImage(GetEntityByID(entityID), std::string(imageName)))
                *result = static_cast<uint64_t>(img->imageHandle);
        }

        static void WidgetComponent_SetImageHandle(uint64_t entityID, const char *imageName, uint64_t handle)
        {
            if (!imageName) return;
            Entity entity = GetEntityByID(entityID);
            if (Ref<WidgetImage> img = FindWidgetImage(entity, std::string(imageName)))
            {
                img->imageHandle = AssetHandle(handle);
                img->image = nullptr;
                if (Ref<WidgetCanvas> c = GetEntityWidgetCanvas(entity)) c->SetDirtyFlag(true);
            }
        }

        static bool AudioSourceComponent_HasAudio(uint64_t entityID)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<AudioSourceComponent>())
            {
                return false;
            }

            return entity.GetComponent<AudioSourceComponent>().handle != AssetHandle(0);
        }

        static void AudioSourceComponent_Play(uint64_t entityID)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<AudioSourceComponent>())
            {
                return;
            }

            auto &audioSource = entity.GetComponent<AudioSourceComponent>();
            Ref<FmodSound> sound = GetAudioSourceSound(entity);
            if (!sound)
            {
                return;
            }

            RebuildAudioSourceDspChain(entity, sound);
            sound->Play();
            sound->SetVolume(audioSource.volume);
            sound->SetPitch(audioSource.pitch);
            sound->SetPan(audioSource.pan);
            sound->SetMode(audioSource.loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);
        }

        static void AudioSourceComponent_Stop(uint64_t entityID)
        {
            Entity entity = GetEntityByID(entityID);
            Ref<FmodSound> sound = GetAudioSourceSound(entity);
            if (!sound)
            {
                return;
            }

            sound->Stop();
        }

        static void AudioSourceComponent_Pause(uint64_t entityID)
        {
            Entity entity = GetEntityByID(entityID);
            Ref<FmodSound> sound = GetAudioSourceSound(entity);
            if (!sound)
            {
                return;
            }

            sound->Pause();
        }

        static void AudioSourceComponent_Resume(uint64_t entityID)
        {
            Entity entity = GetEntityByID(entityID);
            Ref<FmodSound> sound = GetAudioSourceSound(entity);
            if (!sound)
            {
                return;
            }

            sound->Resume();
        }

        static void AudioSourceComponent_GetVolume(uint64_t entityID, float *result)
        {
            if (!result)
            {
                return;
            }

            *result = 1.0f;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<AudioSourceComponent>())
                return;

            *result = entity.GetComponent<AudioSourceComponent>().volume;
        }

        static void AudioSourceComponent_SetVolume(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<AudioSourceComponent>())
                return;

            auto &audioSource = entity.GetComponent<AudioSourceComponent>();
            audioSource.volume = value;

            if (Ref<FmodSound> sound = GetAudioSourceSound(entity))
            {
                sound->SetVolume(audioSource.volume);
            }
        }

        static void AudioSourceComponent_GetPitch(uint64_t entityID, float *result)
        {
            if (!result)
            {
                return;
            }

            *result = 1.0f;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<AudioSourceComponent>())
                return;

            *result = entity.GetComponent<AudioSourceComponent>().pitch;
        }

        static void AudioSourceComponent_SetPitch(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<AudioSourceComponent>())
                return;

            auto &audioSource = entity.GetComponent<AudioSourceComponent>();
            audioSource.pitch = value;

            if (Ref<FmodSound> sound = GetAudioSourceSound(entity))
            {
                sound->SetPitch(audioSource.pitch);
            }
        }

        static void AudioSourceComponent_GetPan(uint64_t entityID, float *result)
        {
            if (!result)
            {
                return;
            }

            *result = 0.0f;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<AudioSourceComponent>())
                return;

            *result = entity.GetComponent<AudioSourceComponent>().pan;
        }

        static void AudioSourceComponent_SetPan(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<AudioSourceComponent>())
                return;

            auto &audioSource = entity.GetComponent<AudioSourceComponent>();
            audioSource.pan = value;

            if (Ref<FmodSound> sound = GetAudioSourceSound(entity))
            {
                sound->SetPan(audioSource.pan);
            }
        }

        static void AudioSourceComponent_GetPlayOnStart(uint64_t entityID, bool *result)
        {
            if (!result)
            {
                return;
            }

            *result = false;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<AudioSourceComponent>())
                return;

            *result = entity.GetComponent<AudioSourceComponent>().playOnStart;
        }

        static void AudioSourceComponent_SetPlayOnStart(uint64_t entityID, bool value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<AudioSourceComponent>())
                return;

            entity.GetComponent<AudioSourceComponent>().playOnStart = value;
        }

        static void AudioSourceComponent_GetLoop(uint64_t entityID, bool *result)
        {
            if (!result)
            {
                return;
            }

            *result = false;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<AudioSourceComponent>())
                return;

            *result = entity.GetComponent<AudioSourceComponent>().loop;
        }

        static void AudioSourceComponent_SetLoop(uint64_t entityID, bool value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<AudioSourceComponent>())
                return;

            auto &audioSource = entity.GetComponent<AudioSourceComponent>();
            audioSource.loop = value;

            if (Ref<FmodSound> sound = GetAudioSourceSound(entity))
            {
                sound->SetMode(audioSource.loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);
            }
        }

        static bool AudioSourceComponent_AddReverbDSP(uint64_t entityID, float decayTime, float earlyDelay, float lateDelay,
            float highFrequencyReference, float diffusion, float density, float lowShelfGain, float highCut, float dryLevel, float wetLevel)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<AudioSourceComponent>())
                return false;

            auto &audioSource = entity.GetComponent<AudioSourceComponent>();
            AudioSourceComponent::DspSettings dsp;
            dsp.type = AudioSourceComponent::DspType::Reverb;
            dsp.reverbDecayTime = decayTime;
            dsp.reverbEarlyDelay = earlyDelay;
            dsp.reverbLateDelay = lateDelay;
            dsp.reverbHighFrequencyReference = highFrequencyReference;
            dsp.reverbDiffusion = diffusion;
            dsp.reverbDensity = density;
            dsp.reverbLowShelfGain = lowShelfGain;
            dsp.reverbHighCut = highCut;
            dsp.reverbDryLevel = dryLevel;
            dsp.reverbWetLevel = wetLevel;
            audioSource.dsps.push_back(dsp);

            if (Ref<FmodSound> sound = GetAudioSourceSound(entity))
            {
                RebuildAudioSourceDspChain(entity, sound);
            }

            return true;
        }

        static bool AudioSourceComponent_AddDistortionDSP(uint64_t entityID, float distortionLevel)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<AudioSourceComponent>())
                return false;

            auto &audioSource = entity.GetComponent<AudioSourceComponent>();
            AudioSourceComponent::DspSettings dsp;
            dsp.type = AudioSourceComponent::DspType::Distortion;
            dsp.distortionLevel = distortionLevel;
            audioSource.dsps.push_back(dsp);

            if (Ref<FmodSound> sound = GetAudioSourceSound(entity))
            {
                RebuildAudioSourceDspChain(entity, sound);
            }

            return true;
        }

        static bool AudioSourceComponent_AddChorusDSP(uint64_t entityID, float mix, float rate, float depth)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<AudioSourceComponent>())
                return false;

            auto &audioSource = entity.GetComponent<AudioSourceComponent>();
            AudioSourceComponent::DspSettings dsp;
            dsp.type = AudioSourceComponent::DspType::Chorus;
            dsp.chorusMix = mix;
            dsp.chorusRate = rate;
            dsp.chorusDepth = depth;
            audioSource.dsps.push_back(dsp);

            if (Ref<FmodSound> sound = GetAudioSourceSound(entity))
            {
                RebuildAudioSourceDspChain(entity, sound);
            }

            return true;
        }

        static bool AudioSourceComponent_AddCompressorDSP(uint64_t entityID, float threshold, float ratio, float release, float gainMakeup, bool useSidechain)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<AudioSourceComponent>())
                return false;

            auto &audioSource = entity.GetComponent<AudioSourceComponent>();
            AudioSourceComponent::DspSettings dsp;
            dsp.type = AudioSourceComponent::DspType::Compressor;
            dsp.compressorThreshold = threshold;
            dsp.compressorRatio = ratio;
            dsp.compressorRelease = release;
            dsp.compressorGainMakeup = gainMakeup;
            dsp.compressorUseSidechain = useSidechain;
            audioSource.dsps.push_back(dsp);

            if (Ref<FmodSound> sound = GetAudioSourceSound(entity))
            {
                RebuildAudioSourceDspChain(entity, sound);
            }

            return true;
        }

        static bool AudioSourceComponent_AddDelayDSP(uint64_t entityID, float delayMs, float feedback)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<AudioSourceComponent>())
                return false;

            auto &audioSource = entity.GetComponent<AudioSourceComponent>();
            AudioSourceComponent::DspSettings dsp;
            dsp.type = AudioSourceComponent::DspType::Delay;
            dsp.delayMs = delayMs;
            dsp.delayFeedback = feedback;
            audioSource.dsps.push_back(dsp);

            if (Ref<FmodSound> sound = GetAudioSourceSound(entity))
            {
                RebuildAudioSourceDspChain(entity, sound);
            }

            return true;
        }

        static void AudioSourceComponent_ClearDSPs(uint64_t entityID)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<AudioSourceComponent>())
                return;

            auto &audioSource = entity.GetComponent<AudioSourceComponent>();
            audioSource.dsps.clear();

            if (Ref<FmodSound> sound = GetAudioSourceSound(entity))
            {
                sound->ClearDsps(true);
            }
        }

        static void TransformComponent_GetForward(uint64_t entityID, glm::vec3 *result)
        {
            if (!result)
            {
                return;
            }

            *result = {};
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TransformComponent>())
                return;

            const auto &transform = entity.GetComponent<TransformComponent>();
            *result = glm::vec3(transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f));
        }

        static void TransformComponent_SetForward(uint64_t entityID, const glm::vec3 *value)
        {
            if (!value) return;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TransformComponent>())
                return;

            glm::vec3 forward = glm::normalize(*value);
            if (glm::length2(forward) <= 0.0f)
            {
                return;
            }

            auto &transform = entity.GetComponent<TransformComponent>();
            const glm::quat rotation = glm::quatLookAtRH(forward, glm::vec3(0.0f, 1.0f, 0.0f));
            transform.localRotation = rotation;
            transform.rotation = rotation;
            transform.dirty = true;
        }

        static void TransformComponent_GetRight(uint64_t entityID, glm::vec3 *result)
        {
            if (!result)
            {
                return;
            }

            *result = {};
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TransformComponent>())
                return;

            const auto &transform = entity.GetComponent<TransformComponent>();
            *result = glm::vec3(transform.rotation * glm::vec3(1.0f, 0.0f, 0.0f));
        }

        static void TransformComponent_SetRight(uint64_t entityID, const glm::vec3 *value)
        {
            if (!value) return;
            glm::vec3 right = glm::normalize(*value);
            if (glm::length2(right) <= 0.0f)
            {
                return;
            }

            glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
            glm::vec3 forward = glm::normalize(glm::cross(worldUp, right));
            if (glm::length2(forward) <= 0.0f)
            {
                return;
            }

            TransformComponent_SetForward(entityID, &forward);
        }

        static void TransformComponent_GetUp(uint64_t entityID, glm::vec3 *result)
        {
            if (!result)
            {
                return;
            }

            *result = {};
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TransformComponent>())
                return;

            const auto &transform = entity.GetComponent<TransformComponent>();
            *result = glm::vec3(transform.rotation * glm::vec3(0.0f, 1.0f, 0.0f));
        }

        static void TransformComponent_SetUp(uint64_t entityID, const glm::vec3 *value)
        {
            if (!value) return;
            glm::vec3 up = glm::normalize(*value);
            if (glm::length2(up) <= 0.0f)
            {
                return;
            }

            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TransformComponent>())
                return;

            auto &transform = entity.GetComponent<TransformComponent>();
            glm::vec3 right = glm::normalize(transform.rotation * glm::vec3(1.0f, 0.0f, 0.0f));
            glm::vec3 forward = glm::normalize(glm::cross(up, right));
            if (glm::length2(forward) <= 0.0f)
            {
                return;
            }

            const glm::quat rotation = glm::quatLookAtRH(forward, up);
            transform.localRotation = rotation;
            transform.rotation = rotation;
            transform.dirty = true;
        }

        static void TransformComponent_GetTranslation(uint64_t entityID, glm::vec3 *result)
        {
            if (!result)
            {
                return;
            }

            *result = {};
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TransformComponent>())
                return;

            *result = glm::vec3(entity.GetComponent<TransformComponent>().translation);
        }

        static void TransformComponent_SetTranslation(uint64_t entityID, const glm::vec3 *value)
        {
            if (!value) return;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TransformComponent>())
                return;

            auto &transform = entity.GetComponent<TransformComponent>();
            transform.localTranslation = *value;
            transform.translation = *value;
            transform.dirty = true;
            transform.dirtyPhysics = true;
        }

        static void TransformComponent_GetRotation(uint64_t entityID, glm::quat *result)
        {
            if (!result)
            {
                return;
            }

            *result = {};
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TransformComponent>())
                return;

            *result = glm::quat(entity.GetComponent<TransformComponent>().rotation);
        }

        static void TransformComponent_SetRotation(uint64_t entityID, const glm::quat *value)
        {
            if (!value) return;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TransformComponent>())
                return;

            auto &transform = entity.GetComponent<TransformComponent>();
            transform.localRotation = *value;
            transform.rotation = *value;
            transform.dirty = true;
            transform.dirtyPhysics = true;
        }

        static void TransformComponent_GetEulerAngles(uint64_t entityID, glm::vec3 *result)
        {
            if (!result)
            {
                return;
            }

            *result = {};
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TransformComponent>())
                return;

            *result = glm::vec3(glm::eulerAngles(entity.GetComponent<TransformComponent>().rotation));
        }

        static void TransformComponent_SetEulerAngles(uint64_t entityID, const glm::vec3 *value)
        {
            if (!value) return;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TransformComponent>())
                return;

            auto &transform = entity.GetComponent<TransformComponent>();
            const glm::quat rotation = glm::quat(*value);
            transform.localRotation = rotation;
            transform.rotation = rotation;
            transform.dirty = true;
        }

        static void TransformComponent_GetScale(uint64_t entityID, glm::vec3 *result)
        {
            if (!result)
            {
                return;
            }

            *result = {};
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TransformComponent>())
                return;

            *result = glm::vec3(entity.GetComponent<TransformComponent>().scale);
        }

        static void TransformComponent_SetScale(uint64_t entityID, const glm::vec3 *value)
        {
            if (!value) return;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TransformComponent>())
                return;

            auto &transform = entity.GetComponent<TransformComponent>();
            transform.localScale = *value;
            transform.scale = *value;
            transform.dirty = true;
        }

        static void Sprite2DComponent_SetColor(uint64_t entityID, const glm::vec4 *color)
        {
            if (!color) return;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Sprite2DComponent>())
                return;

            auto &comp = entity.GetComponent<Sprite2DComponent>();
            comp.color = *color;
        }

        static void Sprite2DComponent_GetColor(uint64_t entityID, glm::vec4 *result)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Sprite2DComponent>())
                return;

            auto &comp = entity.GetComponent<Sprite2DComponent>();
            *result = comp.color;
        }

        static void Circle2DComponent_SetColor(uint64_t entityID, const glm::vec4 *color)
        {
            if (!color) return;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Circle2DComponent>())
                return;

            auto &comp = entity.GetComponent<Circle2DComponent>();
            comp.color = *color;
        }

        static void Circle2DComponent_GetColor(uint64_t entityID, glm::vec4 *result)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Circle2DComponent>())
                return;

            auto &comp = entity.GetComponent<Circle2DComponent>();
            *result = comp.color;
        }

        static void Sprite2DComponent_SetTilingFactor(uint64_t entityID, const glm::vec2 *value)
        {
            if (!value) return;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Sprite2DComponent>())
                return;

            auto &comp = entity.GetComponent<Sprite2DComponent>();
            comp.tilingFactor = *value;
        }

        static void Sprite2DComponent_GetTilingFactor(uint64_t entityID, glm::vec2 *result)
        {
            if (!result)
            {
                return;
            }

            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Sprite2DComponent>())
                return;

            auto &comp = entity.GetComponent<Sprite2DComponent>();
            *result = comp.tilingFactor;
        }

        static void Rigidbody2DComponent_GetType(uint64_t entityID, int32_t *result)
        {
            if (!result)
                return;

            *result = static_cast<int32_t>(Rigidbody2DComponent::EBodyType::Static);
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
                return;

            *result = static_cast<int32_t>(entity.GetComponent<Rigidbody2DComponent>().bodyType);
        }

        static void Rigidbody2DComponent_SetType(uint64_t entityID, int32_t value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
                return;

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            rb.bodyType = static_cast<Rigidbody2DComponent::EBodyType>(value);
            if (b2Body_IsValid(rb.bodyId))
            {
                b2Body_SetType(rb.bodyId, static_cast<b2BodyType>(rb.bodyType));
            }
        }

        static void Rigidbody2DComponent_GetLinearVelocity(uint64_t entityID, glm::vec2 *result)
        {
            if (!result)
                return;

            *result = {};
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
                return;

            *result = entity.GetComponent<Rigidbody2DComponent>().linearVelocity;
        }

        static void Rigidbody2DComponent_SetLinearVelocity(uint64_t entityID, const glm::vec2 *value)
        {
            if (!value) return;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
                return;

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            rb.linearVelocity = *value;
            if (b2Body_IsValid(rb.bodyId))
            {
                b2Body_SetLinearVelocity(rb.bodyId, { value->x, value->y });
            }
        }

        static void Rigidbody2DComponent_GetAngularVelocity(uint64_t entityID, float *result)
        {
            if (!result)
            {
                return;
            }

            *result = 0.0f;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
                return;

            *result = entity.GetComponent<Rigidbody2DComponent>().angularVelocity;
        }

        static void Rigidbody2DComponent_SetAngularVelocity(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
                return;

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            rb.angularVelocity = value;
            if (b2Body_IsValid(rb.bodyId))
            {
                b2Body_SetAngularVelocity(rb.bodyId, value);
            }
        }

        static void Rigidbody2DComponent_GetGravityScale(uint64_t entityID, float *result)
        {
            if (!result)
            {
                return;
            }

            *result = 0.0f;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
                return;

            *result = entity.GetComponent<Rigidbody2DComponent>().gravityScale;
        }

        static void Rigidbody2DComponent_SetGravityScale(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
                return;

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            rb.gravityScale = value;
            if (b2Body_IsValid(rb.bodyId))
            {
                b2Body_SetGravityScale(rb.bodyId, value);
            }
        }

        static void Rigidbody2DComponent_GetLinearDamping(uint64_t entityID, float *result)
        {
            if (!result)
            {
                return;
            }

            *result = 0.0f;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
                return;

            *result = entity.GetComponent<Rigidbody2DComponent>().linearDamping;
        }

        static void Rigidbody2DComponent_SetLinearDamping(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
                return;

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            rb.linearDamping = value;
            if (b2Body_IsValid(rb.bodyId))
            {
                b2Body_SetLinearDamping(rb.bodyId, value);
            }
        }

        static void Rigidbody2DComponent_GetAngularDamping(uint64_t entityID, float *result)
        {
            if (!result)
            {
                return;
            }

            *result = 0.0f;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
                return;

            *result = entity.GetComponent<Rigidbody2DComponent>().angularDamping;
        }

        static void Rigidbody2DComponent_SetAngularDamping(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
                return;

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            rb.angularDamping = value;
            if (b2Body_IsValid(rb.bodyId))
            {
                b2Body_SetAngularDamping(rb.bodyId, value);
            }
        }

        static void Rigidbody2DComponent_GetIsAwake(uint64_t entityID, bool *result)
        {
            if (!result)
            {
                return;
            }

            *result = false;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
                return;

            *result = entity.GetComponent<Rigidbody2DComponent>().isAwake;
        }

        static void Rigidbody2DComponent_SetIsAwake(uint64_t entityID, bool value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
                return;

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            rb.isAwake = value;
            if (b2Body_IsValid(rb.bodyId))
            {
                b2Body_SetAwake(rb.bodyId, value);
            }
        }

        static void Rigidbody2DComponent_GetIsEnabled(uint64_t entityID, bool *result)
        {
            if (!result)
            {
                return;
            }

            *result = false;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
                return;

            *result = entity.GetComponent<Rigidbody2DComponent>().isEnabled;
        }

        static void Rigidbody2DComponent_SetIsEnabled(uint64_t entityID, bool value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
                return;

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            rb.isEnabled = value;
            if (b2Body_IsValid(rb.bodyId))
            {
                if (value)
                {
                    b2Body_Enable(rb.bodyId);
                }
                else
                {
                    b2Body_Disable(rb.bodyId);
                }
            }
        }

        static void Rigidbody2DComponent_GetIsEnableSleep(uint64_t entityID, bool *result)
        {
            if (!result)
            {
                return;
            }

            *result = false;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
                return;

            *result = entity.GetComponent<Rigidbody2DComponent>().isEnableSleep;
        }

        static void Rigidbody2DComponent_SetIsEnableSleep(uint64_t entityID, bool value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
                return;

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            rb.isEnableSleep = value;
            if (b2Body_IsValid(rb.bodyId))
            {
                b2Body_EnableSleep(rb.bodyId, value);
            }
        }

        static void Rigidbody2DComponent_ApplyForce(uint64_t entityID, glm::vec2 force, glm::vec2 point, bool wake)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
                return;

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            if (!b2Body_IsValid(rb.bodyId))
            {
                return;
            }

            b2Body_ApplyForce(rb.bodyId, { force.x, force.y }, { point.x, point.y }, wake);
        }

        static void Rigidbody2DComponent_ApplyForceToCenter(uint64_t entityID, glm::vec2 force, bool wake)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
                return;

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            if (!b2Body_IsValid(rb.bodyId))
            {
                return;
            }

            b2Body_ApplyForceToCenter(rb.bodyId, { force.x, force.y }, wake);
        }

        static void Rigidbody2DComponent_ApplyLinearImpulse(uint64_t entityID, glm::vec2 impulse, glm::vec2 point, bool wake)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
                return;

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            if (!b2Body_IsValid(rb.bodyId))
            {
                return;
            }

            b2Body_ApplyLinearImpulse(rb.bodyId, { impulse.x, impulse.y }, { point.x, point.y }, wake);
        }

        static void Rigidbody2DComponent_ApplyLinearImpulseToCenter(uint64_t entityID, glm::vec2 impulse, bool wake)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
                return;

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            if (!b2Body_IsValid(rb.bodyId))
            {
                return;
            }

            b2Body_ApplyLinearImpulseToCenter(rb.bodyId, { impulse.x, impulse.y }, wake);
        }

        static void Rigidbody2DComponent_ApplyAngularImpulse(uint64_t entityID, float impulse, bool wake)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
                return;

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            if (!b2Body_IsValid(rb.bodyId))
            {
                return;
            }

            b2Body_ApplyAngularImpulse(rb.bodyId, impulse, wake);
        }

        static void Rigidbody2DComponent_ApplyTorque(uint64_t entityID, float torque, bool wake)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
                return;

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            if (!b2Body_IsValid(rb.bodyId))
            {
                return;
            }

            b2Body_ApplyTorque(rb.bodyId, torque, wake);
        }

        static void Rigidbody2DComponent_GetMass(uint64_t entityID, float *result)
        {
            if (!result)
            {
                return;
            }

            *result = 0.0f;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
                return;

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            if (!b2Body_IsValid(rb.bodyId))
            {
                return;
            }

            *result = b2Body_GetMass(rb.bodyId);
        }

        static void Rigidbody2DComponent_GetIsBullet(uint64_t entityID, bool *result)
        {
            if (!result)
            {
                return;
            }

            *result = false;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
                return;

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            if (!b2Body_IsValid(rb.bodyId))
            {
                return;
            }

            *result = b2Body_IsBullet(rb.bodyId);
        }

        static void Rigidbody2DComponent_SetIsBullet(uint64_t entityID, bool value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
                return;

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            if (!b2Body_IsValid(rb.bodyId))
            {
                return;
            }

            b2Body_SetBullet(rb.bodyId, value);
        }

        static void BoxCollider2DComponent_GetSize(uint64_t entityID, glm::vec2 *result)
        {
            if (!result)
            {
                return;
            }

            *result = {};
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<BoxCollider2DComponent>())
                return;

            *result = entity.GetComponent<BoxCollider2DComponent>().size;
        }

        static void BoxCollider2DComponent_SetSize(uint64_t entityID, glm::vec2 value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<BoxCollider2DComponent>())
                return;

            entity.GetComponent<BoxCollider2DComponent>().size = value;
        }

        static void BoxCollider2DComponent_GetOffset(uint64_t entityID, glm::vec2 *result)
        {
            if (!result)
            {
                return;
            }

            *result = {};
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<BoxCollider2DComponent>())
                return;

            *result = entity.GetComponent<BoxCollider2DComponent>().offset;
        }

        static void BoxCollider2DComponent_SetOffset(uint64_t entityID, glm::vec2 value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<BoxCollider2DComponent>())
                return;

            entity.GetComponent<BoxCollider2DComponent>().offset = value;
        }

        static void BoxCollider2DComponent_GetRestitution(uint64_t entityID, float *result)
        {
            if (!result)
            {
                return;
            }

            *result = 0.0f;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<BoxCollider2DComponent>())
                return;

            *result = entity.GetComponent<BoxCollider2DComponent>().restitution;
        }

        static void BoxCollider2DComponent_SetRestitution(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<BoxCollider2DComponent>())
                return;

            entity.GetComponent<BoxCollider2DComponent>().restitution = value;
        }

        static void BoxCollider2DComponent_GetFriction(uint64_t entityID, float *result)
        {
            if (!result)
            {
                return;
            }

            *result = 0.0f;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<BoxCollider2DComponent>())
                return;

            *result = entity.GetComponent<BoxCollider2DComponent>().friction;
        }

        static void BoxCollider2DComponent_SetFriction(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<BoxCollider2DComponent>())
                return;

            entity.GetComponent<BoxCollider2DComponent>().friction = value;
        }

        static void BoxCollider2DComponent_GetDensity(uint64_t entityID, float *result)
        {
            if (!result)
            {
                return;
            }

            *result = 0.0f;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<BoxCollider2DComponent>())
                return;

            *result = entity.GetComponent<BoxCollider2DComponent>().density;
        }

        static void BoxCollider2DComponent_SetDensity(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<BoxCollider2DComponent>())
                return;

            entity.GetComponent<BoxCollider2DComponent>().density = value;
        }

        static void BoxCollider2DComponent_GetIsSensor(uint64_t entityID, bool *result)
        {
            if (!result)
            {
                return;
            }

            *result = false;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<BoxCollider2DComponent>())
                return;

            *result = entity.GetComponent<BoxCollider2DComponent>().isSensor;
        }

        static void BoxCollider2DComponent_SetIsSensor(uint64_t entityID, bool value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<BoxCollider2DComponent>())
                return;

            entity.GetComponent<BoxCollider2DComponent>().isSensor = value;
        }

        static void CircleCollider2DComponent_GetCenter(uint64_t entityID, glm::vec2 *result)
        {
            if (!result)
            {
                return;
            }

            *result = {};
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<CircleCollider2DComponent>())
                return;

            *result = entity.GetComponent<CircleCollider2DComponent>().center;
        }

        static void CircleCollider2DComponent_SetCenter(uint64_t entityID, glm::vec2 value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<CircleCollider2DComponent>())
                return;

            entity.GetComponent<CircleCollider2DComponent>().center = value;
        }

        static void CircleCollider2DComponent_GetRadius(uint64_t entityID, float *result)
        {
            if (!result)
            {
                return;
            }

            *result = {};
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<CircleCollider2DComponent>())
                return;

            *result = entity.GetComponent<CircleCollider2DComponent>().radius;
        }

        static void CircleCollider2DComponent_SetRadius(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<CircleCollider2DComponent>())
                return;

            entity.GetComponent<CircleCollider2DComponent>().radius = value;
        }

        static void CircleCollider2DComponent_GetRestitution(uint64_t entityID, float *result)
        {
            if (!result)
            {
                return;
            }

            *result = 0.0f;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<CircleCollider2DComponent>())
                return;

            *result = entity.GetComponent<CircleCollider2DComponent>().restitution;
        }

        static void CircleCollider2DComponent_SetRestitution(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<CircleCollider2DComponent>())
                return;

            entity.GetComponent<CircleCollider2DComponent>().restitution = value;
        }

        static void CircleCollider2DComponent_GetFriction(uint64_t entityID, float *result)
        {
            if (!result)
            {
                return;
            }

            *result = 0.0f;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<CircleCollider2DComponent>())
                return;

            *result = entity.GetComponent<CircleCollider2DComponent>().friction;
        }

        static void CircleCollider2DComponent_SetFriction(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<CircleCollider2DComponent>())
                return;

            entity.GetComponent<CircleCollider2DComponent>().friction = value;
        }

        static void CircleCollider2DComponent_GetDensity(uint64_t entityID, float *result)
        {
            if (!result)
            {
                return;
            }

            *result = 0.0f;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<CircleCollider2DComponent>())
                return;

            *result = entity.GetComponent<CircleCollider2DComponent>().density;
        }

        static void CircleCollider2DComponent_SetDensity(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<CircleCollider2DComponent>())
                return;

            entity.GetComponent<CircleCollider2DComponent>().density = value;
        }

        static void CircleCollider2DComponent_GetIsSensor(uint64_t entityID, bool *result)
        {
            if (!result)
            {
                return;
            }

            *result = false;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<CircleCollider2DComponent>())
                return;

            *result = entity.GetComponent<CircleCollider2DComponent>().isSensor;
        }

        static void CircleCollider2DComponent_SetIsSensor(uint64_t entityID, bool value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<CircleCollider2DComponent>())
                return;

            entity.GetComponent<CircleCollider2DComponent>().isSensor = value;
        }

        static char *AllocStringForManaged(const std::string &str)
        {
            const size_t len = str.size();
            char *mem = static_cast<char *>(CoTaskMemAlloc(len + 1));
            if (!mem)
                return nullptr;
            std::memcpy(mem, str.c_str(), len);
            mem[len] = '\0';
            return mem;
        }

        static void TextComponent_SetText(uint64_t entityID, const char *value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TextComponent>())
                return;

            entity.GetComponent<TextComponent>().text = std::string(value ? value : "");
        }

        static void TextComponent_GetText(uint64_t entityID, const char **result)
        {
            if (!result)
                return;

            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TextComponent>())
                return;

            const std::string &text = entity.GetComponent<TextComponent>().text;
            *result = AllocStringForManaged(text);
        }

        static void TextComponent_SetColor(uint64_t entityID, const glm::vec4 &value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TextComponent>())
                return;

            entity.GetComponent<TextComponent>().color = value;
        }

        static void TextComponent_GetColor(uint64_t entityID, glm::vec4 *result)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TextComponent>())
                return;

            *result = entity.GetComponent<TextComponent>().color;
        }

        static void TextComponent_SetKerning(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TextComponent>())
                return;

            entity.GetComponent<TextComponent>().kerning = value;
        }

        static void TextComponent_GetKerning(uint64_t entityID, float *result)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TextComponent>())
                return;

            *result = entity.GetComponent<TextComponent>().kerning;
        }

        static void TextComponent_SetLineSpacing(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TextComponent>())
                return;

            entity.GetComponent<TextComponent>().lineSpacing = value;
        }

        static void TextComponent_GetLineSpacing(uint64_t entityID, float *result)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TextComponent>())
                return;

            *result = entity.GetComponent<TextComponent>().lineSpacing;
        }

        // --- RigidbodyComponent ---
        static void RigidbodyComponent_GetType(uint64_t entityID, int32_t *result)
        {
            if (!result) return;
            *result = static_cast<int32_t>(RigidbodyComponent::EBodyType::Static);
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            *result = static_cast<int32_t>(entity.GetComponent<RigidbodyComponent>().bodyType);
        }

        static void RigidbodyComponent_SetType(uint64_t entityID, int32_t value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            rb.bodyType = static_cast<RigidbodyComponent::EBodyType>(value);
            Scene *scene = GetSceneContext();
            if (scene && scene->physics && rb.body)
            {
                JPH::EMotionType motionType = JPH::EMotionType::Static;
                if (rb.bodyType == RigidbodyComponent::EBodyType::Kinematic) motionType = JPH::EMotionType::Kinematic;
                else if (rb.bodyType == RigidbodyComponent::EBodyType::Dynamic) motionType = JPH::EMotionType::Dynamic;
                scene->physics->GetBodyInterface()->SetMotionType(rb.body->GetID(), motionType, JPH::EActivation::Activate);
            }
        }

        static void RigidbodyComponent_GetMotionQuality(uint64_t entityID, int32_t *result)
        {
            if (!result) return;
            *result = static_cast<int32_t>(RigidbodyComponent::EMotionQuality::Discrete);
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            *result = static_cast<int32_t>(entity.GetComponent<RigidbodyComponent>().motionQuality);
        }

        static void RigidbodyComponent_GetUseGravity(uint64_t entityID, bool *result)
        {
            if (!result) return;
            *result = true;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            *result = entity.GetComponent<RigidbodyComponent>().useGravity;
        }

        static void RigidbodyComponent_SetUseGravity(uint64_t entityID, bool value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            rb.useGravity = value;
            Scene *scene = GetSceneContext();
            if (scene && scene->physics && rb.body)
            {
                scene->physics->SetGravityFactor(*rb.body, rb.useGravity ? rb.gravityFactor : 0.0f);
            }
        }

        static void RigidbodyComponent_GetMass(uint64_t entityID, float *result)
        {
            if (!result) return;
            *result = 1.0f;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            *result = entity.GetComponent<RigidbodyComponent>().mass;
        }

        static void RigidbodyComponent_GetGravityFactor(uint64_t entityID, float *result)
        {
            if (!result) return;
            *result = 1.0f;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            *result = entity.GetComponent<RigidbodyComponent>().gravityFactor;
        }

        static void RigidbodyComponent_SetGravityFactor(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            rb.gravityFactor = value;
            Scene *scene = GetSceneContext();
            if (scene && scene->physics && rb.body)
            {
                scene->physics->SetGravityFactor(*rb.body, rb.useGravity ? rb.gravityFactor : 0.0f);
            }
        }

        static void RigidbodyComponent_GetLinearVelocity(uint64_t entityID, glm::vec3 *result)
        {
            if (!result) return;
            *result = glm::vec3(0.0f);
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            Scene *scene = GetSceneContext();
            if (scene && scene->physics && rb.body)
            {
                *result = scene->physics->GetLinearVelocity(*rb.body);
            }
        }

        static void RigidbodyComponent_SetLinearVelocity(uint64_t entityID, const glm::vec3 *value)
        {
            if (!value) return;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            Scene *scene = GetSceneContext();
            if (scene && scene->physics && rb.body)
            {
                scene->physics->SetLinearVelocity(*rb.body, *value);
            }
        }

        static void RigidbodyComponent_GetPosition(uint64_t entityID, glm::vec3 *result)
        {
            if (!result) return;
            *result = glm::vec3(0.0f);
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            Scene *scene = GetSceneContext();
            if (scene && scene->physics && rb.body)
            {
                *result = scene->physics->GetPosition(*rb.body);
            }
        }

        static void RigidbodyComponent_SetPosition(uint64_t entityID, const glm::vec3 *value)
        {
            if (!value) return;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            Scene *scene = GetSceneContext();
            if (scene && scene->physics && rb.body)
            {
                scene->physics->SetPosition(*rb.body, *value, true);
            }
        }

        static void RigidbodyComponent_GetRotation(uint64_t entityID, glm::quat *result)
        {
            if (!result) return;
            *result = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            Scene *scene = GetSceneContext();
            if (scene && scene->physics && rb.body)
            {
                *result = scene->physics->GetRotation(*rb.body);
            }
        }

        static void RigidbodyComponent_SetRotation(uint64_t entityID, const glm::quat *value)
        {
            if (!value) return;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            Scene *scene = GetSceneContext();
            if (scene && scene->physics && rb.body)
            {
                scene->physics->SetRotation(*rb.body, *value, true);
            }
        }

        static void RigidbodyComponent_GetAngularVelocity(uint64_t entityID, glm::vec3 *result)
        {
            if (!result) return;
            *result = glm::vec3(0.0f);
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            Scene *scene = GetSceneContext();
            if (scene && scene->physics && rb.body)
            {
                *result = JoltToGlmVec3(scene->physics->GetBodyInterface()->GetAngularVelocity(rb.body->GetID()));
            }
        }

        static void RigidbodyComponent_GetCenterOfMass(uint64_t entityID, glm::vec3 *result)
        {
            if (!result) return;
            *result = glm::vec3(0.0f);
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            Scene *scene = GetSceneContext();
            if (scene && scene->physics && rb.body)
            {
                *result = scene->physics->GetCenterOfMassPosition(*rb.body);
            }
        }

        static void RigidbodyComponent_IsActive(uint64_t entityID, bool *result)
        {
            if (!result) return;
            *result = false;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            Scene *scene = GetSceneContext();
            if (scene && scene->physics && rb.body)
            {
                *result = scene->physics->IsActive(*rb.body);
            }
        }

        static void RigidbodyComponent_ApplyForce(uint64_t entityID, const glm::vec3 *force, const glm::vec3 *point)
        {
            if (!force || !point) return;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            Scene *scene = GetSceneContext();
            if (scene && scene->physics && rb.body)
            {
                scene->physics->GetBodyInterface()->AddForce(rb.body->GetID(), GlmToJoltVec3(*force), GlmToJoltVec3(*point));
            }
        }

        static void RigidbodyComponent_ApplyForceToCenter(uint64_t entityID, const glm::vec3 *force)
        {
            if (!force) return;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            Scene *scene = GetSceneContext();
            if (scene && scene->physics && rb.body)
            {
                scene->physics->AddForce(*rb.body, *force);
            }
        }

        static void RigidbodyComponent_ApplyTorque(uint64_t entityID, const glm::vec3 *torque)
        {
            if (!torque) return;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            Scene *scene = GetSceneContext();
            if (scene && scene->physics && rb.body)
            {
                scene->physics->AddTorque(*rb.body, *torque);
            }
        }

        static void RigidbodyComponent_ApplyLinearImpulse(uint64_t entityID, const glm::vec3 *impulse, const glm::vec3 *point)
        {
            if (!impulse || !point) return;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            Scene *scene = GetSceneContext();
            if (scene && scene->physics && rb.body)
            {
                scene->physics->GetBodyInterface()->AddImpulse(rb.body->GetID(), GlmToJoltVec3(*impulse), GlmToJoltVec3(*point));
            }
        }

        static void RigidbodyComponent_ApplyLinearImpulseToCenter(uint64_t entityID, const glm::vec3 *impulse)
        {
            if (!impulse) return;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            Scene *scene = GetSceneContext();
            if (scene && scene->physics && rb.body)
            {
                scene->physics->AddImpulse(*rb.body, *impulse);
            }
        }

        static void RigidbodyComponent_ApplyAngularImpulse(uint64_t entityID, const glm::vec3 *impulse)
        {
            if (!impulse) return;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            Scene *scene = GetSceneContext();
            if (scene && scene->physics && rb.body)
            {
                scene->physics->AddAngularImpulse(*rb.body, *impulse);
            }
        }

        static void RigidbodyComponent_Activate(uint64_t entityID)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            Scene *scene = GetSceneContext();
            if (scene && scene->physics && rb.body)
            {
                scene->physics->ActivateBody(*rb.body);
            }
        }

        static void RigidbodyComponent_Deactivate(uint64_t entityID)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            Scene *scene = GetSceneContext();
            if (scene && scene->physics && rb.body)
            {
                scene->physics->DeactivateBody(*rb.body);
            }
        }

        static void RigidbodyComponent_MoveKinematic(uint64_t entityID, const glm::vec3 *targetPosition, const glm::vec3 *targetRotation, float deltaTime)
        {
            if (!targetPosition || !targetRotation) return;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            Scene *scene = GetSceneContext();
            if (scene && scene->physics && rb.body)
            {
                scene->physics->MoveKinematic(*rb.body, *targetPosition, *targetRotation, deltaTime);
            }
        }

        // --- BoxColliderComponent ---
        static void BoxColliderComponent_GetCenter(uint64_t entityID, glm::vec3 *result)
        {
            if (!result) return;
            *result = glm::vec3(0.0f);
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<BoxColliderComponent>())
                *result = entity.GetComponent<BoxColliderComponent>().center;
        }

        static void BoxColliderComponent_SetCenter(uint64_t entityID, const glm::vec3 *value)
        {
            if (!value) return;
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<BoxColliderComponent>())
            {
                entity.GetComponent<BoxColliderComponent>().center = *value;
            }
        }

        static void BoxColliderComponent_GetScale(uint64_t entityID, glm::vec3 *result)
        {
            if (!result) return;
            *result = glm::vec3(1.0f);
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<BoxColliderComponent>())
                *result = entity.GetComponent<BoxColliderComponent>().scale;
        }

        static void BoxColliderComponent_SetScale(uint64_t entityID, const glm::vec3 *value)
        {
            if (!value) return;
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<BoxColliderComponent>())
            {
                entity.GetComponent<BoxColliderComponent>().scale = *value;
            }
        }

        static void BoxColliderComponent_GetFriction(uint64_t entityID, float *result)
        {
            if (!result) return;
            *result = 0.6f;
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<BoxColliderComponent>())
                *result = entity.GetComponent<BoxColliderComponent>().friction;
        }

        static void BoxColliderComponent_SetFriction(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<BoxColliderComponent>())
            {
                auto &col = entity.GetComponent<BoxColliderComponent>();
                col.friction = value;
                Scene *scene = GetSceneContext();
                if (scene && scene->physics && entity.HasComponent<RigidbodyComponent>())
                {
                    auto &rb = entity.GetComponent<RigidbodyComponent>();
                    if (rb.body)
                        scene->physics->GetBodyInterface()->SetFriction(rb.body->GetID(), value);
                }
            }
        }

        static void BoxColliderComponent_GetRestitution(uint64_t entityID, float *result)
        {
            if (!result) return;
            *result = 0.6f;
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<BoxColliderComponent>())
                *result = entity.GetComponent<BoxColliderComponent>().restitution;
        }

        static void BoxColliderComponent_SetRestitution(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<BoxColliderComponent>())
            {
                auto &col = entity.GetComponent<BoxColliderComponent>();
                col.restitution = value;
                Scene *scene = GetSceneContext();
                if (scene && scene->physics && entity.HasComponent<RigidbodyComponent>())
                {
                    auto &rb = entity.GetComponent<RigidbodyComponent>();
                    if (rb.body)
                        scene->physics->GetBodyInterface()->SetRestitution(rb.body->GetID(), value);
                }
            }
        }

        static void BoxColliderComponent_GetDensity(uint64_t entityID, float *result)
        {
            if (!result) return;
            *result = 1.0f;
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<BoxColliderComponent>())
                *result = entity.GetComponent<BoxColliderComponent>().density;
        }

        static void BoxColliderComponent_SetDensity(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<BoxColliderComponent>())
            {
                entity.GetComponent<BoxColliderComponent>().density = value;
            }
        }

        // --- SphereColliderComponent ---
        static void SphereColliderComponent_GetCenter(uint64_t entityID, glm::vec3 *result)
        {
            if (!result) return;
            *result = glm::vec3(0.0f);
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<SphereColliderComponent>())
                *result = entity.GetComponent<SphereColliderComponent>().center;
        }

        static void SphereColliderComponent_SetCenter(uint64_t entityID, const glm::vec3 *value)
        {
            if (!value) return;
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<SphereColliderComponent>())
            {
                entity.GetComponent<SphereColliderComponent>().center = *value;
            }
        }

        static void SphereColliderComponent_GetRadius(uint64_t entityID, float *result)
        {
            if (!result) return;
            *result = 1.0f;
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<SphereColliderComponent>())
                *result = entity.GetComponent<SphereColliderComponent>().radius;
        }

        static void SphereColliderComponent_SetRadius(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<SphereColliderComponent>())
            {
                entity.GetComponent<SphereColliderComponent>().radius = value;
            }
        }

        static void SphereColliderComponent_GetFriction(uint64_t entityID, float *result)
        {
            if (!result) return;
            *result = 0.6f;
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<SphereColliderComponent>())
                *result = entity.GetComponent<SphereColliderComponent>().friction;
        }

        static void SphereColliderComponent_SetFriction(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<SphereColliderComponent>())
            {
                auto &col = entity.GetComponent<SphereColliderComponent>();
                col.friction = value;
                Scene *scene = GetSceneContext();
                if (scene && scene->physics && entity.HasComponent<RigidbodyComponent>())
                {
                    auto &rb = entity.GetComponent<RigidbodyComponent>();
                    if (rb.body)
                        scene->physics->GetBodyInterface()->SetFriction(rb.body->GetID(), value);
                }
            }
        }

        static void SphereColliderComponent_GetRestitution(uint64_t entityID, float *result)
        {
            if (!result) return;
            *result = 0.6f;
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<SphereColliderComponent>())
                *result = entity.GetComponent<SphereColliderComponent>().restitution;
        }

        static void SphereColliderComponent_SetRestitution(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<SphereColliderComponent>())
            {
                auto &col = entity.GetComponent<SphereColliderComponent>();
                col.restitution = value;
                Scene *scene = GetSceneContext();
                if (scene && scene->physics && entity.HasComponent<RigidbodyComponent>())
                {
                    auto &rb = entity.GetComponent<RigidbodyComponent>();
                    if (rb.body)
                        scene->physics->GetBodyInterface()->SetRestitution(rb.body->GetID(), value);
                }
            }
        }

        static void SphereColliderComponent_GetDensity(uint64_t entityID, float *result)
        {
            if (!result) return;
            *result = 1.0f;
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<SphereColliderComponent>())
                *result = entity.GetComponent<SphereColliderComponent>().density;
        }

        static void SphereColliderComponent_SetDensity(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<SphereColliderComponent>())
            {
                entity.GetComponent<SphereColliderComponent>().density = value;
            }
        }

        // --- CapsuleColliderComponent ---
        static void CapsuleColliderComponent_GetCenter(uint64_t entityID, glm::vec3 *result)
        {
            if (!result) return;
            *result = glm::vec3(0.0f);
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CapsuleColliderComponent>())
                *result = entity.GetComponent<CapsuleColliderComponent>().center;
        }

        static void CapsuleColliderComponent_SetCenter(uint64_t entityID, const glm::vec3 *value)
        {
            if (!value) return;
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CapsuleColliderComponent>())
            {
                entity.GetComponent<CapsuleColliderComponent>().center = *value;
            }
        }

        static void CapsuleColliderComponent_GetRadius(uint64_t entityID, float *result)
        {
            if (!result) return;
            *result = 1.0f;
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CapsuleColliderComponent>())
                *result = entity.GetComponent<CapsuleColliderComponent>().radius;
        }

        static void CapsuleColliderComponent_SetRadius(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CapsuleColliderComponent>())
            {
                entity.GetComponent<CapsuleColliderComponent>().radius = value;
            }
        }

        static void CapsuleColliderComponent_GetHeight(uint64_t entityID, float *result)
        {
            if (!result) return;
            *result = 2.0f;
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CapsuleColliderComponent>())
                *result = entity.GetComponent<CapsuleColliderComponent>().height;
        }

        static void CapsuleColliderComponent_SetHeight(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CapsuleColliderComponent>())
            {
                entity.GetComponent<CapsuleColliderComponent>().height = value;
            }
        }

        static void CapsuleColliderComponent_GetFriction(uint64_t entityID, float *result)
        {
            if (!result) return;
            *result = 0.6f;
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CapsuleColliderComponent>())
                *result = entity.GetComponent<CapsuleColliderComponent>().friction;
        }

        static void CapsuleColliderComponent_SetFriction(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CapsuleColliderComponent>())
            {
                auto &col = entity.GetComponent<CapsuleColliderComponent>();
                col.friction = value;
                Scene *scene = GetSceneContext();
                if (scene && scene->physics && entity.HasComponent<RigidbodyComponent>())
                {
                    auto &rb = entity.GetComponent<RigidbodyComponent>();
                    if (rb.body)
                        scene->physics->GetBodyInterface()->SetFriction(rb.body->GetID(), value);
                }
            }
        }

        static void CapsuleColliderComponent_GetRestitution(uint64_t entityID, float *result)
        {
            if (!result) return;
            *result = 0.6f;
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CapsuleColliderComponent>())
                *result = entity.GetComponent<CapsuleColliderComponent>().restitution;
        }

        static void CapsuleColliderComponent_SetRestitution(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CapsuleColliderComponent>())
            {
                auto &col = entity.GetComponent<CapsuleColliderComponent>();
                col.restitution = value;
                Scene *scene = GetSceneContext();
                if (scene && scene->physics && entity.HasComponent<RigidbodyComponent>())
                {
                    auto &rb = entity.GetComponent<RigidbodyComponent>();
                    if (rb.body)
                        scene->physics->GetBodyInterface()->SetRestitution(rb.body->GetID(), value);
                }
            }
        }

        static void CapsuleColliderComponent_GetDensity(uint64_t entityID, float *result)
        {
            if (!result) return;
            *result = 1.0f;
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CapsuleColliderComponent>())
                *result = entity.GetComponent<CapsuleColliderComponent>().density;
        }

        static void CapsuleColliderComponent_SetDensity(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CapsuleColliderComponent>())
            {
                entity.GetComponent<CapsuleColliderComponent>().density = value;
            }
        }

        static const ComponentScriptGlueAPI s_ComponentScriptGlueAPI =
        {
            &Scene_GetScreenToWorldRay,
            &Scene_Raycast,
            &Scene_PhysicsRaycast,
            &Scene_GetPrimaryCamera,
            &Entity_HasComponent,
            &Entity_AddComponent,
            &Entity_FindEntityByName,
            &Entity_FindChildEntityByName,
            &Entity_IsParent,
            &Entity_GetParent,
            &Entity_InstantiateWithName,
            &Entity_Instantiate,
            &Entity_Destroy,
            &Entity_SetVisibility,
            &Entity_GetVisibility,
            &Entity_GetName,
            &WidgetComponent_HasButton,
            &WidgetComponent_AddButtonEventCallback,
            &WidgetComponent_RemoveButtonEventCallback,

            &WidgetComponent_HasLabel,
            &WidgetComponent_GetLabelText,
            &WidgetComponent_SetLabelText,
            &WidgetComponent_GetLabelColor,
            &WidgetComponent_SetLabelColor,
            &WidgetComponent_GetLabelFontSize,
            &WidgetComponent_SetLabelFontSize,

            &WidgetComponent_HasImage,
            &WidgetComponent_GetImageHandle,
            &WidgetComponent_SetImageHandle,

            &AudioSourceComponent_HasAudio,
            &AudioSourceComponent_Play,
            &AudioSourceComponent_Stop,
            &AudioSourceComponent_Pause,
            &AudioSourceComponent_Resume,
            &AudioSourceComponent_GetVolume,
            &AudioSourceComponent_SetVolume,
            &AudioSourceComponent_GetPitch,
            &AudioSourceComponent_SetPitch,
            &AudioSourceComponent_GetPan,
            &AudioSourceComponent_SetPan,
            &AudioSourceComponent_GetPlayOnStart,
            &AudioSourceComponent_SetPlayOnStart,
            &AudioSourceComponent_GetLoop,
            &AudioSourceComponent_SetLoop,
            &AudioSourceComponent_AddReverbDSP,
            &AudioSourceComponent_AddDistortionDSP,
            &AudioSourceComponent_AddChorusDSP,
            &AudioSourceComponent_AddCompressorDSP,
            &AudioSourceComponent_AddDelayDSP,
            &AudioSourceComponent_ClearDSPs,

            &TransformComponent_GetForward,
            &TransformComponent_SetForward,
            &TransformComponent_GetRight,
            &TransformComponent_SetRight,
            &TransformComponent_GetUp,
            &TransformComponent_SetUp,
            &TransformComponent_GetTranslation,
            &TransformComponent_SetTranslation,
            &TransformComponent_GetRotation,
            &TransformComponent_SetRotation,
            &TransformComponent_GetEulerAngles,
            &TransformComponent_SetEulerAngles,
            &TransformComponent_GetScale,
            &TransformComponent_SetScale,

            &Sprite2DComponent_SetColor,
            &Sprite2DComponent_GetColor,
            &Sprite2DComponent_SetTilingFactor,
            &Sprite2DComponent_GetTilingFactor,

            &Circle2DComponent_SetColor,
            &Circle2DComponent_GetColor,

            &Rigidbody2DComponent_GetType,
            &Rigidbody2DComponent_SetType,
            &Rigidbody2DComponent_GetLinearVelocity,
            &Rigidbody2DComponent_SetLinearVelocity,
            &Rigidbody2DComponent_GetAngularVelocity,
            &Rigidbody2DComponent_SetAngularVelocity,
            &Rigidbody2DComponent_GetGravityScale,
            &Rigidbody2DComponent_SetGravityScale,
            &Rigidbody2DComponent_GetLinearDamping,
            &Rigidbody2DComponent_SetLinearDamping,
            &Rigidbody2DComponent_GetAngularDamping,
            &Rigidbody2DComponent_SetAngularDamping,
            &Rigidbody2DComponent_GetIsAwake,
            &Rigidbody2DComponent_SetIsAwake,
            &Rigidbody2DComponent_GetIsEnabled,
            &Rigidbody2DComponent_SetIsEnabled,
            &Rigidbody2DComponent_GetIsEnableSleep,
            &Rigidbody2DComponent_SetIsEnableSleep,
            &Rigidbody2DComponent_ApplyForce,
            &Rigidbody2DComponent_ApplyForceToCenter,
            &Rigidbody2DComponent_ApplyLinearImpulse,
            &Rigidbody2DComponent_ApplyLinearImpulseToCenter,
            &Rigidbody2DComponent_ApplyAngularImpulse,
            &Rigidbody2DComponent_ApplyTorque,
            &Rigidbody2DComponent_GetMass,
            &Rigidbody2DComponent_GetIsBullet,
            &Rigidbody2DComponent_SetIsBullet,

            &BoxCollider2DComponent_GetSize,
            &BoxCollider2DComponent_SetSize,
            &BoxCollider2DComponent_GetOffset,
            &BoxCollider2DComponent_SetOffset,
            &BoxCollider2DComponent_GetRestitution,
            &BoxCollider2DComponent_SetRestitution,
            &BoxCollider2DComponent_GetFriction,
            &BoxCollider2DComponent_SetFriction,
            &BoxCollider2DComponent_GetDensity,
            &BoxCollider2DComponent_SetDensity,
            &BoxCollider2DComponent_GetIsSensor,
            &BoxCollider2DComponent_SetIsSensor,

            &CircleCollider2DComponent_GetCenter,
            &CircleCollider2DComponent_SetCenter,
            &CircleCollider2DComponent_GetRadius,
            &CircleCollider2DComponent_SetRadius,
            &CircleCollider2DComponent_GetRestitution,
            &CircleCollider2DComponent_SetRestitution,
            &CircleCollider2DComponent_GetFriction,
            &CircleCollider2DComponent_SetFriction,
            &CircleCollider2DComponent_GetDensity,
            &CircleCollider2DComponent_SetDensity,
            &CircleCollider2DComponent_GetIsSensor,
            &CircleCollider2DComponent_SetIsSensor,

            &TextComponent_SetText,
            &TextComponent_GetText,
            &TextComponent_SetColor,
            &TextComponent_GetColor,
            &TextComponent_SetKerning,
            &TextComponent_GetKerning,
            &TextComponent_SetLineSpacing,
            &TextComponent_GetLineSpacing,

            &RigidbodyComponent_GetType,
            &RigidbodyComponent_SetType,
            &RigidbodyComponent_GetMotionQuality,
            &RigidbodyComponent_GetUseGravity,
            &RigidbodyComponent_SetUseGravity,
            &RigidbodyComponent_GetMass,
            &RigidbodyComponent_GetGravityFactor,
            &RigidbodyComponent_SetGravityFactor,
            &RigidbodyComponent_GetLinearVelocity,
            &RigidbodyComponent_SetLinearVelocity,
            &RigidbodyComponent_GetAngularVelocity,
            &RigidbodyComponent_GetPosition,
            &RigidbodyComponent_SetPosition,
            &RigidbodyComponent_GetRotation,
            &RigidbodyComponent_SetRotation,
            &RigidbodyComponent_GetCenterOfMass,
            &RigidbodyComponent_IsActive,
            &RigidbodyComponent_ApplyForce,
            &RigidbodyComponent_ApplyForceToCenter,
            &RigidbodyComponent_ApplyTorque,
            &RigidbodyComponent_ApplyLinearImpulse,
            &RigidbodyComponent_ApplyLinearImpulseToCenter,
            &RigidbodyComponent_ApplyAngularImpulse,
            &RigidbodyComponent_Activate,
            &RigidbodyComponent_Deactivate,
            &RigidbodyComponent_MoveKinematic,

            &BoxColliderComponent_GetCenter,
            &BoxColliderComponent_SetCenter,
            &BoxColliderComponent_GetScale,
            &BoxColliderComponent_SetScale,
            &BoxColliderComponent_GetFriction,
            &BoxColliderComponent_SetFriction,
            &BoxColliderComponent_GetRestitution,
            &BoxColliderComponent_SetRestitution,
            &BoxColliderComponent_GetDensity,
            &BoxColliderComponent_SetDensity,

            &SphereColliderComponent_GetCenter,
            &SphereColliderComponent_SetCenter,
            &SphereColliderComponent_GetRadius,
            &SphereColliderComponent_SetRadius,
            &SphereColliderComponent_GetFriction,
            &SphereColliderComponent_SetFriction,
            &SphereColliderComponent_GetRestitution,
            &SphereColliderComponent_SetRestitution,
            &SphereColliderComponent_GetDensity,
            &SphereColliderComponent_SetDensity,

            &CapsuleColliderComponent_GetCenter,
            &CapsuleColliderComponent_SetCenter,
            &CapsuleColliderComponent_GetRadius,
            &CapsuleColliderComponent_SetRadius,
            &CapsuleColliderComponent_GetHeight,
            &CapsuleColliderComponent_SetHeight,
            &CapsuleColliderComponent_GetFriction,
            &CapsuleColliderComponent_SetFriction,
            &CapsuleColliderComponent_GetRestitution,
            &CapsuleColliderComponent_SetRestitution,
            &CapsuleColliderComponent_GetDensity,
            &CapsuleColliderComponent_SetDensity,
        };
    }

    const ComponentScriptGlueAPI *ComponentScriptGlue::GetAPI()
    {
        return &s_ComponentScriptGlueAPI;
    }

    void ComponentScriptGlue::RegisterComponents()
    {
        s_EntityHasComponentFuncs.clear();
        s_EntityAddComponentFuncs.clear();
        s_WidgetButtonEventBindings.clear();
        RegisterComponent(AllComponents {});

        LOG_INFO("[ComponentScriptGlue] Component bridge initialized (HostFXR)");
    }

    void ComponentScriptGlue::RegisterFunctions()
    {
        s_WidgetButtonEventBindings.clear();
        LOG_INFO("[ComponentScriptGlue] Function bridge initialized (HostFXR)");
    }
}
