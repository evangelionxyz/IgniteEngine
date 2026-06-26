// Copyright (c) 2026 Evangelion Manuhutu

#include "pch.hpp"

#include "scene_serializer.hpp"
#include "ignite/scene/component.hpp"

#include "ignite/scripting/script_class.hpp"
#include "ignite/scripting/script_engine.hpp"

#include "ignite/project/project.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/graphics/objects/material.hpp"
#include "ignite/graphics/objects/material_2d.hpp"
#include "ignite/graphics/objects/environment.hpp"
#include "ignite/animation/skeleton.hpp"

#include "ignite/scene/entity.hpp"
#include "ignite/scene/scene_manager.hpp"

#include <ranges>
#include <algorithm>

namespace ignite
{
    SceneSerializer::SceneSerializer(const Ref<Scene> &scene, Project *project)
        : m_Scene(scene), m_Project(project)
    {
    }

    bool SceneSerializer::Serialize(const ignite::Path &filepath)
    {
        if (!m_Scene || !m_Project)
            return false;

        Serializer sr(filepath);

        sr.BeginMap(); // START

        sr.BeginMap("Scene"); // scene file header
        sr.AddKeyValue<uint32_t>("Version", Application::GetVersion());
        sr.AddKeyValue<std::string>("Title", m_Scene->name);
        sr.BeginSequence("Entities");

        // Sort by name first: copy map contents into a vector for sorting
        std::vector<std::pair<UUID, entt::entity>> sortedEntities(m_Scene->entities.begin(), m_Scene->entities.end());
        std::sort(sortedEntities.begin(), sortedEntities.end(), [scene = m_Scene](const auto &a, const auto &b)
        {
            Entity entityA = { a.second, scene.get() };
            Entity entityB = { b.second, scene.get() };

            // Use Entity::GetName() to compare display names
            const std::string &nameA = entityA.GetName();
            const std::string &nameB = entityB.GetName();
            return nameA < nameB;
        });

        // entities sequence
        for (const entt::entity e : sortedEntities | std::views::values)
        {
            Entity entity = { e, m_Scene.get() };
            const IDComponent &idComp = entity.GetComponent<IDComponent>();

            const bool isPrefab = idComp.IsInType(EntityType_Prefab);

            if (isPrefab)
                continue;

            sr.BeginMap(); // START Entity
            {
                // ID Component
                sr.AddKeyValue("ID", idComp.uuid);
                sr.AddKeyValue("Name", idComp.name);
                sr.AddKeyValue("Type", EntityTypeFlagsToString(idComp.type));
                sr.AddKeyValue("Parent", idComp.parent);

                // Transform Component
                if (entity.HasComponent<TransformComponent>())
                {
                    const auto &comp = entity.GetComponent<TransformComponent>();
                    sr.BeginMap("Transform");
                    {
                        sr.AddKeyValue("WorldTranslation", comp.world.translation);
                        sr.AddKeyValue("WorldRotation", comp.world.rotation);
                        sr.AddKeyValue("WorldScale", comp.world.scale);

                        sr.AddKeyValue("LocalTranslation", comp.local.translation);
                        sr.AddKeyValue("LocalRotation", comp.local.rotation);
                        sr.AddKeyValue("LocalScale", comp.local.scale);

                        sr.AddKeyValue("Visible", comp.visible);
                    }
                    sr.EndMap();
                }

                // Directional Light
                if (entity.HasComponent<DirectionalLightComponent>())
                {
                    const DirectionalLightComponent &comp = entity.GetComponent<DirectionalLightComponent>();
                    sr.BeginMap("DirectionalLight");
                    {
                        sr.AddKeyValue("Color", comp.color);
                        sr.AddKeyValue("Intensity", comp.intensity);
                        sr.AddKeyValue("AngularRadius", comp.angularRadius);
                        sr.AddKeyValue("ShadowStrength", comp.shadowStrength);
                        sr.AddKeyValue("ShadowMinBias", comp.shadowMinBias);
                        sr.AddKeyValue("ShadowMaxBias", comp.shadowMaxBias);
                        sr.AddKeyValue("PCFRadius", comp.pcfRadius);
                        sr.AddKeyValue("ShadowResolution", comp.shadowResolution);
                        sr.AddKeyValue("CascadeShadow", comp.cascadeShadow);
                    }
                    sr.EndMap();
                }

                // Camera
                if (entity.HasComponent<CameraComponent>())
                {
                    const auto &comp = entity.GetComponent<CameraComponent>();
                    sr.BeginMap("Camera");
                    {
                        int projectionType = static_cast<int>(comp.camera.projectionType);
                        int aspectRatioPreset = static_cast<int>(comp.camera.GetAspectRatioPreset());
                        sr.AddKeyValue("ProjectionType", projectionType);
                        sr.AddKeyValue("AspectRatioPreset", aspectRatioPreset);
                        sr.AddKeyValue("OrthoSize", comp.camera.orthoSize);
                        sr.AddKeyValue("NearClip", comp.camera.nearPlane);
                        sr.AddKeyValue("FarClip", comp.camera.farPlane);
                        sr.AddKeyValue("Fov", comp.camera.fov);
                        sr.AddKeyValue("Primary", comp.primary);

                        sr.BeginMap("PostProcessing");
                        {
                            const PostProcessing &pp = comp.camera.postProcessing;
                            sr.AddKeyValue("EnableVignette", pp.enableVignette);
                            sr.AddKeyValue("EnableChromAb", pp.enableChromAb);
                            sr.AddKeyValue("EnableBloom", pp.enableBloom);
                            sr.AddKeyValue("EnableSSAO", pp.enableSSAO);
                            sr.AddKeyValue("DebugSSAO", pp.debugSSAO);

                            sr.AddKeyValue("BloomIntensity", pp.bloomIntensity);
                            sr.AddKeyValue("BloomThreshold", pp.bloomThreshold);
                            sr.AddKeyValue("BloomKnee", pp.bloomKnee);
                            sr.AddKeyValue("BloomRadius", pp.bloomRadius);
                            sr.AddKeyValue("BloomIterations", pp.bloomIterations);

                            sr.AddKeyValue("VignetteRadius", pp.vignetteRadius);
                            sr.AddKeyValue("VignetteSoftness", pp.vignetteSoftness);
                            sr.AddKeyValue("VignetteIntensity", pp.vignetteIntensity);
                            sr.AddKeyValue("VignetteColor", pp.vignetteColor);

                            sr.AddKeyValue("ChromAbAmount", pp.chromAbAmount);
                            sr.AddKeyValue("ChromAbRadial", pp.chromAbRadial);

                            sr.AddKeyValue("AoRadius", pp.aoRadius);
                            sr.AddKeyValue("AoBias", pp.aoBias);
                            sr.AddKeyValue("AoIntensity", pp.aoIntensity);
                            sr.AddKeyValue("AoPower", pp.aoPower);
                        }
                        sr.EndMap();
                    }
                    sr.EndMap();
                }

                // Sprite 2D component
                if (entity.HasComponent<Sprite2DComponent>())
                {
                    const auto &comp = entity.GetComponent<Sprite2DComponent>();
                    sr.BeginMap("Sprite2D");
                    {
                        sr.AddKeyValue("MaterialHandle", comp.materialHandle);
                        sr.AddKeyValue("Handle", comp.handle);
                        sr.AddKeyValue("Color", comp.color);
                        sr.AddKeyValue("TilingFactor", comp.tilingFactor);
                        sr.AddKeyValue("UV0", comp.uv0);
                        sr.AddKeyValue("UV1", comp.uv1);
                        sr.AddKeyValue("FlipX", comp.flipX);
                        sr.AddKeyValue("FlipY", comp.flipY);
                    }
                    sr.EndMap();
                }

                if (entity.HasComponent<Animator2DComponent>())
                {
                    const auto &comp = entity.GetComponent<Animator2DComponent>();
                    sr.BeginMap("Animator2D");
                    {
                        sr.AddKeyValue("ControllerHandle", static_cast<uint64_t>(comp.controllerHandle));
                        sr.AddKeyValue("CurrentState", comp.currentStateName);
                    }
                    sr.EndMap();
                }

                // Circle 2D component
                if (entity.HasComponent<Circle2DComponent>())
                {
                    const auto &comp = entity.GetComponent<Circle2DComponent>();
                    sr.BeginMap("Circle2D");
                    {
                        sr.AddKeyValue("Color", comp.color);
                        sr.AddKeyValue("Thickness", comp.thickness);
                        sr.AddKeyValue("Fade", comp.fade);
                    }
                    sr.EndMap();
                }

                if (entity.HasComponent<PointLight2DComponent>())
                {
                    const auto &comp = entity.GetComponent<PointLight2DComponent>();
                    sr.BeginMap("PointLight2D");
                    {
                        sr.AddKeyValue("Color", comp.color);
                        sr.AddKeyValue("Radius", comp.radius);
                        sr.AddKeyValue("Intensity", comp.intensity);
                        sr.AddKeyValue("Enabled", comp.enabled);
                    }
                    sr.EndMap();
                }

                // Rigidbody 2D
                if (entity.HasComponent<Rigidbody2DComponent>())
                {
                    const auto &comp = entity.GetComponent<Rigidbody2DComponent>();
                    sr.BeginMap("Rigidbody2D");
                    {
                        sr.AddKeyValue("BodyType", static_cast<int>(comp.bodyType));
                        sr.AddKeyValue("LinearVelocity", comp.linearVelocity);
                        sr.AddKeyValue("AngularVelocity", comp.angularVelocity);
                        sr.AddKeyValue("GravityScale", comp.gravityScale);
                        sr.AddKeyValue("LinearDamping", comp.linearDamping);
                        sr.AddKeyValue("AngularDamping", comp.angularDamping);
                        sr.AddKeyValue("IsAwake", comp.isAwake);
                        sr.AddKeyValue("FixedRotation", comp.fixedRotation);
                        sr.AddKeyValue("AllowFastRotation", comp.allowFastRotation);
                        sr.AddKeyValue("IsEnabled", comp.isEnabled);
                        sr.AddKeyValue("IsEnableSleep", comp.isEnableSleep);
                    }
                    sr.EndMap();
                }

                // Box collider 2D
                if (entity.HasComponent<BoxCollider2DComponent>())
                {
                    const auto &comp = entity.GetComponent<BoxCollider2DComponent>();
                    sr.BeginMap("BoxCollider2D");
                    {
                        sr.AddKeyValue("Size", comp.size);
                        sr.AddKeyValue("Offset", comp.offset);
                        sr.AddKeyValue("Restitution", comp.restitution);
                        sr.AddKeyValue("Friction", comp.friction);
                        sr.AddKeyValue("Density", comp.density);
                        sr.AddKeyValue("IsSensor", comp.isSensor);
                    }
                    sr.EndMap();
                }

                // Circle collider 2D
                if (entity.HasComponent<CircleCollider2DComponent>())
                {
                    const auto &comp = entity.GetComponent<CircleCollider2DComponent>();
                    sr.BeginMap("CircleCollider2D");
                    {
                        sr.AddKeyValue("Radius", comp.radius);
                        sr.AddKeyValue("Center", comp.center);
                        sr.AddKeyValue("Restitution", comp.restitution);
                        sr.AddKeyValue("Friction", comp.friction);
                        sr.AddKeyValue("Density", comp.density);
                        sr.AddKeyValue("IsSensor", comp.isSensor);
                    }
                    sr.EndMap();
                }

                // Mesh
                if (entity.HasComponent<MeshComponent>())
                {
                    const auto &comp = entity.GetComponent<MeshComponent>();
                    sr.BeginMap("Mesh");
                    {
                        sr.AddKeyValue("Handle", static_cast<uint64_t>(comp.handle));
                        sr.AddKeyValue("AnimatorHandle", static_cast<uint64_t>(comp.runtimeAnimatorHandle));
                        sr.AddKeyValue("UniqueAnimator", comp.uniqueAnimator);

						// Serialize vertices
						sr.BeginSequence("OverrideMaterials");
						for (const auto &[meshIndex, materialHandle] : comp.overrideMaterials)
						{
							sr.BeginMap();
							sr.AddKeyValue("Mesh", meshIndex);
							sr.AddKeyValue("MaterialHandle", static_cast<uint64_t>(materialHandle));
							sr.EndMap();
						}
						sr.EndSequence();
                    }
                    sr.EndMap();
                }

                // Rigidbody
                if (entity.HasComponent<RigidbodyComponent>())
                {
                    const auto &comp = entity.GetComponent<RigidbodyComponent>();
                    sr.BeginMap("Rigidbody");
                    {
                        sr.AddKeyValue("MotionQuality", static_cast<int>(comp.motionQuality));
                        sr.AddKeyValue("BodyType", static_cast<int>(comp.bodyType));
                        sr.AddKeyValue("UseGravity", comp.useGravity);
                        sr.AddKeyValue("RotateX", comp.rotateX);
                        sr.AddKeyValue("RotateY", comp.rotateY);
                        sr.AddKeyValue("RotateZ", comp.rotateZ);
                        sr.AddKeyValue("MoveX", comp.moveX);
                        sr.AddKeyValue("MoveY", comp.moveY);
                        sr.AddKeyValue("MoveZ", comp.moveZ);
                        sr.AddKeyValue("Mass", comp.mass);
                        sr.AddKeyValue("AllowSleeping", comp.allowSleeping);
                        sr.AddKeyValue("RetainAcceleration", comp.retainAcceleration);
                        sr.AddKeyValue("GravityFactor", comp.gravityFactor);
                        sr.AddKeyValue("CenterMass", comp.centerMass);
                    }
                    sr.EndMap();
                }

                if (entity.HasComponent<BoxColliderComponent>())
                {
                    const auto &comp = entity.GetComponent<BoxColliderComponent>();
                    sr.BeginMap("BoxCollider");
                    {
                        sr.AddKeyValue("Scale", comp.scale);
                        sr.AddKeyValue("Center", comp.center);
                        sr.AddKeyValue("Friction", comp.friction);
                        sr.AddKeyValue("StaticFriction", comp.staticFriction);
                        sr.AddKeyValue("Restitution", comp.restitution);
                        sr.AddKeyValue("Density", comp.density);
                    }
                    sr.EndMap();
                }

                // SphereCollider
                if (entity.HasComponent<SphereColliderComponent>())
                {
                    const auto &comp = entity.GetComponent<SphereColliderComponent>();
                    sr.BeginMap("SphereCollider");
                    {
                        sr.AddKeyValue("Radius", comp.radius);
                        sr.AddKeyValue("Center", comp.center);
                        sr.AddKeyValue("Friction", comp.friction);
                        sr.AddKeyValue("StaticFriction", comp.staticFriction);
                        sr.AddKeyValue("Restitution", comp.restitution);
                        sr.AddKeyValue("Density", comp.density);
                    }
                    sr.EndMap();
                }

                // CapsuleCollider
                if (entity.HasComponent<CapsuleColliderComponent>())
                {
                    const auto &comp = entity.GetComponent<CapsuleColliderComponent>();
                    sr.BeginMap("CapsuleCollider");
                    {
                        sr.AddKeyValue("Radius", comp.radius);
                        sr.AddKeyValue("Center", comp.center);
                        sr.AddKeyValue("Height", comp.height);
                        sr.AddKeyValue("Friction", comp.friction);
                        sr.AddKeyValue("StaticFriction", comp.staticFriction);
                        sr.AddKeyValue("Restitution", comp.restitution);
                        sr.AddKeyValue("Density", comp.density);
                    }
                    sr.EndMap();
                }

                // MeshCollider
                if (entity.HasComponent<MeshColliderComponent>())
                {
                    const auto &comp = entity.GetComponent<MeshColliderComponent>();
                    sr.BeginMap("MeshCollider");
                    {
                        sr.AddKeyValue("Convex", comp.convex);
                        sr.AddKeyValue("Friction", comp.friction);
                        sr.AddKeyValue("StaticFriction", comp.staticFriction);
                        sr.AddKeyValue("Restitution", comp.restitution);
                        sr.AddKeyValue("Density", comp.density);

                        // Serialize vertices
                        sr.BeginSequence("Vertices");
                        for (const auto &vertex : comp.vertices)
                        {
                            sr.AddValue(vertex);
                        }
                        sr.EndSequence();

                        // Serialize indices
                        sr.BeginSequence("Indices");
                        for (const auto &index : comp.indices)
                        {
                            sr.AddValue(index);
                        }
                        sr.EndSequence();
                    }
                    sr.EndMap();
                }

                // Audio Source
                if (entity.HasComponent<AudioSourceComponent>())
                {
                    const auto &comp = entity.GetComponent<AudioSourceComponent>();
                    sr.BeginMap("AudioSource");
                    {
                        sr.AddKeyValue("Handle", static_cast<uint64_t>(comp.handle));
                        sr.AddKeyValue("Volume", comp.volume);
                        sr.AddKeyValue("Pitch", comp.pitch);
                        sr.AddKeyValue("Pan", comp.pan);
                        sr.AddKeyValue("PlayOnStart", comp.playOnStart);
                        sr.AddKeyValue("Loop", comp.loop);

                        sr.BeginSequence("DSPs");
                        for (const auto &dsp : comp.dsps)
                        {
                            sr.BeginMap();
                            sr.AddKeyValue("Type", static_cast<int>(dsp.type));
                            sr.AddKeyValue("Enabled", dsp.enabled);

                            switch (dsp.type)
                            {
                                case AudioSourceComponent::DspType::Reverb:
                                sr.AddKeyValue("DecayTime", dsp.reverbDecayTime);
                                sr.AddKeyValue("EarlyDelay", dsp.reverbEarlyDelay);
                                sr.AddKeyValue("LateDelay", dsp.reverbLateDelay);
                                sr.AddKeyValue("HighFrequencyReference", dsp.reverbHighFrequencyReference);
                                sr.AddKeyValue("Diffusion", dsp.reverbDiffusion);
                                sr.AddKeyValue("Density", dsp.reverbDensity);
                                sr.AddKeyValue("LowShelfGain", dsp.reverbLowShelfGain);
                                sr.AddKeyValue("HighCut", dsp.reverbHighCut);
                                sr.AddKeyValue("DryLevel", dsp.reverbDryLevel);
                                sr.AddKeyValue("WetLevel", dsp.reverbWetLevel);
                                break;
                                case AudioSourceComponent::DspType::Distortion:
                                sr.AddKeyValue("DistortionLevel", dsp.distortionLevel);
                                break;
                                case AudioSourceComponent::DspType::Chorus:
                                sr.AddKeyValue("Mix", dsp.chorusMix);
                                sr.AddKeyValue("Rate", dsp.chorusRate);
                                sr.AddKeyValue("Depth", dsp.chorusDepth);
                                break;
                                case AudioSourceComponent::DspType::Compressor:
                                sr.AddKeyValue("Threshold", dsp.compressorThreshold);
                                sr.AddKeyValue("Ratio", dsp.compressorRatio);
                                sr.AddKeyValue("Release", dsp.compressorRelease);
                                sr.AddKeyValue("GainMakeup", dsp.compressorGainMakeup);
                                sr.AddKeyValue("UseSidechain", dsp.compressorUseSidechain);
                                break;
                                case AudioSourceComponent::DspType::Delay:
                                sr.AddKeyValue("DelayMs", dsp.delayMs);
                                sr.AddKeyValue("Feedback", dsp.delayFeedback);
                                break;
                            }

                            sr.EndMap();
                        }
                        sr.EndSequence();
                    }
                    sr.EndMap();
                }

                // World Environment
                if (entity.HasComponent<WorldEnvironment>())
                {
                    const WorldEnvironment &comp = entity.GetComponent<WorldEnvironment>();
                    sr.BeginMap("WorldEnvironment");
                    {
                        sr.AddKeyValue("HDRHandle", static_cast<uint64_t>(comp.hdrHandle));
                        sr.AddKeyValue("Exposure", comp.exposure);
                        sr.AddKeyValue("Gamma", comp.gamma);
                        sr.AddKeyValue("Ambient", comp.ambient);
                    }
                    sr.EndMap();
                }

                // Text Component
                if (entity.HasComponent<TextComponent>())
                {
                    const auto &comp = entity.GetComponent<TextComponent>();
                    sr.BeginMap("TextComponent");
                    {
                        sr.AddKeyValue("FontHandle", comp.fontHandle);
                        sr.AddKeyValue("Material2DHandle", comp.material2dHandle);
                        sr.AddKeyValue("Text", comp.text);
                        sr.AddKeyValue("Color", comp.color);
                        sr.AddKeyValue("Kerning", comp.kerning);
                        sr.AddKeyValue("LineSpacing", comp.lineSpacing);
                        sr.AddKeyValue("ScreenSpace", comp.screenSpace);
                    }
                    sr.EndMap();
                }

                if (entity.HasComponent<WidgetComponent>())
                {
                    const auto &comp = entity.GetComponent<WidgetComponent>();
                    sr.BeginMap("WidgetComponent");
                    {
                        sr.AddKeyValue("WidgetHandle", static_cast<uint64_t>(comp.widgetHandle));
                    }
                    sr.EndMap();
                }

                // Script
                if (entity.HasComponent<ScriptComponent>())
                {
                    auto &comp = entity.GetComponent<ScriptComponent>();
                    sr.BeginMap("Script");
                    {
                        sr.AddKeyValue("ClassName", comp.className);

                        Ref<ScriptClass> scriptClass = ScriptEngine::GetInstance()->GetEntityClassByName(comp.className);
                        if (scriptClass)
                        {
                            if (auto instanceFields = scriptClass->GetInstanceFieldsById(entity.GetUUID()); instanceFields && !instanceFields->empty())
                            {
                                sr.BeginSequence("Fields");
                                for (const auto &name : scriptClass->GetOrderedFieldNames())
                                {
                                    auto fieldIt = instanceFields->find(name);
                                    if (fieldIt == instanceFields->end()) continue;
                                    auto &fieldInstance = fieldIt->second;
                                    sr.BeginMap();
                                    sr.AddKeyValue("Name", name);
                                    sr.AddKeyValue("Type", Utils::ScriptFieldTypeToString(fieldInstance.field.Type));

                                    switch (fieldInstance.field.Type)
                                    {
                                        case ScriptFieldType::String: sr.AddKeyValue("Value", fieldInstance.GetValue<std::string>()); break;
                                        case ScriptFieldType::Float: sr.AddKeyValue("Value", fieldInstance.GetValue<float>()); break;
                                        case ScriptFieldType::Double: sr.AddKeyValue("Value", fieldInstance.GetValue<double>()); break;
                                        case ScriptFieldType::Bool: sr.AddKeyValue("Value", fieldInstance.GetValue<bool>()); break;
                                        case ScriptFieldType::Char: sr.AddKeyValue("Value", fieldInstance.GetValue<char16_t>()); break;
                                        case ScriptFieldType::Byte: sr.AddKeyValue("Value", (int)fieldInstance.GetValue<uint8_t>()); break;
                                        case ScriptFieldType::SByte: sr.AddKeyValue("Value", (int)fieldInstance.GetValue<int8_t>()); break;
                                        case ScriptFieldType::Short: sr.AddKeyValue("Value", fieldInstance.GetValue<int16_t>()); break;
                                        case ScriptFieldType::UShort: sr.AddKeyValue("Value", fieldInstance.GetValue<uint16_t>()); break;
                                        case ScriptFieldType::Int: sr.AddKeyValue("Value", fieldInstance.GetValue<int>()); break;
                                        case ScriptFieldType::UInt: sr.AddKeyValue("Value", fieldInstance.GetValue<uint32_t>()); break;
                                        case ScriptFieldType::Long: sr.AddKeyValue("Value", fieldInstance.GetValue<int64_t>()); break;
                                        case ScriptFieldType::ULong: sr.AddKeyValue("Value", fieldInstance.GetValue<uint64_t>()); break;
                                        case ScriptFieldType::Vector2: sr.AddKeyValue("Value", fieldInstance.GetValue<glm::vec2>()); break;
                                        case ScriptFieldType::Vector3: sr.AddKeyValue("Value", fieldInstance.GetValue<glm::vec3>()); break;
                                        case ScriptFieldType::Vector4: sr.AddKeyValue("Value", fieldInstance.GetValue<glm::vec4>()); break;
                                        case ScriptFieldType::Quat: sr.AddKeyValue("Value", fieldInstance.GetValue<glm::quat>()); break;
                                        case ScriptFieldType::Color: sr.AddKeyValue("Value", fieldInstance.GetValue<glm::vec4>()); break;
                                        case ScriptFieldType::Enum: sr.AddKeyValue("Value", fieldInstance.GetValue<int>()); break;
                                        case ScriptFieldType::Entity: sr.AddKeyValue("Value", fieldInstance.GetValue<uint64_t>()); break;
                                        case ScriptFieldType::Asset: sr.AddKeyValue("Value", fieldInstance.GetValue<uint64_t>()); break;
                                        default: break;
                                    }

                                    sr.EndMap();
                                }
                                sr.EndSequence();
                            }
                        }
                    }
                    sr.EndMap();
                }

            }
            sr.EndMap(); // END Entity
        }

        sr.EndSequence(); // Entities
        sr.EndMap(); // scene

        sr.EndMap(); // END

#if 0
        // Example
        sr.BeginMap(); // START

        sr.BeginMap("Scene"); // scene file header

        sr.AddKeyValue<std::string>("Title", m_Scene->name);
        sr.AddKeyValue<std::string>("Version", ENGINE_VERSION);

        sr.BeginSequence("Entities");


        // entities sequence
        for (int i = 0; i < 10; ++i)
        {
            sr.BeginMap(); // START Entity
            {
                sr.AddKeyValue<std::string>("ID", "ENTITY_ID");
                sr.AddKeyValue<std::string>("Type", "ENTITY_TYPE");
                sr.AddKeyValue<std::string>("Parent", "PARENT_ID");
                sr.BeginMap("Component A");
                {
                    sr.AddKeyValue("Var A", "value");
                    sr.AddKeyValue("Var B", "value");

                    sr.BeginSequence("List Var");
                    {
                        sr.BeginMap();
                        {
                            sr.AddKeyValue("List Var A", "value");
                            sr.AddKeyValue("List Var B", "value");
                        }
                        sr.EndMap();
                    }
                    sr.EndSequence();
                }
                sr.EndMap();
            }
            sr.EndMap(); // END Entity
        }

        sr.EndSequence(); // Entities
        sr.EndMap(); // scene

        sr.EndMap(); // END

#endif
        sr.Serialize();

        // Scene should be not dirty
        m_Scene->SetDirtyFlag(false);

        return true;
    }

    Ref<Scene> SceneSerializer::Deserialize(const ignite::Path &filepath, Project *project)
    {
        if (!ignite::Path::exists(filepath))
        {
            LOG_ERROR("[Scene SR] File does not exists!\n{}", filepath.generic_string());
            return nullptr;
        }

        LOG_ASSERT(project, "[Scene SR] Invalid project");

        YAML::Node sceneFileNode = Serializer::Deserialize(filepath);
        YAML::Node sceneNode = sceneFileNode["Scene"];

        LOG_ASSERT(sceneNode, "[Scene SR] Invalid scene file");
        if (!sceneNode)
            return nullptr;

        std::string title = sceneNode["Title"].as<std::string>();
        Ref<Scene> desScene = Scene::Create(project, title);

        // Open commandlist for asset deserialization
        auto device = DeviceManager::GetInstance()->GetDevice();
        nvrhi::CommandListHandle cmd = device->createCommandList();

        for (YAML::Node entityNode : sceneNode["Entities"])
        {
            UUID uuid = UUID(entityNode["ID"].as<uint64_t>());
            std::string name = entityNode["Name"].as<std::string>();
            EntityType type = EntityTypeFromStringFlags(entityNode["Type"].as<std::string>());

            Entity desEntity = SceneManager::CreateEntity(desScene.get(), name, type, uuid);
            UUID parent = UUID(entityNode["Parent"].as<uint64_t>());
            desEntity.GetComponent<IDComponent>().parent = parent;

            // Transform component
            if (YAML::Node node = entityNode["Transform"])
            {
                auto &comp = desEntity.AddComponent<TransformComponent>();
                comp.world.translation = node["WorldTranslation"].as<glm::vec3>();
                comp.world.rotation = node["WorldRotation"].as<glm::quat>();
                comp.world.scale = node["WorldScale"].as<glm::vec3>();

                comp.local.translation = node["LocalTranslation"].as<glm::vec3>();
                comp.local.rotation = node["LocalRotation"].as<glm::quat>();
                comp.local.scale = node["LocalScale"].as<glm::vec3>();

                comp.visible = node["Visible"].as<bool>();
            }

            // Camera component
            if (YAML::Node node = entityNode["Camera"])
            {
                auto &comp = desEntity.AddComponent<CameraComponent>();
                comp.camera.projectionType = static_cast<ProjectionType>(node["ProjectionType"].as<int>());
                if (node["AspectRatioPreset"])
                {
                    comp.camera.SetAspectRatioPreset(static_cast<SceneCamera::AspectRatioPreset>(node["AspectRatioPreset"].as<int>()));
                }

                if (auto n = node["OrthoSize"]) comp.camera.orthoSize = n.as<float>();
                if (auto n = node["NearClip"]) comp.camera.nearPlane = n.as<float>();
                if (auto n = node["FarClip"]) comp.camera.farPlane = n.as<float>();
                if (auto n = node["Fov"]) comp.camera.fov = n.as<float>();
                if (auto n = node["Primary"]) comp.primary = n.as<bool>();

                if (YAML::Node ppNode = node["PostProcessing"])
                {
                    auto &pp = comp.camera.postProcessing;
                    if (auto n = ppNode["EnableVignette"]) pp.enableVignette = n.as<bool>();
                    if (auto n = ppNode["EnableChromAb"]) pp.enableChromAb = n.as<bool>();
                    if (auto n = ppNode["EnableBloom"]) pp.enableBloom = n.as<bool>();
                    if (auto n = ppNode["EnableSSAO"]) pp.enableSSAO = n.as<bool>();
                    if (auto n = ppNode["DebugSSAO"]) pp.debugSSAO = n.as<bool>();

                    if (auto n = ppNode["BloomIntensity"]) pp.bloomIntensity = n.as<float>();
                    if (auto n = ppNode["BloomThreshold"]) pp.bloomThreshold = n.as<float>();
                    if (auto n = ppNode["BloomKnee"]) pp.bloomKnee = n.as<float>();
                    if (auto n = ppNode["BloomRadius"]) pp.bloomRadius = n.as<float>();
                    if (auto n = ppNode["BloomIterations"]) pp.bloomIterations = n.as<int>();

                    if (auto n = ppNode["VignetteRadius"]) pp.vignetteRadius = n.as<float>();
                    if (auto n = ppNode["VignetteSoftness"]) pp.vignetteSoftness = n.as<float>();
                    if (auto n = ppNode["VignetteIntensity"]) pp.vignetteIntensity = n.as<float>();
                    if (auto n = ppNode["VignetteColor"]) pp.vignetteColor = n.as<glm::vec3>();

                    if (auto n = ppNode["ChromAbAmount"]) pp.chromAbAmount = n.as<float>();
                    if (auto n = ppNode["ChromAbRadial"]) pp.chromAbRadial = n.as<float>();

                    if (auto n = ppNode["AoRadius"]) pp.aoRadius = n.as<float>();
                    if (auto n = ppNode["AoBias"]) pp.aoBias = n.as<float>();
                    if (auto n = ppNode["AoIntensity"]) pp.aoIntensity = n.as<float>();
                    if (auto n = ppNode["AoPower"]) pp.aoPower = n.as<float>();
                }

                comp.camera.UpdateView();
                comp.camera.UpdateProjection(1280, 720);
            }

            // Sprite 2D component
            if (YAML::Node node = entityNode["Sprite2D"])
            {
                auto &comp = desEntity.AddComponent<Sprite2DComponent>();
                if (auto n = node["MaterialHandle"])comp.materialHandle = AssetHandle(n.as<uint64_t>());
                if (auto n = node["Handle"]) comp.handle = AssetHandle(n.as<uint64_t>());
                if (auto n = node["Color"]) comp.color = n.as<glm::vec4>();
                if (auto n = node["TilingFactor"]) comp.tilingFactor = n.as<glm::vec2>();
                if (auto n = node["UV0"]) comp.uv0 = n.as<glm::vec2>();
                if (auto n = node["UV1"]) comp.uv1 = n.as<glm::vec2>();
                if (auto n = node["FlipX"])comp.flipX = n.as<bool>();
                if (auto n = node["FlipY"]) comp.flipY = n.as<bool>();
            }

            if (YAML::Node node = entityNode["Animator2D"])
            {
                auto &comp = desEntity.AddComponent<Animator2DComponent>();
                if (auto n = node["ControllerHandle"]) comp.controllerHandle = AssetHandle(n.as<uint64_t>());
                if (auto n = node["CurrentState"])    comp.currentStateName = n.as<std::string>();
            }

            // Circle 2D component
            if (YAML::Node node = entityNode["Circle2D"])
            {
                auto &comp = desEntity.AddComponent<Circle2DComponent>();
                comp.color = node["Color"].as<glm::vec4>();
                comp.thickness = node["Thickness"].as<float>();
                comp.fade = node["Fade"].as<float>();
            }

            if (YAML::Node node = entityNode["PointLight2D"])
            {
                auto &comp = desEntity.AddComponent<PointLight2DComponent>();
                comp.color = node["Color"].as<glm::vec4>();
                comp.radius = node["Radius"].as<float>();
                comp.intensity = node["Intensity"].as<float>();
                comp.enabled = node["Enabled"].as<bool>();
            }

            // Rigidbody 2D
            if (YAML::Node node = entityNode["Rigidbody2D"])
            {
                auto &comp = desEntity.AddComponent<Rigidbody2DComponent>();
                if (auto n = node["BodyType"]) comp.bodyType = static_cast<Rigidbody2DComponent::EBodyType>(n.as<int>());
                if (auto n = node["LinearVelocity"]) comp.linearVelocity = n.as<glm::vec2>();
                if (auto n = node["AngularVelocity"]) comp.angularVelocity = n.as<float>();
                if (auto n = node["GravityScale"]) comp.gravityScale = n.as<float>();
                if (auto n = node["LinearDamping"]) comp.linearDamping = n.as<float>();
                if (auto n = node["AngularDamping"]) comp.angularDamping = n.as<float>();
                if (auto n = node["FixedRotation"]) comp.fixedRotation = n.as<bool>();
                if (auto n = node["AllowFastRotation"]) comp.allowFastRotation = n.as<bool>();
                if (auto n = node["IsAwake"]) comp.isAwake = n.as<bool>();
                if (auto n = node["IsEnabled"]) comp.isEnabled = n.as<bool>();
                if (auto n = node["IsEnableSleep"]) comp.isEnableSleep = n.as<bool>();
            }

            // BoxCollider 2D
            if (YAML::Node node = entityNode["BoxCollider2D"])
            {
                auto &comp = desEntity.AddComponent<BoxCollider2DComponent>();
                if (auto n = node["Size"]) comp.size = n.as<glm::vec2>();
                if (auto n = node["Offset"]) comp.offset = n.as<glm::vec2>();
                if (auto n = node["Restitution"]) comp.restitution = n.as<float>();
                if (auto n = node["Friction"]) comp.friction = n.as<float>();
                if (auto n = node["Density"]) comp.density = n.as<float>();
                if (auto n = node["IsSensor"]) comp.isSensor = n.as<bool>();
            }

            // CircleCollider 2D
            if (YAML::Node node = entityNode["CircleCollider2D"])
            {
                auto &comp = desEntity.AddComponent<CircleCollider2DComponent>();
                if (auto n = node["Center"]) comp.center = n.as<glm::vec2>();
                if (auto n = node["Radius"]) comp.radius = n.as<float>();
                if (auto n = node["Restitution"]) comp.restitution = n.as<float>();
                if (auto n = node["Friction"]) comp.friction = n.as<float>();
                if (auto n = node["Density"]) comp.density = n.as<float>();
                if (auto n = node["IsSensor"]) comp.isSensor = n.as<bool>();
            }

            // Rigidbody
            if (YAML::Node node = entityNode["Rigidbody"])
            {
                auto &comp = desEntity.AddComponent<RigidbodyComponent>();
                if (auto n = node["MotionQuality"]) comp.motionQuality = static_cast<RigidbodyComponent::EMotionQuality>(n.as<int>());
                if (auto n = node["BodyType"]) comp.bodyType = static_cast<RigidbodyComponent::EBodyType>(n.as<int>());
                if (auto n = node["UseGravity"]) comp.useGravity = n.as<bool>();
                if (auto n = node["RotateX"]) comp.rotateX = n.as<bool>();
                if (auto n = node["RotateY"]) comp.rotateY = n.as<bool>();
                if (auto n = node["RotateZ"]) comp.rotateZ = n.as<bool>();
                if (auto n = node["MoveX"]) comp.moveX = n.as<bool>();
                if (auto n = node["MoveY"]) comp.moveY = n.as<bool>();
                if (auto n = node["MoveZ"]) comp.moveZ = n.as<bool>();
                if (auto n = node["Mass"]) comp.mass = n.as<float>();
                if (auto n = node["AllowSleeping"]) comp.allowSleeping = n.as<bool>();
                if (auto n = node["RetainAcceleration"]) comp.retainAcceleration = n.as<bool>();
                if (auto n = node["GravityFactor"]) comp.gravityFactor = n.as<float>();
                if (auto n = node["CenterMass"]) comp.centerMass = n.as<glm::vec3>();
            }

            // BoxCollider
            if (YAML::Node node = entityNode["BoxCollider"])
            {
                auto &comp = desEntity.AddComponent<BoxColliderComponent>();
                if (auto n = node["Scale"]) comp.scale = n.as<glm::vec3>();
                if (auto n = node["Center"]) comp.center = n.as<glm::vec3>();
                if (auto n = node["Friction"]) comp.friction = n.as<float>();
                if (auto n = node["StaticFriction"]) comp.staticFriction = n.as<float>();
                if (auto n = node["Restitution"]) comp.restitution = n.as<float>();
                if (auto n = node["Density"]) comp.density = n.as<float>();
            }

            // SphereCollider
            if (YAML::Node node = entityNode["SphereCollider"])
            {
                auto &comp = desEntity.AddComponent<SphereColliderComponent>();
                if (auto n = node["Radius"]) comp.radius = n.as<float>();
                if (auto n = node["Center"]) comp.center = n.as<glm::vec3>();
                if (auto n = node["Friction"]) comp.friction = n.as<float>();
                if (auto n = node["StaticFriction"]) comp.staticFriction = n.as<float>();
                if (auto n = node["Restitution"]) comp.restitution = n.as<float>();
                if (auto n = node["Density"]) comp.density = n.as<float>();
            }

            // CapsuleCollider
            if (YAML::Node node = entityNode["CapsuleCollider"])
            {
                auto &comp = desEntity.AddComponent<CapsuleColliderComponent>();
                if (auto n = node["Radius"]) comp.radius = n.as<float>();
                if (auto n = node["Center"]) comp.center = n.as<glm::vec3>();
                if (auto n = node["Height"]) comp.height = n.as<float>();
                if (auto n = node["Friction"]) comp.friction = n.as<float>();
                if (auto n = node["StaticFriction"]) comp.staticFriction = n.as<float>();
                if (auto n = node["Restitution"]) comp.restitution = n.as<float>();
                if (auto n = node["Density"]) comp.density = n.as<float>();
            }

            // MeshCollider
            if (YAML::Node node = entityNode["MeshCollider"])
            {
                auto &comp = desEntity.AddComponent<MeshColliderComponent>();
                comp.convex = node["Convex"].as<bool>();
                comp.friction = node["Friction"].as<float>();
                comp.staticFriction = node["StaticFriction"].as<float>();
                comp.restitution = node["Restitution"].as<float>();
                comp.density = node["Density"].as<float>();

                // Deserialize vertices
                if (YAML::Node verticesNode = node["Vertices"])
                {
                    comp.vertices.clear();
                    for (const auto &vertexNode : verticesNode)
                    {
                        comp.vertices.push_back(vertexNode.as<glm::vec3>());
                    }
                }

                // Deserialize indices
                if (YAML::Node indicesNode = node["Indices"])
                {
                    comp.indices.clear();
                    for (const auto &indexNode : indicesNode)
                    {
                        comp.indices.push_back(indexNode.as<uint32_t>());
                    }
                }
            }

            // Directional Light component
            if (YAML::Node node = entityNode["DirectionalLight"])
            {
                auto &comp = desEntity.AddComponent<DirectionalLightComponent>();
                if (auto n = node["Color"]) comp.color = n.as<glm::vec4>();
                if (auto n = node["Intensity"]) comp.intensity = n.as<float>();
                if (auto n = node["AngularRadius"]) comp.angularRadius = n.as<float>();
                if (auto n = node["ShadowStrength"]) comp.shadowStrength = n.as<float>();
                if (auto n = node["ShadowMinBias"]) comp.shadowMinBias = n.as<float>();
                if (auto n = node["ShadowMaxBias"]) comp.shadowMaxBias = n.as<float>();
                if (auto n = node["PCFRadius"]) comp.pcfRadius = n.as<float>();
                if (auto n = node["ShadowResolution"]) comp.shadowResolution = n.as<int>();
                if (auto n = node["CascadeShadow"]) comp.cascadeShadow = n.as<bool>();
            }

            // Audio Source
            if (YAML::Node node = entityNode["AudioSource"])
            {
                auto &comp = desEntity.AddComponent<AudioSourceComponent>();
                comp.handle = AssetHandle(node["Handle"].as<uint64_t>());

                if (auto n = node["Volume"]) comp.volume = node["Volume"].as<float>();
                if (auto n = node["Pitch"]) comp.pitch = node["Pitch"].as<float>();
                if (auto n = node["Pan"]) comp.pan = node["Pan"].as<float>();
                if (auto n = node["PlayOnStart"]) comp.playOnStart = node["PlayOnStart"].as<bool>();
                if (auto n = node["Loop"]) comp.loop = node["Loop"].as<bool>();

                comp.dsps.clear();
                if (YAML::Node dspsNode = node["DSPs"]; dspsNode && dspsNode.IsSequence())
                {
                    for (const YAML::Node &dspNode : dspsNode)
                    {
                        AudioSourceComponent::DspSettings dsp;
                        const int dspType = dspNode["Type"] ? dspNode["Type"].as<int>() : 0;
                        dsp.type = static_cast<AudioSourceComponent::DspType>(std::clamp(dspType, 0, 4));
                        if (dspNode["Enabled"]) dsp.enabled = dspNode["Enabled"].as<bool>();

                        switch (dsp.type)
                        {
                            case AudioSourceComponent::DspType::Reverb:
                            if (dspNode["DecayTime"]) dsp.reverbDecayTime = dspNode["DecayTime"].as<float>();
                            if (dspNode["EarlyDelay"]) dsp.reverbEarlyDelay = dspNode["EarlyDelay"].as<float>();
                            if (dspNode["LateDelay"]) dsp.reverbLateDelay = dspNode["LateDelay"].as<float>();
                            if (dspNode["HighFrequencyReference"]) dsp.reverbHighFrequencyReference = dspNode["HighFrequencyReference"].as<float>();
                            if (dspNode["Diffusion"]) dsp.reverbDiffusion = dspNode["Diffusion"].as<float>();
                            if (dspNode["Density"]) dsp.reverbDensity = dspNode["Density"].as<float>();
                            if (dspNode["LowShelfGain"]) dsp.reverbLowShelfGain = dspNode["LowShelfGain"].as<float>();
                            if (dspNode["HighCut"]) dsp.reverbHighCut = dspNode["HighCut"].as<float>();
                            if (dspNode["DryLevel"]) dsp.reverbDryLevel = dspNode["DryLevel"].as<float>();
                            if (dspNode["WetLevel"]) dsp.reverbWetLevel = dspNode["WetLevel"].as<float>();
                            break;
                            case AudioSourceComponent::DspType::Distortion:
                            if (dspNode["DistortionLevel"]) dsp.distortionLevel = dspNode["DistortionLevel"].as<float>();
                            break;
                            case AudioSourceComponent::DspType::Chorus:
                            if (dspNode["Mix"]) dsp.chorusMix = dspNode["Mix"].as<float>();
                            if (dspNode["Rate"]) dsp.chorusRate = dspNode["Rate"].as<float>();
                            if (dspNode["Depth"]) dsp.chorusDepth = dspNode["Depth"].as<float>();
                            break;
                            case AudioSourceComponent::DspType::Compressor:
                            if (dspNode["Threshold"]) dsp.compressorThreshold = dspNode["Threshold"].as<float>();
                            if (dspNode["Ratio"]) dsp.compressorRatio = dspNode["Ratio"].as<float>();
                            if (dspNode["Release"]) dsp.compressorRelease = dspNode["Release"].as<float>();
                            if (dspNode["GainMakeup"]) dsp.compressorGainMakeup = dspNode["GainMakeup"].as<float>();
                            if (dspNode["UseSidechain"]) dsp.compressorUseSidechain = dspNode["UseSidechain"].as<bool>();
                            break;
                            case AudioSourceComponent::DspType::Delay:
                            if (dspNode["DelayMs"]) dsp.delayMs = dspNode["DelayMs"].as<float>();
                            if (dspNode["Feedback"]) dsp.delayFeedback = dspNode["Feedback"].as<float>();
                            break;
                        }

                        comp.dsps.push_back(dsp);
                    }
                }
            }

            // World Environment
            if (YAML::Node node = entityNode["WorldEnvironment"])
            {
                auto &world = desEntity.AddComponent<WorldEnvironment>();
                if (node["HDRHandle"])
                {
                    world.hdrHandle = AssetHandle(node["HDRHandle"].as<uint64_t>());
                }
                if (node["Exposure"])
                {
                    world.exposure = node["Exposure"].as<float>();
                }
                if (node["Gamma"])
                {
                    world.gamma = node["Gamma"].as<float>();
                }
                if (node["Ambient"])
                {
                    world.ambient = node["Ambient"].as<float>();
                }
            }

            // Text Component
            if (YAML::Node node = entityNode["TextComponent"])
            {
                auto &comp = desEntity.AddComponent<TextComponent>();
                if (node["FontHandle"])
                {
                    comp.fontHandle = AssetHandle(node["FontHandle"].as<uint64_t>());
                }
                if (node["Material2DHandle"])
                {
                    comp.material2dHandle = AssetHandle(node["Material2DHandle"].as<uint64_t>());
                }
                if (node["Text"])
                {
                    comp.text = node["Text"].as<std::string>();
                }
                if (node["Color"])
                {
                    comp.color = node["Color"].as<glm::vec4>();
                }
                if (node["Kerning"])
                {
                    comp.kerning = node["Kerning"].as<float>();
                }
                if (node["LineSpacing"])
                {
                    comp.lineSpacing = node["LineSpacing"].as<float>();
                }
                if (node["ScreenSpace"])
                {
                    comp.screenSpace = node["ScreenSpace"].as<bool>();
                }
            }

            if (YAML::Node node = entityNode["WidgetComponent"])
            {
                auto &comp = desEntity.AddComponent<WidgetComponent>();
                if (node["WidgetHandle"])
                {
                    comp.widgetHandle = AssetHandle(node["WidgetHandle"].as<uint64_t>());
                }
            }

            if (YAML::Node node = entityNode["Mesh"])
            {
                auto &comp = desEntity.AddComponent<MeshComponent>();
                if (auto n = node["Handle"])
                {
                    comp.handle = AssetHandle(n.as<uint64_t>());
                }
                if (auto n = node["AnimatorHandle"])
                {
                    comp.runtimeAnimatorHandle = AssetHandle(n.as<uint64_t>());
                }
                if (auto n = node["UniqueAnimator"])
                {
                    // comp.uniqueAnimator = n.as<bool>();
                }

                if (auto overrideMaterialsNode = node["OverrideMaterials"])
                {
                    for (auto materials : overrideMaterialsNode)
                    {
                        int meshIndex = -1;
                        AssetHandle materialHandle = AssetHandle(0);

                        if (auto n = materials["Mesh"]) meshIndex = n.as<int>();
                        if (auto n = materials["MaterialHandle"]) materialHandle = AssetHandle(n.as<uint64_t>());

                        if (meshIndex != -1 || materialHandle != AssetHandle(0))
                            comp.overrideMaterials[meshIndex] = materialHandle;
                    }
                }
            }

            // Script
            if (YAML::Node node = entityNode["Script"])
            {
                auto &sc = desEntity.AddComponent<ScriptComponent>();
                sc.className = node["ClassName"].as<std::string>();

                Ref<ScriptClass> scriptClass = ScriptEngine::GetInstance()->GetEntityClassByName(sc.className);

                if (scriptClass)
                {
                    if (YAML::Node classFieldsNode = node["Fields"])
                    {
                        std::unordered_map<std::string, ScriptInstanceField> instanceFields;
                        for (YAML::Node fieldNode : classFieldsNode)
                        {
                            // Get name and type
                            std::string name = fieldNode["Name"].as<std::string>();
                            ScriptFieldType type = Utils::ScriptFieldTypeFromString(fieldNode["Type"].as<std::string>());

                            auto &classFields = scriptClass->GetFields();
                            auto classFieldIt = classFields.find(name);
                            if (classFieldIt == classFields.end())
                            {
                                continue;
                            }

                            ScriptInstanceField instanceField;
                            instanceField.field = classFieldIt->second;

                            if (instanceField.field.Type != type)
                            {
                                continue;
                            }

                            // Set the value
                            switch (type)
                            {
                                case ScriptFieldType::String: instanceField.SetValue(fieldNode["Value"].as<std::string>()); break;
                                case ScriptFieldType::Float: instanceField.SetValue(fieldNode["Value"].as<float>()); break;
                                case ScriptFieldType::Double: instanceField.SetValue(fieldNode["Value"].as<double>()); break;
                                case ScriptFieldType::Bool: instanceField.SetValue(fieldNode["Value"].as<bool>()); break;
                                case ScriptFieldType::Char: instanceField.SetValue(fieldNode["Value"].as<char16_t>()); break;
                                case ScriptFieldType::Byte: instanceField.SetValue(static_cast<uint8_t>(fieldNode["Value"].as<int>())); break;
                                case ScriptFieldType::SByte: instanceField.SetValue(static_cast<int8_t>(fieldNode["Value"].as<int>())); break;
                                case ScriptFieldType::Short: instanceField.SetValue(fieldNode["Value"].as<int16_t>()); break;
                                case ScriptFieldType::UShort: instanceField.SetValue(fieldNode["Value"].as<uint16_t>()); break;
                                case ScriptFieldType::Int: instanceField.SetValue(fieldNode["Value"].as<int>()); break;
                                case ScriptFieldType::UInt: instanceField.SetValue(fieldNode["Value"].as<uint32_t>()); break;
                                case ScriptFieldType::Long: instanceField.SetValue(fieldNode["Value"].as<int64_t>()); break;
                                case ScriptFieldType::ULong: instanceField.SetValue(fieldNode["Value"].as<uint64_t>()); break;
                                case ScriptFieldType::Vector2: instanceField.SetValue(fieldNode["Value"].as<glm::vec2>()); break;
                                case ScriptFieldType::Vector3: instanceField.SetValue(fieldNode["Value"].as<glm::vec3>()); break;
                                case ScriptFieldType::Vector4: instanceField.SetValue(fieldNode["Value"].as<glm::vec4>()); break;
                                case ScriptFieldType::Quat: instanceField.SetValue(fieldNode["Value"].as<glm::quat>()); break;
                                case ScriptFieldType::Color: instanceField.SetValue(fieldNode["Value"].as<glm::vec4>()); break;
                                case ScriptFieldType::Enum: instanceField.SetValue(fieldNode["Value"].as<int>()); break;
                                case ScriptFieldType::Entity: instanceField.SetValue(fieldNode["Value"].as<uint64_t>()); break;
                                case ScriptFieldType::Asset: instanceField.SetValue(fieldNode["Value"].as<uint64_t>()); break;
                            }

                            instanceFields[name] = instanceField;
                        }

                        // Insert for instances
                        scriptClass->InsertInstanceFields(desEntity.GetUUID(), instanceFields);
                    }
                }
            }
        }

        // attach each node to it's parent
        for (auto &[uuid, e] : desScene->entities)
        {
            Entity entity { e, desScene.get() };

            if (entity.GetParentUUID() != UUID(0))
            {
                Entity parent = SceneManager::GetEntity(desScene.get(), entity.GetParentUUID());
                SceneManager::AddChild(desScene.get(), parent, entity);
            }
        }

        return desScene;
    }
}
