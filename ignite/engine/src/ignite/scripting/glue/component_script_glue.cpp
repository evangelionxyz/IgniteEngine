// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

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

#include "ignite/physics/3d/physics_3d.hpp"
#include "ignite/physics/2d/physics_2d.hpp"
#include "ignite/animation/animation_montage.hpp"
#include "ignite/asset/asset_manager.hpp"

#include <glm/gtx/quaternion.hpp>

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

        static uint64_t Scene_Raycast(const glm::vec3 *origin, const glm::vec3 *direction, float maxDistance, uint32_t layerMask, glm::vec3 *outHitPoint, glm::vec3 *outHitNormal)
        {
            if (outHitPoint) *outHitPoint = glm::vec3(0.0f);
            if (outHitNormal) *outHitNormal = glm::vec3(0.0f, 1.0f, 0.0f);

            Scene *scene = GetSceneContext();
            if (!scene || !scene->GetPhysics3D() || !origin || !direction)
                return 0;

            Ray ray{ *origin, *direction };
            RaycastHit hit;
            if (!scene->GetPhysics3D()->Raycast(ray, hit, maxDistance, layerMask))
                return 0;

            if (outHitPoint)
                *outHitPoint = hit.hitPoint;

            if (outHitNormal)
                *outHitNormal = hit.hitNormal;

            return hit.userData;
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

            FMOD::System *fmodSystem = FmodAudio::GetFmodSystem();
            if (!fmodSystem || fmodSystem->createDSPByType(dspType, &dsp) != FMOD_OK || !dsp)
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
        static std::unordered_map<std::string, std::function<void(Entity)>> s_EntityRemoveComponentFuncs;

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
                const auto removeComponentFunc = [](Entity entity)
                {
                    if constexpr (!std::is_same_v<Component, IDComponent> && !std::is_same_v<Component, TransformComponent>)
                    {
                        if (entity.HasComponent<Component>())
                            entity.RemoveComponent<Component>();
                    }
                };

                s_EntityHasComponentFuncs[managedTypeName] = hasComponentFunc;
                s_EntityAddComponentFuncs[managedTypeName] = addComponentFunc;
                s_EntityRemoveComponentFuncs[managedTypeName] = removeComponentFunc;

                s_EntityHasComponentFuncs[nativeTypeName] = hasComponentFunc;
                s_EntityAddComponentFuncs[nativeTypeName] = addComponentFunc;
                s_EntityRemoveComponentFuncs[nativeTypeName] = removeComponentFunc;

                if (nativeTypeName == "SkeletalMeshComponent")
                {
                    s_EntityHasComponentFuncs["AnimatorComponent"] = hasComponentFunc;
                    s_EntityHasComponentFuncs["Ignite.AnimatorComponent"] = hasComponentFunc;
                    s_EntityHasComponentFuncs["Animator"] = hasComponentFunc;
                    s_EntityAddComponentFuncs["AnimatorComponent"] = addComponentFunc;
                    s_EntityAddComponentFuncs["Ignite.AnimatorComponent"] = addComponentFunc;
                    s_EntityAddComponentFuncs["Animator"] = addComponentFunc;
                    s_EntityRemoveComponentFuncs["AnimatorComponent"] = removeComponentFunc;
                    s_EntityRemoveComponentFuncs["Ignite.AnimatorComponent"] = removeComponentFunc;
                    s_EntityRemoveComponentFuncs["Animator"] = removeComponentFunc;
                }
                else if (nativeTypeName == "WorldEnvironment")
                {
                    s_EntityHasComponentFuncs["WorldEnvironmentComponent"] = hasComponentFunc;
                    s_EntityHasComponentFuncs["Ignite.WorldEnvironmentComponent"] = hasComponentFunc;
                    s_EntityAddComponentFuncs["WorldEnvironmentComponent"] = addComponentFunc;
                    s_EntityAddComponentFuncs["Ignite.WorldEnvironmentComponent"] = addComponentFunc;
                    s_EntityRemoveComponentFuncs["WorldEnvironmentComponent"] = removeComponentFunc;
                    s_EntityRemoveComponentFuncs["Ignite.WorldEnvironmentComponent"] = removeComponentFunc;
                }
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

            return assetManager->GetAssetImmediate<WidgetCanvas>(widgetComponent.widgetHandle);
        }

        static Ref<IWidgetItem> FindWidgetItemInCanvasRecursive(const Ref<WidgetCanvas> &canvas, const std::string &itemName, Ref<WidgetCanvas> *outOwnerCanvas = nullptr)
        {
            if (!canvas || itemName.empty())
            {
                return nullptr;
            }

            if (Ref<IWidgetItem> found = canvas->FindItemByName(itemName))
            {
                if (outOwnerCanvas) *outOwnerCanvas = canvas;
                return found;
            }

            AssetManager *am = AssetManager::GetInstance();
            if (!am)
            {
                return nullptr;
            }

            for (const WidgetChildEntry &child : canvas->GetChildWidgets())
            {
                if (child.handle == AssetHandle(0))
                    continue;
                if (Ref<WidgetCanvas> childCanvas = am->GetAssetImmediate<WidgetCanvas>(child.handle))
                {
                    if (Ref<IWidgetItem> found = FindWidgetItemInCanvasRecursive(childCanvas, itemName, outOwnerCanvas))
                    {
                        return found;
                    }
                }
            }
            return nullptr;
        }

        static Ref<IWidgetItem> FindWidgetItem(Entity entity, const std::string &itemName, Ref<WidgetCanvas> *outOwnerCanvas = nullptr)
        {
            Ref<WidgetCanvas> widgetCanvas = GetEntityWidgetCanvas(entity);
            return FindWidgetItemInCanvasRecursive(widgetCanvas, itemName, outOwnerCanvas);
        }

        static void MarkWidgetLayoutChanged(const Ref<WidgetCanvas> &canvas, const Ref<IWidgetItem> &item)
        {
            if (item)
            {
                item->MarkLayoutDirty();
                item->MarkPaintDirty();
            }
            if (canvas && canvas->GetRoot())
            {
                canvas->GetRoot()->MarkLayoutDirty();
                canvas->GetRoot()->MarkPaintDirty();
            }
        }

        static Ref<WidgetButton> FindWidgetButton(Entity entity, const std::string &buttonName)
        {
            Ref<IWidgetItem> item = FindWidgetItem(entity, buttonName);
            return (item && item->GetWidgetType() == WidgetType::Button) ? item->As<WidgetButton>() : nullptr;
        }

        static void InvokeWidgetButtonCallbacks(const WidgetButtonEventKey &key)
        {
            if (!GetEntityByID(key.entityID).IsValid())
            {
                return;
            }

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
                ? std::function<void()>([key]()
                {
                    if (Application::GetInstance())
                    {
                        Application::SubmitToMainThread([key]() { InvokeWidgetButtonCallbacks(key); }, "WidgetButtonEvent");
                    }
                    else
                    {
                        InvokeWidgetButtonCallbacks(key);
                    }
                })
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

        static void Entity_RemoveComponent(uint64_t entityID, const char *componentTypeName)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid())
                return;

            const std::string typeName = NormalizeManagedTypeName(componentTypeName);
            if (typeName.empty())
                return;

            const auto removeIt = s_EntityRemoveComponentFuncs.find(typeName);
            if (removeIt == s_EntityRemoveComponentFuncs.end())
                return;

            removeIt->second(entity);
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

        static Entity FindChildByNameRecursive(Scene *scene, Entity parent, const std::string &childName)
        {
            if (!scene || !parent.IsValid())
                return Entity{};

            if (!parent.HasComponent<IDComponent>())
                return Entity{};

            const auto &children = parent.GetComponent<IDComponent>().children;
            for (UUID childUUID : children)
            {
                Entity child = SceneManager::GetEntity(scene, childUUID);
                if (!child.IsValid())
                    continue;

                if (child.GetName() == childName)
                    return child;

                if (Entity found = FindChildByNameRecursive(scene, child, childName); found.IsValid())
                    return found;
            }

            return Entity{};
        }

        static uint64_t Entity_FindChildEntityByName(uint64_t entityID, const char *childName)
        {
            Scene *scene = GetSceneContext();
            if (!scene || entityID == 0 || !childName || childName[0] == '\0')
                return 0;

            Entity entity = SceneManager::GetEntity(scene, UUID(entityID));
            Entity childEntity = FindChildByNameRecursive(scene, entity, std::string(childName));
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
            entity.GetComponent<TransformComponent>().world.translation = *value;

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

            copyEntity.GetComponent<TransformComponent>().world.translation = *value;

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
            if (!entity.IsValid() || !entity.HasComponent<RenderingComponent>())
                return;

            entity.GetComponent<RenderingComponent>().visible = value;
        }

        static void Entity_GetVisibility(uint64_t entityID, bool *result)
        {
            if (!result)
                return;

            *result = false;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RenderingComponent>())
                return;

            *result = entity.GetComponent<RenderingComponent>().visible;
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

            const int methodId = scriptInstance->GetScriptClass()->BindInstanceMethod(scriptInstance->GetInstanceId(), resolvedMethodName);
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

        static Ref<WidgetLabel> FindWidgetLabel(Entity entity, const std::string &name, Ref<WidgetCanvas> *outCanvas = nullptr)
        {
            Ref<IWidgetItem> item = FindWidgetItem(entity, name, outCanvas);
            return (item && item->GetWidgetType() == WidgetType::Label) ? item->As<WidgetLabel>() : nullptr;
        }

        static Ref<WidgetImage> FindWidgetImage(Entity entity, const std::string &name, Ref<WidgetCanvas> *outCanvas = nullptr)
        {
            Ref<IWidgetItem> item = FindWidgetItem(entity, name, outCanvas);
            return (item && item->GetWidgetType() == WidgetType::Image) ? item->As<WidgetImage>() : nullptr;
        }

        // --- Label ---
        static bool WidgetComponent_HasLabel(uint64_t entityID, const char *labelName)
        {
            if (!labelName) return false;
            return static_cast<bool>(FindWidgetLabel(GetEntityByID(entityID), std::string(labelName)));
        }

        static std::string s_WidgetLabelTextBuffer;
        static void WidgetComponent_GetLabelText(uint64_t entityID, const char *labelName, const char **result)
        {
            if (!labelName || !result) return;
            if (Ref<WidgetLabel> lbl = FindWidgetLabel(GetEntityByID(entityID), std::string(labelName)))
            {
                s_WidgetLabelTextBuffer = lbl->text;
                *result = s_WidgetLabelTextBuffer.c_str();
            }
        }

        static void WidgetComponent_SetLabelText(uint64_t entityID, const char *labelName, const char *text)
        {
            if (!labelName || !text) return;
            Entity entity = GetEntityByID(entityID);
            Ref<WidgetCanvas> ownerCanvas;
            if (Ref<WidgetLabel> lbl = FindWidgetLabel(entity, std::string(labelName), &ownerCanvas))
            {
                lbl->text = text;
                MarkWidgetLayoutChanged(ownerCanvas, lbl);
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
            Ref<WidgetCanvas> ownerCanvas;
            if (Ref<WidgetLabel> lbl = FindWidgetLabel(entity, std::string(labelName), &ownerCanvas))
            {
                lbl->style.color = *color;
                MarkWidgetLayoutChanged(ownerCanvas, lbl);
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
            Ref<WidgetCanvas> ownerCanvas;
            if (Ref<WidgetLabel> lbl = FindWidgetLabel(entity, std::string(labelName), &ownerCanvas))
            {
                lbl->style.fontSize = size;
                MarkWidgetLayoutChanged(ownerCanvas, lbl);
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
            Ref<WidgetCanvas> ownerCanvas;
            if (Ref<WidgetImage> img = FindWidgetImage(entity, std::string(imageName), &ownerCanvas))
            {
                img->imageHandle = AssetHandle(handle);
                img->image = nullptr;
                MarkWidgetLayoutChanged(ownerCanvas, img);
            }
        }

        // --- Extended Widget Item / Canvas / Button APIs ---
        static void WidgetComponent_GetCanvasEnabled(uint64_t entityID, bool *result)
        {
            if (!result) return;
            Ref<WidgetCanvas> c = GetEntityWidgetCanvas(GetEntityByID(entityID));
            *result = c ? c->IsEnabled() : false;
        }

        static void WidgetComponent_SetCanvasEnabled(uint64_t entityID, bool enabled)
        {
            if (Ref<WidgetCanvas> c = GetEntityWidgetCanvas(GetEntityByID(entityID)))
            {
                c->SetEnabled(enabled);
                if (c->GetRoot()) c->GetRoot()->MarkLayoutDirty();
            }
        }

        static bool WidgetComponent_HasItem(uint64_t entityID, const char *itemName)
        {
            if (!itemName) return false;
            return static_cast<bool>(FindWidgetItem(GetEntityByID(entityID), std::string(itemName)));
        }

        static void WidgetComponent_GetItemVisible(uint64_t entityID, const char *itemName, bool *result)
        {
            if (!itemName || !result) return;
            Ref<IWidgetItem> item = FindWidgetItem(GetEntityByID(entityID), std::string(itemName));
            *result = item ? item->IsVisible() : false;
        }

        static void WidgetComponent_SetItemVisible(uint64_t entityID, const char *itemName, bool visible)
        {
            if (!itemName) return;
            Ref<WidgetCanvas> ownerCanvas;
            if (Ref<IWidgetItem> item = FindWidgetItem(GetEntityByID(entityID), std::string(itemName), &ownerCanvas))
            {
                item->SetVisible(visible);
                MarkWidgetLayoutChanged(ownerCanvas, item);
            }
        }

        static void WidgetComponent_GetItemBackgroundColor(uint64_t entityID, const char *itemName, glm::vec4 *result)
        {
            if (!itemName || !result) return;
            if (Ref<IWidgetItem> item = FindWidgetItem(GetEntityByID(entityID), std::string(itemName)))
                *result = item->baseStyle.backgroundColor;
        }

        static void WidgetComponent_SetItemBackgroundColor(uint64_t entityID, const char *itemName, const glm::vec4 *color)
        {
            if (!itemName || !color) return;
            Ref<WidgetCanvas> ownerCanvas;
            if (Ref<IWidgetItem> item = FindWidgetItem(GetEntityByID(entityID), std::string(itemName), &ownerCanvas))
            {
                item->baseStyle.backgroundColor = *color;
                MarkWidgetLayoutChanged(ownerCanvas, item);
            }
        }

        static void WidgetComponent_GetItemBorderColor(uint64_t entityID, const char *itemName, glm::vec4 *result)
        {
            if (!itemName || !result) return;
            if (Ref<IWidgetItem> item = FindWidgetItem(GetEntityByID(entityID), std::string(itemName)))
                *result = item->baseStyle.borderColor;
        }

        static void WidgetComponent_SetItemBorderColor(uint64_t entityID, const char *itemName, const glm::vec4 *color)
        {
            if (!itemName || !color) return;
            Ref<WidgetCanvas> ownerCanvas;
            if (Ref<IWidgetItem> item = FindWidgetItem(GetEntityByID(entityID), std::string(itemName), &ownerCanvas))
            {
                item->baseStyle.borderColor = *color;
                if (item->GetWidgetType() == WidgetType::Button)
                {
                    item->As<WidgetButton>()->style.borderColor = *color;
                }
                MarkWidgetLayoutChanged(ownerCanvas, item);
            }
        }

        static void WidgetComponent_GetItemOpacity(uint64_t entityID, const char *itemName, float *result)
        {
            if (!itemName || !result) return;
            if (Ref<IWidgetItem> item = FindWidgetItem(GetEntityByID(entityID), std::string(itemName)))
                *result = item->baseStyle.opacity;
        }

        static void WidgetComponent_SetItemOpacity(uint64_t entityID, const char *itemName, float opacity)
        {
            if (!itemName) return;
            Ref<WidgetCanvas> ownerCanvas;
            if (Ref<IWidgetItem> item = FindWidgetItem(GetEntityByID(entityID), std::string(itemName), &ownerCanvas))
            {
                item->baseStyle.opacity = opacity;
                MarkWidgetLayoutChanged(ownerCanvas, item);
            }
        }

        static void WidgetComponent_GetItemSize(uint64_t entityID, const char *itemName, glm::vec2 *result)
        {
            if (!itemName || !result) return;
            if (Ref<IWidgetItem> item = FindWidgetItem(GetEntityByID(entityID), std::string(itemName)))
                *result = item->GetSize();
        }

        static void WidgetComponent_SetItemSize(uint64_t entityID, const char *itemName, const glm::vec2 *size)
        {
            if (!itemName || !size) return;
            Ref<WidgetCanvas> ownerCanvas;
            if (Ref<IWidgetItem> item = FindWidgetItem(GetEntityByID(entityID), std::string(itemName), &ownerCanvas))
            {
                item->SetSize(*size);
                MarkWidgetLayoutChanged(ownerCanvas, item);
            }
        }

        static void WidgetComponent_GetItemPosition(uint64_t entityID, const char *itemName, glm::vec2 *result)
        {
            if (!itemName || !result) return;
            if (Ref<IWidgetItem> item = FindWidgetItem(GetEntityByID(entityID), std::string(itemName)))
                *result = item->GetPosition();
        }

        static void WidgetComponent_SetItemPosition(uint64_t entityID, const char *itemName, const glm::vec2 *pos)
        {
            if (!itemName || !pos) return;
            Ref<WidgetCanvas> ownerCanvas;
            if (Ref<IWidgetItem> item = FindWidgetItem(GetEntityByID(entityID), std::string(itemName), &ownerCanvas))
            {
                item->SetPosition(*pos);
                MarkWidgetLayoutChanged(ownerCanvas, item);
            }
        }

        static std::string s_WidgetButtonTextBuffer;
        static void WidgetComponent_GetButtonText(uint64_t entityID, const char *buttonName, const char **result)
        {
            if (!buttonName || !result) return;
            if (Ref<WidgetButton> btn = FindWidgetButton(GetEntityByID(entityID), std::string(buttonName)))
            {
                s_WidgetButtonTextBuffer = btn->label ? btn->label->text : "";
                *result = s_WidgetButtonTextBuffer.c_str();
            }
        }

        static void WidgetComponent_SetButtonText(uint64_t entityID, const char *buttonName, const char *text)
        {
            if (!buttonName || !text) return;
            Ref<WidgetCanvas> ownerCanvas;
            if (Ref<IWidgetItem> item = FindWidgetItem(GetEntityByID(entityID), std::string(buttonName), &ownerCanvas))
            {
                if (item->GetWidgetType() == WidgetType::Button)
                {
                    Ref<WidgetButton> btn = item->As<WidgetButton>();
                    if (btn->label)
                    {
                        btn->label->text = text;
                        MarkWidgetLayoutChanged(ownerCanvas, btn);
                    }
                }
            }
        }

        static void WidgetComponent_GetButtonInteractable(uint64_t entityID, const char *buttonName, bool *result)
        {
            if (!buttonName || !result) return;
            if (Ref<WidgetButton> btn = FindWidgetButton(GetEntityByID(entityID), std::string(buttonName)))
                *result = btn->interactable;
        }

        static void WidgetComponent_SetButtonInteractable(uint64_t entityID, const char *buttonName, bool interactable)
        {
            if (!buttonName) return;
            if (Ref<WidgetButton> btn = FindWidgetButton(GetEntityByID(entityID), std::string(buttonName)))
            {
                btn->interactable = interactable;
                if (!interactable)
                    btn->ResetInteractionState();
                btn->MarkPaintDirty();
            }
        }

        static void WidgetComponent_IsButtonHovered(uint64_t entityID, const char *buttonName, bool *result)
        {
            if (!buttonName || !result) return;
            Ref<WidgetButton> btn = FindWidgetButton(GetEntityByID(entityID), std::string(buttonName));
            *result = btn ? btn->hovered : false;
        }

        static void WidgetComponent_SetButtonColors(uint64_t entityID, const char *buttonName, const glm::vec4 *normalColor, const glm::vec4 *hoverColor, const glm::vec4 *pressedColor)
        {
            if (!buttonName) return;
            if (Ref<WidgetButton> btn = FindWidgetButton(GetEntityByID(entityID), std::string(buttonName)))
            {
                if (normalColor) btn->style.color = *normalColor;
                if (hoverColor) btn->style.hoverColor = *hoverColor;
                if (pressedColor) btn->style.pressedColor = *pressedColor;
                btn->MarkPaintDirty();
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
            *result = glm::vec3(transform.world.rotation * glm::vec3(0.0f, 0.0f, -1.0f));
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
            transform.local.rotation = rotation;
            transform.world.rotation = rotation;
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
            *result = glm::vec3(transform.world.rotation * glm::vec3(1.0f, 0.0f, 0.0f));
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
            *result = glm::vec3(transform.world.rotation * glm::vec3(0.0f, 1.0f, 0.0f));
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
            glm::vec3 right = glm::normalize(transform.world.rotation * glm::vec3(1.0f, 0.0f, 0.0f));
            glm::vec3 forward = glm::normalize(glm::cross(up, right));
            if (glm::length2(forward) <= 0.0f)
            {
                return;
            }

            const glm::quat rotation = glm::quatLookAtRH(forward, up);
            transform.local.rotation = rotation;
            transform.world.rotation = rotation;
            transform.dirty = true;
        }

        static void SyncEntityWorldTransform(Entity entity, TransformComponent &transform)
        {
            Scene *scene = GetSceneContext();
            if (entity.GetParentUUID() != UUID(0) && scene)
            {
                Entity parent = SceneManager::GetEntity(scene, entity.GetParentUUID());
                if (parent.IsValid() && parent.HasComponent<TransformComponent>())
                {
                    const auto &parentTr = parent.GetComponent<TransformComponent>();
                    const glm::mat4 worldMat = parentTr.world.GetMatrix() * transform.local.GetMatrix();
                    Transform::Decompose(worldMat, transform.world);
                }
                else
                {
                    transform.world.translation = transform.local.translation;
                    transform.world.rotation = transform.local.rotation;
                    transform.world.scale = transform.local.scale;
                }
            }
            else
            {
                transform.world.translation = transform.local.translation;
                transform.world.rotation = transform.local.rotation;
                transform.world.scale = transform.local.scale;
            }

            transform.dirty = true;

            if (scene && entity.HasComponent<IDComponent>())
            {
                const auto &idc = entity.GetComponent<IDComponent>();
                const glm::mat4 parentWorld = transform.world.GetMatrix();
                for (UUID childUUID : idc.children)
                {
                    Entity child = SceneManager::GetEntity(scene, childUUID);
                    if (child.IsValid() && child.HasComponent<TransformComponent>())
                    {
                        auto &childTr = child.GetComponent<TransformComponent>();
                        const glm::mat4 childWorld = parentWorld * childTr.local.GetMatrix();
                        Transform::Decompose(childWorld, childTr.world);
                        childTr.dirty = true;
                    }
                }
            }
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

            *result = glm::vec3(entity.GetComponent<TransformComponent>().world.translation);
        }

        static void TransformComponent_SetTranslation(uint64_t entityID, const glm::vec3 *value)
        {
            if (!value) return;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TransformComponent>())
                return;

            auto &transform = entity.GetComponent<TransformComponent>();
            transform.local.translation = *value;
            SyncEntityWorldTransform(entity, transform);

            if (entity.HasComponent<CharacterControllerComponent>())
            {
                auto &cc = entity.GetComponent<CharacterControllerComponent>();
                if (auto actor = cc.character.lock())
                {
                    actor->SetPosition(transform.world.translation);
                }
            }
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

            *result = glm::quat(entity.GetComponent<TransformComponent>().world.rotation);
        }

        static void TransformComponent_SetRotation(uint64_t entityID, const glm::quat *value)
        {
            if (!value) return;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TransformComponent>())
                return;

            auto &transform = entity.GetComponent<TransformComponent>();
            transform.local.rotation = *value;
            SyncEntityWorldTransform(entity, transform);

            if (entity.HasComponent<CharacterControllerComponent>())
            {
                auto &cc = entity.GetComponent<CharacterControllerComponent>();
                if (auto actor = cc.character.lock())
                {
                    actor->SetRotation(transform.world.rotation);
                }
            }
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

            *result = glm::vec3(glm::eulerAngles(entity.GetComponent<TransformComponent>().world.rotation));
        }

        static void TransformComponent_SetEulerAngles(uint64_t entityID, const glm::vec3 *value)
        {
            if (!value) return;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TransformComponent>())
                return;

            auto &transform = entity.GetComponent<TransformComponent>();
            const glm::quat rotation = glm::quat(*value);
            transform.local.rotation = rotation;
            SyncEntityWorldTransform(entity, transform);

            if (entity.HasComponent<CharacterControllerComponent>())
            {
                auto &cc = entity.GetComponent<CharacterControllerComponent>();
                if (auto actor = cc.character.lock())
                {
                    actor->SetRotation(transform.world.rotation);
                }
            }
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

            *result = glm::vec3(entity.GetComponent<TransformComponent>().world.scale);
        }

        static void TransformComponent_SetScale(uint64_t entityID, const glm::vec3 *value)
        {
            if (!value) return;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<TransformComponent>())
                return;

            auto &transform = entity.GetComponent<TransformComponent>();
            transform.local.scale = *value;
            SyncEntityWorldTransform(entity, transform);
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

            *result = static_cast<int32_t>(physics::BodyType::Static);
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
            rb.bodyType = static_cast<physics::BodyType>(value);
            if (Scene *scene = GetSceneContext(); scene->GetPhysics2D())
            {
                scene->GetPhysics2D()->SetBodyType(rb.bodyId, static_cast<b2BodyType>(rb.bodyType));
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
            if (Scene *scene = GetSceneContext(); scene->GetPhysics2D())
            {
                scene->GetPhysics2D()->SetLinearVelocity(rb.bodyId, *value);
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
            if (Scene *scene = GetSceneContext(); scene->GetPhysics2D())
            {
                scene->GetPhysics2D()->SetAngularVelocity(rb.bodyId, value);
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
            if (Scene *scene = GetSceneContext(); scene->GetPhysics2D())
            {
                scene->GetPhysics2D()->SetGravityScale(rb.bodyId, value);
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
            if (Scene *scene = GetSceneContext(); scene->GetPhysics2D())
            {
                scene->GetPhysics2D()->SetLinearDamping(rb.bodyId, value);
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
            if (Scene *scene = GetSceneContext(); scene->GetPhysics2D())
            {
                scene->GetPhysics2D()->SetAngularDamping(rb.bodyId, value);
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
            if (Scene *scene = GetSceneContext(); scene->GetPhysics2D())
            {
                scene->GetPhysics2D()->SetAwake(rb.bodyId, value);
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
            if (Scene *scene = GetSceneContext(); scene->GetPhysics2D())
            {
                if (value)
                {
                    scene->GetPhysics2D()->ActivateBody(rb.bodyId);
                }
                else
                {
                    scene->GetPhysics2D()->DeactivateBody(rb.bodyId);
                }
            }
        }

        static void Rigidbody2DComponent_GetIsEnableSleeping(uint64_t entityID, bool *result)
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
            if (Scene *scene = GetSceneContext(); scene->GetPhysics2D())
            {
                *result = scene->GetPhysics2D()->IsSleepingEnabled(rb.bodyId);
            }
        }

        static void Rigidbody2DComponent_SetIsEnableSleeping(uint64_t entityID, bool value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
                return;

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            if (Scene *scene = GetSceneContext(); scene->GetPhysics2D())
            {
                rb.enableSleeping = value;
                scene->GetPhysics2D()->SetEnableSleeping(rb.bodyId, value);
            }
        }

        static void Rigidbody2DComponent_ApplyForce(uint64_t entityID, glm::vec2 force, glm::vec2 point, bool wake)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
                return;

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            if (Scene *scene = GetSceneContext(); scene->GetPhysics2D())
            {
                scene->GetPhysics2D()->ApplyForce(rb.bodyId, force, point, wake);
            }
        }

        static void Rigidbody2DComponent_ApplyForceToCenter(uint64_t entityID, glm::vec2 force, bool wake)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
                return;

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            if (Scene *scene = GetSceneContext(); scene->GetPhysics2D())
            {
                scene->GetPhysics2D()->ApplyForceToCenter(rb.bodyId, force, wake);
            }
        }

        static void Rigidbody2DComponent_ApplyLinearImpulse(uint64_t entityID, glm::vec2 impulse, glm::vec2 point, bool wake)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
                return;

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            if (Scene *scene = GetSceneContext(); scene->GetPhysics2D())
            {
                scene->GetPhysics2D()->ApplyLinearImpulse(rb.bodyId, impulse, point, wake);
            }
        }

        static void Rigidbody2DComponent_ApplyLinearImpulseToCenter(uint64_t entityID, glm::vec2 impulse, bool wake)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
                return;

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            if (Scene *scene = GetSceneContext(); scene->GetPhysics2D())
            {
                scene->GetPhysics2D()->ApplyLinearImpulseToCenter(rb.bodyId, impulse, wake);
            }
        }

        static void Rigidbody2DComponent_ApplyAngularImpulse(uint64_t entityID, float impulse, bool wake)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
                return;

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            if (Scene *scene = GetSceneContext(); scene->GetPhysics2D())
            {
                scene->GetPhysics2D()->ApplyAngularImpulse(rb.bodyId, impulse, wake);
            }
        }

        static void Rigidbody2DComponent_ApplyTorque(uint64_t entityID, float torque, bool wake)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
                return;

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            if (Scene *scene = GetSceneContext(); scene->GetPhysics2D())
            {
                scene->GetPhysics2D()->ApplyTorque(rb.bodyId, torque, wake);
            }
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
            if (Scene *scene = GetSceneContext(); scene->GetPhysics2D())
            {
                *result = scene->GetPhysics2D()->GetMass(rb.bodyId);
            }
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
            if (Scene *scene = GetSceneContext(); scene->GetPhysics2D())
            {
                *result = scene->GetPhysics2D()->IsBullet(rb.bodyId);
            }
        }

        static void Rigidbody2DComponent_SetIsBullet(uint64_t entityID, bool value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<Rigidbody2DComponent>())
                return;

            auto &rb = entity.GetComponent<Rigidbody2DComponent>();
            if (Scene *scene = GetSceneContext(); scene->GetPhysics2D())
            {
                scene->GetPhysics2D()->SetBullet(rb.bodyId, value);
            }
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
			*result = static_cast<int32_t>(physics::BodyType::Static);
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
            rb.bodyType = static_cast<physics::BodyType>(value);
            if (auto dynActor = rb.dynamicActor.lock())
            {
                dynActor->SetMotionType(static_cast<physics::BodyType>(rb.bodyType));
            }
        }

        static void RigidbodyComponent_GetMotionQuality(uint64_t entityID, int32_t *result)
        {
            if (!result) return;
            *result = static_cast<int32_t>(physics::MotionQuality::Discrete);
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
            if (auto dynActor = rb.dynamicActor.lock())
            {
                dynActor->SetGravityFactor(rb.useGravity ? rb.gravityFactor : 0.0f);
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
            if (auto dynActor = rb.dynamicActor.lock())
            {
                dynActor->SetGravityFactor(rb.useGravity ? rb.gravityFactor : 0.0f);
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
            if (auto dynActor = rb.dynamicActor.lock())
            {
                *result = dynActor->GetLinearVelocity();
            }
            else
            {
                *result = rb.linearVelocity;
            }
        }

        static void RigidbodyComponent_GetLayer(uint64_t entityID, uint32_t *result)
        {
            if (!result) return;
            *result = 0;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            *result = entity.GetComponent<RigidbodyComponent>().layer;
        }

        static void RigidbodyComponent_SetLayer(uint64_t entityID, uint32_t value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            rb.layer = value;
        }

        static void RigidbodyComponent_SetLinearVelocity(uint64_t entityID, const glm::vec3 *value)
        {
            if (!value) return;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            rb.linearVelocity = *value;
            if (auto dynActor = rb.dynamicActor.lock())
            {
                dynActor->SetLinearVelocity(*value);
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
            if (auto dynActor = rb.dynamicActor.lock())
            {
                *result = dynActor->GetPosition();
            }
            else if (auto statActor = rb.staticActor.lock())
            {
                *result = statActor->GetPosition();
            }
        }

        static void RigidbodyComponent_SetPosition(uint64_t entityID, const glm::vec3 *value)
        {
            if (!value) return;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            if (auto dynActor = rb.dynamicActor.lock())
            {
                dynActor->SetPosition(*value, true);
            }
            else if (auto statActor = rb.staticActor.lock())
            {
                statActor->SetPosition(*value, true);
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
            if (auto dynActor = rb.dynamicActor.lock())
            {
                *result = dynActor->GetRotation();
            }
            else if (auto statActor = rb.staticActor.lock())
            {
                *result = statActor->GetRotation();
            }
        }

        static void RigidbodyComponent_SetRotation(uint64_t entityID, const glm::quat *value)
        {
            if (!value) return;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            if (auto dynActor = rb.dynamicActor.lock())
            {
                dynActor->SetRotation(*value, true);
            }
            else if (auto statActor = rb.staticActor.lock())
            {
                statActor->SetRotation(*value, true);
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
            if (auto dynActor = rb.dynamicActor.lock())
            {
                *result = dynActor->GetAngularVelocity();
            }
            else
            {
                *result = rb.angularVelocity;
            }
        }

        static void RigidbodyComponent_SetAngularVelocity(uint64_t entityID, const glm::vec3 *value)
        {
            if (!value) return;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            rb.angularVelocity = *value;
            if (auto dynActor = rb.dynamicActor.lock())
            {
                dynActor->SetAngularVelocity(*value);
            }
        }

        static void RigidbodyComponent_SetMass(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            rb.mass = value;
        }

        static void RigidbodyComponent_GetLinearDamping(uint64_t entityID, float *result)
        {
            if (!result) return;
            *result = 0.0f;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            *result = entity.GetComponent<RigidbodyComponent>().linearDamping;
        }

        static void RigidbodyComponent_SetLinearDamping(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            rb.linearDamping = value;
            if (auto dynActor = rb.dynamicActor.lock())
            {
                dynActor->SetLinearDamping(value);
            }
        }

        static void RigidbodyComponent_GetAngularDamping(uint64_t entityID, float *result)
        {
            if (!result) return;
            *result = 0.0f;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            *result = entity.GetComponent<RigidbodyComponent>().angularDamping;
        }

        static void RigidbodyComponent_SetAngularDamping(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            rb.angularDamping = value;
            if (auto dynActor = rb.dynamicActor.lock())
            {
                dynActor->SetAngularDamping(value);
            }
        }

        static void RigidbodyComponent_GetFriction(uint64_t entityID, float *result)
        {
            if (!result) return;
            *result = 0.0f;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            if (auto dynActor = rb.dynamicActor.lock())
            {
                *result = dynActor->GetFriction();
            }
            else if (auto statActor = rb.staticActor.lock())
            {
                *result = statActor->GetFriction();
            }
            else
            {
                *result = rb.friction;
            }
        }

        static void RigidbodyComponent_SetFriction(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            rb.friction = value;
            if (auto dynActor = rb.dynamicActor.lock())
            {
                dynActor->SetFriction(value);
            }
            else if (auto statActor = rb.staticActor.lock())
            {
                statActor->SetFriction(value);
            }
        }

        static void RigidbodyComponent_GetRestitution(uint64_t entityID, float *result)
        {
            if (!result) return;
            *result = 0.0f;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            if (auto dynActor = rb.dynamicActor.lock())
            {
                *result = dynActor->GetRestitution();
            }
            else if (auto statActor = rb.staticActor.lock())
            {
                *result = statActor->GetRestitution();
            }
            else
            {
                *result = rb.restitution;
            }
        }

        static void RigidbodyComponent_SetRestitution(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            rb.restitution = value;
            if (auto dynActor = rb.dynamicActor.lock())
            {
                dynActor->SetRestitution(value);
            }
            else if (auto statActor = rb.staticActor.lock())
            {
                statActor->SetRestitution(value);
            }
        }

        static void RigidbodyComponent_GetMaxLinearVelocity(uint64_t entityID, float *result)
        {
            if (!result) return;
            *result = 0.0f;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            *result = entity.GetComponent<RigidbodyComponent>().maxLinearVelocity;
        }

        static void RigidbodyComponent_SetMaxLinearVelocity(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            rb.maxLinearVelocity = value;
            if (auto dynActor = rb.dynamicActor.lock())
            {
                dynActor->SetMaxLinearVelocity(value);
            }
        }

        static void RigidbodyComponent_GetMaxAngularVelocity(uint64_t entityID, float *result)
        {
            if (!result) return;
            *result = 0.0f;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            *result = entity.GetComponent<RigidbodyComponent>().maxAngularVelocity;
        }

        static void RigidbodyComponent_SetMaxAngularVelocity(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            rb.maxAngularVelocity = value;
            if (auto dynActor = rb.dynamicActor.lock())
            {
                dynActor->SetMaxAngularVelocity(value);
            }
        }

        static void RigidbodyComponent_GetApplyGyroscopicForce(uint64_t entityID, bool *result)
        {
            if (!result) return;
            *result = false;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            *result = entity.GetComponent<RigidbodyComponent>().applyGyroscopicForce;
        }

        static void RigidbodyComponent_SetApplyGyroscopicForce(uint64_t entityID, bool value)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            rb.applyGyroscopicForce = value;
        }

        static void RigidbodyComponent_GetCenterOfMass(uint64_t entityID, glm::vec3 *result)
        {
            if (!result) return;
            *result = glm::vec3(0.0f);
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            if (auto dynActor = rb.dynamicActor.lock())
            {
                *result = dynActor->GetCenterOfMassPosition();
            }
            else
            {
                *result = rb.centerMass;
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
            if (auto dynActor = rb.dynamicActor.lock())
            {
                *result = dynActor->IsActive();
            }
            else if (auto statActor = rb.staticActor.lock())
            {
                *result = statActor->IsActive();
            }
        }

        static void RigidbodyComponent_ApplyForce(uint64_t entityID, const glm::vec3 *force, const glm::vec3 *point)
        {
            if (!force || !point) return;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            if (auto dynActor = rb.dynamicActor.lock())
            {
                dynActor->AddForceAtPosition(*force, *point);
            }
        }

        static void RigidbodyComponent_ApplyForceToCenter(uint64_t entityID, const glm::vec3 *force)
        {
            if (!force) return;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            if (auto dynActor = rb.dynamicActor.lock())
            {
                dynActor->AddForce(*force);
            }
        }

        static void RigidbodyComponent_ApplyTorque(uint64_t entityID, const glm::vec3 *torque)
        {
            if (!torque) return;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            if (auto dynActor = rb.dynamicActor.lock())
            {
                dynActor->AddTorque(*torque);
            }
        }

        static void RigidbodyComponent_ApplyLinearImpulse(uint64_t entityID, const glm::vec3 *impulse, const glm::vec3 *point)
        {
            if (!impulse || !point) return;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            if (auto dynActor = rb.dynamicActor.lock())
            {
                dynActor->AddImpulseAtPosition(*impulse, *point);
            }
        }

        static void RigidbodyComponent_ApplyLinearImpulseToCenter(uint64_t entityID, const glm::vec3 *impulse)
        {
            if (!impulse) return;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            if (auto dynActor = rb.dynamicActor.lock())
            {
                dynActor->AddImpulse(*impulse);
            }
        }

        static void RigidbodyComponent_ApplyAngularImpulse(uint64_t entityID, const glm::vec3 *impulse)
        {
            if (!impulse) return;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            if (auto dynActor = rb.dynamicActor.lock())
            {
                dynActor->AddAngularImpulse(*impulse);
            }
        }

        static void RigidbodyComponent_Activate(uint64_t entityID)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            if (auto dynActor = rb.dynamicActor.lock())
            {
                dynActor->ActivateBody();
            }
            else if (auto statActor = rb.staticActor.lock())
            {
                statActor->ActivateBody();
            }
        }

        static void RigidbodyComponent_Deactivate(uint64_t entityID)
        {
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            if (auto dynActor = rb.dynamicActor.lock())
            {
                dynActor->DeactivateBody();
            }
            else if (auto statActor = rb.staticActor.lock())
            {
                statActor->DeactivateBody();
            }
        }

        static void RigidbodyComponent_MoveKinematic(uint64_t entityID, const glm::vec3 *targetPosition, const glm::vec3 *targetRotation, float deltaTime)
        {
            if (!targetPosition || !targetRotation) return;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<RigidbodyComponent>())
                return;
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            if (auto dynActor = rb.dynamicActor.lock())
            {
                dynActor->SetPosition(*targetPosition, true);
                dynActor->SetEulerAngleRotation(*targetRotation, true);
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
                auto &comp = entity.GetComponent<BoxColliderComponent>();
                comp.center = *value;
                comp.dirty = true;
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
                auto &comp = entity.GetComponent<BoxColliderComponent>();
                comp.scale = *value;
                comp.dirty = true;
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
                auto &comp = entity.GetComponent<SphereColliderComponent>();
                comp.center = *value;
                comp.dirty = true;
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
                auto &comp = entity.GetComponent<SphereColliderComponent>();
                comp.radius = value;
                comp.dirty = true;
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
                auto &comp = entity.GetComponent<CapsuleColliderComponent>();
                comp.center = *value;
                comp.dirty = true;
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
                auto &comp = entity.GetComponent<CapsuleColliderComponent>();
                comp.radius = value;
                comp.dirty = true;
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
                auto &comp = entity.GetComponent<CapsuleColliderComponent>();
                comp.height = value;
                comp.dirty = true;
            }
        }

        // --- CharacterControllerComponent ---
        static void CharacterControllerComponent_GetCenter(uint64_t entityID, glm::vec3 *result)
        {
            if (!result) return;
            *result = glm::vec3(0.0f);
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CharacterControllerComponent>())
                *result = entity.GetComponent<CharacterControllerComponent>().center;
        }

        static void CharacterControllerComponent_SetCenter(uint64_t entityID, const glm::vec3 *value)
        {
            if (!value) return;
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CharacterControllerComponent>())
            {
                auto &cc = entity.GetComponent<CharacterControllerComponent>();
                cc.center = *value;
                cc.dirty = true;
            }
        }

        static void CharacterControllerComponent_GetRadius(uint64_t entityID, float *result)
        {
            if (!result) return;
            *result = 0.5f;
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CharacterControllerComponent>())
                *result = entity.GetComponent<CharacterControllerComponent>().radius;
        }

        static void CharacterControllerComponent_SetRadius(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CharacterControllerComponent>())
            {
                auto &cc = entity.GetComponent<CharacterControllerComponent>();
                cc.radius = value;
                cc.dirty = true;
            }
        }

        static void CharacterControllerComponent_GetHeight(uint64_t entityID, float *result)
        {
            if (!result) return;
            *result = 2.0f;
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CharacterControllerComponent>())
                *result = entity.GetComponent<CharacterControllerComponent>().height;
        }

        static void CharacterControllerComponent_SetHeight(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CharacterControllerComponent>())
            {
                auto &cc = entity.GetComponent<CharacterControllerComponent>();
                cc.height = value;
                cc.dirty = true;
            }
        }

        static void CharacterControllerComponent_GetMaxStepHeight(uint64_t entityID, float *result)
        {
            if (!result) return;
            *result = 0.4f;
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CharacterControllerComponent>())
                *result = entity.GetComponent<CharacterControllerComponent>().maxStepHeight;
        }

        static void CharacterControllerComponent_SetMaxStepHeight(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CharacterControllerComponent>())
            {
                auto &cc = entity.GetComponent<CharacterControllerComponent>();
                cc.maxStepHeight = value;
                cc.dirty = true;
            }
        }

        static void CharacterControllerComponent_GetMaxSlopeAngle(uint64_t entityID, float *result)
        {
            if (!result) return;
            *result = 45.0f;
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CharacterControllerComponent>())
                *result = entity.GetComponent<CharacterControllerComponent>().maxSlopeAngle;
        }

        static void CharacterControllerComponent_SetMaxSlopeAngle(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CharacterControllerComponent>())
            {
                auto &cc = entity.GetComponent<CharacterControllerComponent>();
                cc.maxSlopeAngle = value;
                cc.dirty = true;
            }
        }

        static void CharacterControllerComponent_GetMass(uint64_t entityID, float *result)
        {
            if (!result) return;
            *result = 80.0f;
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CharacterControllerComponent>())
                *result = entity.GetComponent<CharacterControllerComponent>().mass;
        }

        static void CharacterControllerComponent_SetMass(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CharacterControllerComponent>())
            {
                auto &cc = entity.GetComponent<CharacterControllerComponent>();
                cc.mass = value;
                cc.dirty = true;
            }
        }

        static void CharacterControllerComponent_GetFriction(uint64_t entityID, float *result)
        {
            if (!result) return;
            *result = 0.2f;
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CharacterControllerComponent>())
                *result = entity.GetComponent<CharacterControllerComponent>().friction;
        }

        static void CharacterControllerComponent_SetFriction(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CharacterControllerComponent>())
            {
                auto &cc = entity.GetComponent<CharacterControllerComponent>();
                cc.friction = value;
                cc.dirty = true;
            }
        }

        static void CharacterControllerComponent_GetGravityFactor(uint64_t entityID, float *result)
        {
            if (!result) return;
            *result = 1.0f;
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CharacterControllerComponent>())
                *result = entity.GetComponent<CharacterControllerComponent>().gravityFactor;
        }

        static void CharacterControllerComponent_SetGravityFactor(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CharacterControllerComponent>())
            {
                auto &cc = entity.GetComponent<CharacterControllerComponent>();
                cc.gravityFactor = value;
                cc.dirty = true;
            }
        }

        static void CharacterControllerComponent_GetUp(uint64_t entityID, glm::vec3 *result)
        {
            if (!result) return;
            *result = glm::vec3(0.0f, 1.0f, 0.0f);
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CharacterControllerComponent>())
                *result = entity.GetComponent<CharacterControllerComponent>().up;
        }

        static void CharacterControllerComponent_SetUp(uint64_t entityID, const glm::vec3 *value)
        {
            if (!value) return;
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CharacterControllerComponent>())
            {
                auto &cc = entity.GetComponent<CharacterControllerComponent>();
                cc.up = *value;
                cc.dirty = true;
            }
        }

        static void CharacterControllerComponent_GetLinearVelocity(uint64_t entityID, glm::vec3 *result)
        {
            if (!result) return;
            *result = glm::vec3(0.0f);
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CharacterControllerComponent>())
            {
                auto &cc = entity.GetComponent<CharacterControllerComponent>();
                if (auto charCtrl = cc.character.lock())
                    *result = charCtrl->GetLinearVelocity();
                else
                    *result = cc.linearVelocity;
            }
        }

        static void CharacterControllerComponent_SetLinearVelocity(uint64_t entityID, const glm::vec3 *value)
        {
            if (!value) return;
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CharacterControllerComponent>())
            {
                auto &cc = entity.GetComponent<CharacterControllerComponent>();
                cc.linearVelocity = *value;
                cc.desiredVelocity = *value;
                if (auto charCtrl = cc.character.lock())
                    charCtrl->SetLinearVelocity(*value);
            }
        }

        static void CharacterControllerComponent_IsOnGround(uint64_t entityID, bool *result)
        {
            if (!result) return;
            *result = false;
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CharacterControllerComponent>())
            {
                auto &cc = entity.GetComponent<CharacterControllerComponent>();
                if (auto charCtrl = cc.character.lock())
                    *result = charCtrl->IsOnGround();
            }
        }

        static void CharacterControllerComponent_IsSupported(uint64_t entityID, bool *result)
        {
            if (!result) return;
            *result = false;
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CharacterControllerComponent>())
            {
                auto &cc = entity.GetComponent<CharacterControllerComponent>();
                if (auto charCtrl = cc.character.lock())
                    *result = charCtrl->IsSupported();
            }
        }

        static void CharacterControllerComponent_GetGroundNormal(uint64_t entityID, glm::vec3 *result)
        {
            if (!result) return;
            *result = glm::vec3(0.0f, 1.0f, 0.0f);
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CharacterControllerComponent>())
            {
                auto &cc = entity.GetComponent<CharacterControllerComponent>();
                if (auto charCtrl = cc.character.lock())
                    *result = charCtrl->GetGroundNormal();
            }
        }

        static void CharacterControllerComponent_Move(uint64_t entityID, const glm::vec3 *displacement, float deltaTime)
        {
            if (!displacement) return;
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CharacterControllerComponent>())
            {
                auto &cc = entity.GetComponent<CharacterControllerComponent>();
                if (auto charCtrl = cc.character.lock())
                    charCtrl->Move(*displacement, deltaTime);
            }
        }

        static void CharacterControllerComponent_GetPosition(uint64_t entityID, glm::vec3 *result)
        {
            if (!result) return;
            *result = glm::vec3(0.0f);
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CharacterControllerComponent>())
            {
                auto &cc = entity.GetComponent<CharacterControllerComponent>();
                if (auto charCtrl = cc.character.lock())
                    *result = charCtrl->GetPosition();
                else
                    *result = entity.GetTransform().world.translation;
            }
        }

        static void CharacterControllerComponent_SetPosition(uint64_t entityID, const glm::vec3 *position)
        {
            if (!position) return;
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CharacterControllerComponent>())
            {
                auto &cc = entity.GetComponent<CharacterControllerComponent>();
                if (auto charCtrl = cc.character.lock())
                    charCtrl->SetPosition(*position);
                entity.GetTransform().world.translation = *position;
                entity.GetTransform().local.translation = *position;
                entity.GetTransform().dirty = true;
            }
        }

        static void CharacterControllerComponent_GetRotation(uint64_t entityID, glm::quat *result)
        {
            if (!result) return;
            *result = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CharacterControllerComponent>())
            {
                auto &cc = entity.GetComponent<CharacterControllerComponent>();
                if (auto charCtrl = cc.character.lock())
                    *result = charCtrl->GetRotation();
                else
                    *result = entity.GetTransform().world.rotation;
            }
        }

        static void CharacterControllerComponent_SetRotation(uint64_t entityID, const glm::quat *rotation)
        {
            if (!rotation) return;
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CharacterControllerComponent>())
            {
                auto &cc = entity.GetComponent<CharacterControllerComponent>();
                if (auto charCtrl = cc.character.lock())
                    charCtrl->SetRotation(*rotation);
                entity.GetTransform().world.rotation = *rotation;
                entity.GetTransform().local.rotation = *rotation;
                entity.GetTransform().dirty = true;
            }
        }

        static void AnimatorComponent_SetFloat(uint64_t entityID, const char *paramName, float value)
        {
            if (!paramName)
                return;

            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<SkeletalMeshComponent>())
                return;
            auto &smc = entity.GetComponent<SkeletalMeshComponent>();
            if (smc.runtimeAnimatorInstance)
                smc.runtimeAnimatorInstance->SetParamFloat(paramName, value);

            auto it = smc.runtimeParams.find(paramName);
            if (it != smc.runtimeParams.end())
            {
                it->second.floatVal = value;
            }
            else
            {
                smc.runtimeParams[paramName] = { paramName, "", value, 0, false, AnimParam::Type::Float };
            }
        }

        static void AnimatorComponent_GetFloat(uint64_t entityID, const char *paramName, float *result)
        {
            if (!result || !paramName)
                return;

            *result = 0.0f;
            Entity entity = GetEntityByID(entityID);

            if (!entity.IsValid() || !entity.HasComponent<SkeletalMeshComponent>())
                return;

            auto &smc = entity.GetComponent<SkeletalMeshComponent>();
            if (smc.runtimeAnimatorInstance)
            {
                if (const AnimParam *p = smc.runtimeAnimatorInstance->GetParam(paramName))
                {
                    *result = p->floatVal;
                    return;
                }
            }

            auto it = smc.runtimeParams.find(paramName);
            if (it != smc.runtimeParams.end())
            {
                *result = it->second.floatVal;
            }
        }

        static void AnimatorComponent_SetInt(uint64_t entityID, const char *paramName, int32_t value)
        {
            if (!paramName)
                return;

            Entity entity = GetEntityByID(entityID);

            if (!entity.IsValid() || !entity.HasComponent<SkeletalMeshComponent>())
                return;

            auto &smc = entity.GetComponent<SkeletalMeshComponent>();
            if (smc.runtimeAnimatorInstance)
            {
                smc.runtimeAnimatorInstance->SetParamInt(paramName, value);
            }

            auto it = smc.runtimeParams.find(paramName);
            if (it != smc.runtimeParams.end())
            {
                it->second.intVal = value;
            }
            else
            {
                smc.runtimeParams[paramName] = { paramName, "", 0.0f, value, false, AnimParam::Type::Int };
            }
        }

        static void AnimatorComponent_GetInt(uint64_t entityID, const char *paramName, int32_t *result)
        {
            if (!result || !paramName)
                return;

            *result = 0;

            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<SkeletalMeshComponent>())
                return;

            auto &smc = entity.GetComponent<SkeletalMeshComponent>();
            if (smc.runtimeAnimatorInstance)
            {
                if (const AnimParam *p = smc.runtimeAnimatorInstance->GetParam(paramName))
                {
                    *result = p->intVal;
                    return;
                }
            }

            auto it = smc.runtimeParams.find(paramName);
            if (it != smc.runtimeParams.end())
            {
                *result = it->second.intVal;
            }
        }

        static void AnimatorComponent_SetBool(uint64_t entityID, const char *paramName, bool value)
        {
            if (!paramName)
                return;

            Entity entity = GetEntityByID(entityID);

            if (!entity.IsValid() || !entity.HasComponent<SkeletalMeshComponent>())
                return;

            auto &smc = entity.GetComponent<SkeletalMeshComponent>();
            if (smc.runtimeAnimatorInstance)
            {
                smc.runtimeAnimatorInstance->SetParamBool(paramName, value);
            }

            auto it = smc.runtimeParams.find(paramName);
            if (it != smc.runtimeParams.end())
            {
                it->second.boolVal = value;
            }
            else
            {
                smc.runtimeParams[paramName] = { paramName, "", 0.0f, 0, value, AnimParam::Type::Bool };
            }
        }

        static void AnimatorComponent_GetBool(uint64_t entityID, const char *paramName, bool *result)
        {
            if (!result || !paramName) return;
            *result = false;

            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<SkeletalMeshComponent>())
                return;

            auto &smc = entity.GetComponent<SkeletalMeshComponent>();
            if (smc.runtimeAnimatorInstance)
            {
                if (const AnimParam *p = smc.runtimeAnimatorInstance->GetParam(paramName))
                {
                    *result = p->boolVal;
                    return;
                }
            }

            auto it = smc.runtimeParams.find(paramName);
            if (it != smc.runtimeParams.end())
            {
                *result = it->second.boolVal;
            }
        }

        static void AnimatorComponent_SetString(uint64_t entityID, const char *paramName, const char *value)
        {
            if (!paramName || !value)
                return;

            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<SkeletalMeshComponent>())
                return;

            auto &smc = entity.GetComponent<SkeletalMeshComponent>();
            if (smc.runtimeAnimatorInstance)
                smc.runtimeAnimatorInstance->SetParamString(paramName, value);

            auto it = smc.runtimeParams.find(paramName);
            if (it != smc.runtimeParams.end())
            {
                it->second.strVal = value;
            }
            else
            {
                smc.runtimeParams[paramName] = { paramName, value, 0.0f, 0, false, AnimParam::Type::String };
            }
        }

        static void AnimatorComponent_GetString(uint64_t entityID, const char *paramName, const char **result)
        {
            if (!result || !paramName) return;
            *result = "";
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<SkeletalMeshComponent>()) return;
            auto &smc = entity.GetComponent<SkeletalMeshComponent>();
            if (smc.runtimeAnimatorInstance)
            {
                if (const AnimParam *p = smc.runtimeAnimatorInstance->GetParam(paramName))
                {
                    *result = p->strVal.c_str();
                    return;
                }
            }

            auto it = smc.runtimeParams.find(paramName);
            if (it != smc.runtimeParams.end())
            {
                *result = it->second.strVal.c_str();
            }
        }

        static void AnimatorComponent_SetState(uint64_t entityID, const char *stateName)
        {
            if (!stateName) return;
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<SkeletalMeshComponent>()) return;
            auto &smc = entity.GetComponent<SkeletalMeshComponent>();
            smc.currentStateName = stateName;
            smc.stateElapsed = 0.0f;
            smc.stateNormalized = 0.0f;
        }

        static void AnimatorComponent_GetCurrentStateName(uint64_t entityID, const char **result)
        {
            if (!result) return;
            *result = "";
            Entity entity = GetEntityByID(entityID);
            if (!entity.IsValid() || !entity.HasComponent<SkeletalMeshComponent>()) return;
            auto &smc = entity.GetComponent<SkeletalMeshComponent>();
            *result = smc.currentStateName.c_str();
        }

        static int32_t AnimationMontage_GetNotifyCallbackCount(uint64_t montageHandle)
        {
            if (auto *am = AssetManager::GetInstance())
            {
                if (auto montage = am->GetAsset<AnimationMontage>(AssetHandle(montageHandle)))
                {
                    return static_cast<int32_t>(montage->GetNotifyCallbacks().size());
                }
            }
            return 0;
        }

        static bool AnimationMontage_GetNotifyCallbackAt(uint64_t montageHandle, int32_t index, float *outTimestep, uint8_t *outActionType, const char **outName)
        {
            if (auto *am = AssetManager::GetInstance())
            {
                if (auto montage = am->GetAsset<AnimationMontage>(AssetHandle(montageHandle)))
                {
                    const auto &cbs = montage->GetNotifyCallbacks();
                    if (index >= 0 && index < static_cast<int32_t>(cbs.size()))
                    {
                        if (outTimestep) *outTimestep = cbs[index].timestep;
                        if (outActionType) *outActionType = static_cast<uint8_t>(cbs[index].actionType);
                        if (outName) *outName = cbs[index].callbackName.c_str();
                        return true;
                    }
                }
            }
            return false;
        }

        static void AnimationMontage_AddNotifyCallback(uint64_t montageHandle, float timestep, uint8_t actionType, const char *name)
        {
            if (auto *am = AssetManager::GetInstance())
            {
                if (auto montage = am->GetAsset<AnimationMontage>(AssetHandle(montageHandle)))
                {
                    montage->AddNotifyCallback(timestep, static_cast<AnimationTimelineEvent::Action>(actionType), name ? name : "");
                }
            }
        }

        static void AnimationMontage_RemoveNotifyCallback(uint64_t montageHandle, int32_t index)
        {
            if (auto *am = AssetManager::GetInstance())
            {
                if (auto montage = am->GetAsset<AnimationMontage>(AssetHandle(montageHandle)))
                {
                    montage->RemoveNotifyCallback(static_cast<size_t>(index));
                }
            }
        }

        // =========================================================================
        // RenderingComponent
        // =========================================================================
        static void RenderingComponent_GetVisible(uint64_t entityID, bool *result)
        {
            if (!result) return;
            *result = true;
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<RenderingComponent>())
                *result = entity.GetComponent<RenderingComponent>().visible;
        }

        static void RenderingComponent_SetVisible(uint64_t entityID, bool value)
        {
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<RenderingComponent>())
                entity.GetComponent<RenderingComponent>().visible = value;
        }

        // =========================================================================
        // CameraComponent
        // =========================================================================
        static void CameraComponent_GetPrimary(uint64_t entityID, bool *result)
        {
            if (!result) return;
            *result = false;
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CameraComponent>())
                *result = entity.GetComponent<CameraComponent>().primary;
        }

        static void CameraComponent_SetPrimary(uint64_t entityID, bool value)
        {
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CameraComponent>())
                entity.GetComponent<CameraComponent>().primary = value;
        }

        static void CameraComponent_GetProjectionType(uint64_t entityID, int32_t *result)
        {
            if (!result) return;
            *result = static_cast<int32_t>(ProjectionType::Perspective);
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CameraComponent>())
                *result = static_cast<int32_t>(entity.GetComponent<CameraComponent>().camera.projectionType);
        }

        static void CameraComponent_SetProjectionType(uint64_t entityID, int32_t value)
        {
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CameraComponent>())
            {
                auto &cam = entity.GetComponent<CameraComponent>().camera;
                cam.projectionType = static_cast<ProjectionType>(value);
                const auto vp = cam.GetViewportSize();
                cam.UpdateProjection(vp.x > 0 ? vp.x : 1280, vp.y > 0 ? vp.y : 720);
            }
        }

        static void CameraComponent_GetFov(uint64_t entityID, float *result)
        {
            if (!result) return;
            *result = 45.0f;
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CameraComponent>())
                *result = entity.GetComponent<CameraComponent>().camera.fov;
        }

        static void CameraComponent_SetFov(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CameraComponent>())
            {
                auto &cam = entity.GetComponent<CameraComponent>().camera;
                cam.fov = value;
                const auto vp = cam.GetViewportSize();
                cam.UpdateProjection(vp.x > 0 ? vp.x : 1280, vp.y > 0 ? vp.y : 720);
            }
        }

        static void CameraComponent_GetOrthoSize(uint64_t entityID, float *result)
        {
            if (!result) return;
            *result = 10.0f;
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CameraComponent>())
                *result = entity.GetComponent<CameraComponent>().camera.orthoSize;
        }

        static void CameraComponent_SetOrthoSize(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CameraComponent>())
            {
                auto &cam = entity.GetComponent<CameraComponent>().camera;
                cam.orthoSize = value;
                const auto vp = cam.GetViewportSize();
                cam.UpdateProjection(vp.x > 0 ? vp.x : 1280, vp.y > 0 ? vp.y : 720);
            }
        }

        static void CameraComponent_GetNearClip(uint64_t entityID, float *result)
        {
            if (!result) return;
            *result = 0.1f;
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CameraComponent>())
                *result = entity.GetComponent<CameraComponent>().camera.nearPlane;
        }

        static void CameraComponent_SetNearClip(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CameraComponent>())
            {
                auto &cam = entity.GetComponent<CameraComponent>().camera;
                cam.nearPlane = value;
                const auto vp = cam.GetViewportSize();
                cam.UpdateProjection(vp.x > 0 ? vp.x : 1280, vp.y > 0 ? vp.y : 720);
            }
        }

        static void CameraComponent_GetFarClip(uint64_t entityID, float *result)
        {
            if (!result) return;
            *result = 500.0f;
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CameraComponent>())
                *result = entity.GetComponent<CameraComponent>().camera.farPlane;
        }

        static void CameraComponent_SetFarClip(uint64_t entityID, float value)
        {
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CameraComponent>())
            {
                auto &cam = entity.GetComponent<CameraComponent>().camera;
                cam.farPlane = value;
                const auto vp = cam.GetViewportSize();
                cam.UpdateProjection(vp.x > 0 ? vp.x : 1280, vp.y > 0 ? vp.y : 720);
            }
        }

        static void CameraComponent_GetAspectRatioPreset(uint64_t entityID, int32_t *result)
        {
            if (!result) return;
            *result = 0;
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CameraComponent>())
                *result = static_cast<int32_t>(entity.GetComponent<CameraComponent>().camera.GetAspectRatioPreset());
        }

        static void CameraComponent_SetAspectRatioPreset(uint64_t entityID, int32_t value)
        {
            Entity entity = GetEntityByID(entityID);
            if (entity.IsValid() && entity.HasComponent<CameraComponent>())
                entity.GetComponent<CameraComponent>().camera.SetAspectRatioPreset(static_cast<SceneCamera::AspectRatioPreset>(value));
        }

        // =========================================================================
        // DirectionalLightComponent
        // =========================================================================
        static void DirectionalLightComponent_GetColor(uint64_t entityID, glm::vec4 *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<DirectionalLightComponent>()) *result = e.GetComponent<DirectionalLightComponent>().color;
        }
        static void DirectionalLightComponent_SetColor(uint64_t entityID, const glm::vec4 *value)
        {
            if (!value) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<DirectionalLightComponent>()) e.GetComponent<DirectionalLightComponent>().color = *value;
        }
        static void DirectionalLightComponent_GetIntensity(uint64_t entityID, float *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<DirectionalLightComponent>()) *result = e.GetComponent<DirectionalLightComponent>().intensity;
        }
        static void DirectionalLightComponent_SetIntensity(uint64_t entityID, float value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<DirectionalLightComponent>()) e.GetComponent<DirectionalLightComponent>().intensity = value;
        }
        static void DirectionalLightComponent_GetAngularRadius(uint64_t entityID, float *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<DirectionalLightComponent>()) *result = e.GetComponent<DirectionalLightComponent>().angularRadius;
        }
        static void DirectionalLightComponent_SetAngularRadius(uint64_t entityID, float value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<DirectionalLightComponent>()) e.GetComponent<DirectionalLightComponent>().angularRadius = value;
        }
        static void DirectionalLightComponent_GetShadowStrength(uint64_t entityID, float *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<DirectionalLightComponent>()) *result = e.GetComponent<DirectionalLightComponent>().shadowStrength;
        }
        static void DirectionalLightComponent_SetShadowStrength(uint64_t entityID, float value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<DirectionalLightComponent>()) e.GetComponent<DirectionalLightComponent>().shadowStrength = value;
        }
        static void DirectionalLightComponent_GetShadowDistance(uint64_t entityID, float *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<DirectionalLightComponent>()) *result = e.GetComponent<DirectionalLightComponent>().shadowDistance;
        }
        static void DirectionalLightComponent_SetShadowDistance(uint64_t entityID, float value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<DirectionalLightComponent>()) e.GetComponent<DirectionalLightComponent>().shadowDistance = value;
        }
        static void DirectionalLightComponent_GetShadowResolution(uint64_t entityID, int32_t *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<DirectionalLightComponent>()) *result = e.GetComponent<DirectionalLightComponent>().shadowResolution;
        }
        static void DirectionalLightComponent_SetShadowResolution(uint64_t entityID, int32_t value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<DirectionalLightComponent>()) e.GetComponent<DirectionalLightComponent>().shadowResolution = value;
        }
        static void DirectionalLightComponent_GetCascadeShadow(uint64_t entityID, bool *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<DirectionalLightComponent>()) *result = e.GetComponent<DirectionalLightComponent>().cascadeShadow;
        }
        static void DirectionalLightComponent_SetCascadeShadow(uint64_t entityID, bool value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<DirectionalLightComponent>()) e.GetComponent<DirectionalLightComponent>().cascadeShadow = value;
        }
        static void DirectionalLightComponent_GetSoftShadows(uint64_t entityID, bool *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<DirectionalLightComponent>()) *result = e.GetComponent<DirectionalLightComponent>().softShadows;
        }
        static void DirectionalLightComponent_SetSoftShadows(uint64_t entityID, bool value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<DirectionalLightComponent>()) e.GetComponent<DirectionalLightComponent>().softShadows = value;
        }

        // =========================================================================
        // PointLightComponent
        // =========================================================================
        static void PointLightComponent_GetColor(uint64_t entityID, glm::vec4 *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<PointLightComponent>()) *result = e.GetComponent<PointLightComponent>().color;
        }
        static void PointLightComponent_SetColor(uint64_t entityID, const glm::vec4 *value)
        {
            if (!value) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<PointLightComponent>()) e.GetComponent<PointLightComponent>().color = *value;
        }
        static void PointLightComponent_GetIntensity(uint64_t entityID, float *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<PointLightComponent>()) *result = e.GetComponent<PointLightComponent>().intensity;
        }
        static void PointLightComponent_SetIntensity(uint64_t entityID, float value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<PointLightComponent>()) e.GetComponent<PointLightComponent>().intensity = value;
        }
        static void PointLightComponent_GetRange(uint64_t entityID, float *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<PointLightComponent>()) *result = e.GetComponent<PointLightComponent>().range;
        }
        static void PointLightComponent_SetRange(uint64_t entityID, float value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<PointLightComponent>()) e.GetComponent<PointLightComponent>().range = value;
        }
        static void PointLightComponent_GetEnabled(uint64_t entityID, bool *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<PointLightComponent>()) *result = e.GetComponent<PointLightComponent>().enabled;
        }
        static void PointLightComponent_SetEnabled(uint64_t entityID, bool value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<PointLightComponent>()) e.GetComponent<PointLightComponent>().enabled = value;
        }
        static void PointLightComponent_GetConstantAttenuation(uint64_t entityID, float *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<PointLightComponent>()) *result = e.GetComponent<PointLightComponent>().constantAttenuation;
        }
        static void PointLightComponent_SetConstantAttenuation(uint64_t entityID, float value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<PointLightComponent>()) e.GetComponent<PointLightComponent>().constantAttenuation = value;
        }
        static void PointLightComponent_GetLinearAttenuation(uint64_t entityID, float *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<PointLightComponent>()) *result = e.GetComponent<PointLightComponent>().linearAttenuation;
        }
        static void PointLightComponent_SetLinearAttenuation(uint64_t entityID, float value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<PointLightComponent>()) e.GetComponent<PointLightComponent>().linearAttenuation = value;
        }
        static void PointLightComponent_GetQuadraticAttenuation(uint64_t entityID, float *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<PointLightComponent>()) *result = e.GetComponent<PointLightComponent>().quadraticAttenuation;
        }
        static void PointLightComponent_SetQuadraticAttenuation(uint64_t entityID, float value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<PointLightComponent>()) e.GetComponent<PointLightComponent>().quadraticAttenuation = value;
        }

        // =========================================================================
        // SpotLightComponent
        // =========================================================================
        static void SpotLightComponent_GetColor(uint64_t entityID, glm::vec4 *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<SpotLightComponent>()) *result = e.GetComponent<SpotLightComponent>().color;
        }
        static void SpotLightComponent_SetColor(uint64_t entityID, const glm::vec4 *value)
        {
            if (!value) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<SpotLightComponent>()) e.GetComponent<SpotLightComponent>().color = *value;
        }
        static void SpotLightComponent_GetIntensity(uint64_t entityID, float *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<SpotLightComponent>()) *result = e.GetComponent<SpotLightComponent>().intensity;
        }
        static void SpotLightComponent_SetIntensity(uint64_t entityID, float value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<SpotLightComponent>()) e.GetComponent<SpotLightComponent>().intensity = value;
        }
        static void SpotLightComponent_GetRange(uint64_t entityID, float *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<SpotLightComponent>()) *result = e.GetComponent<SpotLightComponent>().range;
        }
        static void SpotLightComponent_SetRange(uint64_t entityID, float value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<SpotLightComponent>()) e.GetComponent<SpotLightComponent>().range = value;
        }
        static void SpotLightComponent_GetEnabled(uint64_t entityID, bool *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<SpotLightComponent>()) *result = e.GetComponent<SpotLightComponent>().enabled;
        }
        static void SpotLightComponent_SetEnabled(uint64_t entityID, bool value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<SpotLightComponent>()) e.GetComponent<SpotLightComponent>().enabled = value;
        }
        static void SpotLightComponent_GetInnerConeAngle(uint64_t entityID, float *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<SpotLightComponent>()) *result = e.GetComponent<SpotLightComponent>().innerConeAngle;
        }
        static void SpotLightComponent_SetInnerConeAngle(uint64_t entityID, float value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<SpotLightComponent>()) e.GetComponent<SpotLightComponent>().innerConeAngle = value;
        }
        static void SpotLightComponent_GetOuterConeAngle(uint64_t entityID, float *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<SpotLightComponent>()) *result = e.GetComponent<SpotLightComponent>().outerConeAngle;
        }
        static void SpotLightComponent_SetOuterConeAngle(uint64_t entityID, float value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<SpotLightComponent>()) e.GetComponent<SpotLightComponent>().outerConeAngle = value;
        }
        static void SpotLightComponent_GetConstantAttenuation(uint64_t entityID, float *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<SpotLightComponent>()) *result = e.GetComponent<SpotLightComponent>().constantAttenuation;
        }
        static void SpotLightComponent_SetConstantAttenuation(uint64_t entityID, float value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<SpotLightComponent>()) e.GetComponent<SpotLightComponent>().constantAttenuation = value;
        }
        static void SpotLightComponent_GetLinearAttenuation(uint64_t entityID, float *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<SpotLightComponent>()) *result = e.GetComponent<SpotLightComponent>().linearAttenuation;
        }
        static void SpotLightComponent_SetLinearAttenuation(uint64_t entityID, float value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<SpotLightComponent>()) e.GetComponent<SpotLightComponent>().linearAttenuation = value;
        }
        static void SpotLightComponent_GetQuadraticAttenuation(uint64_t entityID, float *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<SpotLightComponent>()) *result = e.GetComponent<SpotLightComponent>().quadraticAttenuation;
        }
        static void SpotLightComponent_SetQuadraticAttenuation(uint64_t entityID, float value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<SpotLightComponent>()) e.GetComponent<SpotLightComponent>().quadraticAttenuation = value;
        }

        // =========================================================================
        // PointLight2DComponent
        // =========================================================================
        static void PointLight2DComponent_GetColor(uint64_t entityID, glm::vec4 *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<PointLight2DComponent>()) *result = e.GetComponent<PointLight2DComponent>().color;
        }
        static void PointLight2DComponent_SetColor(uint64_t entityID, const glm::vec4 *value)
        {
            if (!value) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<PointLight2DComponent>()) e.GetComponent<PointLight2DComponent>().color = *value;
        }
        static void PointLight2DComponent_GetRadius(uint64_t entityID, float *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<PointLight2DComponent>()) *result = e.GetComponent<PointLight2DComponent>().radius;
        }
        static void PointLight2DComponent_SetRadius(uint64_t entityID, float value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<PointLight2DComponent>()) e.GetComponent<PointLight2DComponent>().radius = value;
        }
        static void PointLight2DComponent_GetIntensity(uint64_t entityID, float *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<PointLight2DComponent>()) *result = e.GetComponent<PointLight2DComponent>().intensity;
        }
        static void PointLight2DComponent_SetIntensity(uint64_t entityID, float value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<PointLight2DComponent>()) e.GetComponent<PointLight2DComponent>().intensity = value;
        }
        static void PointLight2DComponent_GetEnabled(uint64_t entityID, bool *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<PointLight2DComponent>()) *result = e.GetComponent<PointLight2DComponent>().enabled;
        }
        static void PointLight2DComponent_SetEnabled(uint64_t entityID, bool value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<PointLight2DComponent>()) e.GetComponent<PointLight2DComponent>().enabled = value;
        }

        // =========================================================================
        // StaticMeshComponent
        // =========================================================================
        static void StaticMeshComponent_GetMeshHandle(uint64_t entityID, uint64_t *result)
        {
            if (!result) return;
            *result = 0;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<StaticMeshComponent>())
                *result = static_cast<uint64_t>(e.GetComponent<StaticMeshComponent>().handle);
        }
        static void StaticMeshComponent_SetMeshHandle(uint64_t entityID, uint64_t handle)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<StaticMeshComponent>())
                e.GetComponent<StaticMeshComponent>().handle = AssetHandle(handle);
        }
        static void StaticMeshComponent_SetOverrideMaterial(uint64_t entityID, int32_t slot, uint64_t materialHandle)
        {
            Entity e = GetEntityByID(entityID);
            if (!e.IsValid() || !e.HasComponent<StaticMeshComponent>())
                return;
            AssetHandle meshHandle = e.GetComponent<StaticMeshComponent>().handle;
            if (meshHandle == AssetHandle(0) || !AssetManager::GetInstance())
                return;
            if (auto mesh = AssetManager::GetInstance()->GetAsset<StaticMesh>(meshHandle))
            {
                const auto &instances = mesh->GetMeshInstances();
                if (slot >= 0 && static_cast<size_t>(slot) < instances.size() && instances[slot])
                    instances[slot]->SetMaterial(AssetHandle(materialHandle));
            }
        }
        static void StaticMeshComponent_GetOverrideMaterial(uint64_t entityID, int32_t slot, uint64_t *result)
        {
            if (!result) return;
            *result = 0;
            Entity e = GetEntityByID(entityID);
            if (!e.IsValid() || !e.HasComponent<StaticMeshComponent>())
                return;
            AssetHandle meshHandle = e.GetComponent<StaticMeshComponent>().handle;
            if (meshHandle == AssetHandle(0) || !AssetManager::GetInstance())
                return;
            if (auto mesh = AssetManager::GetInstance()->GetAsset<StaticMesh>(meshHandle))
            {
                const auto &instances = mesh->GetMeshInstances();
                if (slot >= 0 && static_cast<size_t>(slot) < instances.size() && instances[slot])
                    *result = static_cast<uint64_t>(instances[slot]->GetMaterialAssetHandle());
            }
        }

        // =========================================================================
        // WorldEnvironmentComponent
        // =========================================================================
        static void WorldEnvironmentComponent_GetExposure(uint64_t entityID, float *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<WorldEnvironment>()) *result = e.GetComponent<WorldEnvironment>().exposure;
        }
        static void WorldEnvironmentComponent_SetExposure(uint64_t entityID, float value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<WorldEnvironment>()) e.GetComponent<WorldEnvironment>().exposure = value;
        }
        static void WorldEnvironmentComponent_GetGamma(uint64_t entityID, float *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<WorldEnvironment>()) *result = e.GetComponent<WorldEnvironment>().gamma;
        }
        static void WorldEnvironmentComponent_SetGamma(uint64_t entityID, float value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<WorldEnvironment>()) e.GetComponent<WorldEnvironment>().gamma = value;
        }
        static void WorldEnvironmentComponent_GetAmbient(uint64_t entityID, float *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<WorldEnvironment>()) *result = e.GetComponent<WorldEnvironment>().ambient;
        }
        static void WorldEnvironmentComponent_SetAmbient(uint64_t entityID, float value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<WorldEnvironment>()) e.GetComponent<WorldEnvironment>().ambient = value;
        }
        static void WorldEnvironmentComponent_GetEnableIBL(uint64_t entityID, bool *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<WorldEnvironment>()) *result = e.GetComponent<WorldEnvironment>().enableIBL;
        }
        static void WorldEnvironmentComponent_SetEnableIBL(uint64_t entityID, bool value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<WorldEnvironment>()) e.GetComponent<WorldEnvironment>().enableIBL = value;
        }
        static void WorldEnvironmentComponent_GetFogDensity(uint64_t entityID, float *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<WorldEnvironment>()) *result = e.GetComponent<WorldEnvironment>().fogDensity;
        }
        static void WorldEnvironmentComponent_SetFogDensity(uint64_t entityID, float value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<WorldEnvironment>()) e.GetComponent<WorldEnvironment>().fogDensity = value;
        }
        static void WorldEnvironmentComponent_GetFogColor(uint64_t entityID, glm::vec4 *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<WorldEnvironment>()) *result = e.GetComponent<WorldEnvironment>().fogColor;
        }
        static void WorldEnvironmentComponent_SetFogColor(uint64_t entityID, const glm::vec4 *value)
        {
            if (!value) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<WorldEnvironment>()) e.GetComponent<WorldEnvironment>().fogColor = *value;
        }
        static void WorldEnvironmentComponent_GetFogStart(uint64_t entityID, float *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<WorldEnvironment>()) *result = e.GetComponent<WorldEnvironment>().fogStart;
        }
        static void WorldEnvironmentComponent_SetFogStart(uint64_t entityID, float value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<WorldEnvironment>()) e.GetComponent<WorldEnvironment>().fogStart = value;
        }
        static void WorldEnvironmentComponent_GetFogEnd(uint64_t entityID, float *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<WorldEnvironment>()) *result = e.GetComponent<WorldEnvironment>().fogEnd;
        }
        static void WorldEnvironmentComponent_SetFogEnd(uint64_t entityID, float value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<WorldEnvironment>()) e.GetComponent<WorldEnvironment>().fogEnd = value;
        }
        static void WorldEnvironmentComponent_MarkDirty(uint64_t entityID)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<WorldEnvironment>()) e.GetComponent<WorldEnvironment>().dirtyEnvironment = true;
        }

        // =========================================================================
        // ParticleSystemComponent
        // =========================================================================
        static void ParticleSystemComponent_GetIsPlaying(uint64_t entityID, bool *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<ParticleSystemComponent>()) *result = e.GetComponent<ParticleSystemComponent>().isPlaying;
        }
        static void ParticleSystemComponent_SetIsPlaying(uint64_t entityID, bool value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<ParticleSystemComponent>()) e.GetComponent<ParticleSystemComponent>().isPlaying = value;
        }
        static void ParticleSystemComponent_GetEmissionRate(uint64_t entityID, float *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<ParticleSystemComponent>()) *result = e.GetComponent<ParticleSystemComponent>().settings.emissionRate;
        }
        static void ParticleSystemComponent_SetEmissionRate(uint64_t entityID, float value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<ParticleSystemComponent>()) e.GetComponent<ParticleSystemComponent>().settings.emissionRate = value;
        }
        static void ParticleSystemComponent_GetMaxParticles(uint64_t entityID, uint32_t *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<ParticleSystemComponent>()) *result = e.GetComponent<ParticleSystemComponent>().settings.maxParticles;
        }
        static void ParticleSystemComponent_SetMaxParticles(uint64_t entityID, uint32_t value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<ParticleSystemComponent>()) e.GetComponent<ParticleSystemComponent>().settings.maxParticles = value;
        }
        static void ParticleSystemComponent_GetStartColor(uint64_t entityID, glm::vec4 *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<ParticleSystemComponent>()) *result = e.GetComponent<ParticleSystemComponent>().settings.startColor;
        }
        static void ParticleSystemComponent_SetStartColor(uint64_t entityID, const glm::vec4 *value)
        {
            if (!value) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<ParticleSystemComponent>()) e.GetComponent<ParticleSystemComponent>().settings.startColor = *value;
        }
        static void ParticleSystemComponent_GetEndColor(uint64_t entityID, glm::vec4 *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<ParticleSystemComponent>()) *result = e.GetComponent<ParticleSystemComponent>().settings.endColor;
        }
        static void ParticleSystemComponent_SetEndColor(uint64_t entityID, const glm::vec4 *value)
        {
            if (!value) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<ParticleSystemComponent>()) e.GetComponent<ParticleSystemComponent>().settings.endColor = *value;
        }
        static void ParticleSystemComponent_GetStartSize(uint64_t entityID, glm::vec2 *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<ParticleSystemComponent>()) *result = e.GetComponent<ParticleSystemComponent>().settings.startSize;
        }
        static void ParticleSystemComponent_SetStartSize(uint64_t entityID, const glm::vec2 *value)
        {
            if (!value) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<ParticleSystemComponent>()) e.GetComponent<ParticleSystemComponent>().settings.startSize = *value;
        }
        static void ParticleSystemComponent_GetEndSize(uint64_t entityID, glm::vec2 *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<ParticleSystemComponent>()) *result = e.GetComponent<ParticleSystemComponent>().settings.endSize;
        }
        static void ParticleSystemComponent_SetEndSize(uint64_t entityID, const glm::vec2 *value)
        {
            if (!value) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<ParticleSystemComponent>()) e.GetComponent<ParticleSystemComponent>().settings.endSize = *value;
        }
        static void ParticleSystemComponent_Play(uint64_t entityID)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<ParticleSystemComponent>()) e.GetComponent<ParticleSystemComponent>().isPlaying = true;
        }
        static void ParticleSystemComponent_Stop(uint64_t entityID)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<ParticleSystemComponent>()) e.GetComponent<ParticleSystemComponent>().isPlaying = false;
        }
        static void ParticleSystemComponent_Restart(uint64_t entityID)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<ParticleSystemComponent>())
            {
                auto &ps = e.GetComponent<ParticleSystemComponent>();
                ps.system.reset();
                ps.isPlaying = true;
            }
        }
        static void ParticleSystemComponent_SetPreset(uint64_t entityID, int32_t preset)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<ParticleSystemComponent>())
            {
                auto &ps = e.GetComponent<ParticleSystemComponent>();
                if (preset >= 1000)
                {
                    const uint32_t burstCount = static_cast<uint32_t>(preset - 1000);
                    if (!ps.system)
                    {
                        ps.system = ParticleSystem::Create(ps.settings.maxParticles);
                    }
                    ps.system->EmitBurst(burstCount);
                    ps.isPlaying = true;
                    return;
                }

                switch (static_cast<ParticleEmitterType>(preset))
                {
                    case ParticleEmitterType::Fire:        ps.settings = ParticleEmitterSettings::CreateFire(); break;
                    case ParticleEmitterType::Spark:       ps.settings = ParticleEmitterSettings::CreateSpark(); break;
                    case ParticleEmitterType::Plasma:      ps.settings = ParticleEmitterSettings::CreatePlasma(); break;
                    case ParticleEmitterType::MuzzleFlash: ps.settings = ParticleEmitterSettings::CreateMuzzleFlash(); break;
                    case ParticleEmitterType::Shockwave:   ps.settings = ParticleEmitterSettings::CreateShockwave(); break;
                    case ParticleEmitterType::Smoke:
                    default:                               ps.settings = ParticleEmitterSettings::CreateSmoke(); break;
                }
            }
        }

        // =========================================================================
        // NetworkIdentityComponent
        // =========================================================================
        static std::string s_NetPrefabBuffer;
        static void NetworkIdentityComponent_GetNetId(uint64_t entityID, uint64_t *result)
        {
            if (!result) return;
            *result = 0;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<NetworkIdentityComponent>()) *result = e.GetComponent<NetworkIdentityComponent>().netId;
        }
        static void NetworkIdentityComponent_GetOwnerClient(uint64_t entityID, uint32_t *result)
        {
            if (!result) return;
            *result = 0;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<NetworkIdentityComponent>()) *result = e.GetComponent<NetworkIdentityComponent>().ownerClient;
        }
        static void NetworkIdentityComponent_SetOwnerClient(uint64_t entityID, uint32_t value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<NetworkIdentityComponent>()) e.GetComponent<NetworkIdentityComponent>().ownerClient = value;
        }
        static void NetworkIdentityComponent_GetIsLocalPlayer(uint64_t entityID, bool *result)
        {
            if (!result) return;
            *result = false;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<NetworkIdentityComponent>()) *result = e.GetComponent<NetworkIdentityComponent>().isLocalPlayer;
        }
        static void NetworkIdentityComponent_SetIsLocalPlayer(uint64_t entityID, bool value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<NetworkIdentityComponent>()) e.GetComponent<NetworkIdentityComponent>().isLocalPlayer = value;
        }
        static void NetworkIdentityComponent_GetReplicate(uint64_t entityID, bool *result)
        {
            if (!result) return;
            *result = false;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<NetworkIdentityComponent>()) *result = e.GetComponent<NetworkIdentityComponent>().replicate;
        }
        static void NetworkIdentityComponent_SetReplicate(uint64_t entityID, bool value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<NetworkIdentityComponent>()) e.GetComponent<NetworkIdentityComponent>().replicate = value;
        }
        static void NetworkIdentityComponent_GetPrefab(uint64_t entityID, const char **result)
        {
            if (!result) return;
            *result = "";
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<NetworkIdentityComponent>())
            {
                s_NetPrefabBuffer = e.GetComponent<NetworkIdentityComponent>().prefab;
                *result = s_NetPrefabBuffer.c_str();
            }
        }
        static void NetworkIdentityComponent_SetPrefab(uint64_t entityID, const char *value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<NetworkIdentityComponent>())
                e.GetComponent<NetworkIdentityComponent>().prefab = value ? value : "";
        }
        static void NetworkIdentityComponent_GetReplicatedMovementSpeed(uint64_t entityID, float *result)
        {
            if (!result) return;
            *result = 0.0f;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<NetworkIdentityComponent>()) *result = e.GetComponent<NetworkIdentityComponent>().replicatedMovementSpeed;
        }
        static void NetworkIdentityComponent_SetReplicatedMovementSpeed(uint64_t entityID, float value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<NetworkIdentityComponent>()) e.GetComponent<NetworkIdentityComponent>().replicatedMovementSpeed = value;
        }
        static void NetworkIdentityComponent_GetReplicatedIsGrounded(uint64_t entityID, bool *result)
        {
            if (!result) return;
            *result = false;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<NetworkIdentityComponent>()) *result = e.GetComponent<NetworkIdentityComponent>().replicatedIsGrounded;
        }
        static void NetworkIdentityComponent_SetReplicatedIsGrounded(uint64_t entityID, bool value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<NetworkIdentityComponent>()) e.GetComponent<NetworkIdentityComponent>().replicatedIsGrounded = value;
        }
        static void NetworkIdentityComponent_GetSyncComponentsMask(uint64_t entityID, uint32_t *result)
        {
            if (!result) return;
            *result = 0;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<NetworkIdentityComponent>()) *result = e.GetComponent<NetworkIdentityComponent>().syncComponentsMask;
        }
        static void NetworkIdentityComponent_SetSyncComponentsMask(uint64_t entityID, uint32_t value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<NetworkIdentityComponent>()) e.GetComponent<NetworkIdentityComponent>().syncComponentsMask = value;
        }
        static void NetworkIdentityComponent_MarkDirty(uint64_t entityID, uint32_t flags)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<NetworkIdentityComponent>()) e.GetComponent<NetworkIdentityComponent>().MarkDirty(flags);
        }

        // =========================================================================
        // PlaneColliderComponent
        // =========================================================================
        static void PlaneColliderComponent_GetCenter(uint64_t entityID, glm::vec3 *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<PlaneColliderComponent>()) *result = e.GetComponent<PlaneColliderComponent>().center;
        }
        static void PlaneColliderComponent_SetCenter(uint64_t entityID, const glm::vec3 *value)
        {
            if (!value) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<PlaneColliderComponent>()) e.GetComponent<PlaneColliderComponent>().center = *value;
        }
        static void PlaneColliderComponent_GetScale(uint64_t entityID, glm::vec3 *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<PlaneColliderComponent>()) *result = e.GetComponent<PlaneColliderComponent>().scale;
        }
        static void PlaneColliderComponent_SetScale(uint64_t entityID, const glm::vec3 *value)
        {
            if (!value) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<PlaneColliderComponent>()) e.GetComponent<PlaneColliderComponent>().scale = *value;
        }

        // =========================================================================
        // TextComponent extensions
        // =========================================================================
        static void TextComponent_GetFontSize(uint64_t entityID, float *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<TextComponent>()) *result = e.GetComponent<TextComponent>().fontSize;
        }
        static void TextComponent_SetFontSize(uint64_t entityID, float value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<TextComponent>()) e.GetComponent<TextComponent>().fontSize = value;
        }
        static void TextComponent_GetScreenSpace(uint64_t entityID, bool *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<TextComponent>()) *result = e.GetComponent<TextComponent>().screenSpace;
        }
        static void TextComponent_SetScreenSpace(uint64_t entityID, bool value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<TextComponent>()) e.GetComponent<TextComponent>().screenSpace = value;
        }
        static void TextComponent_GetBillboard(uint64_t entityID, bool *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<TextComponent>()) *result = e.GetComponent<TextComponent>().billboard;
        }
        static void TextComponent_SetBillboard(uint64_t entityID, bool value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<TextComponent>()) e.GetComponent<TextComponent>().billboard = value;
        }
        static void TextComponent_GetCenterAlign(uint64_t entityID, bool *result)
        {
            if (!result) return;
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<TextComponent>()) *result = e.GetComponent<TextComponent>().centerAlign;
        }
        static void TextComponent_SetCenterAlign(uint64_t entityID, bool value)
        {
            Entity e = GetEntityByID(entityID);
            if (e.IsValid() && e.HasComponent<TextComponent>()) e.GetComponent<TextComponent>().centerAlign = value;
        }

        static const ComponentScriptGlueAPI s_ComponentScriptGlueAPI =
        {
            &Scene_GetScreenToWorldRay,
            &Scene_Raycast,
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
            &Rigidbody2DComponent_GetIsEnableSleeping,
            &Rigidbody2DComponent_SetIsEnableSleeping,
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
            &RigidbodyComponent_SetMass,
            &RigidbodyComponent_GetLinearDamping,
            &RigidbodyComponent_SetLinearDamping,
            &RigidbodyComponent_GetAngularDamping,
            &RigidbodyComponent_SetAngularDamping,
            &RigidbodyComponent_GetFriction,
            &RigidbodyComponent_SetFriction,
            &RigidbodyComponent_GetRestitution,
            &RigidbodyComponent_SetRestitution,
            &RigidbodyComponent_GetMaxLinearVelocity,
            &RigidbodyComponent_SetMaxLinearVelocity,
            &RigidbodyComponent_GetMaxAngularVelocity,
            &RigidbodyComponent_SetMaxAngularVelocity,
            &RigidbodyComponent_GetApplyGyroscopicForce,
            &RigidbodyComponent_SetApplyGyroscopicForce,
            &RigidbodyComponent_SetAngularVelocity,
            &RigidbodyComponent_GetLayer,
            &RigidbodyComponent_SetLayer,

            &BoxColliderComponent_GetCenter,
            &BoxColliderComponent_SetCenter,
            &BoxColliderComponent_GetScale,
            &BoxColliderComponent_SetScale,

            &SphereColliderComponent_GetCenter,
            &SphereColliderComponent_SetCenter,
            &SphereColliderComponent_GetRadius,
            &SphereColliderComponent_SetRadius,

            &CapsuleColliderComponent_GetCenter,
            &CapsuleColliderComponent_SetCenter,
            &CapsuleColliderComponent_GetRadius,
            &CapsuleColliderComponent_SetRadius,
            &CapsuleColliderComponent_GetHeight,
            &CapsuleColliderComponent_SetHeight,

            &CharacterControllerComponent_GetCenter,
            &CharacterControllerComponent_SetCenter,
            &CharacterControllerComponent_GetRadius,
            &CharacterControllerComponent_SetRadius,
            &CharacterControllerComponent_GetHeight,
            &CharacterControllerComponent_SetHeight,
            &CharacterControllerComponent_GetMaxStepHeight,
            &CharacterControllerComponent_SetMaxStepHeight,
            &CharacterControllerComponent_GetMaxSlopeAngle,
            &CharacterControllerComponent_SetMaxSlopeAngle,
            &CharacterControllerComponent_GetMass,
            &CharacterControllerComponent_SetMass,
            &CharacterControllerComponent_GetFriction,
            &CharacterControllerComponent_SetFriction,
            &CharacterControllerComponent_GetGravityFactor,
            &CharacterControllerComponent_SetGravityFactor,
            &CharacterControllerComponent_GetUp,
            &CharacterControllerComponent_SetUp,
            &CharacterControllerComponent_GetLinearVelocity,
            &CharacterControllerComponent_SetLinearVelocity,
            &CharacterControllerComponent_IsOnGround,
            &CharacterControllerComponent_IsSupported,
            &CharacterControllerComponent_GetGroundNormal,
            &CharacterControllerComponent_Move,
            &CharacterControllerComponent_GetPosition,
            &CharacterControllerComponent_SetPosition,
            &CharacterControllerComponent_GetRotation,
            &CharacterControllerComponent_SetRotation,

            &AnimatorComponent_SetFloat,
            &AnimatorComponent_GetFloat,
            &AnimatorComponent_SetInt,
            &AnimatorComponent_GetInt,
            &AnimatorComponent_SetBool,
            &AnimatorComponent_GetBool,
            &AnimatorComponent_SetString,
            &AnimatorComponent_GetString,
            &AnimatorComponent_SetState,
            &AnimatorComponent_GetCurrentStateName,

            &AnimationMontage_GetNotifyCallbackCount,
            &AnimationMontage_GetNotifyCallbackAt,
            &AnimationMontage_AddNotifyCallback,
            &AnimationMontage_RemoveNotifyCallback,

            &Entity_RemoveComponent,

            &RenderingComponent_GetVisible,
            &RenderingComponent_SetVisible,

            &CameraComponent_GetPrimary,
            &CameraComponent_SetPrimary,
            &CameraComponent_GetProjectionType,
            &CameraComponent_SetProjectionType,
            &CameraComponent_GetFov,
            &CameraComponent_SetFov,
            &CameraComponent_GetOrthoSize,
            &CameraComponent_SetOrthoSize,
            &CameraComponent_GetNearClip,
            &CameraComponent_SetNearClip,
            &CameraComponent_GetFarClip,
            &CameraComponent_SetFarClip,
            &CameraComponent_GetAspectRatioPreset,
            &CameraComponent_SetAspectRatioPreset,

            &DirectionalLightComponent_GetColor,
            &DirectionalLightComponent_SetColor,
            &DirectionalLightComponent_GetIntensity,
            &DirectionalLightComponent_SetIntensity,
            &DirectionalLightComponent_GetAngularRadius,
            &DirectionalLightComponent_SetAngularRadius,
            &DirectionalLightComponent_GetShadowStrength,
            &DirectionalLightComponent_SetShadowStrength,
            &DirectionalLightComponent_GetShadowDistance,
            &DirectionalLightComponent_SetShadowDistance,
            &DirectionalLightComponent_GetShadowResolution,
            &DirectionalLightComponent_SetShadowResolution,
            &DirectionalLightComponent_GetCascadeShadow,
            &DirectionalLightComponent_SetCascadeShadow,
            &DirectionalLightComponent_GetSoftShadows,
            &DirectionalLightComponent_SetSoftShadows,

            &PointLightComponent_GetColor,
            &PointLightComponent_SetColor,
            &PointLightComponent_GetIntensity,
            &PointLightComponent_SetIntensity,
            &PointLightComponent_GetRange,
            &PointLightComponent_SetRange,
            &PointLightComponent_GetEnabled,
            &PointLightComponent_SetEnabled,
            &PointLightComponent_GetConstantAttenuation,
            &PointLightComponent_SetConstantAttenuation,
            &PointLightComponent_GetLinearAttenuation,
            &PointLightComponent_SetLinearAttenuation,
            &PointLightComponent_GetQuadraticAttenuation,
            &PointLightComponent_SetQuadraticAttenuation,

            &SpotLightComponent_GetColor,
            &SpotLightComponent_SetColor,
            &SpotLightComponent_GetIntensity,
            &SpotLightComponent_SetIntensity,
            &SpotLightComponent_GetRange,
            &SpotLightComponent_SetRange,
            &SpotLightComponent_GetEnabled,
            &SpotLightComponent_SetEnabled,
            &SpotLightComponent_GetInnerConeAngle,
            &SpotLightComponent_SetInnerConeAngle,
            &SpotLightComponent_GetOuterConeAngle,
            &SpotLightComponent_SetOuterConeAngle,
            &SpotLightComponent_GetConstantAttenuation,
            &SpotLightComponent_SetConstantAttenuation,
            &SpotLightComponent_GetLinearAttenuation,
            &SpotLightComponent_SetLinearAttenuation,
            &SpotLightComponent_GetQuadraticAttenuation,
            &SpotLightComponent_SetQuadraticAttenuation,

            &PointLight2DComponent_GetColor,
            &PointLight2DComponent_SetColor,
            &PointLight2DComponent_GetRadius,
            &PointLight2DComponent_SetRadius,
            &PointLight2DComponent_GetIntensity,
            &PointLight2DComponent_SetIntensity,
            &PointLight2DComponent_GetEnabled,
            &PointLight2DComponent_SetEnabled,

            &StaticMeshComponent_GetMeshHandle,
            &StaticMeshComponent_SetMeshHandle,
            &StaticMeshComponent_SetOverrideMaterial,
            &StaticMeshComponent_GetOverrideMaterial,

            &WorldEnvironmentComponent_GetExposure,
            &WorldEnvironmentComponent_SetExposure,
            &WorldEnvironmentComponent_GetGamma,
            &WorldEnvironmentComponent_SetGamma,
            &WorldEnvironmentComponent_GetAmbient,
            &WorldEnvironmentComponent_SetAmbient,
            &WorldEnvironmentComponent_GetEnableIBL,
            &WorldEnvironmentComponent_SetEnableIBL,
            &WorldEnvironmentComponent_GetFogDensity,
            &WorldEnvironmentComponent_SetFogDensity,
            &WorldEnvironmentComponent_GetFogColor,
            &WorldEnvironmentComponent_SetFogColor,
            &WorldEnvironmentComponent_GetFogStart,
            &WorldEnvironmentComponent_SetFogStart,
            &WorldEnvironmentComponent_GetFogEnd,
            &WorldEnvironmentComponent_SetFogEnd,
            &WorldEnvironmentComponent_MarkDirty,

            &ParticleSystemComponent_GetIsPlaying,
            &ParticleSystemComponent_SetIsPlaying,
            &ParticleSystemComponent_GetEmissionRate,
            &ParticleSystemComponent_SetEmissionRate,
            &ParticleSystemComponent_GetMaxParticles,
            &ParticleSystemComponent_SetMaxParticles,
            &ParticleSystemComponent_GetStartColor,
            &ParticleSystemComponent_SetStartColor,
            &ParticleSystemComponent_GetEndColor,
            &ParticleSystemComponent_SetEndColor,
            &ParticleSystemComponent_GetStartSize,
            &ParticleSystemComponent_SetStartSize,
            &ParticleSystemComponent_GetEndSize,
            &ParticleSystemComponent_SetEndSize,
            &ParticleSystemComponent_Play,
            &ParticleSystemComponent_Stop,
            &ParticleSystemComponent_Restart,
            &ParticleSystemComponent_SetPreset,

            &NetworkIdentityComponent_GetNetId,
            &NetworkIdentityComponent_GetOwnerClient,
            &NetworkIdentityComponent_SetOwnerClient,
            &NetworkIdentityComponent_GetIsLocalPlayer,
            &NetworkIdentityComponent_SetIsLocalPlayer,
            &NetworkIdentityComponent_GetReplicate,
            &NetworkIdentityComponent_SetReplicate,
            &NetworkIdentityComponent_GetPrefab,
            &NetworkIdentityComponent_SetPrefab,
            &NetworkIdentityComponent_GetReplicatedMovementSpeed,
            &NetworkIdentityComponent_SetReplicatedMovementSpeed,
            &NetworkIdentityComponent_GetReplicatedIsGrounded,
            &NetworkIdentityComponent_SetReplicatedIsGrounded,
            &NetworkIdentityComponent_GetSyncComponentsMask,
            &NetworkIdentityComponent_SetSyncComponentsMask,
            &NetworkIdentityComponent_MarkDirty,

            &PlaneColliderComponent_GetCenter,
            &PlaneColliderComponent_SetCenter,
            &PlaneColliderComponent_GetScale,
            &PlaneColliderComponent_SetScale,

            &TextComponent_GetFontSize,
            &TextComponent_SetFontSize,
            &TextComponent_GetScreenSpace,
            &TextComponent_SetScreenSpace,
            &TextComponent_GetBillboard,
            &TextComponent_SetBillboard,
            &TextComponent_GetCenterAlign,
            &TextComponent_SetCenterAlign,

            &WidgetComponent_GetCanvasEnabled,
            &WidgetComponent_SetCanvasEnabled,
            &WidgetComponent_HasItem,
            &WidgetComponent_GetItemVisible,
            &WidgetComponent_SetItemVisible,
            &WidgetComponent_GetItemBackgroundColor,
            &WidgetComponent_SetItemBackgroundColor,
            &WidgetComponent_GetItemBorderColor,
            &WidgetComponent_SetItemBorderColor,
            &WidgetComponent_GetItemOpacity,
            &WidgetComponent_SetItemOpacity,
            &WidgetComponent_GetItemSize,
            &WidgetComponent_SetItemSize,
            &WidgetComponent_GetItemPosition,
            &WidgetComponent_SetItemPosition,
            &WidgetComponent_GetButtonText,
            &WidgetComponent_SetButtonText,
            &WidgetComponent_GetButtonInteractable,
            &WidgetComponent_SetButtonInteractable,
            &WidgetComponent_IsButtonHovered,
            &WidgetComponent_SetButtonColors
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
        s_EntityRemoveComponentFuncs.clear();
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
