// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"
#include "agent_controller.hpp"

#include "ignite/asset/asset_manager.hpp"
#include "ignite/asset/asset_importer.hpp"
#include "ignite/audio/fmod_sound.hpp"
#include "ignite/core/application.hpp"
#include "ignite/core/input/input_system.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/graphics/objects/material.hpp"
#include "ignite/graphics/render_target.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/renderer/scene_renderer.hpp"
#include "ignite/graphics/gi/gi_baker.hpp"
#include "ignite/graphics/texture.hpp"
#include "ignite/graphics/ui/game_ui_system.hpp"
#include "ignite/graphics/ui/widget.hpp"
#include "ignite/graphics/ui/widget_button.hpp"
#include "ignite/graphics/ui/widget_canvas.hpp"
#include "ignite/graphics/ui/widget_label.hpp"
#include "ignite/net/network_manager.hpp"
#include "ignite/physics/2d/physics_2d_component.hpp"
#include "ignite/physics/3d/physics_3d.hpp"
#include "ignite/project/game_packager.hpp"
#include "ignite/project/project.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/scene/editor_camera.hpp"
#include "ignite/scene/entity.hpp"
#include "ignite/scene/icamera.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/scene/scene_manager.hpp"
#include "ignite/scripting/lua/lua_script_runtime.hpp"
#include "ignite/serializer/scene_serializer.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/packing.hpp>
#include <glm/gtx/quaternion.hpp>
#include <stb_image_write.h>

#include <chrono>
#include <fstream>
#include <sstream>

namespace ignite
{
    std::atomic<bool> AgentController::s_AutomationActive{ false };

    namespace
    {
        glm::vec2 ReadJsonVec2(const nlohmann::ordered_json &j, const glm::vec2 &def = glm::vec2(0.0f))
        {
            if (j.is_array() && j.size() >= 2)
                return { j[0].get<float>(), j[1].get<float>() };
            if (j.is_object())
                return { j.value("x", def.x), j.value("y", def.y) };
            return def;
        }

        glm::vec3 ReadJsonVec3(const nlohmann::ordered_json &j, const glm::vec3 &def = glm::vec3(0.0f))
        {
            if (j.is_array() && j.size() >= 3)
                return { j[0].get<float>(), j[1].get<float>(), j[2].get<float>() };
            if (j.is_object())
                return { j.value("x", def.x), j.value("y", def.y), j.value("z", def.z) };
            return def;
        }

        glm::vec4 ReadJsonVec4(const nlohmann::ordered_json &j, const glm::vec4 &def = glm::vec4(1.0f))
        {
            if (j.is_array() && j.size() >= 4)
                return { j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>() };
            if (j.is_object())
                return { j.value("x", def.x), j.value("y", def.y), j.value("z", def.z), j.value("w", def.w) };
            return def;
        }

        nlohmann::ordered_json Vec3ToJson(const glm::vec3 &v)
        {
            return nlohmann::ordered_json::array({ v.x, v.y, v.z });
        }

        nlohmann::ordered_json Vec4ToJson(const glm::vec4 &v)
        {
            return nlohmann::ordered_json::array({ v.x, v.y, v.z, v.w });
        }

        const char *SceneStateName(ESceneState st)
        {
            switch (st)
            {
            case ESceneState::Stop: return "Stop";
            case ESceneState::Play: return "Play";
            case ESceneState::Simulate: return "Simulate";
            case ESceneState::Paused: return "Paused";
            default: return "None";
            }
        }

        Entity ResolveTargetEntity(Scene *scene, const nlohmann::ordered_json &cmd, const char *key = "target")
        {
            if (!scene || !scene->registry)
                return {};

            const char *resolvedKey = key;
            if (!cmd.contains(resolvedKey) && std::string_view(key) == "target" && cmd.contains("entity"))
            {
                resolvedKey = "entity";
            }

            if (cmd.contains(resolvedKey))
            {
                const auto &val = cmd[resolvedKey];
                if (val.is_number_unsigned() || val.is_number_integer())
                    return SceneManager::GetEntity(scene, UUID(val.get<uint64_t>()));
                if (val.is_string())
                {
                    const std::string s = val.get<std::string>();
                    // Try exact entity name first
                    if (Entity byName = SceneManager::GetEntity(scene, s))
                        return byName;
                    // Fallback: numeric UUID string
                    try
                    {
                        const uint64_t raw = std::stoull(s);
                        if (Entity byId = SceneManager::GetEntity(scene, UUID(raw)))
                            return byId;
                    }
                    catch (...) {}
                }
            }

            if (cmd.contains("id") && (cmd["id"].is_number_unsigned() || cmd["id"].is_number_integer()))
                return SceneManager::GetEntity(scene, UUID(cmd["id"].get<uint64_t>()));

            if (cmd.contains("name") && cmd["name"].is_string())
                return SceneManager::GetEntity(scene, cmd["name"].get<std::string>());

            return {};
        }

        std::vector<std::string> ListEntityComponents(Entity e)
        {
            std::vector<std::string> comps;
            if (!e) return comps;
            if (e.HasComponent<TransformComponent>()) comps.push_back("Transform");
            if (e.HasComponent<RenderingComponent>()) comps.push_back("Rendering");
            if (e.HasComponent<StaticMeshComponent>()) comps.push_back("StaticMesh");
            if (e.HasComponent<SkeletalMeshComponent>()) comps.push_back("SkeletalMesh");
            if (e.HasComponent<CameraComponent>()) comps.push_back("Camera");
            if (e.HasComponent<DirectionalLightComponent>()) comps.push_back("DirectionalLight");
            if (e.HasComponent<PointLightComponent>()) comps.push_back("PointLight");
            if (e.HasComponent<SpotLightComponent>()) comps.push_back("SpotLight");
            if (e.HasComponent<WorldEnvironment>()) comps.push_back("WorldEnvironment");
            if (e.HasComponent<RigidbodyComponent>()) comps.push_back("Rigidbody");
            if (e.HasComponent<Rigidbody2DComponent>()) comps.push_back("Rigidbody2D");
            if (e.HasComponent<CharacterControllerComponent>()) comps.push_back("CharacterController");
            if (e.HasComponent<BoxColliderComponent>()) comps.push_back("BoxCollider");
            if (e.HasComponent<SphereColliderComponent>()) comps.push_back("SphereCollider");
            if (e.HasComponent<CapsuleColliderComponent>()) comps.push_back("CapsuleCollider");
            if (e.HasComponent<BoxCollider2DComponent>()) comps.push_back("BoxCollider2D");
            if (e.HasComponent<CircleCollider2DComponent>()) comps.push_back("CircleCollider2D");
            if (e.HasComponent<Sprite2DComponent>()) comps.push_back("Sprite2D");
            if (e.HasComponent<Circle2DComponent>()) comps.push_back("Circle2D");
            if (e.HasComponent<TextComponent>()) comps.push_back("Text");
            if (e.HasComponent<WidgetComponent>()) comps.push_back("Widget");
            if (e.HasComponent<AudioSourceComponent>()) comps.push_back("AudioSource");
            if (e.HasComponent<LuaScriptComponent>()) comps.push_back("LuaScript");
            if (e.HasComponent<ScriptComponent>()) comps.push_back("Script");
            if (e.HasComponent<NetworkIdentityComponent>()) comps.push_back("NetworkIdentity");
            if (e.HasComponent<PhysicsJointComponent>()) comps.push_back("PhysicsJoint");
            if (e.HasComponent<ParticleSystemComponent>()) comps.push_back("ParticleSystem");
            if (e.HasComponent<IrradianceVolumeComponent>()) comps.push_back("IrradianceVolume");
            if (e.HasComponent<ReflectionProbeComponent>()) comps.push_back("ReflectionProbe");
            return comps;
        }

        nlohmann::ordered_json SerializeEntitySummary(Entity e, bool detailed)
        {
            nlohmann::ordered_json j;
            j["id"] = static_cast<uint64_t>(e.GetUUID());
            j["name"] = e.GetName();
            j["parent"] = static_cast<uint64_t>(e.GetParentUUID());
            j["components"] = ListEntityComponents(e);

            if (e.HasComponent<TransformComponent>())
            {
                const auto &tc = e.GetComponent<TransformComponent>();
                j["position"] = Vec3ToJson(tc.local.translation);
                j["rotation"] = Vec3ToJson(glm::degrees(glm::eulerAngles(tc.local.rotation)));
                j["scale"] = Vec3ToJson(tc.local.scale);
                j["world_position"] = Vec3ToJson(tc.world.translation);
            }

            if (!detailed)
                return j;

            if (e.HasComponent<RenderingComponent>())
            {
                const auto &rc = e.GetComponent<RenderingComponent>();
                j["Rendering"] = {
                    { "visible", rc.visible },
                    { "mobility", rc.mobility == EntityMobility::Static ? "Static" : "Movable" },
                    { "cast_shadow", rc.castShadow },
                    { "receive_shadow", rc.receiveShadow },
                    { "contribute_gi", rc.contributeGI },
                    { "receive_gi", rc.receiveGI }
                };
            }

            if (e.HasComponent<StaticMeshComponent>())
            {
                const auto &smc = e.GetComponent<StaticMeshComponent>();
                j["StaticMesh"] = { { "asset_handle", static_cast<uint64_t>(smc.handle) } };
            }

            if (e.HasComponent<CameraComponent>())
            {
                const auto &cc = e.GetComponent<CameraComponent>();
                j["Camera"] = {
                    { "primary", cc.primary },
                    { "fov", cc.camera.fov },
                    { "near_plane", cc.camera.nearPlane },
                    { "far_plane", cc.camera.farPlane },
                    { "ortho_size", cc.camera.orthoSize },
                    { "exposure", cc.camera.lens.exposure },
                    { "gamma", cc.camera.lens.gamma },
                    { "ssao", cc.camera.postProcessing.enableSSAO },
                    { "ao_radius", cc.camera.postProcessing.aoRadius },
                    { "ao_intensity", cc.camera.postProcessing.aoIntensity },
                    { "tonemap_mode", static_cast<int>(cc.camera.postProcessing.tonemapMode) }
                };
            }

            if (e.HasComponent<DirectionalLightComponent>())
            {
                const auto &dl = e.GetComponent<DirectionalLightComponent>();
                j["DirectionalLight"] = {
                    { "color", Vec4ToJson(dl.color) },
                    { "intensity", dl.intensity },
                    { "shadow_distance", dl.shadowDistance },
                    { "shadow_strength", dl.shadowStrength },
                    { "shadow_min_bias", dl.shadowMinBias },
                    { "shadow_max_bias", dl.shadowMaxBias },
                    { "pcf_radius", dl.pcfRadius },
                    { "shadow_resolution", dl.shadowResolution },
                    { "cascade_shadow", dl.cascadeShadow },
                    { "soft_shadows", dl.softShadows }
                };
            }

            if (e.HasComponent<PointLightComponent>())
            {
                const auto &pl = e.GetComponent<PointLightComponent>();
                j["PointLight"] = {
                    { "color", Vec4ToJson(pl.color) },
                    { "intensity", pl.intensity },
                    { "range", pl.range },
                    { "enabled", pl.enabled },
                    { "bake_indirect", pl.bakeIndirect },
                    { "cast_shadow", pl.castShadow },
                    { "bake_static_shadow", pl.bakeStaticShadow },
                    { "soft_shadows", pl.softShadows },
                    { "shadow_strength", pl.shadowStrength },
                    { "shadow_bias", pl.shadowBias },
                    { "pcf_radius", pl.pcfRadius }
                };
            }

            if (e.HasComponent<SpotLightComponent>())
            {
                const auto &sl = e.GetComponent<SpotLightComponent>();
                j["SpotLight"] = {
                    { "color", Vec4ToJson(sl.color) },
                    { "intensity", sl.intensity },
                    { "range", sl.range },
                    { "enabled", sl.enabled },
                    { "bake_indirect", sl.bakeIndirect },
                    { "inner_cone_angle", sl.innerConeAngle },
                    { "outer_cone_angle", sl.outerConeAngle },
                    { "cast_shadow", sl.castShadow },
                    { "bake_static_shadow", sl.bakeStaticShadow },
                    { "soft_shadows", sl.softShadows },
                    { "shadow_strength", sl.shadowStrength },
                    { "shadow_bias", sl.shadowBias },
                    { "pcf_radius", sl.pcfRadius }
                };
            }

            if (e.HasComponent<RigidbodyComponent>())
            {
                const auto &rb = e.GetComponent<RigidbodyComponent>();
                j["Rigidbody"] = {
                    { "body_type", static_cast<int>(rb.bodyType) },
                    { "use_gravity", rb.useGravity },
                    { "mass", rb.mass },
                    { "linear_velocity", Vec3ToJson(rb.linearVelocity) },
                    { "angular_velocity", Vec3ToJson(rb.angularVelocity) }
                };
            }

            if (e.HasComponent<LuaScriptComponent>())
            {
                const auto &ls = e.GetComponent<LuaScriptComponent>();
                nlohmann::ordered_json fieldsJson = nlohmann::ordered_json::object();
                for (const auto &[name, field] : ls.fields)
                {
                    nlohmann::ordered_json fj;
                    fj["type"] = ScriptFieldTypeToString(field.type);
                    std::visit([&fj](const auto &v)
                    {
                        using T = std::decay_t<decltype(v)>;
                        if constexpr (std::is_same_v<T, std::monostate>) fj["value"] = nullptr;
                        else if constexpr (std::is_same_v<T, glm::vec2>) fj["value"] = nlohmann::ordered_json::array({ v.x, v.y });
                        else if constexpr (std::is_same_v<T, glm::vec3>) fj["value"] = Vec3ToJson(v);
                        else if constexpr (std::is_same_v<T, glm::vec4>) fj["value"] = Vec4ToJson(v);
                        else fj["value"] = v;
                    }, field.value);
                    fieldsJson[name] = fj;
                }
                j["LuaScript"] = {
                    { "script_handle", static_cast<uint64_t>(ls.scriptHandle) },
                    { "script_path", ls.scriptPath.generic_string() },
                    { "fields", fieldsJson }
                };
            }

            if (e.HasComponent<NetworkIdentityComponent>())
            {
                const auto &ni = e.GetComponent<NetworkIdentityComponent>();
                j["NetworkIdentity"] = {
                    { "net_id", ni.netId },
                    { "owner_client", ni.ownerClient },
                    { "is_local_player", ni.isLocalPlayer },
                    { "replicate", ni.replicate },
                    { "sync_mask", ni.syncComponentsMask }
                };
            }

            if (e.HasComponent<TextComponent>())
            {
                const auto &tc = e.GetComponent<TextComponent>();
                j["Text"] = {
                    { "text", tc.text },
                    { "color", Vec4ToJson(tc.color) },
                    { "font_size", tc.fontSize },
                    { "screen_space", tc.screenSpace },
                    { "billboard", tc.billboard },
                    { "center_align", tc.centerAlign }
                };
            }

            if (e.HasComponent<WorldEnvironment>())
            {
                const auto &we = e.GetComponent<WorldEnvironment>();
                j["WorldEnvironment"] = {
                    { "sky_type", we.skyType == SkyType::ProceduralSky ? "procedural" : "hdri" },
                    { "enable_ibl", we.enableIBL },
                    { "exposure", we.exposure },
                    { "gamma", we.gamma },
                    { "ambient", we.ambient },
                    { "fog_density", we.fogDensity },
                    { "fog_color", Vec4ToJson(we.fogColor) }
                };
            }

            if (e.HasComponent<IrradianceVolumeComponent>())
            {
                const auto &iv = e.GetComponent<IrradianceVolumeComponent>();
                j["IrradianceVolume"] = {
                    { "extents", Vec3ToJson(iv.extents) },
                    { "spacing", iv.spacing },
                    { "bounces", iv.bounces },
                    { "capture_resolution", iv.captureResolution },
                    { "runtime_mode", iv.runtimeMode },
                    { "intensity", iv.intensity },
                    { "priority", iv.priority },
                    { "baked_asset_path", iv.bakedAssetPath },
                    { "bake_hash", iv.bakeHash }
                };
            }

            if (e.HasComponent<ReflectionProbeComponent>())
            {
                const auto &rp = e.GetComponent<ReflectionProbeComponent>();
                j["ReflectionProbe"] = {
                    { "influence_extents", Vec3ToJson(rp.influenceExtents) },
                    { "projection_extents", Vec3ToJson(rp.projectionExtents) },
                    { "capture_offset", Vec3ToJson(rp.captureOffset) },
                    { "blend_distance", rp.blendDistance },
                    { "resolution", rp.resolution },
                    { "priority", rp.priority },
                    { "intensity", rp.intensity },
                    { "normalize", rp.normalize },
                    { "normalization_strength", rp.normalizationStrength },
                    { "box_projection", rp.boxProjection },
                    { "exclude_radius", rp.excludeRadius },
                    { "baked_asset_path", rp.bakedAssetPath },
                    { "bake_hash", rp.bakeHash }
                };
            }

            return j;
        }

        bool ApplyComponentProperties(Scene *scene, Entity e, const std::string &type, const nlohmann::ordered_json &props, std::string &outError)
        {
            if (!e)
            {
                outError = "Invalid entity";
                return false;
            }

            if (type == "Transform")
            {
                auto &tc = e.GetComponent<TransformComponent>();
                if (props.contains("position")) tc.local.translation = ReadJsonVec3(props["position"], tc.local.translation);
                if (props.contains("rotation")) tc.local.rotation = glm::quat(glm::radians(ReadJsonVec3(props["rotation"])));
                if (props.contains("scale")) tc.local.scale = ReadJsonVec3(props["scale"], tc.local.scale);
                tc.dirty = true;
                if (scene)
                    scene->UpdateTransformRecursive(e, glm::mat4(1.0f));
                return true;
            }

            if (type == "Rendering")
            {
                auto &rc = e.HasComponent<RenderingComponent>() ? e.GetComponent<RenderingComponent>() : e.AddComponent<RenderingComponent>();
                if (props.contains("visible")) rc.visible = props["visible"].get<bool>();
                if (props.contains("mobility"))
                {
                    if (props["mobility"].is_string())
                    {
                        const std::string m = props["mobility"].get<std::string>();
                        rc.mobility = (m == "Movable" || m == "movable" || m == "Dynamic" || m == "dynamic")
                            ? EntityMobility::Movable : EntityMobility::Static;
                    }
                    else if (props["mobility"].is_number_integer())
                    {
                        rc.mobility = static_cast<EntityMobility>(std::clamp(props["mobility"].get<int>(), 0, 1));
                    }
                }
                if (props.contains("cast_shadow")) rc.castShadow = props["cast_shadow"].get<bool>();
                if (props.contains("receive_shadow")) rc.receiveShadow = props["receive_shadow"].get<bool>();
                if (props.contains("contribute_gi")) rc.contributeGI = props["contribute_gi"].get<bool>();
                if (props.contains("receive_gi")) rc.receiveGI = props["receive_gi"].get<bool>();
                return true;
            }

            if (type == "StaticMesh")
            {
                auto &smc = e.HasComponent<StaticMeshComponent>() ? e.GetComponent<StaticMeshComponent>() : e.AddComponent<StaticMeshComponent>();
                if (props.contains("primitive") && props["primitive"].is_string())
                {
                    EMeshType meshType = EMeshType::CUBE;
                    if (Renderer::ParseMeshType(props["primitive"].get<std::string>(), meshType))
                        smc.handle = Renderer::GetDefaultMeshHandle(meshType);
                }
                else if (props.contains("asset_handle"))
                    smc.handle = AssetHandle(props["asset_handle"].get<uint64_t>());
                if (props.contains("material_handle"))
                {
                    smc.SetMaterialOverride(0, AssetHandle(props["material_handle"].get<uint64_t>()));
                }
                return true;
            }

            if (type == "SkeletalMesh")
            {
                auto &smc = e.HasComponent<SkeletalMeshComponent>() ? e.GetComponent<SkeletalMeshComponent>() : e.AddComponent<SkeletalMeshComponent>();
                if (props.contains("asset_handle"))
                    smc.handle = AssetHandle(props["asset_handle"].get<uint64_t>());
                if (props.contains("animator_handle"))
                    smc.runtimeAnimatorHandle = AssetHandle(props["animator_handle"].get<uint64_t>());
                if (props.contains("unique_animator"))
                    smc.uniqueAnimator = props["unique_animator"].get<bool>();
                return true;
            }

            if (type == "Camera")
            {
                auto &cc = e.HasComponent<CameraComponent>() ? e.GetComponent<CameraComponent>() : e.AddComponent<CameraComponent>();
                if (props.contains("primary")) cc.primary = props["primary"].get<bool>();
                if (props.contains("fov")) cc.camera.fov = props["fov"].get<float>();
                if (props.contains("near_plane")) cc.camera.nearPlane = props["near_plane"].get<float>();
                if (props.contains("far_plane")) cc.camera.farPlane = props["far_plane"].get<float>();
                if (props.contains("ortho_size")) cc.camera.orthoSize = props["ortho_size"].get<float>();
                if (props.contains("exposure")) cc.camera.lens.exposure = props["exposure"].get<float>();
                if (props.contains("gamma")) cc.camera.lens.gamma = props["gamma"].get<float>();
                if (props.contains("ssao")) cc.camera.postProcessing.enableSSAO = props["ssao"].get<bool>();
                if (props.contains("enable_ssao")) cc.camera.postProcessing.enableSSAO = props["enable_ssao"].get<bool>();
                if (props.contains("ao_mode"))
                {
                    if (props["ao_mode"].is_string())
                    {
                        const std::string m = props["ao_mode"].get<std::string>();
                        if (m == "off") cc.camera.postProcessing.aoMode = AOMode::Off;
                        else if (m == "ssao") cc.camera.postProcessing.aoMode = AOMode::SSAO;
                        else if (m == "gtao" || m == "xegtao") cc.camera.postProcessing.aoMode = AOMode::GTAO;
                    }
                    else if (props["ao_mode"].is_number_integer())
                    {
                        cc.camera.postProcessing.aoMode = static_cast<AOMode>(std::clamp(props["ao_mode"].get<int>(), 0, 2));
                    }
                }
                if (props.contains("ao_radius")) cc.camera.postProcessing.aoRadius = props["ao_radius"].get<float>();
                if (props.contains("ao_intensity")) cc.camera.postProcessing.aoIntensity = props["ao_intensity"].get<float>();
                if (props.contains("ao_bias")) cc.camera.postProcessing.aoBias = props["ao_bias"].get<float>();
                if (props.contains("ao_power")) cc.camera.postProcessing.aoPower = props["ao_power"].get<float>();
                if (props.contains("tonemap_mode"))
                {
                    if (props["tonemap_mode"].is_string())
                    {
                        const std::string tm = props["tonemap_mode"].get<std::string>();
                        if (tm == "reinhard") cc.camera.postProcessing.tonemapMode = TonemapMode::Reinhard;
                        else if (tm == "uncharted2") cc.camera.postProcessing.tonemapMode = TonemapMode::Uncharted2;
                        else if (tm == "filmic" || tm == "aces" || tm == "aces_filmic") cc.camera.postProcessing.tonemapMode = TonemapMode::Filmic;
                    }
                    else if (props["tonemap_mode"].is_number_integer())
                    {
                        cc.camera.postProcessing.tonemapMode = static_cast<TonemapMode>(props["tonemap_mode"].get<int>());
                    }
                }
                if (props.contains("projection_type") && props["projection_type"].is_string())
                {
                    const std::string p = props["projection_type"].get<std::string>();
                    cc.camera.projectionType = (p == "orthographic" || p == "ortho") ? ProjectionType::Orthographic : ProjectionType::Perspective;
                }
                cc.dirty = true;
                return true;
            }

            if (type == "DirectionalLight")
            {
                auto &dl = e.HasComponent<DirectionalLightComponent>() ? e.GetComponent<DirectionalLightComponent>() : e.AddComponent<DirectionalLightComponent>();
                if (props.contains("color")) dl.color = ReadJsonVec4(props["color"], dl.color);
                if (props.contains("intensity")) dl.intensity = props["intensity"].get<float>();
                if (props.contains("shadow_distance")) dl.shadowDistance = props["shadow_distance"].get<float>();
                if (props.contains("shadow_strength")) dl.shadowStrength = props["shadow_strength"].get<float>();
                if (props.contains("shadow_min_bias")) dl.shadowMinBias = props["shadow_min_bias"].get<float>();
                if (props.contains("shadow_max_bias")) dl.shadowMaxBias = props["shadow_max_bias"].get<float>();
                if (props.contains("pcf_radius")) dl.pcfRadius = props["pcf_radius"].get<float>();
                if (props.contains("shadow_resolution")) dl.shadowResolution = props["shadow_resolution"].get<uint32_t>();
                if (props.contains("cascade_shadow")) dl.cascadeShadow = props["cascade_shadow"].get<bool>();
                if (props.contains("soft_shadows")) dl.softShadows = props["soft_shadows"].get<bool>();
                if (props.contains("bake_indirect")) dl.bakeIndirect = props["bake_indirect"].get<bool>();
                return true;
            }

            if (type == "PointLight")
            {
                auto &pl = e.HasComponent<PointLightComponent>() ? e.GetComponent<PointLightComponent>() : e.AddComponent<PointLightComponent>();
                if (props.contains("color")) pl.color = ReadJsonVec4(props["color"], pl.color);
                if (props.contains("intensity")) pl.intensity = props["intensity"].get<float>();
                if (props.contains("range")) pl.range = props["range"].get<float>();
                if (props.contains("enabled")) pl.enabled = props["enabled"].get<bool>();
                if (props.contains("bake_indirect")) pl.bakeIndirect = props["bake_indirect"].get<bool>();
                if (props.contains("cast_shadow")) pl.castShadow = props["cast_shadow"].get<bool>();
                if (props.contains("bake_static_shadow")) pl.bakeStaticShadow = props["bake_static_shadow"].get<bool>();
                if (props.contains("soft_shadows")) pl.softShadows = props["soft_shadows"].get<bool>();
                if (props.contains("shadow_strength")) pl.shadowStrength = props["shadow_strength"].get<float>();
                if (props.contains("shadow_bias")) pl.shadowBias = props["shadow_bias"].get<float>();
                if (props.contains("pcf_radius")) pl.pcfRadius = props["pcf_radius"].get<float>();
                return true;
            }

            if (type == "SpotLight")
            {
                auto &sl = e.HasComponent<SpotLightComponent>() ? e.GetComponent<SpotLightComponent>() : e.AddComponent<SpotLightComponent>();
                if (props.contains("color")) sl.color = ReadJsonVec4(props["color"], sl.color);
                if (props.contains("intensity")) sl.intensity = props["intensity"].get<float>();
                if (props.contains("range")) sl.range = props["range"].get<float>();
                if (props.contains("enabled")) sl.enabled = props["enabled"].get<bool>();
                if (props.contains("inner_cone_angle")) sl.innerConeAngle = props["inner_cone_angle"].get<float>();
                if (props.contains("outer_cone_angle")) sl.outerConeAngle = props["outer_cone_angle"].get<float>();
                if (props.contains("bake_indirect")) sl.bakeIndirect = props["bake_indirect"].get<bool>();
                if (props.contains("cast_shadow")) sl.castShadow = props["cast_shadow"].get<bool>();
                if (props.contains("bake_static_shadow")) sl.bakeStaticShadow = props["bake_static_shadow"].get<bool>();
                if (props.contains("soft_shadows")) sl.softShadows = props["soft_shadows"].get<bool>();
                if (props.contains("shadow_strength")) sl.shadowStrength = props["shadow_strength"].get<float>();
                if (props.contains("shadow_bias")) sl.shadowBias = props["shadow_bias"].get<float>();
                if (props.contains("pcf_radius")) sl.pcfRadius = props["pcf_radius"].get<float>();
                return true;
            }

            if (type == "WorldEnvironment")
            {
                auto &we = e.HasComponent<WorldEnvironment>() ? e.GetComponent<WorldEnvironment>() : e.AddComponent<WorldEnvironment>();
                if (props.contains("enable_ibl")) we.enableIBL = props["enable_ibl"].get<bool>();
                if (props.contains("exposure")) we.exposure = props["exposure"].get<float>();
                if (props.contains("gamma")) we.gamma = props["gamma"].get<float>();
                if (props.contains("ambient")) we.ambient = props["ambient"].get<float>();
                if (props.contains("fog_density")) we.fogDensity = props["fog_density"].get<float>();
                if (props.contains("fog_color")) we.fogColor = ReadJsonVec4(props["fog_color"], we.fogColor);
                if (props.contains("sky_type") && props["sky_type"].is_string())
                {
                    const std::string skyStr = props["sky_type"].get<std::string>();
                    we.skyType = (skyStr == "atmosphere" || skyStr == "procedural" || skyStr == "procedural_sky")
                        ? SkyType::ProceduralSky : SkyType::HDRI;
                    we.dirtyEnvironment = true;
                }
                return true;
            }

            if (type == "Rigidbody" || type == "Rigidbody3D")
            {
                auto &rb = e.HasComponent<RigidbodyComponent>() ? e.GetComponent<RigidbodyComponent>() : e.AddComponent<RigidbodyComponent>();
                if (props.contains("body_type") && props["body_type"].is_string())
                {
                    const std::string bt = props["body_type"].get<std::string>();
                    if (bt == "dynamic") rb.bodyType = physics::BodyType::Dynamic;
                    else if (bt == "kinematic") rb.bodyType = physics::BodyType::Kinematic;
                    else rb.bodyType = physics::BodyType::Static;
                }
                if (props.contains("use_gravity")) rb.useGravity = props["use_gravity"].get<bool>();
                if (props.contains("gravity_factor")) rb.gravityFactor = props["gravity_factor"].get<float>();
                if (props.contains("mass")) rb.mass = props["mass"].get<float>();
                if (props.contains("friction")) rb.friction = props["friction"].get<float>();
                if (props.contains("restitution")) rb.restitution = props["restitution"].get<float>();
                if (props.contains("is_sensor")) rb.isSensor = props["is_sensor"].get<bool>();
                if (props.contains("linear_velocity"))
                {
                    rb.linearVelocity = ReadJsonVec3(props["linear_velocity"], rb.linearVelocity);
                    if (auto dyn = rb.dynamicActor.lock()) dyn->SetLinearVelocity(rb.linearVelocity);
                }
                if (props.contains("angular_velocity"))
                {
                    rb.angularVelocity = ReadJsonVec3(props["angular_velocity"], rb.angularVelocity);
                    if (auto dyn = rb.dynamicActor.lock()) dyn->SetAngularVelocity(rb.angularVelocity);
                }
                rb.dirty = true;
                return true;
            }

            if (type == "Rigidbody2D")
            {
                auto &rb = e.HasComponent<Rigidbody2DComponent>() ? e.GetComponent<Rigidbody2DComponent>() : e.AddComponent<Rigidbody2DComponent>();
                if (props.contains("body_type") && props["body_type"].is_string())
                {
                    const std::string bt = props["body_type"].get<std::string>();
                    if (bt == "dynamic") rb.bodyType = physics::BodyType::Dynamic;
                    else if (bt == "kinematic") rb.bodyType = physics::BodyType::Kinematic;
                    else rb.bodyType = physics::BodyType::Static;
                }
                if (props.contains("gravity_scale")) rb.gravityScale = props["gravity_scale"].get<float>();
                if (props.contains("fixed_rotation")) rb.fixedRotation = props["fixed_rotation"].get<bool>();
                if (props.contains("linear_velocity")) rb.linearVelocity = ReadJsonVec2(props["linear_velocity"], rb.linearVelocity);
                rb.dirty = true;
                return true;
            }

            if (type == "BoxCollider")
            {
                auto &bc = e.HasComponent<BoxColliderComponent>() ? e.GetComponent<BoxColliderComponent>() : e.AddComponent<BoxColliderComponent>();
                if (props.contains("center")) bc.center = ReadJsonVec3(props["center"], bc.center);
                if (props.contains("scale")) bc.scale = ReadJsonVec3(props["scale"], bc.scale);
                else if (props.contains("size")) bc.scale = ReadJsonVec3(props["size"], bc.scale);
                bc.dirty = true;
                return true;
            }

            if (type == "SphereCollider")
            {
                auto &sc = e.HasComponent<SphereColliderComponent>() ? e.GetComponent<SphereColliderComponent>() : e.AddComponent<SphereColliderComponent>();
                if (props.contains("center")) sc.center = ReadJsonVec3(props["center"], sc.center);
                if (props.contains("radius")) sc.radius = props["radius"].get<float>();
                sc.dirty = true;
                return true;
            }

            if (type == "CapsuleCollider")
            {
                auto &cc = e.HasComponent<CapsuleColliderComponent>() ? e.GetComponent<CapsuleColliderComponent>() : e.AddComponent<CapsuleColliderComponent>();
                if (props.contains("center")) cc.center = ReadJsonVec3(props["center"], cc.center);
                if (props.contains("radius")) cc.radius = props["radius"].get<float>();
                if (props.contains("height")) cc.height = props["height"].get<float>();
                cc.dirty = true;
                return true;
            }

            if (type == "BoxCollider2D")
            {
                auto &bc = e.HasComponent<BoxCollider2DComponent>() ? e.GetComponent<BoxCollider2DComponent>() : e.AddComponent<BoxCollider2DComponent>();
                if (props.contains("size")) bc.size = ReadJsonVec2(props["size"], bc.size);
                if (props.contains("offset")) bc.offset = ReadJsonVec2(props["offset"], bc.offset);
                if (props.contains("friction")) bc.friction = props["friction"].get<float>();
                if (props.contains("restitution")) bc.restitution = props["restitution"].get<float>();
                if (props.contains("density")) bc.density = props["density"].get<float>();
                if (props.contains("is_sensor")) bc.isSensor = props["is_sensor"].get<bool>();
                bc.dirty = true;
                return true;
            }

            if (type == "CircleCollider2D")
            {
                auto &cc = e.HasComponent<CircleCollider2DComponent>() ? e.GetComponent<CircleCollider2DComponent>() : e.AddComponent<CircleCollider2DComponent>();
                if (props.contains("center")) cc.center = ReadJsonVec2(props["center"], cc.center);
                if (props.contains("radius")) cc.radius = props["radius"].get<float>();
                if (props.contains("friction")) cc.friction = props["friction"].get<float>();
                if (props.contains("restitution")) cc.restitution = props["restitution"].get<float>();
                if (props.contains("density")) cc.density = props["density"].get<float>();
                if (props.contains("is_sensor")) cc.isSensor = props["is_sensor"].get<bool>();
                cc.dirty = true;
                return true;
            }

            if (type == "CharacterController")
            {
                auto &cc = e.HasComponent<CharacterControllerComponent>() ? e.GetComponent<CharacterControllerComponent>() : e.AddComponent<CharacterControllerComponent>();
                if (props.contains("radius")) { cc.radius = props["radius"].get<float>(); cc.dirty = true; }
                if (props.contains("height")) { cc.height = props["height"].get<float>(); cc.dirty = true; }
                if (props.contains("center")) { cc.center = ReadJsonVec3(props["center"], cc.center); cc.dirty = true; }
                if (props.contains("max_step_height")) { cc.maxStepHeight = props["max_step_height"].get<float>(); cc.dirty = true; }
                if (props.contains("max_slope_angle")) { cc.maxSlopeAngle = props["max_slope_angle"].get<float>(); cc.dirty = true; }
                if (props.contains("gravity_factor")) { cc.gravityFactor = props["gravity_factor"].get<float>(); cc.dirty = true; }
                if (props.contains("linear_velocity"))
                {
                    cc.linearVelocity = ReadJsonVec3(props["linear_velocity"], cc.linearVelocity);
                    cc.desiredVelocity = cc.linearVelocity;
                    if (auto actor = cc.character.lock())
                        actor->SetLinearVelocity(cc.desiredVelocity);
                }
                return true;
            }

            if (type == "AudioSource")
            {
                auto &as = e.HasComponent<AudioSourceComponent>() ? e.GetComponent<AudioSourceComponent>() : e.AddComponent<AudioSourceComponent>();
                if (props.contains("handle")) as.handle = AssetHandle(props["handle"].get<uint64_t>());
                if (props.contains("volume")) as.volume = props["volume"].get<float>();
                if (props.contains("pitch")) as.pitch = props["pitch"].get<float>();
                if (props.contains("pan")) as.pan = props["pan"].get<float>();
                if (props.contains("loop")) as.loop = props["loop"].get<bool>();
                if (props.contains("play_on_start")) as.playOnStart = props["play_on_start"].get<bool>();
                return true;
            }

            if (type == "LuaScript")
            {
                auto &ls = e.HasComponent<LuaScriptComponent>() ? e.GetComponent<LuaScriptComponent>() : e.AddComponent<LuaScriptComponent>();
                if (props.contains("script_path") && props["script_path"].is_string())
                    ls.scriptPath = props["script_path"].get<std::string>();
                if (props.contains("script_handle"))
                    ls.scriptHandle = AssetHandle(props["script_handle"].get<uint64_t>());
                if (props.contains("fields") && props["fields"].is_object())
                {
                    for (auto &[fname, fval] : props["fields"].items())
                    {
                        if (!fval.is_object() || !fval.contains("type") || !fval.contains("value"))
                            continue;
                        const ScriptFieldType ft = ScriptFieldTypeFromString(fval["type"].get<std::string>());
                        const nlohmann::ordered_json &jv = fval["value"];
                        LuaSerializedField sf;
                        sf.type = ft;
                        switch (ft)
                        {
                            case ScriptFieldType::Bool:    sf.value = jv.get<bool>(); break;
                            case ScriptFieldType::Int:     sf.value = jv.get<int32_t>(); break;
                            case ScriptFieldType::Float:   sf.value = jv.get<float>(); break;
                            case ScriptFieldType::String:  sf.value = jv.get<std::string>(); break;
                            case ScriptFieldType::Vector2:
                                if (jv.is_array() && jv.size() >= 2)
                                    sf.value = glm::vec2(jv[0].get<float>(), jv[1].get<float>());
                                break;
                            case ScriptFieldType::Vector3: sf.value = ReadJsonVec3(jv, glm::vec3(0.0f)); break;
                            case ScriptFieldType::Vector4:
                            case ScriptFieldType::Color:   sf.value = ReadJsonVec4(jv, glm::vec4(0.0f)); break;
                            case ScriptFieldType::Entity:
                            case ScriptFieldType::Asset:   sf.value = jv.get<uint64_t>(); break;
                            case ScriptFieldType::Enum:
                                if (jv.is_string()) sf.value = jv.get<std::string>();
                                else if (jv.is_number_integer()) sf.value = jv.get<int32_t>();
                                break;
                            default: break;
                        }
                        if (IsLuaFieldValueCompatible(sf.type, sf.value))
                        {
                            ls.fields[fname] = sf;
                            if (scene && scene->GetLuaScriptRuntime())
                                scene->GetLuaScriptRuntime()->SetFieldValue(e.GetUUID(), fname, sf.value);
                        }
                    }
                }
                return true;
            }

            if (type == "NetworkIdentity")
            {
                auto &ni = e.HasComponent<NetworkIdentityComponent>() ? e.GetComponent<NetworkIdentityComponent>() : e.AddComponent<NetworkIdentityComponent>();
                if (props.contains("net_id")) ni.netId = props["net_id"].get<uint64_t>();
                if (props.contains("owner_client")) ni.ownerClient = props["owner_client"].get<uint32_t>();
                if (props.contains("is_local_player")) ni.isLocalPlayer = props["is_local_player"].get<bool>();
                if (props.contains("replicate")) ni.replicate = props["replicate"].get<bool>();
                if (props.contains("sync_mask")) ni.syncComponentsMask = props["sync_mask"].get<uint32_t>();
                if (scene && ni.netId != 0) scene->RegisterNetEntity(ni.netId, e);
                return true;
            }

            if (type == "Sprite2D")
            {
                auto &sp = e.HasComponent<Sprite2DComponent>() ? e.GetComponent<Sprite2DComponent>() : e.AddComponent<Sprite2DComponent>();
                if (props.contains("color")) sp.color = ReadJsonVec4(props["color"], sp.color);
                if (props.contains("flip_x")) sp.flipX = props["flip_x"].get<bool>();
                if (props.contains("flip_y")) sp.flipY = props["flip_y"].get<bool>();
                return true;
            }

            if (type == "Text")
            {
                auto &tx = e.HasComponent<TextComponent>() ? e.GetComponent<TextComponent>() : e.AddComponent<TextComponent>();
                if (props.contains("font_handle")) tx.fontHandle = AssetHandle(props["font_handle"].get<uint64_t>());
                if (props.contains("text") && props["text"].is_string()) tx.text = props["text"].get<std::string>();
                if (props.contains("color")) tx.color = ReadJsonVec4(props["color"], tx.color);
                if (props.contains("font_size")) tx.fontSize = props["font_size"].get<float>();
                if (props.contains("kerning")) tx.kerning = props["kerning"].get<float>();
                if (props.contains("line_spacing")) tx.lineSpacing = props["line_spacing"].get<float>();
                if (props.contains("screen_space")) tx.screenSpace = props["screen_space"].get<bool>();
                if (props.contains("billboard")) tx.billboard = props["billboard"].get<bool>();
                if (props.contains("center_align")) tx.centerAlign = props["center_align"].get<bool>();
                return true;
            }

            if (type == "PhysicsJoint" || type == "Joint")
            {
                auto &jc = e.HasComponent<PhysicsJointComponent>() ? e.GetComponent<PhysicsJointComponent>() : e.AddComponent<PhysicsJointComponent>();
                if (props.contains("connected_entity"))
                {
                    if (props["connected_entity"].is_number())
                        jc.connectedEntity = UUID(props["connected_entity"].get<uint64_t>());
                    else if (props["connected_entity"].is_string())
                    {
                        const std::string name = props["connected_entity"].get<std::string>();
                        if (scene)
                        {
                            Entity target = SceneManager::GetEntity(scene, name);
                            if (target) jc.connectedEntity = target.GetUUID();
                        }
                    }
                }
                if (props.contains("type")) jc.desc.type = physics::JointTypeFromString(props["type"].get<std::string>());
                if (props.contains("space")) jc.desc.space = (props["space"].get<std::string>() == "World") ? physics::JointSpace::World : physics::JointSpace::Local;
                if (props.contains("anchor_a")) jc.desc.localAnchorA = ReadJsonVec3(props["anchor_a"], jc.desc.localAnchorA);
                if (props.contains("anchor_b")) jc.desc.localAnchorB = ReadJsonVec3(props["anchor_b"], jc.desc.localAnchorB);
                if (props.contains("axis_a")) jc.desc.localAxisA = ReadJsonVec3(props["axis_a"], jc.desc.localAxisA);
                if (props.contains("axis_b")) jc.desc.localAxisB = ReadJsonVec3(props["axis_b"], jc.desc.localAxisB);
                if (props.contains("min_distance")) jc.desc.minDistance = props["min_distance"].get<float>();
                if (props.contains("max_distance")) jc.desc.maxDistance = props["max_distance"].get<float>();
                if (props.contains("enable_limits")) jc.desc.enableLimits = props["enable_limits"].get<bool>();
                if (props.contains("limits_min")) jc.desc.limitsMin = props["limits_min"].get<float>();
                if (props.contains("limits_max")) jc.desc.limitsMax = props["limits_max"].get<float>();
                if (props.contains("enable_spring")) jc.desc.enableSpring = props["enable_spring"].get<bool>();
                if (props.contains("spring_frequency")) jc.desc.springFrequency = props["spring_frequency"].get<float>();
                if (props.contains("spring_damping")) jc.desc.springDamping = props["spring_damping"].get<float>();
                if (props.contains("motor_state")) jc.desc.motorState = physics::JointMotorStateFromString(props["motor_state"].get<std::string>());
                if (props.contains("motor_velocity")) jc.desc.motorTargetVelocity = props["motor_velocity"].get<float>();
                if (props.contains("max_motor_force")) jc.desc.maxMotorForce = props["max_motor_force"].get<float>();
                if (props.contains("break_force")) jc.desc.breakForce = props["break_force"].get<float>();
                jc.dirty = true;
                return true;
            }

            if (type == "ParticleSystem" || type == "Particle")
            {
                if (!e.HasComponent<RenderingComponent>())
                    e.AddComponent<RenderingComponent>();

                auto &pc = e.HasComponent<ParticleSystemComponent>() ? e.GetComponent<ParticleSystemComponent>() : e.AddComponent<ParticleSystemComponent>();
                if (props.contains("preset") && props["preset"].is_string())
                {
                    const std::string pr = props["preset"].get<std::string>();
                    if (pr == "Fire") pc.settings = ParticleEmitterSettings::CreateFire();
                    else if (pr == "Smoke") pc.settings = ParticleEmitterSettings::CreateSmoke();
                    else if (pr == "Spark") pc.settings = ParticleEmitterSettings::CreateSpark();
                    pc.system = nullptr;
                }
                if (props.contains("is_playing")) pc.isPlaying = props["is_playing"].get<bool>();
                if (props.contains("max_particles")) pc.settings.maxParticles = props["max_particles"].get<uint32_t>();
                if (props.contains("emission_rate")) pc.settings.emissionRate = props["emission_rate"].get<float>();
                if (props.contains("lifetime_min")) pc.settings.lifetimeMin = props["lifetime_min"].get<float>();
                if (props.contains("lifetime_max")) pc.settings.lifetimeMax = props["lifetime_max"].get<float>();
                if (props.contains("velocity_min")) pc.settings.velocityMin = ReadJsonVec3(props["velocity_min"], pc.settings.velocityMin);
                if (props.contains("velocity_max")) pc.settings.velocityMax = ReadJsonVec3(props["velocity_max"], pc.settings.velocityMax);
                if (props.contains("acceleration")) pc.settings.acceleration = ReadJsonVec3(props["acceleration"], pc.settings.acceleration);
                if (props.contains("drag")) pc.settings.drag = props["drag"].get<float>();
                if (props.contains("spawn_radius")) pc.settings.spawnRadius = props["spawn_radius"].get<float>();
                if (props.contains("start_color")) pc.settings.startColor = ReadJsonVec4(props["start_color"], pc.settings.startColor);
                if (props.contains("end_color")) pc.settings.endColor = ReadJsonVec4(props["end_color"], pc.settings.endColor);
                if (props.contains("start_size")) pc.settings.startSize = ReadJsonVec2(props["start_size"], pc.settings.startSize);
                if (props.contains("end_size")) pc.settings.endSize = ReadJsonVec2(props["end_size"], pc.settings.endSize);
                return true;
            }

            if (type == "IrradianceVolume")
            {
                auto &iv = e.HasComponent<IrradianceVolumeComponent>() ? e.GetComponent<IrradianceVolumeComponent>() : e.AddComponent<IrradianceVolumeComponent>();
                if (props.contains("extents")) iv.extents = ReadJsonVec3(props["extents"], iv.extents);
                if (props.contains("spacing")) iv.spacing = props["spacing"].get<float>();
                if (props.contains("bounces")) iv.bounces = props["bounces"].get<int>();
                if (props.contains("capture_resolution")) iv.captureResolution = props["capture_resolution"].get<int>();
                if (props.contains("runtime_mode")) iv.runtimeMode = std::clamp(props["runtime_mode"].get<int>(), 0, 2);
                if (props.contains("intensity")) iv.intensity = props["intensity"].get<float>();
                if (props.contains("priority")) iv.priority = props["priority"].get<int>();
                return true;
            }

            if (type == "ReflectionProbe")
            {
                auto &rp = e.HasComponent<ReflectionProbeComponent>() ? e.GetComponent<ReflectionProbeComponent>() : e.AddComponent<ReflectionProbeComponent>();
                if (props.contains("influence_extents")) rp.influenceExtents = ReadJsonVec3(props["influence_extents"], rp.influenceExtents);
                if (props.contains("projection_extents")) rp.projectionExtents = ReadJsonVec3(props["projection_extents"], rp.projectionExtents);
                if (props.contains("capture_offset")) rp.captureOffset = ReadJsonVec3(props["capture_offset"], rp.captureOffset);
                if (props.contains("blend_distance")) rp.blendDistance = props["blend_distance"].get<float>();
                if (props.contains("resolution")) rp.resolution = props["resolution"].get<int>();
                if (props.contains("priority")) rp.priority = props["priority"].get<int>();
                if (props.contains("intensity")) rp.intensity = props["intensity"].get<float>();
                if (props.contains("normalize")) rp.normalize = props["normalize"].get<bool>();
                if (props.contains("normalization_strength")) rp.normalizationStrength = props["normalization_strength"].get<float>();
                if (props.contains("box_projection")) rp.boxProjection = props["box_projection"].get<bool>();
                if (props.contains("exclude_radius")) rp.excludeRadius = props["exclude_radius"].get<float>();
                return true;
            }

            outError = "Unsupported component type: " + type;
            return false;
        }
    }

    AgentController &AgentController::Get()
    {
        static AgentController instance;
        return instance;
    }

    bool AgentController::IsAutomationActive()
    {
        return s_AutomationActive.load(std::memory_order_relaxed);
    }

    void AgentController::InitializeFromArgs(const ApplicationCommandLineArgs &args)
    {
        std::filesystem::path scriptPath;
        std::filesystem::path reportPath;
        std::filesystem::path watchPath;
        std::filesystem::path responsePath;
        bool quitAfterScript = false;

        for (int i = 1; i < args.count; ++i)
        {
            const std::string_view arg(args[i]);
            constexpr std::string_view kScriptPrefix = "--command-script=";
            constexpr std::string_view kReportPrefix = "--command-report=";
            constexpr std::string_view kWatchPrefix = "--watch-command-file=";
            constexpr std::string_view kResponsePrefix = "--response-file=";

            if (arg.starts_with(kScriptPrefix))
            {
                scriptPath = std::string(arg.substr(kScriptPrefix.size()));
            }
            else if (arg == "--command-script" && i + 1 < args.count)
            {
                scriptPath = args[++i];
            }
            else if (arg.starts_with(kReportPrefix))
            {
                reportPath = std::string(arg.substr(kReportPrefix.size()));
            }
            else if (arg == "--command-report" && i + 1 < args.count)
            {
                reportPath = args[++i];
            }
            else if (arg.starts_with(kWatchPrefix))
            {
                watchPath = std::string(arg.substr(kWatchPrefix.size()));
            }
            else if (arg == "--watch-command-file" && i + 1 < args.count)
            {
                watchPath = args[++i];
            }
            else if (arg.starts_with(kResponsePrefix))
            {
                responsePath = std::string(arg.substr(kResponsePrefix.size()));
            }
            else if (arg == "--response-file" && i + 1 < args.count)
            {
                responsePath = args[++i];
            }
            else if (arg == "--quit-after-script")
            {
                quitAfterScript = true;
            }
        }

        if (!reportPath.empty())
            m_ReportPath = reportPath;

        m_QuitAfterScript = quitAfterScript;

        if (!scriptPath.empty())
            LoadCommandScript(scriptPath);

        if (!watchPath.empty())
            SetWatchCommandFile(watchPath, responsePath);
    }

    void AgentController::LoadCommandScript(const std::filesystem::path &scriptPath)
    {
        std::ifstream ifs(scriptPath);
        if (!ifs.is_open())
        {
            LOG_ERROR("[AgentController] Failed to open command script '{}'", scriptPath.string());
            return;
        }

        try
        {
            nlohmann::ordered_json root = nlohmann::ordered_json::parse(ifs);
            m_ScriptCommands.clear();
            if (root.is_array())
            {
                for (const auto &item : root)
                    m_ScriptCommands.push_back(item);
            }
            else if (root.is_object() && root.contains("commands") && root["commands"].is_array())
            {
                if (root.contains("report_path") && m_ReportPath.empty())
                    m_ReportPath = root["report_path"].get<std::string>();
                if (root.contains("quit_after"))
                    m_QuitAfterScript = root["quit_after"].get<bool>();
                for (const auto &item : root["commands"])
                    m_ScriptCommands.push_back(item);
            }
            else if (root.is_object())
            {
                m_ScriptCommands.push_back(root);
            }

            m_ScriptPath = scriptPath;
            if (m_ReportPath.empty())
            {
                m_ReportPath = scriptPath;
                m_ReportPath += ".result.json";
            }
            m_ScriptIndex = 0;
            m_ScriptLoaded = true;
            m_ScriptReportWritten = false;
            m_Results.clear();
            s_AutomationActive.store(true, std::memory_order_relaxed);
            LOG_INFO("[AgentController] Loaded {} commands from '{}'", m_ScriptCommands.size(), scriptPath.string());
        }
        catch (const std::exception &e)
        {
            LOG_ERROR("[AgentController] Failed parsing '{}': {}", scriptPath.string(), e.what());
        }
    }

    void AgentController::SetWatchCommandFile(const std::filesystem::path &commandFilePath, const std::filesystem::path &responseFilePath)
    {
        m_WatchCommandPath = commandFilePath;
        m_WatchResponsePath = responseFilePath.empty()
            ? std::filesystem::path(commandFilePath.string() + ".response.json")
            : responseFilePath;
        m_HasLastWatchWriteTime = false;
        s_AutomationActive.store(true, std::memory_order_relaxed);
        LOG_INFO("[AgentController] Watching MCP command file '{}'", m_WatchCommandPath.string());
    }

    void AgentController::EnqueueCommand(const nlohmann::ordered_json &command)
    {
        std::lock_guard<std::mutex> lock(m_QueueMutex);
        m_AdhocQueue.push(command);
        s_AutomationActive.store(true, std::memory_order_relaxed);
    }

    bool AgentController::HasPendingWork() const
    {
        return (m_ScriptLoaded && m_ScriptIndex < m_ScriptCommands.size()) || !m_WatchCommandPath.empty();
    }

    void AgentController::WriteReportIfConfigured()
    {
        if (m_ReportPath.empty() || m_ScriptReportWritten)
            return;

        m_ScriptReportWritten = true;
        nlohmann::ordered_json report;
        report["script"] = m_ScriptPath.generic_string();
        report["total_commands"] = m_Results.size();
        size_t passed = 0;
        for (const auto &r : m_Results)
        {
            if (r.value("ok", false))
                ++passed;
        }
        report["passed"] = passed;
        report["failed"] = m_Results.size() - passed;
        report["results"] = m_Results;

        std::error_code ec;
        if (m_ReportPath.has_parent_path())
            std::filesystem::create_directories(m_ReportPath.parent_path(), ec);

        std::ofstream ofs(m_ReportPath);
        if (ofs.is_open())
        {
            ofs << report.dump(2);
            LOG_INFO("[AgentController] Wrote command report to '{}'", m_ReportPath.string());
        }
    }

    void AgentController::WriteWatchResponse(const nlohmann::ordered_json &response)
    {
        if (m_WatchResponsePath.empty())
            return;

        std::error_code ec;
        if (m_WatchResponsePath.has_parent_path())
            std::filesystem::create_directories(m_WatchResponsePath.parent_path(), ec);

        const std::filesystem::path tmpPath = std::filesystem::path(m_WatchResponsePath.string() + ".tmp");
        {
            std::ofstream ofs(tmpPath);
            if (!ofs.is_open())
                return;
            ofs << response.dump(2);
        }
        std::filesystem::rename(tmpPath, m_WatchResponsePath, ec);
        if (ec)
        {
            std::filesystem::copy_file(tmpPath, m_WatchResponsePath, std::filesystem::copy_options::overwrite_existing, ec);
            std::filesystem::remove(tmpPath, ec);
        }
    }

    void AgentController::Update(float deltaTime, AgentExecutionContext &ctx)
    {
        // The editor and headless command-script paths share the same
        // time-sliced baker. Advancing it here keeps UI and automation in lockstep.
        if (ctx.giBaker)
        {
            const bool isBaking = ctx.giBaker->IsBaking();
            if (isBaking)
            {
                m_WasGIBaking = true;
                ctx.giBaker->StepBake(32);
            }
            if (m_WasGIBaking && !ctx.giBaker->IsBaking())
            {
                m_WasGIBaking = false;
                if (ctx.giBaker->DidFinishSuccessfully() && ctx.sceneRenderer)
                {
                    Ref<IrradianceVolume> uploadedVol = CreateRef<IrradianceVolume>();
                    if (uploadedVol->UploadFromData(ctx.giBaker->GetLastBakedData()))
                    {
                        ctx.sceneRenderer->SetIrradianceVolume(uploadedVol);
                    }
                    else
                    {
                        ctx.sceneRenderer->ClearIrradianceVolume();
                    }
                }
            }
        }

        // 1. Handle wait barriers for command scripts
        if (m_WaitFrames > 0)
        {
            --m_WaitFrames;
            return;
        }
        if (m_WaitSeconds > 0.0f)
        {
            m_WaitSeconds -= deltaTime;
            if (m_WaitSeconds > 0.0f)
                return;
            m_WaitSeconds = 0.0f;
        }

        // 2. Drain queued ad-hoc commands
        std::vector<nlohmann::ordered_json> adhoc;
        {
            std::lock_guard<std::mutex> lock(m_QueueMutex);
            while (!m_AdhocQueue.empty())
            {
                adhoc.push_back(std::move(m_AdhocQueue.front()));
                m_AdhocQueue.pop();
            }
        }
        for (const auto &cmd : adhoc)
        {
            m_Results.push_back(ExecuteCommand(cmd, ctx));
        }

        // 3. Poll file-watched command channel for Python MCP bridge
        if (!m_WatchCommandPath.empty())
        {
            std::error_code ec;
            if (std::filesystem::exists(m_WatchCommandPath, ec))
            {
                const auto writeTime = std::filesystem::last_write_time(m_WatchCommandPath, ec);
                if (!ec && (!m_HasLastWatchWriteTime || writeTime != m_LastWatchWriteTime))
                {
                    std::ifstream ifs(m_WatchCommandPath);
                    if (ifs.is_open())
                    {
                        try
                        {
                            const nlohmann::ordered_json req = nlohmann::ordered_json::parse(ifs);
                            const std::string reqId = req.contains("request_id") ? req["request_id"].dump() : "";
                            if (reqId.empty() || reqId != m_LastProcessedRequestId)
                            {
                                m_LastProcessedRequestId = reqId;
                                m_LastWatchWriteTime = writeTime;
                                m_HasLastWatchWriteTime = true;

                                nlohmann::ordered_json res = ExecuteCommand(req, ctx);
                                if (req.contains("request_id"))
                                    res["request_id"] = req["request_id"];
                                WriteWatchResponse(res);
                            }
                        }
                        catch (const std::exception &)
                        {
                            // File may still be flushing; retry next frame
                        }
                    }
                }
            }
        }

        // 4. Advance command script sequentially
        if (m_ScriptLoaded)
        {
            while (m_ScriptIndex < m_ScriptCommands.size())
            {
                const nlohmann::ordered_json &peekCmd = m_ScriptCommands[m_ScriptIndex];
                const std::string peekAction = peekCmd.value("command", peekCmd.value("action", ""));
                if (ctx.giBaker && ctx.giBaker->IsBaking()
                    && peekAction != "get_gi_status" && peekAction != "cancel_gi_bake")
                {
                    return;
                }

                const nlohmann::ordered_json &cmd = m_ScriptCommands[m_ScriptIndex++];
                const std::string action = cmd.value("command", cmd.value("action", ""));
                if (action == "wait_frames")
                {
                    m_WaitFrames = std::max(0, cmd.value("count", cmd.value("frames", 1)));
                    nlohmann::ordered_json r;
                    r["command"] = "wait_frames";
                    r["ok"] = true;
                    r["frames"] = m_WaitFrames;
                    m_Results.push_back(std::move(r));
                    if (m_WaitFrames > 0)
                        return;
                }
                else if (action == "wait_seconds")
                {
                    m_WaitSeconds = std::max(0.0f, cmd.value("seconds", 0.5f));
                    nlohmann::ordered_json r;
                    r["command"] = "wait_seconds";
                    r["ok"] = true;
                    r["seconds"] = m_WaitSeconds;
                    m_Results.push_back(std::move(r));
                    if (m_WaitSeconds > 0.0f)
                        return;
                }
                else
                {
                    nlohmann::ordered_json res = ExecuteCommand(cmd, ctx);
                    m_Results.push_back(res);
                    if (action == "quit")
                    {
                        WriteReportIfConfigured();
                        return;
                    }
                }
            }

            WriteReportIfConfigured();
            m_ScriptLoaded = false;
            if (m_QuitAfterScript)
            {
                if (Application::GetInstance() && Application::GetInstance()->GetWindow())
                {
                    Application::Shutdown();
                }
            }
        }
    }

    std::string AgentController::ExecuteCommandJsonString(const std::string &jsonText, AgentExecutionContext &ctx)
    {
        try
        {
            const nlohmann::ordered_json cmd = nlohmann::ordered_json::parse(jsonText);
            return ExecuteCommand(cmd, ctx).dump();
        }
        catch (const std::exception &e)
        {
            nlohmann::ordered_json err;
            err["ok"] = false;
            err["error"] = e.what();
            return err.dump();
        }
    }

    nlohmann::ordered_json AgentController::ExecuteCommand(const nlohmann::ordered_json &cmd, AgentExecutionContext &ctx)
    {
        nlohmann::ordered_json res;
        const std::string action = cmd.value("command", cmd.value("action", ""));
        res["command"] = action;
        if (cmd.contains("id") && cmd["id"].is_string())
            res["id"] = cmd["id"];
        res["ok"] = false;

        if (action.empty())
        {
            res["error"] = "Missing 'command' field";
            return res;
        }

        if (action == "ping" || action == "get_status")
        {
            res["ok"] = true;
            nlohmann::ordered_json data;
            data["has_project"] = (ctx.project != nullptr);
            if (ctx.project)
            {
                data["project_name"] = ctx.project->GetInfo().name;
                data["project_path"] = ctx.project->GetInfo().filepath.generic_string();
            }
            data["has_scene"] = (ctx.scene != nullptr);
            if (ctx.scene)
            {
                data["scene_state"] = SceneStateName(ctx.scene->GetState());
                data["scene_time"] = ctx.scene->timeInSeconds;
                data["entity_count"] = ctx.scene->entities.size();
                data["scene_ready"] = ctx.scene->IsReadyToPlay();
            }
            if (ctx.camera)
            {
                data["camera_position"] = Vec3ToJson(ctx.camera->position);
                data["camera_fov"] = ctx.camera->fov;
            }
            if (auto *dm = DeviceManager::GetInstance())
            {
                double avgDt = dm->GetAverageFrameTimeSeconds();
                if (avgDt <= 1e-6 && Application::GetInstance())
                {
                    avgDt = static_cast<double>(Application::GetAverageFrameTime());
                }
                data["avg_frame_time_ms"] = avgDt * 1000.0;
                data["fps"] = (avgDt > 1e-6) ? (1.0 / avgDt) : 0.0;
            }
            if (ctx.sceneRenderer)
            {
                const auto &rs = ctx.sceneRenderer->sceneRenderSettings;
                const auto &pktStats = ctx.sceneRenderer->GetLastFramePacket().stats;
                data["scalability_overall"] = static_cast<int>(rs.scalability.overall);
                data["aa_mode"] = static_cast<int>(rs.sampleMethod);
                data["render_scale"] = rs.renderScale;
                data["shadow_draw_calls"] = Renderer::Stats.shadowDrawCallCount;
                data["draw_calls"] = Renderer::Stats.drawCallCount;
                data["static_meshes_total"] = pktStats.totalStaticMeshes;
                data["static_meshes_visible"] = pktStats.visibleStaticMeshes;
            }
            if (auto *nm = NetworkManager::GetInstance())
            {
                NetStats stats = nm->GetStats();
                data["net_server_running"] = nm->IsServerRunning();
                data["net_connected"] = nm->IsConnected();
                data["net_client_id"] = nm->GetClientId();
                data["net_client_count"] = stats.clientCount;
                data["net_rtt_ms"] = stats.rttMs;
            }
            res["data"] = data;
            return res;
        }

        if (action == "play")
        {
            if (ctx.scene && !ctx.scene->IsReadyToPlay())
            {
                res["error"] = "Scene is not ready to play: related assets are still loading";
                return res;
            }
            if (ctx.onPlay)
            {
                ctx.onPlay();
                res["ok"] = true;
            }
            else if (ctx.scene)
            {
                ctx.scene->OnStart(ESceneState::Play);
                res["ok"] = true;
            }
            return res;
        }

        if (action == "simulate")
        {
            if (ctx.scene && !ctx.scene->IsReadyToPlay())
            {
                res["error"] = "Scene is not ready to simulate: related assets are still loading";
                return res;
            }
            if (ctx.onSimulate)
            {
                ctx.onSimulate();
                res["ok"] = true;
            }
            else if (ctx.scene)
            {
                ctx.scene->OnStart(ESceneState::Simulate);
                res["ok"] = true;
            }
            return res;
        }

        if (action == "stop")
        {
            if (ctx.onStop)
            {
                ctx.onStop();
                res["ok"] = true;
            }
            else if (ctx.scene)
            {
                ctx.scene->OnStop();
                res["ok"] = true;
            }
            return res;
        }

        if (action == "pause")
        {
            if (!ctx.scene) { res["error"] = "No active scene"; return res; }
            ctx.scene->Pause();
            res["ok"] = true;
            return res;
        }

        if (action == "resume")
        {
            if (!ctx.scene) { res["error"] = "No active scene"; return res; }
            ctx.scene->Resume();
            res["ok"] = true;
            return res;
        }

        if (action == "step_frames")
        {
            if (!ctx.scene) { res["error"] = "No active scene"; return res; }
            const int frames = std::max(1, cmd.value("count", 1));
            ctx.scene->StepFrame(frames);
            res["ok"] = true;
            return res;
        }

        if (action == "load_scene")
        {
            if (!ctx.project) { res["error"] = "No active project"; return res; }
            if (cmd.contains("handle") && cmd["handle"].is_number_unsigned())
            {
                SceneManager::Transition(AssetHandle(cmd["handle"].get<uint64_t>()));
                res["ok"] = true;
                return res;
            }
            const std::filesystem::path scenePath = cmd.value("path", "");
            std::filesystem::path resolved = scenePath;
            if (!std::filesystem::exists(resolved))
                resolved = ctx.project->GetAssetDirectory() / scenePath;
            if (!std::filesystem::exists(resolved))
            {
                res["error"] = "Scene file not found: " + scenePath.string();
                return res;
            }
            Ref<Scene> loaded = SceneSerializer::Deserialize(resolved, ctx.project);
            if (!loaded)
            {
                res["error"] = "Failed to deserialize scene";
                return res;
            }
            if (ctx.onSceneReplaced)
                ctx.onSceneReplaced(loaded);
            else
                ctx.scene = loaded.get();
            res["ok"] = true;
            return res;
        }

        if (action == "save_scene")
        {
            if (!ctx.scene) { res["error"] = "No active scene"; return res; }
            const std::filesystem::path savePath = cmd.value("path", "");
            if (savePath.empty()) { res["error"] = "Missing 'path'"; return res; }
            Ref<Scene> activeScene = ctx.project ? ctx.project->LockActiveScene() : nullptr;
            if (!activeScene) { res["error"] = "No Ref<Scene> handle available to serialize"; return res; }
            SceneSerializer sr(activeScene, ctx.project);
            res["ok"] = sr.Serialize(savePath);
            if (!res["ok"].get<bool>())
                res["error"] = "Failed saving scene";
            return res;
        }

        if (action == "import_asset")
        {
            if (!ctx.project) { res["error"] = "No active project"; return res; }
            auto *am = AssetManager::GetInstance();
            if (!am) { res["error"] = "AssetManager unavailable"; return res; }

            const std::string srcPathStr = cmd.value("path", "");
            const std::string typeStr = cmd.value("type", "StaticMesh");
            const std::string targetDirStr = cmd.value("target_dir", "");
            if (srcPathStr.empty()) { res["error"] = "Missing 'path'"; return res; }

            std::filesystem::path srcPath = ctx.project->GetProjectFilepath(srcPathStr);
            if (!std::filesystem::exists(srcPath)) { res["error"] = "File not found: " + srcPath.string(); return res; }

            const std::filesystem::path targetDir = targetDirStr.empty()
                ? srcPath.parent_path()
                : ctx.project->GetProjectFilepath(targetDirStr);

            const AssetType assetType = AssetTypeFromString(typeStr);
            if (assetType == AssetType::Invalid) { res["error"] = "Invalid asset type: " + typeStr; return res; }

            AssetMetaData md;
            md.filepath = ctx.project->GetProjectRelativeFilepath(srcPath);
            md.type = assetType;

            AssetHandle handle = am->GetAssetHandle(md.filepath);
            if (handle == AssetHandle(0))
                handle = AssetHandle();

            Ref<Asset> imported;
            if (assetType == AssetType::SkeletalMesh)
            {
                SkeletalMeshImportPayload payload;
                payload.targetDirectory = targetDir;
                payload.forceRebuild = cmd.value("force_rebuild", false);
                AssetMetaData fullMd = md;
                fullMd.filepath = srcPath;
                imported = AssetImporter::ImportSkeletalMesh(handle, fullMd, am, payload);
            }
            else if (assetType == AssetType::StaticMesh)
            {
                StaticMeshImportPayload payload;
                payload.targetDirectory = targetDir;
                payload.forceRebuild = cmd.value("force_rebuild", false);
                AssetMetaData fullMd = md;
                fullMd.filepath = srcPath;
                imported = AssetImporter::ImportStaticMesh(handle, fullMd, am, payload);
            }
            else
            {
                am->AssignMetaData(handle, md);
                imported = am->GetAssetImmediate<Asset>(handle);
            }

            if (!imported)
            {
                res["error"] = "Failed importing asset: " + srcPath.string();
                return res;
            }

            Project::SaveActive();
            res["ok"] = true;
            res["data"] = {
                { "handle", static_cast<uint64_t>(imported->handle) },
                { "type", AssetTypeToString(imported->GetAssetType()) }
            };
            return res;
        }

        if (action == "list_assets")
        {
            if (!AssetManager::GetInstance()) { res["error"] = "AssetManager unavailable"; return res; }
            nlohmann::ordered_json arr = nlohmann::ordered_json::array();
            const std::string filterType = cmd.value("filter_type", "");
            for (const auto &[handle, meta] : AssetManager::GetInstance()->GetAssetAssetRegistry())
            {
                const std::string typeStr = AssetTypeToString(meta.type);
                if (!filterType.empty() && typeStr != filterType)
                    continue;
                arr.push_back({
                    { "handle", static_cast<uint64_t>(handle) },
                    { "type", typeStr },
                    { "path", meta.filepath.generic_string() }
                });
            }
            res["ok"] = true;
            res["data"] = { { "assets", arr } };
            return res;
        }

        if (action == "list_entities")
        {
            if (!ctx.scene || !ctx.scene->registry) { res["error"] = "No active scene"; return res; }
            nlohmann::ordered_json arr = nlohmann::ordered_json::array();
            for (const auto &[uuid, enttId] : ctx.scene->entities)
            {
                if (!ctx.scene->registry->valid(enttId)) continue;
                arr.push_back(SerializeEntitySummary(Entity(enttId, ctx.scene), false));
            }
            res["ok"] = true;
            res["data"] = { { "entities", arr } };
            return res;
        }

        if (action == "get_entity")
        {
            Entity e = ResolveTargetEntity(ctx.scene, cmd);
            if (!e) { res["error"] = "Entity not found"; return res; }
            res["ok"] = true;
            res["data"] = SerializeEntitySummary(e, true);
            return res;
        }

        if (action == "create_entity")
        {
            if (!ctx.scene) { res["error"] = "No active scene"; return res; }
            const std::string name = cmd.value("name", "");
            Entity e;
            if (cmd.contains("primitive") && cmd["primitive"].is_string())
            {
                EMeshType meshType = EMeshType::CUBE;
                Renderer::ParseMeshType(cmd["primitive"].get<std::string>(), meshType);
                e = SceneManager::CreatePrimitive(ctx.scene, meshType, name);
            }
            else
            {
                e = SceneManager::CreateEmptyEntity(ctx.scene, name.empty() ? "Entity" : name);
            }
            if (!e) { res["error"] = "Failed to create entity"; return res; }

            auto &tc = e.GetComponent<TransformComponent>();
            if (cmd.contains("position")) tc.local.translation = ReadJsonVec3(cmd["position"], tc.local.translation);
            if (cmd.contains("rotation")) tc.local.rotation = glm::quat(glm::radians(ReadJsonVec3(cmd["rotation"])));
            if (cmd.contains("scale")) tc.local.scale = ReadJsonVec3(cmd["scale"], tc.local.scale);
            tc.dirty = true;
            ctx.scene->UpdateTransformRecursive(e, glm::mat4(1.0f));

            if (cmd.contains("parent"))
            {
                if (Entity p = ResolveTargetEntity(ctx.scene, cmd, "parent"))
                    SceneManager::AddChild(ctx.scene, p, e);
            }

            if (cmd.contains("components") && cmd["components"].is_object())
            {
                for (auto &[compName, compProps] : cmd["components"].items())
                {
                    std::string err;
                    ApplyComponentProperties(ctx.scene, e, compName, compProps, err);
                }
            }

            res["ok"] = true;
            res["data"] = SerializeEntitySummary(e, true);
            return res;
        }

        if (action == "duplicate_entity")
        {
            Entity src = ResolveTargetEntity(ctx.scene, cmd);
            if (!src) { res["error"] = "Source entity not found"; return res; }
            Entity dup = SceneManager::DuplicateEntity(ctx.scene, src);
            if (!dup) { res["error"] = "Failed duplicating entity"; return res; }
            res["ok"] = true;
            res["data"] = SerializeEntitySummary(dup, true);
            return res;
        }

        if (action == "destroy_entity")
        {
            Entity e = ResolveTargetEntity(ctx.scene, cmd);
            if (!e) { res["error"] = "Entity not found"; return res; }
            SceneManager::DestroyEntity(ctx.scene, e);
            res["ok"] = true;
            return res;
        }

        if (action == "rename_entity")
        {
            Entity e = ResolveTargetEntity(ctx.scene, cmd);
            if (!e) { res["error"] = "Entity not found"; return res; }
            const std::string newName = cmd.value("new_name", "");
            if (newName.empty()) { res["error"] = "Missing 'new_name'"; return res; }
            SceneManager::RenameEntity(ctx.scene, e, newName);
            res["ok"] = true;
            return res;
        }

        if (action == "add_component" || action == "set_component" || action == "update_component")
        {
            Entity e = ResolveTargetEntity(ctx.scene, cmd);
            if (!e) { res["error"] = "Entity not found"; return res; }
            const std::string type = cmd.value("type", "");
            if (type.empty()) { res["error"] = "Missing component 'type'"; return res; }
            const nlohmann::ordered_json props = cmd.contains("properties") && cmd["properties"].is_object()
                ? cmd["properties"]
                : nlohmann::ordered_json::object();

            std::string err;
            if (!ApplyComponentProperties(ctx.scene, e, type, props, err))
            {
                res["error"] = err;
                return res;
            }
            res["ok"] = true;
            res["data"] = SerializeEntitySummary(e, true);
            return res;
        }

        if (action == "remove_component")
        {
            Entity e = ResolveTargetEntity(ctx.scene, cmd);
            if (!e) { res["error"] = "Entity not found"; return res; }
            const std::string type = cmd.value("type", "");
            if (type == "Rendering" && e.HasComponent<RenderingComponent>()) e.RemoveComponent<RenderingComponent>();
            else if (type == "StaticMesh" && e.HasComponent<StaticMeshComponent>()) e.RemoveComponent<StaticMeshComponent>();
            else if (type == "Camera" && e.HasComponent<CameraComponent>()) e.RemoveComponent<CameraComponent>();
            else if (type == "DirectionalLight" && e.HasComponent<DirectionalLightComponent>()) e.RemoveComponent<DirectionalLightComponent>();
            else if (type == "PointLight" && e.HasComponent<PointLightComponent>()) e.RemoveComponent<PointLightComponent>();
            else if (type == "SpotLight" && e.HasComponent<SpotLightComponent>()) e.RemoveComponent<SpotLightComponent>();
            else if ((type == "Rigidbody" || type == "Rigidbody3D") && e.HasComponent<RigidbodyComponent>()) e.RemoveComponent<RigidbodyComponent>();
            else if (type == "Rigidbody2D" && e.HasComponent<Rigidbody2DComponent>()) { ctx.scene->DestroyPhysics2DBody(e); e.RemoveComponent<Rigidbody2DComponent>(); }
            else if (type == "BoxCollider" && e.HasComponent<BoxColliderComponent>()) e.RemoveComponent<BoxColliderComponent>();
            else if (type == "SphereCollider" && e.HasComponent<SphereColliderComponent>()) e.RemoveComponent<SphereColliderComponent>();
            else if (type == "CapsuleCollider" && e.HasComponent<CapsuleColliderComponent>()) e.RemoveComponent<CapsuleColliderComponent>();
            else if (type == "AudioSource" && e.HasComponent<AudioSourceComponent>()) e.RemoveComponent<AudioSourceComponent>();
            else if (type == "LuaScript" && e.HasComponent<LuaScriptComponent>()) e.RemoveComponent<LuaScriptComponent>();
            else if (type == "NetworkIdentity" && e.HasComponent<NetworkIdentityComponent>()) e.RemoveComponent<NetworkIdentityComponent>();
            else if (type == "Text" && e.HasComponent<TextComponent>()) e.RemoveComponent<TextComponent>();
            else if ((type == "PhysicsJoint" || type == "Joint") && e.HasComponent<PhysicsJointComponent>()) e.RemoveComponent<PhysicsJointComponent>();
            else if ((type == "ParticleSystem" || type == "Particle") && e.HasComponent<ParticleSystemComponent>()) e.RemoveComponent<ParticleSystemComponent>();
            res["ok"] = true;
            return res;
        }

        if (action == "set_camera")
        {
            ICamera *cam = ctx.camera;
            if (!cam && ctx.scene)
            {
                if (Entity primaryCam = ctx.scene->GetPrimaryCamera())
                {
                    if (primaryCam.HasComponent<CameraComponent>())
                        cam = &primaryCam.GetComponent<CameraComponent>().camera;
                }
            }
            if (!cam) { res["error"] = "No active camera"; return res; }

            if (cmd.contains("position")) cam->position = ReadJsonVec3(cmd["position"], cam->position);
            if (cmd.contains("fov"))
            {
                cam->fov = cmd["fov"].get<float>();
                const auto vp = cam->GetViewportSize();
                if (vp.x > 0 && vp.y > 0) cam->UpdateProjection(vp.x, vp.y);
            }
            if (cmd.contains("pitch")) cam->pitch = glm::radians(cmd["pitch"].get<float>());
            if (cmd.contains("yaw")) cam->yaw = glm::radians(cmd["yaw"].get<float>());
            if (cmd.contains("target"))
            {
                const glm::vec3 target = ReadJsonVec3(cmd["target"]);
                glm::vec3 offset = cam->position - target;
                float distance = glm::length(offset);
                if (distance < 1e-4f)
                {
                    offset = glm::vec3(0.0f, 0.0f, 1.0f);
                    distance = 1.0f;
                }

                if (auto *editorCam = dynamic_cast<EditorCamera *>(cam))
                {
                    editorCam->SetTarget(target);
                    editorCam->SetDistance(distance);
                    editorCam->pitch = glm::clamp(std::asin(glm::clamp(offset.y / distance, -0.999f, 0.999f)), editorCam->controls.minPitch, editorCam->controls.maxPitch);
                    editorCam->yaw = std::atan2(offset.z, offset.x);
                    if (editorCam->GetNavigationMode() != EditorCamera::NavigationMode::Fly)
                    {
                        editorCam->UpdateSphericalPosition();
                    }
                    editorCam->UpdateView();
                }
                else
                {
                    const glm::vec3 dir = (target - cam->position) / distance;
                    cam->pitch = glm::asin(glm::clamp(dir.y, -0.999f, 0.999f));
                    cam->yaw = std::atan2(dir.x, -dir.z);
                    cam->SetView(glm::lookAt(cam->position, target, glm::vec3(0.0f, 1.0f, 0.0f)));
                }
            }
            else if (cmd.contains("position"))
            {
                if (auto *editorCam = dynamic_cast<EditorCamera *>(cam))
                {
                    glm::vec3 offset = editorCam->position - editorCam->GetTarget();
                    float distance = glm::length(offset);
                    if (distance < 1e-4f)
                    {
                        offset = glm::vec3(0.0f, 0.0f, 1.0f);
                        distance = 1.0f;
                    }
                    editorCam->SetDistance(distance);
                    editorCam->pitch = glm::clamp(std::asin(glm::clamp(offset.y / distance, -0.999f, 0.999f)), editorCam->controls.minPitch, editorCam->controls.maxPitch);
                    editorCam->yaw = std::atan2(offset.z, offset.x);
                    editorCam->UpdateView();
                }
            }
            else
            {
                cam->UpdateView();
            }

            res["ok"] = true;
            res["data"] = { { "position", Vec3ToJson(cam->position) }, { "fov", cam->fov } };
            return res;
        }

        if (action == "set_render_settings")
        {
            if (ctx.sceneRenderer)
            {
                if (cmd.contains("render_scale")) ctx.sceneRenderer->sceneRenderSettings.renderScale = cmd["render_scale"].get<float>();
                auto parseRenderMode = [](const nlohmann::ordered_json &val) -> int
                {
                    if (val.is_string())
                    {
                        const std::string s = val.get<std::string>();
                        if (s == "color" || s == "default") return RENDER_MODE_COLOR;
                        if (s == "gi_diffuse" || s == "giDiffuse") return RENDER_MODE_GI_DIFFUSE;
                        if (s == "gi_specular" || s == "giSpecular") return RENDER_MODE_GI_SPECULAR;
                        if (s == "ao") return RENDER_MODE_AO;
                        if (s == "probe_weights" || s == "probeWeights") return RENDER_MODE_PROBE_WEIGHTS;
                        return RENDER_MODE_COLOR;
                    }
                    if (val.is_number_integer())
                        return val.get<int>();
                    return RENDER_MODE_COLOR;
                };
                if (cmd.contains("render_mode")) ctx.sceneRenderer->SetRenderMode(parseRenderMode(cmd["render_mode"]));
                else if (cmd.contains("renderMode")) ctx.sceneRenderer->SetRenderMode(parseRenderMode(cmd["renderMode"]));
                if (cmd.contains("gi_debug_mode"))
                    ctx.sceneRenderer->SetRenderMode(std::clamp(cmd["gi_debug_mode"].get<int>(), 9, 12));
                if (cmd.contains("debug_shadow")) ctx.sceneRenderer->SetDebugShadowMode(cmd["debug_shadow"].get<int>());
                auto parseGIMode = [](const nlohmann::ordered_json &val) -> int
                {
                    if (val.is_string())
                    {
                        const std::string s = val.get<std::string>();
                        if (s == "off") return 0;
                        if (s == "fast") return 1;
                        if (s == "leak_resistant" || s == "leakResistant" || s == "leak-resistant") return 2;
                        return -1;
                    }
                    if (val.is_number_integer())
                        return std::clamp(val.get<int>(), -1, 2);
                    return -1;
                };
                auto parseAOMode = [](const nlohmann::ordered_json &val) -> int
                {
                    if (val.is_string())
                    {
                        const std::string s = val.get<std::string>();
                        if (s == "off" || s == "none" || s == "NONE" || s == "None") return 0;
                        if (s == "ssao" || s == "SSAO") return 1;
                        if (s == "gtao" || s == "GTAO" || s == "xegtao") return 2;
                        return -1;
                    }
                    if (val.is_number_integer())
                        return std::clamp(val.get<int>(), -1, 2);
                    return -1;
                };
                if (cmd.contains("gi_mode")) ctx.sceneRenderer->sceneRenderSettings.giModeOverride = parseGIMode(cmd["gi_mode"]);
                else if (cmd.contains("giMode")) ctx.sceneRenderer->sceneRenderSettings.giModeOverride = parseGIMode(cmd["giMode"]);
                if (cmd.contains("ao_mode") || cmd.contains("aoMode"))
                {
                    const int aoVal = parseAOMode(cmd.contains("ao_mode") ? cmd["ao_mode"] : cmd["aoMode"]);
                    ctx.sceneRenderer->sceneRenderSettings.aoModeOverride = aoVal;
                    if (aoVal >= 0)
                        ctx.sceneRenderer->sceneRenderSettings.scalability.ambientOcclusion = static_cast<AOMode>(aoVal);
                }
                if (cmd.contains("depth_prepass")) ctx.sceneRenderer->sceneRenderSettings.enableDepthPrepass = cmd["depth_prepass"].get<bool>();
                else if (cmd.contains("depthPrepass")) ctx.sceneRenderer->sceneRenderSettings.enableDepthPrepass = cmd["depthPrepass"].get<bool>();
                if (cmd.contains("ssr")) ctx.sceneRenderer->sceneRenderSettings.enableSSR = cmd["ssr"].get<bool>();
                if (cmd.contains("bloom")) ctx.sceneRenderer->sceneRenderSettings.enableBloom = cmd["bloom"].get<bool>();
                if (cmd.contains("shadow_quality"))
                {
                    const auto &sq = cmd["shadow_quality"];
                    if (sq.is_string())
                    {
                        const std::string s = sq.get<std::string>();
                        if (s == "low" || s == "LOW") ctx.sceneRenderer->sceneRenderSettings.SyncFromDirectionalLight(0);
                        else if (s == "medium" || s == "MEDIUM") ctx.sceneRenderer->sceneRenderSettings.SyncFromDirectionalLight(1);
                        else if (s == "high" || s == "HIGH") ctx.sceneRenderer->sceneRenderSettings.SyncFromDirectionalLight(2);
                        else if (s == "ultra" || s == "ULTRA") ctx.sceneRenderer->sceneRenderSettings.SyncFromDirectionalLight(3);
                        else if (s == "ultimate" || s == "ULTIMATE") ctx.sceneRenderer->sceneRenderSettings.SyncFromDirectionalLight(4);
                    }
                    else if (sq.is_number_integer())
                    {
                        ctx.sceneRenderer->sceneRenderSettings.SyncFromDirectionalLight(static_cast<uint32_t>(std::max(0, sq.get<int>())));
                    }
                }
                if (cmd.contains("show_gi_probes")) ctx.sceneRenderer->sceneRenderSettings.showGIProbes = cmd["show_gi_probes"].get<bool>();
                else if (cmd.contains("showGIProbes")) ctx.sceneRenderer->sceneRenderSettings.showGIProbes = cmd["showGIProbes"].get<bool>();
                if (cmd.contains("show_colliders")) ctx.sceneRenderer->sceneRenderSettings.showPhysicsCollider = cmd["show_colliders"].get<bool>();
                if (cmd.contains("show_bounds")) ctx.sceneRenderer->sceneRenderSettings.showBoundingBox = cmd["show_bounds"].get<bool>();
                if (cmd.contains("show_entity_names")) ctx.sceneRenderer->sceneRenderSettings.showEntityNames = cmd["show_entity_names"].get<bool>();
                if (cmd.contains("fxaa"))
                {
                    ctx.sceneRenderer->SetSampleMethod(cmd["fxaa"].get<bool>() ? SampleMethod::FXAA : SampleMethod::None);
                }
                auto parseScalability = [](const nlohmann::ordered_json &val, ScalabilityLevel &out) -> bool
                {
                    if (val.is_string())
                    {
                        const std::string s = val.get<std::string>();
                        if (s == "low" || s == "LOW") { out = ScalabilityLevel::Low; return true; }
                        if (s == "medium" || s == "MEDIUM" || s == "med") { out = ScalabilityLevel::Medium; return true; }
                        if (s == "high" || s == "HIGH") { out = ScalabilityLevel::High; return true; }
                        if (s == "ultra" || s == "ULTRA") { out = ScalabilityLevel::Ultra; return true; }
                    }
                    else if (val.is_number_integer())
                    {
                        out = static_cast<ScalabilityLevel>(std::clamp(val.get<int>(), 0, 3));
                        return true;
                    }
                    return false;
                };
                if (cmd.contains("scalability"))
                {
                    ScalabilityLevel lvl = ScalabilityLevel::High;
                    if (parseScalability(cmd["scalability"], lvl))
                        ctx.sceneRenderer->sceneRenderSettings.ApplyOverallScalability(lvl);
                }
                auto applyAAKey = [&](const nlohmann::ordered_json &val)
                {
                    if (val.is_string())
                    {
                        const std::string s = val.get<std::string>();
                        if (s == "none" || s == "NONE" || s == "off") ctx.sceneRenderer->SetSampleMethod(SampleMethod::NONE);
                        else if (s == "taa" || s == "TAA") ctx.sceneRenderer->SetSampleMethod(SampleMethod::TAA);
                        else if (s == "msaa" || s == "MSAA") ctx.sceneRenderer->SetSampleMethod(SampleMethod::MSAA);
                        else if (s == "fxaa" || s == "FXAA") ctx.sceneRenderer->SetSampleMethod(SampleMethod::FXAA);
                    }
                    else if (val.is_number_integer())
                    {
                        ctx.sceneRenderer->SetSampleMethod(static_cast<SampleMethod>(std::clamp(val.get<int>(), 0, 3)));
                    }
                };
                if (cmd.contains("aa_mode")) applyAAKey(cmd["aa_mode"]);
                else if (cmd.contains("anti_aliasing")) applyAAKey(cmd["anti_aliasing"]);
                if (cmd.contains("msaa_samples"))
                {
                    const int s = cmd["msaa_samples"].get<int>();
                    ctx.sceneRenderer->sceneRenderSettings.msaaProperties.sampleCount = (s <= 2) ? 2 : (s <= 4 ? 4 : 8);
                }
                if (cmd.contains("taa_blend_factor"))
                    ctx.sceneRenderer->sceneRenderSettings.taaProperties.blendFactor = std::clamp(cmd["taa_blend_factor"].get<float>(), 0.01f, 1.0f);
                if (cmd.contains("fxaa_subpix"))
                    ctx.sceneRenderer->sceneRenderSettings.fxaaProperties.subpixQuality = std::clamp(cmd["fxaa_subpix"].get<float>(), 0.0f, 1.0f);
                if (cmd.contains("fxaa_edge_threshold"))
                    ctx.sceneRenderer->sceneRenderSettings.fxaaProperties.edgeThreshold = std::clamp(cmd["fxaa_edge_threshold"].get<float>(), 0.05f, 0.5f);
                if (cmd.contains("fxaa_edge_threshold_min"))
                    ctx.sceneRenderer->sceneRenderSettings.fxaaProperties.edgeThresholdMin = std::clamp(cmd["fxaa_edge_threshold_min"].get<float>(), 0.01f, 0.2f);
                if (cmd.contains("save_game_settings") && cmd["save_game_settings"].get<bool>() && ctx.project)
                {
                    ctx.sceneRenderer->sceneRenderSettings.SerializeToFile(ctx.project->GetDirectory() / "GameSettings.ign");
                }
            }
            if (ctx.camera)
            {
                auto &pp = ctx.camera->postProcessing;
                if (cmd.contains("bloom")) pp.enableBloom = cmd["bloom"].get<bool>();
                if (cmd.contains("bloom_intensity")) pp.bloomIntensity = cmd["bloom_intensity"].get<float>();
                if (cmd.contains("ssao")) pp.enableSSAO = cmd["ssao"].get<bool>();
                if (cmd.contains("debug_ssao")) pp.debugSSAO = cmd["debug_ssao"].get<bool>();
                if (cmd.contains("ao_radius")) pp.aoRadius = cmd["ao_radius"].get<float>();
                if (cmd.contains("ao_intensity")) pp.aoIntensity = cmd["ao_intensity"].get<float>();
                if (cmd.contains("ao_bias")) pp.aoBias = cmd["ao_bias"].get<float>();
                if (cmd.contains("ao_power")) pp.aoPower = cmd["ao_power"].get<float>();
                if (cmd.contains("dof")) ctx.camera->lens.enabledDOF = cmd["dof"].get<bool>();
                if (cmd.contains("exposure")) ctx.camera->lens.exposure = cmd["exposure"].get<float>();
                if (cmd.contains("gamma")) ctx.camera->lens.gamma = cmd["gamma"].get<float>();
                if (cmd.contains("tonemap_mode") && cmd["tonemap_mode"].is_string())
                {
                    const std::string tm = cmd["tonemap_mode"].get<std::string>();
                    if (tm == "uncharted2") pp.tonemapMode = TonemapMode::Uncharted2;
                    else if (tm == "filmic" || tm == "aces" || tm == "aces_filmic") pp.tonemapMode = TonemapMode::Filmic;
                    else pp.tonemapMode = TonemapMode::Reinhard;
                }
            }
            if (ctx.scene && ctx.scene->registry)
            {
                if (cmd.contains("enable_ibl"))
                {
                    const bool ibl = cmd["enable_ibl"].get<bool>();
                    for (auto e : ctx.scene->registry->view<WorldEnvironment>())
                    {
                        ctx.scene->registry->get<WorldEnvironment>(e).enableIBL = ibl;
                    }
                }
                if (cmd.contains("soft_shadows"))
                {
                    const bool soft = cmd["soft_shadows"].get<bool>();
                    for (auto e : ctx.scene->registry->view<DirectionalLightComponent>())
                    {
                        ctx.scene->registry->get<DirectionalLightComponent>(e).softShadows = soft;
                    }
                }
            }
            if (ctx.sceneRenderer)
            {
                ctx.sceneRenderer->sceneRenderSettings.SyncToScene(ctx.scene, ctx.camera);
            }
            res["ok"] = true;
            return res;
        }

        if (action == "bake_gi")
        {
            if (!ctx.giBaker || !ctx.scene || !ctx.scene->registry)
            {
                res["error"] = "GI baker is unavailable";
                return res;
            }

            Entity volume = ResolveTargetEntity(ctx.scene, cmd, "volume");
            if (!volume || !volume.HasComponent<IrradianceVolumeComponent>())
            {
                for (entt::entity e : ctx.scene->registry->view<IrradianceVolumeComponent>())
                {
                    volume = Entity(e, ctx.scene);
                    break;
                }
            }
            if (!volume || !volume.HasComponent<IrradianceVolumeComponent>())
            {
                res["error"] = "No irradiance volume found";
                return res;
            }

            {
                auto &volComp = volume.GetComponent<IrradianceVolumeComponent>();
                if (cmd.contains("bounces")) volComp.bounces = std::clamp(cmd["bounces"].get<int>(), 1, 4);
                if (cmd.contains("resolution")) volComp.captureResolution = std::clamp(cmd["resolution"].get<int>(), 16, 128);
                if (cmd.contains("spacing")) volComp.spacing = std::max(cmd["spacing"].get<float>(), 0.25f);
            }

            std::filesystem::path outputPath = cmd.value("output_path", "");
            if (outputPath.empty() && ctx.project)
            {
                outputPath = ctx.project->GetDirectory() / "Baked" / "AgentGI"
                    / (std::to_string(static_cast<uint64_t>(volume.GetUUID())) + ".igi");
            }
            const bool started = ctx.giBaker->StartBake(ctx.scene, ctx.scene->GetAssetManager(), volume, outputPath);
            res["ok"] = started;
            res["data"] = {
                { "bake_hash", ComputeSceneBakeHash(ctx.scene) },
                { "output_path", outputPath.generic_string() },
                { "total_probes", ctx.giBaker->GetTotalProbes() }
            };
            if (!started)
                res["error"] = "Failed to start GI bake";
            return res;
        }

        if (action == "bake_reflection_probes")
        {
            if (!ctx.giBaker || !ctx.scene || !ctx.scene->registry)
            {
                res["error"] = "GI baker is unavailable";
                return res;
            }

            std::filesystem::path outputDir = cmd.value("output_dir", "");
            if (outputDir.empty() && ctx.project)
            {
                outputDir = ctx.project->GetDirectory() / "Baked" / "AgentGI";
            }

            std::vector<std::filesystem::path> savedPaths;
            const IrradianceVolume *activeVol = ctx.sceneRenderer ? ctx.sceneRenderer->GetIrradianceVolume().get() : nullptr;
            const bool ok = ctx.giBaker->BakeReflectionProbes(
                ctx.scene, ctx.scene->GetAssetManager(), activeVol, outputDir, &savedPaths);
            if (ctx.sceneRenderer)
            {
                ctx.sceneRenderer->ClearReflectionProbeSet();
            }

            nlohmann::ordered_json pathArray = nlohmann::ordered_json::array();
            for (const auto &p : savedPaths)
                pathArray.push_back(p.generic_string());

            res["ok"] = ok;
            res["data"] = {
                { "probe_count", savedPaths.size() },
                { "saved_paths", std::move(pathArray) }
            };
            if (!ok)
                res["error"] = "Failed to bake reflection probes (ensure at least one ReflectionProbe component exists)";
            return res;
        }

        if (action == "clear_baked_gi")
        {
            if (!ctx.scene || !ctx.scene->registry)
            {
                res["error"] = "No active scene";
                return res;
            }
            uint32_t clearedVolumes = 0;
            uint32_t clearedProbes = 0;
            for (entt::entity e : ctx.scene->registry->view<IrradianceVolumeComponent>())
            {
                auto &vol = ctx.scene->registry->get<IrradianceVolumeComponent>(e);
                vol.bakedAssetPath.clear();
                vol.bakeHash = 0;
                ++clearedVolumes;
            }
            for (entt::entity e : ctx.scene->registry->view<ReflectionProbeComponent>())
            {
                auto &rp = ctx.scene->registry->get<ReflectionProbeComponent>(e);
                rp.bakedAssetPath.clear();
                rp.bakeHash = 0;
                ++clearedProbes;
            }
            if (ctx.sceneRenderer)
            {
                ctx.sceneRenderer->ClearIrradianceVolume();
                ctx.sceneRenderer->ClearReflectionProbeSet();
            }
            res["ok"] = true;
            res["data"] = {
                { "cleared_volumes", clearedVolumes },
                { "cleared_probes", clearedProbes }
            };
            return res;
        }

        if (action == "get_gi_status")
        {
            if (!ctx.giBaker)
            {
                res["error"] = "GI baker is unavailable";
                return res;
            }
            uint32_t volumeCount = 0;
            uint32_t probeCount = 0;
            uint64_t sceneBakeHash = 0;
            bool outOfDate = false;
            if (ctx.scene && ctx.scene->registry)
            {
                sceneBakeHash = ComputeSceneBakeHash(ctx.scene);
                outOfDate = IsSceneGIOutOfDate(ctx.scene);
                for ([[maybe_unused]] entt::entity e : ctx.scene->registry->view<IrradianceVolumeComponent>())
                    ++volumeCount;
                for ([[maybe_unused]] entt::entity e : ctx.scene->registry->view<ReflectionProbeComponent>())
                    ++probeCount;
            }
            uint32_t bakedProbeCount = 0;
            uint32_t invalidProbes = 0;
            uint32_t relocatedProbes = 0;
            float invalidPercent = 0.0f;
            const IrradianceVolumeData *bakedDataPtr = nullptr;
            if (ctx.sceneRenderer && ctx.sceneRenderer->GetIrradianceVolume() && ctx.sceneRenderer->GetIrradianceVolume()->IsValid())
            {
                bakedDataPtr = &ctx.sceneRenderer->GetIrradianceVolume()->GetData();
            }
            else if (ctx.giBaker && ctx.giBaker->DidFinishSuccessfully())
            {
                bakedDataPtr = &ctx.giBaker->GetLastBakedData();
            }
            if (bakedDataPtr)
            {
                bakedProbeCount = bakedDataPtr->GetProbeCount();
                const size_t expectedHalfCount = static_cast<size_t>(bakedProbeCount) * 4u;
                if (bakedDataPtr->probeOffsetsHalfRGBA.size() == expectedHalfCount)
                {
                    for (uint32_t i = 0; i < bakedProbeCount; ++i)
                    {
                        const float st = glm::unpackHalf1x16(bakedDataPtr->probeOffsetsHalfRGBA[static_cast<size_t>(i) * 4u + 3u]);
                        if (st < 0.5f)
                            ++invalidProbes;
                        else if (st > 1.5f)
                            ++relocatedProbes;
                    }
                }
                else if (bakedDataPtr->shHalfRGBA[6].size() == expectedHalfCount)
                {
                    for (uint32_t i = 0; i < bakedProbeCount; ++i)
                    {
                        const float validity = glm::unpackHalf1x16(bakedDataPtr->shHalfRGBA[6][static_cast<size_t>(i) * 4u + 3u]);
                        if (validity < 0.5f)
                            ++invalidProbes;
                    }
                }
                if (bakedProbeCount > 0)
                {
                    invalidPercent = (100.0f * static_cast<float>(invalidProbes)) / static_cast<float>(bakedProbeCount);
                }
            }

            res["ok"] = true;
            res["data"] = {
                { "baking", ctx.giBaker->IsBaking() },
                { "finished", ctx.giBaker->DidFinishSuccessfully() },
                { "progress", ctx.giBaker->GetProgress() },
                { "completed_probes", ctx.giBaker->GetCompletedProbes() },
                { "total_probes", ctx.giBaker->GetTotalProbes() },
                { "baked_probe_count", bakedProbeCount },
                { "invalid_probes", invalidProbes },
                { "relocated_probes", relocatedProbes },
                { "invalid_percent", invalidPercent },
                { "scene_bake_hash", sceneBakeHash },
                { "out_of_date", outOfDate },
                { "volume_count", volumeCount },
                { "reflection_probe_count", probeCount }
            };
            return res;
        }

        if (action == "cancel_gi_bake")
        {
            if (!ctx.giBaker)
            {
                res["error"] = "GI baker is unavailable";
                return res;
            }
            ctx.giBaker->CancelBake();
            res["ok"] = true;
            return res;
        }

        if (action == "set_gi_debug_mode")
        {
            if (!ctx.sceneRenderer)
            {
                res["error"] = "SceneRenderer unavailable";
                return res;
            }
            const int mode = cmd.value("mode", RENDER_MODE_COLOR);
            if (mode != RENDER_MODE_COLOR && (mode < RENDER_MODE_GI_DIFFUSE || mode > RENDER_MODE_PROBE_WEIGHTS))
            {
                res["error"] = "GI debug mode must be 0 or in the range 9..12";
                return res;
            }
            ctx.sceneRenderer->SetRenderMode(mode);
            res["ok"] = true;
            res["data"] = { { "mode", mode } };
            return res;
        }

        if (action == "get_gpu_timings")
        {
            if (!ctx.sceneRenderer)
            {
                res["error"] = "SceneRenderer unavailable";
                return res;
            }
            res["ok"] = true;
            nlohmann::ordered_json timings = nlohmann::ordered_json::object();
            for (const auto &[name, milliseconds] : ctx.sceneRenderer->GetLastGPUTimingsMs())
                timings[name] = milliseconds;
            res["data"] = { { "timings_ms", std::move(timings) } };
            return res;
        }

        if (action == "run_lua")
        {
            if (!ctx.scene) { res["error"] = "No active scene"; return res; }
            const std::string code = cmd.value("code", "");
            if (code.empty()) { res["error"] = "Missing 'code'"; return res; }

            std::string resultStr, errStr;
            bool ok = false;
            if (auto *rt = ctx.scene->GetLuaScriptRuntime())
            {
                ok = rt->ExecuteSnippet(code, resultStr, errStr);
            }
            else
            {
                lua::ScriptRuntime tempRuntime(ctx.scene);
                ok = tempRuntime.ExecuteSnippet(code, resultStr, errStr);
            }

            res["ok"] = ok;
            if (ok)
                res["data"] = { { "result", resultStr } };
            else
                res["error"] = errStr;
            return res;
        }

        if (action == "physics_raycast")
        {
            if (!ctx.scene || !ctx.scene->GetPhysics3D()) { res["error"] = "Physics3D unavailable"; return res; }
            Ray ray;
            ray.origin = ReadJsonVec3(cmd.value("origin", nlohmann::ordered_json::array({ 0.0f, 0.0f, 0.0f })));
            const glm::vec3 dir = ReadJsonVec3(cmd.value("direction", nlohmann::ordered_json::array({ 0.0f, -1.0f, 0.0f })));
            const float len = glm::length(dir);
            if (len < 1e-6f) { res["error"] = "Ray direction must be non-zero"; return res; }
            ray.direction = dir / len;
            const float maxDist = cmd.value("max_distance", 1000.0f);
            const uint32_t layerMask = cmd.value("layer_mask", 0xFFFFFFFFu);

            RaycastHit hit;
            const bool didHit = ctx.scene->GetPhysics3D()->Raycast(ray, hit, maxDist, layerMask);
            nlohmann::ordered_json data;
            data["hit"] = didHit;
            if (didHit)
            {
                data["point"] = Vec3ToJson(hit.hitPoint);
                data["normal"] = Vec3ToJson(hit.hitNormal);
                data["fraction"] = hit.fraction;
                data["distance"] = hit.fraction * maxDist;
                data["entity_id"] = hit.userData;
                if (Entity e = SceneManager::GetEntity(ctx.scene, UUID(hit.userData)))
                    data["entity_name"] = e.GetName();
            }
            res["ok"] = true;
            res["data"] = data;
            return res;
        }

        if (action == "net_start_server")
        {
            auto *nm = NetworkManager::GetInstance();
            if (!nm) { res["error"] = "NetworkManager unavailable"; return res; }
            const uint16_t port = cmd.value("port", nm->GetPort());
            const uint16_t tickRate = cmd.value("tick_rate", uint16_t(60));
            const uint16_t snapRate = cmd.value("snapshot_rate", uint16_t(20));
            res["ok"] = nm->StartServer(port, tickRate, snapRate);
            return res;
        }

        if (action == "net_stop_server")
        {
            auto *nm = NetworkManager::GetInstance();
            if (!nm) { res["error"] = "NetworkManager unavailable"; return res; }
            nm->StopServer();
            res["ok"] = true;
            return res;
        }

        if (action == "net_connect")
        {
            auto *nm = NetworkManager::GetInstance();
            if (!nm) { res["error"] = "NetworkManager unavailable"; return res; }
            const std::string addr = cmd.value("address", "127.0.0.1");
            const uint16_t port = cmd.value("port", nm->GetPort());
            const std::string name = cmd.value("player_name", "AgentPlayer");
            res["ok"] = nm->Connect(addr, port, name);
            return res;
        }

        if (action == "net_disconnect")
        {
            auto *nm = NetworkManager::GetInstance();
            if (!nm) { res["error"] = "NetworkManager unavailable"; return res; }
            nm->Disconnect();
            res["ok"] = true;
            return res;
        }

        if (action == "net_send_input")
        {
            auto *nm = NetworkManager::GetInstance();
            if (!nm) { res["error"] = "NetworkManager unavailable"; return res; }
            const glm::vec2 move = ReadJsonVec2(cmd.value("move", nlohmann::ordered_json::array({ 0.0f, 0.0f })));
            const uint16_t buttons = cmd.value("buttons", uint16_t(0));
            const float yaw = cmd.value("yaw", 0.0f);
            nm->SendInput(move, buttons, yaw);
            res["ok"] = true;
            return res;
        }

        if (action == "net_send_chat")
        {
            auto *nm = NetworkManager::GetInstance();
            if (!nm) { res["error"] = "NetworkManager unavailable"; return res; }
            nm->SendChat(cmd.value("text", ""));
            res["ok"] = true;
            return res;
        }

        if (action == "capture_screenshot")
        {
            if (!ctx.sceneRenderer) { res["error"] = "SceneRenderer unavailable"; return res; }
            ICamera *cam = ctx.camera;
            if (!cam && ctx.scene)
            {
                if (Entity primaryCam = ctx.scene->GetPrimaryCamera())
                {
                    if (primaryCam.HasComponent<CameraComponent>())
                        cam = &primaryCam.GetComponent<CameraComponent>().camera;
                }
            }
            if (!cam) { res["error"] = "No camera render target available"; return res; }

            auto target = ctx.sceneRenderer->GetRenderTarget(cam);
            if (!target || !target->compositeRT)
            {
                res["error"] = "Camera composite render target not created yet";
                return res;
            }

            auto colorTex = target->compositeRT->GetColorAttachment(0);
            if (!colorTex)
            {
                res["error"] = "Composite color attachment unavailable";
                return res;
            }

            const uint32_t w = colorTex->GetWidth();
            const uint32_t h = colorTex->GetHeight();
            std::vector<uint8_t> pixels(static_cast<size_t>(w) * h * 4u);
            if (!colorTex->ReadPixelData(pixels.data(), pixels.size()))
            {
                res["error"] = "ReadPixelData failed on composite texture";
                return res;
            }

            std::filesystem::path outPath = cmd.value("path", "screenshot.png");
            if (outPath.extension() != ".png")
                outPath += ".png";
            std::error_code ec;
            if (outPath.has_parent_path())
                std::filesystem::create_directories(outPath.parent_path(), ec);

            const int wrote = stbi_write_png(outPath.string().c_str(), static_cast<int>(w), static_cast<int>(h), 4, pixels.data(), static_cast<int>(w * 4u));
            res["ok"] = (wrote != 0);
            res["data"] = { { "path", outPath.generic_string() }, { "width", w }, { "height", h } };
            if (!res["ok"].get<bool>())
                res["error"] = "stbi_write_png failed";
            return res;
        }

        if (action == "package_game")
        {
            PackageBuildConfig cfg;
            cfg.outputDirectory = cmd.value("output_dir", "");
            cfg.includeDedicatedServer = cmd.value("include_server", false);
            cfg.overwriteExisting = cmd.value("overwrite", true);
            cfg.includePdbs = cmd.value("include_pdbs", false);
            cfg.executableName = cmd.value("executable_name", "");

            const PackageBuildResult pkg = GamePackager::PackageProject(ctx.project, cfg);
            res["ok"] = pkg.success;
            res["data"] = {
                { "output_directory", pkg.outputDirectory.generic_string() },
                { "executable_path", pkg.executablePath.generic_string() },
                { "server_executable_path", pkg.serverExecutablePath.generic_string() },
                { "files_copied", pkg.filesCopied },
                { "total_bytes_copied", pkg.totalBytesCopied }
            };
            if (!pkg.success)
                res["error"] = pkg.errorMessage;
            return res;
        }

        if (action == "get_ui_info")
        {
            if (!ctx.scene) { res["error"] = "No active scene"; return res; }
            nlohmann::ordered_json itemsJson = nlohmann::ordered_json::array();

            std::function<void(const Ref<IWidgetItem> &)> collectItems = [&](const Ref<IWidgetItem> &item)
            {
                if (!item) return;
                nlohmann::ordered_json ij;
                ij["id"] = item->id;
                ij["name"] = item->name;
                ij["type"] = static_cast<int>(item->GetWidgetType());
                ij["visible"] = item->IsEffectivelyVisible();
                ij["world_rect"] = { item->worldRect.min.x, item->worldRect.min.y, item->worldRect.max.x, item->worldRect.max.y };
                ij["center"] = { (item->worldRect.min.x + item->worldRect.max.x) * 0.5f, (item->worldRect.min.y + item->worldRect.max.y) * 0.5f };
                if (item->GetWidgetType() == WidgetType::Button)
                {
                    auto btn = item->As<WidgetButton>();
                    ij["hovered"] = btn->hovered;
                    ij["pressed"] = btn->pressed;
                    ij["interactable"] = btn->interactable;
                    ij["text"] = btn->label ? btn->label->text : "";
                }
                else if (item->GetWidgetType() == WidgetType::Label)
                {
                    ij["text"] = item->As<WidgetLabel>()->text;
                }
                itemsJson.push_back(std::move(ij));
                for (const auto &child : item->children)
                    collectItems(child);
            };

            if (Ref<WidgetCanvas> rootWidget = ctx.scene->GetRootWidget())
            {
                if (rootWidget->GetRoot())
                    collectItems(rootWidget->GetRoot()->As<IWidgetItem>());
                if (auto *am = AssetManager::GetInstance())
                {
                    for (const WidgetChildEntry &child : rootWidget->GetChildWidgets())
                    {
                        if (!child.enabled || child.handle == AssetHandle(0)) continue;
                        if (Ref<WidgetCanvas> cc = am->GetAsset<WidgetCanvas>(child.handle))
                        {
                            if (cc->GetRoot())
                                collectItems(cc->GetRoot()->As<IWidgetItem>());
                        }
                    }
                }
            }

            const glm::vec2 gpMouse = InputSystem::GetGameplayMousePosition();
            res["ok"] = true;
            res["data"] = {
                { "cursor_mode", static_cast<int>(InputSystem::GetCursorMode()) },
                { "mouse_x", gpMouse.x },
                { "mouse_y", gpMouse.y },
                { "is_mouse_over_ui", GameUISystem::IsMouseOverUI(gpMouse.x, gpMouse.y) },
                { "hovered_item", GameUISystem::GetHoveredItemName() },
                { "items", itemsJson }
            };
            return res;
        }

        if (action == "inject_mouse_move")
        {
            const bool overrideActive = cmd.value("enabled", true);
            const float x = cmd.value("x", 0.0f);
            const float y = cmd.value("y", 0.0f);
            const float dx = cmd.value("dx", 0.0f);
            const float dy = cmd.value("dy", 0.0f);

            InputSystem::SetSyntheticMouseOverride(overrideActive, x, y);
            if (auto *sys = InputSystem::GetActiveSystem())
            {
                if (dx != 0.0f || dy != 0.0f)
                    sys->AddMouseDelta(dx, dy);
            }
            if (ctx.sceneRenderer && overrideActive)
            {
                const bool cursorEnabled = InputSystem::GetCursorMode() != CursorMode::Disabled;
                ctx.sceneRenderer->SetGameplayWidgetMousePosition(
                    static_cast<uint32_t>(std::max(0.0f, x)),
                    static_cast<uint32_t>(std::max(0.0f, y)),
                    cursorEnabled);
            }
            GameUISystem::HandleMouseMotion(x, y);
            res["ok"] = true;
            return res;
        }

        if (action == "inject_mouse_button")
        {
            const std::string btnStr = cmd.value("button", "left");
            const bool pressed = cmd.value("pressed", true);
            if (cmd.contains("x") && cmd.contains("y"))
            {
                const float x = cmd.value("x", 0.0f);
                const float y = cmd.value("y", 0.0f);
                InputSystem::SetSyntheticMouseOverride(true, x, y);
                if (ctx.sceneRenderer)
                {
                    ctx.sceneRenderer->SetGameplayWidgetMousePosition(
                        static_cast<uint32_t>(std::max(0.0f, x)),
                        static_cast<uint32_t>(std::max(0.0f, y)),
                        InputSystem::GetCursorMode() != CursorMode::Disabled);
                }
                GameUISystem::HandleMouseMotion(x, y);
            }

            MouseCode code = Mouse::ButtonLeft;
            if (btnStr == "right") code = Mouse::ButtonRight;
            else if (btnStr == "middle") code = Mouse::ButtonMiddle;

            if (auto *sys = InputSystem::GetActiveSystem())
                sys->SetMouseButton(code, pressed);
            res["ok"] = true;
            return res;
        }

        if (action == "inject_key")
        {
            const std::string keyStr = cmd.value("key", "");
            const bool pressed = cmd.value("pressed", true);
            KeyCode code = 0;
            if (keyStr == "W") code = Key::W;
            else if (keyStr == "A") code = Key::A;
            else if (keyStr == "S") code = Key::S;
            else if (keyStr == "D") code = Key::D;
            else if (keyStr == "R") code = Key::R;
            else if (keyStr == "E") code = Key::E;
            else if (keyStr == "Q") code = Key::Q;
            else if (keyStr == "Space") code = Key::Space;
            else if (keyStr == "Escape") code = Key::Escape;
            else if (keyStr == "Enter") code = Key::Enter;
            else if (keyStr == "Tab") code = Key::Tab;
            else if (keyStr == "LeftShift") code = IGNITE_KEY_LSHIFT;
            else if (keyStr == "D1") code = Key::D1;
            else if (keyStr == "D2") code = Key::D2;
            else if (keyStr == "D3") code = Key::D3;
            else if (keyStr == "F1") code = Key::F1;
            else if (keyStr == "F2") code = Key::F2;
            else if (keyStr == "F3") code = Key::F3;

            if (code == 0) { res["error"] = "Unsupported key: " + keyStr; return res; }
            if (auto *sys = InputSystem::GetActiveSystem())
                sys->SetKey(code, pressed);
            res["ok"] = true;
            return res;
        }

        if (action == "quit")
        {
            if (Application::GetInstance() && Application::GetInstance()->GetWindow())
                Application::Shutdown();
            res["ok"] = true;
            return res;
        }

        res["error"] = "Unknown command: " + action;
        return res;
    }
}
