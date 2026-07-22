// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "entity_serializer.hpp"
#include "ignite/serializer/serializer.hpp"
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
#include "ignite/core/application.hpp"
#include "ignite/scene/entity.hpp"
#include "ignite/scene/scene_manager.hpp"

namespace ignite
{
    static void SerializeScriptFieldValue(Serializer &sr, const std::string &name, const ScriptField &fieldDef, const ScriptInstanceField &field)
    {
        switch (fieldDef.Type)
        {
        case ScriptFieldType::Bool:
            sr.AddKeyValue(name.c_str(), field.GetValue<bool>());
            break;
        case ScriptFieldType::Char:
            sr.AddKeyValue(name.c_str(), static_cast<uint16_t>(field.GetValue<char16_t>()));
            break;
        case ScriptFieldType::Byte:
            sr.AddKeyValue(name.c_str(), static_cast<int>(field.GetValue<uint8_t>()));
            break;
        case ScriptFieldType::SByte:
            sr.AddKeyValue(name.c_str(), static_cast<int>(field.GetValue<int8_t>()));
            break;
        case ScriptFieldType::Short:
            sr.AddKeyValue(name.c_str(), static_cast<int>(field.GetValue<int16_t>()));
            break;
        case ScriptFieldType::UShort:
            sr.AddKeyValue(name.c_str(), static_cast<int>(field.GetValue<uint16_t>()));
            break;
        case ScriptFieldType::Int:
            sr.AddKeyValue(name.c_str(), field.GetValue<int32_t>());
            break;
        case ScriptFieldType::UInt:
            sr.AddKeyValue(name.c_str(), field.GetValue<uint32_t>());
            break;
        case ScriptFieldType::Long:
            sr.AddKeyValue(name.c_str(), field.GetValue<int64_t>());
            break;
        case ScriptFieldType::ULong:
            sr.AddKeyValue(name.c_str(), field.GetValue<uint64_t>());
            break;
        case ScriptFieldType::Float:
            sr.AddKeyValue(name.c_str(), field.GetValue<float>());
            break;
        case ScriptFieldType::Double:
            sr.AddKeyValue(name.c_str(), field.GetValue<double>());
            break;
        case ScriptFieldType::String:
            sr.AddKeyValue(name.c_str(), field.GetValue<std::string>());
            break;
        case ScriptFieldType::Vector2:
            sr.AddKeyValue(name.c_str(), field.GetValue<glm::vec2>());
            break;
        case ScriptFieldType::Vector3:
            sr.AddKeyValue(name.c_str(), field.GetValue<glm::vec3>());
            break;
        case ScriptFieldType::Vector4:
        case ScriptFieldType::Color:
            sr.AddKeyValue(name.c_str(), field.GetValue<glm::vec4>());
            break;
        case ScriptFieldType::Quat:
            sr.AddKeyValue(name.c_str(), field.GetValue<glm::quat>());
            break;
        case ScriptFieldType::Entity:
        case ScriptFieldType::Asset:
            sr.AddKeyValue(name.c_str(), field.GetValue<uint64_t>());
            break;
        case ScriptFieldType::Enum:
            sr.AddKeyValue(name.c_str(), field.GetValue<int32_t>());
            break;
        default:
            break;
        }
    }

    static void DeserializeScriptFieldValue(const YAML::Node &fieldsNode, const std::string &name, const ScriptField &fieldDef, ScriptInstanceField &outField)
    {
        if (!fieldsNode[name]) return;
        const auto &valueNode = fieldsNode[name];

        outField.field = fieldDef;
        try
        {
            switch (fieldDef.Type)
            {
            case ScriptFieldType::Bool:
                outField.SetValue<bool>(valueNode.as<bool>());
                break;
            case ScriptFieldType::Char:
                outField.SetValue<char16_t>(static_cast<char16_t>(valueNode.as<uint16_t>()));
                break;
            case ScriptFieldType::Byte:
                outField.SetValue<uint8_t>(static_cast<uint8_t>(valueNode.as<int>()));
                break;
            case ScriptFieldType::SByte:
                outField.SetValue<int8_t>(static_cast<int8_t>(valueNode.as<int>()));
                break;
            case ScriptFieldType::Short:
                outField.SetValue<int16_t>(static_cast<int16_t>(valueNode.as<int>()));
                break;
            case ScriptFieldType::UShort:
                outField.SetValue<uint16_t>(static_cast<uint16_t>(valueNode.as<int>()));
                break;
            case ScriptFieldType::Int:
                outField.SetValue<int32_t>(valueNode.as<int32_t>());
                break;
            case ScriptFieldType::UInt:
                outField.SetValue<uint32_t>(valueNode.as<uint32_t>());
                break;
            case ScriptFieldType::Long:
                outField.SetValue<int64_t>(valueNode.as<int64_t>());
                break;
            case ScriptFieldType::ULong:
                outField.SetValue<uint64_t>(valueNode.as<uint64_t>());
                break;
            case ScriptFieldType::Float:
                outField.SetValue<float>(valueNode.as<float>());
                break;
            case ScriptFieldType::Double:
                outField.SetValue<double>(valueNode.as<double>());
                break;
            case ScriptFieldType::String:
                outField.SetValue<std::string>(valueNode.as<std::string>());
                break;
            case ScriptFieldType::Vector2:
                outField.SetValue<glm::vec2>(valueNode.as<glm::vec2>());
                break;
            case ScriptFieldType::Vector3:
                outField.SetValue<glm::vec3>(valueNode.as<glm::vec3>());
                break;
            case ScriptFieldType::Vector4:
            case ScriptFieldType::Color:
                outField.SetValue<glm::vec4>(valueNode.as<glm::vec4>());
                break;
            case ScriptFieldType::Quat:
                outField.SetValue<glm::quat>(valueNode.as<glm::quat>());
                break;
            case ScriptFieldType::Entity:
            case ScriptFieldType::Asset:
                outField.SetValue<uint64_t>(valueNode.as<uint64_t>());
                break;
            case ScriptFieldType::Enum:
                outField.SetValue<int32_t>(valueNode.as<int32_t>());
                break;
            default:
                break;
            }
        }
        catch (...)
        {
            LOG_WARN("[EntitySerializer] Failed to deserialize field '{}'", name);
        }
    }

    void EntitySerializer::SerializeEntity(Serializer &sr, Entity entity)
    {
        if (!entity.IsValid())
            return;

        const IDComponent &idComp = entity.GetComponent<IDComponent>();

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
                    sr.AddKeyValue("ShadowDistance", comp.shadowDistance);
                    sr.AddKeyValue("ShadowResolution", comp.shadowResolution);
                    sr.AddKeyValue("CascadeShadow", comp.cascadeShadow);
                }
                sr.EndMap();
            }

            // Point Light
            if (entity.HasComponent<PointLightComponent>())
            {
                const PointLightComponent &comp = entity.GetComponent<PointLightComponent>();
                sr.BeginMap("PointLight");
                {
                    sr.AddKeyValue("Color", comp.color);
                    sr.AddKeyValue("Intensity", comp.intensity);
                    sr.AddKeyValue("Range", comp.range);
                    sr.AddKeyValue("Enabled", comp.enabled);
                    sr.AddKeyValue("ConstantAttenuation", comp.constantAttenuation);
                    sr.AddKeyValue("LinearAttenuation", comp.linearAttenuation);
                    sr.AddKeyValue("QuadraticAttenuation", comp.quadraticAttenuation);
                }
                sr.EndMap();
            }

            // Spot Light
            if (entity.HasComponent<SpotLightComponent>())
            {
                const SpotLightComponent &comp = entity.GetComponent<SpotLightComponent>();
                sr.BeginMap("SpotLight");
                {
                    sr.AddKeyValue("Color", comp.color);
                    sr.AddKeyValue("Intensity", comp.intensity);
                    sr.AddKeyValue("Range", comp.range);
                    sr.AddKeyValue("Enabled", comp.enabled);
                    sr.AddKeyValue("ConstantAttenuation", comp.constantAttenuation);
                    sr.AddKeyValue("LinearAttenuation", comp.linearAttenuation);
                    sr.AddKeyValue("QuadraticAttenuation", comp.quadraticAttenuation);
                    sr.AddKeyValue("InnerConeAngle", comp.innerConeAngle);
                    sr.AddKeyValue("OuterConeAngle", comp.outerConeAngle);
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

                    sr.BeginMap("Lens");
                    {
                        sr.AddKeyValue("EnabledDOF", comp.camera.lens.enabledDOF);
                        sr.AddKeyValue("FocalLength", comp.camera.lens.focalLength);
                        sr.AddKeyValue("FocalDistance", comp.camera.lens.focalDistance);
                        sr.AddKeyValue("FStop", comp.camera.lens.fStop);
                        sr.AddKeyValue("FocusRange", comp.camera.lens.focusRange);
                        sr.AddKeyValue("BlurAmount", comp.camera.lens.blurAmount);
                    }
                    sr.EndMap();

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

                        sr.AddKeyValue("TAA", pp.taaProperties.enable);
                        sr.AddKeyValue("TAABlendFactor", pp.taaProperties.blendFactor);
                        sr.AddKeyValue("MSAA", pp.msaaProperties.enable);
                        sr.AddKeyValue("MSAASampleCount", pp.msaaProperties.sampleCount);
                        sr.AddKeyValue("TonemapMode", static_cast<int>(pp.tonemapMode));
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

            // Static Mesh
            if (entity.HasComponent<StaticMeshComponent>())
            {
                const auto &comp = entity.GetComponent<StaticMeshComponent>();
                sr.BeginMap("StaticMesh");
                {
                    sr.AddKeyValue("Handle", static_cast<uint64_t>(comp.handle));

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

            // Skeletal Mesh
            if (entity.HasComponent<SkeletalMeshComponent>())
            {
                const auto &comp = entity.GetComponent<SkeletalMeshComponent>();
                sr.BeginMap("SkeletalMesh");
                {
                    sr.AddKeyValue("Handle", static_cast<uint64_t>(comp.handle));
                    sr.AddKeyValue("AnimatorHandle", static_cast<uint64_t>(comp.runtimeAnimatorHandle));
                    sr.AddKeyValue("UniqueAnimator", comp.uniqueAnimator);

                    sr.BeginSequence("OverrideMaterials");
                    for (const auto &[meshIndex, materialHandle] : comp.overrideMaterials)
                    {
                        sr.BeginMap();
                        sr.AddKeyValue("Mesh", meshIndex);
                        sr.AddKeyValue("MaterialHandle", static_cast<uint64_t>(materialHandle));
                        sr.EndMap();
                    }
                    sr.EndSequence();

                    sr.BeginSequence("SocketAttachments");
                    for (const auto &[socketName, attachedMeshHandle] : comp.socketAttachments)
                    {
                        sr.BeginMap();
                        sr.AddKeyValue("SocketName", socketName);
                        sr.AddKeyValue("MeshHandle", static_cast<uint64_t>(attachedMeshHandle));
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
                    sr.AddKeyValue("LinearVelocity", comp.linearVelocity);
                    sr.AddKeyValue("LinearDamping", comp.linearDamping);
                    sr.AddKeyValue("MaxLinearVelocity", comp.maxLinearVelocity);
                    sr.AddKeyValue("AngularVelocity", comp.angularVelocity);
                    sr.AddKeyValue("AngularDamping", comp.angularDamping);
                    sr.AddKeyValue("MaxAngularVelocity", comp.maxAngularVelocity);
                    sr.AddKeyValue("Restitution", comp.restitution);
                    sr.AddKeyValue("Friction", comp.friction);
                    sr.AddKeyValue("ApplyGyroscopicForce", comp.applyGyroscopicForce);
                    sr.AddKeyValue("AllowSleeping", comp.allowSleeping);
                    sr.AddKeyValue("IsSensor", comp.isSensor);
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
                }
                sr.EndMap();
            }

            // MeshCollider
            if (entity.HasComponent<MeshColliderComponent>())
            {
                const auto &comp = entity.GetComponent<MeshColliderComponent>();
                sr.BeginMap("MeshCollider");
                {
                    sr.BeginSequence("Vertices");
                    for (const auto &vertex : comp.vertices)
                    {
                        sr.AddValue(vertex);
                    }
                    sr.EndSequence();

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
                                sr.AddKeyValue("LowShelfFrequency", dsp.reverbLowShelfGain);
                                sr.AddKeyValue("HighCut", dsp.reverbHighCut);
                                sr.AddKeyValue("DryLevel", dsp.reverbDryLevel);
                                sr.AddKeyValue("WetLevel", dsp.reverbWetLevel);
                                break;
                            case AudioSourceComponent::DspType::Distortion:
                                sr.AddKeyValue("Level", dsp.distortionLevel);
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
                const auto &comp = entity.GetComponent<WorldEnvironment>();
                sr.BeginMap("WorldEnvironment");
                {
                    sr.AddKeyValue("HDRHandle", static_cast<uint64_t>(comp.hdrHandle));
                    sr.AddKeyValue("Exposure", comp.exposure);
                    sr.AddKeyValue("Gamma", comp.gamma);
                    sr.AddKeyValue("Ambient", comp.ambient);
                    sr.AddKeyValue("FogDensity", comp.fogDensity);
                    sr.AddKeyValue("FogColor", comp.fogColor);
                    sr.AddKeyValue("FogStart", comp.fogStart);
                    sr.AddKeyValue("FogEnd", comp.fogEnd);
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

            // Script component
            if (entity.HasComponent<ScriptComponent>())
            {
                const auto &comp = entity.GetComponent<ScriptComponent>();
                sr.BeginMap("Script");
                {
                    sr.AddKeyValue("ClassName", comp.className);

                    ScriptEngine *scriptEngine = ScriptEngine::GetInstance();
                    if (scriptEngine && scriptEngine->IsEntityClassExists(comp.className))
                    {
                        if (Ref<ScriptClass> scriptClass = scriptEngine->GetEntityClassByName(comp.className))
                        {
                            if (auto *instanceFields = scriptClass->GetInstanceFieldsById(entity.GetUUID()))
                            {
                                sr.BeginMap("Fields");
                                for (const auto &[fieldName, instanceField] : *instanceFields)
                                {
                                    if (scriptClass->GetFields().contains(fieldName))
                                    {
                                        const ScriptField &fieldDef = scriptClass->GetFields().at(fieldName);
                                        SerializeScriptFieldValue(sr, fieldName, fieldDef, instanceField);
                                    }
                                }
                                sr.EndMap();
                            }
                        }
                    }
                }
                sr.EndMap();
            }
        }
        sr.EndMap(); // END Entity
    }

    Entity EntitySerializer::DeserializeEntity(const YAML::Node &entityNode, Scene *scene, Project *project)
    {
        UUID uuid = UUID(entityNode["ID"].as<uint64_t>());
        std::string name = entityNode["Name"].as<std::string>();
        EntityType type = EntityTypeFromStringFlags(entityNode["Type"].as<std::string>());

        Entity desEntity = SceneManager::CreateEntity(scene, name, type, uuid);
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

            if (YAML::Node lensNode = node["Lens"])
            {
                if (auto n = lensNode["EnabledDOF"]) comp.camera.lens.enabledDOF = n.as<bool>();
                if (auto n = lensNode["FocalLength"]) comp.camera.lens.focalLength = n.as<float>();
                if (auto n = lensNode["FocalDistance"]) comp.camera.lens.focalDistance = n.as<float>();
                if (auto n = lensNode["FStop"]) comp.camera.lens.fStop = n.as<float>();
                if (auto n = lensNode["FocusRange"]) comp.camera.lens.focusRange = n.as<float>();
                if (auto n = lensNode["BlurAmount"]) comp.camera.lens.blurAmount = n.as<float>();
            }

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

                if (auto n = ppNode["TAA"]) pp.taaProperties.enable = n.as<bool>();
                if (auto n = ppNode["TAABlendFactor"]) pp.taaProperties.blendFactor = n.as<float>();
                if (auto n = ppNode["MSAA"]) pp.msaaProperties.enable = n.as<bool>();
                if (auto n = ppNode["MSAASampleCount"]) pp.msaaProperties.sampleCount = n.as<int>();
                if (auto n = ppNode["TonemapMode"]) pp.tonemapMode = static_cast<TonemapMode>(n.as<int>());
            }
        }

        // Directional Light
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
            if (auto n = node["ShadowDistance"]) comp.shadowDistance = n.as<float>();
            if (auto n = node["ShadowResolution"]) comp.shadowResolution = n.as<uint32_t>();
            if (auto n = node["CascadeShadow"]) comp.cascadeShadow = n.as<bool>();
        }

        // Point Light
        if (YAML::Node node = entityNode["PointLight"])
        {
            auto &comp = desEntity.AddComponent<PointLightComponent>();
            if (auto n = node["Color"]) comp.color = n.as<glm::vec4>();
            if (auto n = node["Intensity"]) comp.intensity = n.as<float>();
            if (auto n = node["Range"]) comp.range = n.as<float>();
            if (auto n = node["Enabled"]) comp.enabled = n.as<bool>();
            if (auto n = node["ConstantAttenuation"]) comp.constantAttenuation = n.as<float>();
            if (auto n = node["LinearAttenuation"]) comp.linearAttenuation = n.as<float>();
            if (auto n = node["QuadraticAttenuation"]) comp.quadraticAttenuation = n.as<float>();
        }

        // Spot Light
        if (YAML::Node node = entityNode["SpotLight"])
        {
            auto &comp = desEntity.AddComponent<SpotLightComponent>();
            if (auto n = node["Color"]) comp.color = n.as<glm::vec4>();
            if (auto n = node["Intensity"]) comp.intensity = n.as<float>();
            if (auto n = node["Range"]) comp.range = n.as<float>();
            if (auto n = node["Enabled"]) comp.enabled = n.as<bool>();
            if (auto n = node["ConstantAttenuation"]) comp.constantAttenuation = n.as<float>();
            if (auto n = node["LinearAttenuation"]) comp.linearAttenuation = n.as<float>();
            if (auto n = node["QuadraticAttenuation"]) comp.quadraticAttenuation = n.as<float>();
            if (auto n = node["InnerConeAngle"]) comp.innerConeAngle = n.as<float>();
            if (auto n = node["OuterConeAngle"]) comp.outerConeAngle = n.as<float>();
        }

        // Sprite 2D component
        if (YAML::Node node = entityNode["Sprite2D"])
        {
            auto &comp = desEntity.AddComponent<Sprite2DComponent>();
            if (auto n = node["MaterialHandle"]) comp.materialHandle = AssetHandle(n.as<uint64_t>());
            if (auto n = node["Handle"]) comp.handle = AssetHandle(n.as<uint64_t>());
            if (auto n = node["Color"]) comp.color = n.as<glm::vec4>();
            if (auto n = node["TilingFactor"]) comp.tilingFactor = n.as<glm::vec2>();
            if (auto n = node["UV0"]) comp.uv0 = n.as<glm::vec2>();
            if (auto n = node["UV1"]) comp.uv1 = n.as<glm::vec2>();
            if (auto n = node["FlipX"]) comp.flipX = n.as<bool>();
            if (auto n = node["FlipY"]) comp.flipY = n.as<bool>();
        }

        if (YAML::Node node = entityNode["Animator2D"])
        {
            auto &comp = desEntity.AddComponent<Animator2DComponent>();
            if (auto n = node["ControllerHandle"]) comp.controllerHandle = AssetHandle(n.as<uint64_t>());
            if (auto n = node["CurrentState"]) comp.currentStateName = n.as<std::string>();
        }

        // Circle 2D component
        if (YAML::Node node = entityNode["Circle2D"])
        {
            auto &comp = desEntity.AddComponent<Circle2DComponent>();
            if (auto n = node["Color"]) comp.color = n.as<glm::vec4>();
            if (auto n = node["Thickness"]) comp.thickness = n.as<float>();
            if (auto n = node["Fade"]) comp.fade = n.as<float>();
        }

        if (YAML::Node node = entityNode["PointLight2D"])
        {
            auto &comp = desEntity.AddComponent<PointLight2DComponent>();
            if (auto n = node["Color"]) comp.color = n.as<glm::vec4>();
            if (auto n = node["Radius"]) comp.radius = n.as<float>();
            if (auto n = node["Intensity"]) comp.intensity = n.as<float>();
            if (auto n = node["Enabled"]) comp.enabled = n.as<bool>();
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
            if (auto n = node["IsAwake"]) comp.isAwake = n.as<bool>();
            if (auto n = node["FixedRotation"]) comp.fixedRotation = n.as<bool>();
            if (auto n = node["AllowFastRotation"]) comp.allowFastRotation = n.as<bool>();
            if (auto n = node["IsEnabled"]) comp.isEnabled = n.as<bool>();
            if (auto n = node["IsEnableSleep"]) comp.isEnableSleep = n.as<bool>();
        }

        // Box collider 2D
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

        // Circle collider 2D
        if (YAML::Node node = entityNode["CircleCollider2D"])
        {
            auto &comp = desEntity.AddComponent<CircleCollider2DComponent>();
            if (auto n = node["Radius"]) comp.radius = n.as<float>();
            if (auto n = node["Center"]) comp.center = n.as<glm::vec2>();
            if (auto n = node["Restitution"]) comp.restitution = n.as<float>();
            if (auto n = node["Friction"]) comp.friction = n.as<float>();
            if (auto n = node["Density"]) comp.density = n.as<float>();
            if (auto n = node["IsSensor"]) comp.isSensor = n.as<bool>();
        }

        // Static Mesh Component
        if (YAML::Node node = entityNode["StaticMesh"])
        {
            auto &comp = desEntity.AddComponent<StaticMeshComponent>();
            if (auto n = node["Handle"]) comp.handle = AssetHandle(n.as<uint64_t>());

            if (YAML::Node matsNode = node["OverrideMaterials"])
            {
                for (YAML::Node matNode : matsNode)
                {
                    uint32_t meshIndex = matNode["Mesh"].as<uint32_t>();
                    AssetHandle materialHandle = AssetHandle(matNode["MaterialHandle"].as<uint64_t>());
                    comp.overrideMaterials[meshIndex] = materialHandle;
                }
            }
        }

        // Skeletal Mesh Component
        if (YAML::Node node = entityNode["SkeletalMesh"])
        {
            auto &comp = desEntity.AddComponent<SkeletalMeshComponent>();
            if (auto n = node["Handle"]) comp.handle = AssetHandle(n.as<uint64_t>());
            if (auto n = node["AnimatorHandle"]) comp.runtimeAnimatorHandle = AssetHandle(n.as<uint64_t>());
            if (auto n = node["UniqueAnimator"]) comp.uniqueAnimator = n.as<bool>();

            if (YAML::Node matsNode = node["OverrideMaterials"])
            {
                for (YAML::Node matNode : matsNode)
                {
                    uint32_t meshIndex = matNode["Mesh"].as<uint32_t>();
                    AssetHandle materialHandle = AssetHandle(matNode["MaterialHandle"].as<uint64_t>());
                    comp.overrideMaterials[meshIndex] = materialHandle;
                }
            }

            if (YAML::Node socketsNode = node["SocketAttachments"])
            {
                for (YAML::Node socketNode : socketsNode)
                {
                    std::string socketName = socketNode["SocketName"].as<std::string>();
                    AssetHandle meshHandle = AssetHandle(socketNode["MeshHandle"].as<uint64_t>());
                    comp.socketAttachments[socketName] = meshHandle;
                }
            }
        }

        // Rigidbody Component
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
            if (auto n = node["LinearVelocity"]) comp.linearVelocity = n.as<glm::vec3>();
            if (auto n = node["LinearDamping"]) comp.linearDamping = n.as<float>();
            if (auto n = node["MaxLinearVelocity"]) comp.maxLinearVelocity = n.as<float>();
            if (auto n = node["AngularVelocity"]) comp.angularVelocity = n.as<glm::vec3>();
            if (auto n = node["AngularDamping"]) comp.angularDamping = n.as<float>();
            if (auto n = node["MaxAngularVelocity"]) comp.maxAngularVelocity = n.as<float>();
            if (auto n = node["Restitution"]) comp.restitution = n.as<float>();
            if (auto n = node["Friction"]) comp.friction = n.as<float>();
            if (auto n = node["ApplyGyroscopicForce"]) comp.applyGyroscopicForce = n.as<bool>();
            if (auto n = node["AllowSleeping"]) comp.allowSleeping = n.as<bool>();
            if (auto n = node["IsSensor"]) comp.isSensor = n.as<bool>();
            if (auto n = node["RetainAcceleration"]) comp.retainAcceleration = n.as<bool>();
            if (auto n = node["GravityFactor"]) comp.gravityFactor = n.as<float>();
            if (auto n = node["CenterMass"]) comp.centerMass = n.as<glm::vec3>();
        }

        // BoxCollider Component
        if (YAML::Node node = entityNode["BoxCollider"])
        {
            auto &comp = desEntity.AddComponent<BoxColliderComponent>();
            if (auto n = node["Scale"]) comp.scale = n.as<glm::vec3>();
            if (auto n = node["Center"]) comp.center = n.as<glm::vec3>();
        }

        // SphereCollider Component
        if (YAML::Node node = entityNode["SphereCollider"])
        {
            auto &comp = desEntity.AddComponent<SphereColliderComponent>();
            if (auto n = node["Radius"]) comp.radius = n.as<float>();
            if (auto n = node["Center"]) comp.center = n.as<glm::vec3>();
        }

        // CapsuleCollider Component
        if (YAML::Node node = entityNode["CapsuleCollider"])
        {
            auto &comp = desEntity.AddComponent<CapsuleColliderComponent>();
            if (auto n = node["Radius"]) comp.radius = n.as<float>();
            if (auto n = node["Center"]) comp.center = n.as<glm::vec3>();
            if (auto n = node["Height"]) comp.height = n.as<float>();
        }

        // MeshCollider Component
        if (YAML::Node node = entityNode["MeshCollider"])
        {
            auto &comp = desEntity.AddComponent<MeshColliderComponent>();
            if (YAML::Node verticesNode = node["Vertices"])
            {
                for (YAML::Node vNode : verticesNode)
                {
                    comp.vertices.push_back(vNode.as<glm::vec3>());
                }
            }
            if (YAML::Node indicesNode = node["Indices"])
            {
                for (YAML::Node iNode : indicesNode)
                {
                    comp.indices.push_back(iNode.as<uint32_t>());
                }
            }
        }

        // AudioSource Component
        if (YAML::Node node = entityNode["AudioSource"])
        {
            auto &comp = desEntity.AddComponent<AudioSourceComponent>();
            if (auto n = node["Handle"]) comp.handle = AssetHandle(n.as<uint64_t>());
            if (auto n = node["Volume"]) comp.volume = n.as<float>();
            if (auto n = node["Pitch"]) comp.pitch = n.as<float>();
            if (auto n = node["Pan"]) comp.pan = n.as<float>();
            if (auto n = node["PlayOnStart"]) comp.playOnStart = n.as<bool>();
            if (auto n = node["Loop"]) comp.loop = n.as<bool>();

            if (YAML::Node dspsNode = node["DSPs"])
            {
                for (YAML::Node dspNode : dspsNode)
                {
                    AudioSourceComponent::DspSettings settings;
                    if (auto n = dspNode["Type"]) settings.type = static_cast<AudioSourceComponent::DspType>(n.as<int>());
                    if (auto n = dspNode["Enabled"]) settings.enabled = n.as<bool>();

                    switch (settings.type)
                    {
                        case AudioSourceComponent::DspType::Reverb:
                            if (auto n = dspNode["DecayTime"]) settings.reverbDecayTime = n.as<float>();
                            if (auto n = dspNode["EarlyDelay"]) settings.reverbEarlyDelay = n.as<float>();
                            if (auto n = dspNode["LateDelay"]) settings.reverbLateDelay = n.as<float>();
                            if (auto n = dspNode["HighFrequencyReference"]) settings.reverbHighFrequencyReference = n.as<float>();
                            if (auto n = dspNode["Diffusion"]) settings.reverbDiffusion = n.as<float>();
                            if (auto n = dspNode["Density"]) settings.reverbDensity = n.as<float>();
                            if (auto n = dspNode["LowShelfFrequency"]) settings.reverbLowShelfGain = n.as<float>();
                            if (auto n = dspNode["HighCut"]) settings.reverbHighCut = n.as<float>();
                            if (auto n = dspNode["DryLevel"]) settings.reverbDryLevel = n.as<float>();
                            if (auto n = dspNode["WetLevel"]) settings.reverbWetLevel = n.as<float>();
                            break;
                        case AudioSourceComponent::DspType::Distortion:
                            if (auto n = dspNode["Level"]) settings.distortionLevel = n.as<float>();
                            break;
                        case AudioSourceComponent::DspType::Chorus:
                            if (auto n = dspNode["Mix"]) settings.chorusMix = n.as<float>();
                            if (auto n = dspNode["Rate"]) settings.chorusRate = n.as<float>();
                            if (auto n = dspNode["Depth"]) settings.chorusDepth = n.as<float>();
                            break;
                        case AudioSourceComponent::DspType::Compressor:
                            if (auto n = dspNode["Threshold"]) settings.compressorThreshold = n.as<float>();
                            if (auto n = dspNode["Ratio"]) settings.compressorRatio = n.as<float>();
                            if (auto n = dspNode["Release"]) settings.compressorRelease = n.as<float>();
                            if (auto n = dspNode["GainMakeup"]) settings.compressorGainMakeup = n.as<float>();
                            if (auto n = dspNode["UseSidechain"]) settings.compressorUseSidechain = n.as<bool>();
                            break;
                        case AudioSourceComponent::DspType::Delay:
                            if (auto n = dspNode["DelayMs"]) settings.delayMs = n.as<float>();
                            if (auto n = dspNode["Feedback"]) settings.delayFeedback = n.as<float>();
                            break;
                    }
                    comp.dsps.push_back(settings);
                }
            }
        }

        // WorldEnvironment
        if (YAML::Node node = entityNode["WorldEnvironment"])
        {
            auto &comp = desEntity.AddComponent<WorldEnvironment>();
            if (auto n = node["HDRHandle"]) comp.hdrHandle = AssetHandle(n.as<uint64_t>());
            if (auto n = node["Exposure"]) comp.exposure = n.as<float>();
            if (auto n = node["Gamma"]) comp.gamma = n.as<float>();
            if (auto n = node["Ambient"]) comp.ambient = n.as<float>();
            if (auto n = node["FogDensity"]) comp.fogDensity = n.as<float>();
            if (auto n = node["FogColor"]) comp.fogColor = n.as<glm::vec4>();
            if (auto n = node["FogStart"]) comp.fogStart = n.as<float>();
            if (auto n = node["FogEnd"]) comp.fogEnd = n.as<float>();
        }

        // Text Component
        if (YAML::Node node = entityNode["TextComponent"])
        {
            auto &comp = desEntity.AddComponent<TextComponent>();
            if (auto n = node["FontHandle"]) comp.fontHandle = AssetHandle(n.as<uint64_t>());
            if (auto n = node["Material2DHandle"]) comp.material2dHandle = AssetHandle(n.as<uint64_t>());
            if (auto n = node["Text"]) comp.text = n.as<std::string>();
            if (auto n = node["Color"]) comp.color = n.as<glm::vec4>();
            if (auto n = node["Kerning"]) comp.kerning = n.as<float>();
            if (auto n = node["LineSpacing"]) comp.lineSpacing = n.as<float>();
            if (auto n = node["ScreenSpace"]) comp.screenSpace = n.as<bool>();
        }

        // Widget Component
        if (YAML::Node node = entityNode["WidgetComponent"])
        {
            auto &comp = desEntity.AddComponent<WidgetComponent>();
            if (auto n = node["WidgetHandle"]) comp.widgetHandle = AssetHandle(n.as<uint64_t>());
        }

        // Script component
        if (YAML::Node node = entityNode["Script"])
        {
            auto &comp = desEntity.AddComponent<ScriptComponent>();
            if (auto n = node["ClassName"]) comp.className = n.as<std::string>();
            else if (auto n = node["Class"]) comp.className = n.as<std::string>();

            ScriptEngine *scriptEngine = ScriptEngine::GetInstance();
            if (scriptEngine && scriptEngine->IsEntityClassExists(comp.className))
            {
                if (Ref<ScriptClass> scriptClass = scriptEngine->GetEntityClassByName(comp.className))
                {
                    if (YAML::Node fieldsNode = node["Fields"])
                    {
                        std::unordered_map<std::string, ScriptInstanceField> instanceFields;
                        const auto &classFields = scriptClass->GetFields();

                        for (auto it = fieldsNode.begin(); it != fieldsNode.end(); ++it)
                        {
                            std::string fieldName = it->first.as<std::string>();
                            if (classFields.contains(fieldName))
                            {
                                const ScriptField &fieldDef = classFields.at(fieldName);
                                ScriptInstanceField instanceField;
                                DeserializeScriptFieldValue(fieldsNode, fieldName, fieldDef, instanceField);
                                instanceFields[fieldName] = instanceField;
                            }
                        }

                        scriptClass->InsertInstanceFields(desEntity.GetUUID(), instanceFields);
                    }
                }
            }
        }

        return desEntity;
    }
}
