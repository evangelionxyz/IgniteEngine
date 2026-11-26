// Copyright (c) 2026 Evangelion Manuhutu

#include "pch.hpp"
#include "scene_panel.hpp"
#include "content_browser_panel.hpp"
#include "ignite/core/signal_bus.hpp"
#include "ignite/graphics/gi/gi_baker.hpp"
#include "editor_layer.hpp"
#include "ignite/audio/fmod_sound.hpp"
#include "ignite/audio/fmod_dsp.hpp"
#include "ignite/terrain/terrain_builder.hpp"
#include "ignite/core/application.hpp"
#include "ignite/core/input/event.hpp"
#include "ignite/core/input/input_system.hpp"
#include "ignite/core/input/key_event.hpp"
#include "ignite/core/input/mouse_event.hpp"
#include "ignite/core/input/joystick_event.hpp"
#include "ignite/graphics/texture.hpp"
#include "ignite/scene/icomponent.hpp"
#include "ignite/core/platform_utils.hpp"
#include "ignite/graphics/objects/mesh.hpp"
#include "ignite/graphics/objects/material_2d.hpp"
#include "ignite/graphics/font.hpp"
#include "ignite/graphics/window.hpp"
#include "ignite/graphics/ui/widget.hpp"
#include "ignite/imgui/imgui_layer.hpp"
#include "ignite/math/math.hpp"
#include "ignite/math/transform.hpp"
#include "ignite/math/frustum.hpp"
#include "ignite/scripting/script_engine.hpp"
#include "ignite/scripting/script_field.hpp"
#include "ignite/scripting/script_instance.hpp"
#include "ignite/scripting/lua/lua_script.hpp"
#include "ignite/scripting/lua/lua_script_runtime.hpp"
#include "ignite/animation/animator/animator_controller.hpp"
#include "ignite/asset/asset_importer.hpp"
#include "ignite/core/profiler/profiler.hpp"
#include "ignite/scene/entity.hpp"
#include "ignite/scene/prefab.hpp"
#include "ignite/scene/sprite_sheet.hpp"
#include "ignite/scene/entity_command_manager.hpp"
#include "ignite/scene/entity_destroy_command.hpp"
#include "ignite/scene/entity_rename_command.hpp"
#include "ignite/scene/entity_reparent_command.hpp"
#include "ignite/scene/component_property_command.hpp"
#include "ignite/globals/globals.hpp"

#include "ignite/physics/2d/physics_2d.hpp"
#include "ignite/physics/3d/jolt/jolt_physics.hpp"

#include "ext/imgui_orientation.hpp"
#include "ext/imgui_knobs.hpp"
#include "ext/editor_ui.hpp"

#include "states.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <string_view>
#include <format>
#include <algorithm>
#include <ranges>
#include <cmath>

#ifdef _WIN32
#include <dwmapi.h>
#include <ShellScalingApi.h>
#pragma comment(lib, "Dwmapi.lib") // Link to DWM API
#pragma comment(lib, "shcore.lib")
#endif

namespace ignite
{
    namespace
    {
        void ComputeSharedGizmoPivotAndBounds(const ScenePanel *panel, const std::unordered_map<UUID, Entity> &selectedEntities, glm::vec3 &outPivot, AABB &outBounds)
        {
            outPivot = glm::vec3(0.0f);
            if (selectedEntities.empty())
            {
                outBounds = AABB(glm::vec3(0.0f), glm::vec3(1.0f));
                return;
            }

            for (Entity entity : selectedEntities | std::views::values)
            {
                outPivot += entity.GetTransform().world.translation;
            }
            outPivot /= static_cast<float>(selectedEntities.size());

            glm::vec3 boundsMin(std::numeric_limits<float>::max());
            glm::vec3 boundsMax(std::numeric_limits<float>::lowest());
            bool anyValid = false;

            for (Entity entity : selectedEntities | std::views::values)
            {
                const AABB localBounds = panel->ComputeEntityLocalBounds(entity);
                const glm::mat4 worldMat = entity.GetTransform().world.GetMatrix();
                for (int corner = 0; corner < 8; ++corner)
                {
                    const glm::vec3 localCorner(
                        (corner & 1) ? localBounds.max.x : localBounds.min.x,
                        (corner & 2) ? localBounds.max.y : localBounds.min.y,
                        (corner & 4) ? localBounds.max.z : localBounds.min.z);
                    const glm::vec3 relWorld = glm::vec3(worldMat * glm::vec4(localCorner, 1.0f)) - outPivot;
                    boundsMin = glm::min(boundsMin, relWorld);
                    boundsMax = glm::max(boundsMax, relWorld);
                    anyValid = true;
                }
            }

            if (!anyValid || glm::any(glm::lessThanEqual(boundsMax - boundsMin, glm::vec3(1e-4f))))
            {
                outBounds = AABB(glm::vec3(0.0f), glm::vec3(1.0f));
            }
            else
            {
                outBounds = AABB::FromMinMax(boundsMin, boundsMax);
            }
        }

        std::vector<UUID> FilterTopLevelUUIDs(Scene *scene, const std::vector<UUID> &inputUUIDs)
        {
            std::unordered_set<UUID> uuidSet(inputUUIDs.begin(), inputUUIDs.end());
            std::vector<UUID> result;
            result.reserve(inputUUIDs.size());

            for (UUID uuid : inputUUIDs)
            {
                Entity e = SceneManager::GetEntity(scene, uuid);
                if (!e.IsValid())
                    continue;

                bool hasSelectedAncestor = false;
                UUID curParent = e.GetComponent<IDComponent>().parent;
                while (curParent != UUID(0))
                {
                    if (uuidSet.contains(curParent))
                    {
                        hasSelectedAncestor = true;
                        break;
                    }
                    Entity parentEnt = SceneManager::GetEntity(scene, curParent);
                    if (!parentEnt.IsValid())
                        break;
                    curParent = parentEnt.GetComponent<IDComponent>().parent;
                }

                if (!hasSelectedAncestor && std::find(result.begin(), result.end(), uuid) == result.end())
                {
                    result.push_back(uuid);
                }
            }

            return result;
        }

        std::unordered_map<UUID, size_t> BuildHierarchyVisualOrderMap(Scene *scene)
        {
            std::unordered_map<UUID, size_t> orderMap;
            if (!scene)
                return orderMap;

            size_t nextIndex = 0;
            std::function<void(Entity)> visit = [&](Entity e)
            {
                if (!e.IsValid())
                    return;

                const IDComponent &id = e.GetComponent<IDComponent>();
                if (orderMap.contains(id.uuid))
                    return;

                orderMap[id.uuid] = nextIndex++;
                for (UUID childUUID : id.children)
                {
                    visit(SceneManager::GetEntity(scene, childUUID));
                }
            };

            for (UUID rootUUID : scene->rootEntityOrder)
            {
                visit(SceneManager::GetEntity(scene, rootUUID));
            }
            return orderMap;
        }

        size_t CountDescendantsRecursive(Scene *scene, Entity entity)
        {
            if (!scene || !entity.IsValid())
                return 0;

            size_t count = 0;
            const auto &children = entity.GetComponent<IDComponent>().children;
            for (UUID childUUID : children)
            {
                Entity child = SceneManager::GetEntity(scene, childUUID);
                if (child.IsValid())
                {
                    count += 1 + CountDescendantsRecursive(scene, child);
                }
            }
            return count;
        }

        struct NodeVisual
        {
            const char *icon = ICON_LC_CIRCLE_DOT;
            glm::vec4 color = UI::GetTheme().mutedForeground;
        };

        NodeVisual GetComponentVisual(CompType type)
        {
            const auto &t = UI::GetTheme();
            switch (type)
            {
                case CompType_Transform:            return { ICON_LC_MOVE_3D, UI::Accent::Camera };
                case CompType_Rendering:            return { ICON_LC_EYE, t.foreground };
                case CompType_Camera:               return { ICON_LC_CAMERA, UI::Accent::Camera };
                case CompType_Widget:               return { ICON_LC_LAYOUT_TEMPLATE, UI::Accent::Widget };
                case CompType_Sprite2D:             return { ICON_LC_IMAGE, UI::Accent::Material };
                case CompType_Animator2D:           return { ICON_LC_CLAPPERBOARD, UI::Accent::Animation };
                case CompType_Circle2D:             return { ICON_LC_CIRCLE, UI::Accent::Material };
                case CompType_PointLight2D:         return { ICON_LC_LIGHTBULB, UI::Accent::Folder };
                case CompType_Text:                 return { ICON_LC_TYPE, UI::Accent::Widget };
                case CompType_StaticMesh:           return { ICON_LC_BOX, UI::Accent::Mesh };
                case CompType_SkeletalMesh:         return { ICON_LC_BONE, UI::Accent::Animation };
                case CompType_DirectionalLight:     return { ICON_LC_SUN, UI::Accent::Folder };
                case CompType_PointLight:           return { ICON_LC_LIGHTBULB, UI::Accent::Folder };
                case CompType_SpotLight:            return { ICON_LC_FLASHLIGHT, UI::Accent::Folder };
                case CompType_WorldEnvironment:     return { ICON_LC_GLOBE, UI::Accent::Environment };
                case CompType_IrradianceVolume:     return { ICON_LC_BOXES, UI::Accent::Environment };
                case CompType_ReflectionProbe:      return { ICON_LC_RADAR, UI::Accent::Camera };
                case CompType_Terrain:              return { ICON_LC_MOUNTAIN, UI::Accent::Environment };
                case CompType_Rigidbody:
                case CompType_Rigidbody2D:          return { ICON_LC_ACTIVITY, UI::Accent::Physics };
                case CompType_BoxCollider:
                case CompType_BoxCollider2D:        return { ICON_LC_SQUARE, UI::Accent::Physics };
                case CompType_SphereCollider:
                case CompType_CircleCollider2D:     return { ICON_LC_CIRCLE, UI::Accent::Physics };
                case CompType_CapsuleCollider:      return { ICON_LC_PILL, UI::Accent::Physics };
                case CompType_MeshCollider:         return { ICON_LC_SHIELD, UI::Accent::Physics };
                case CompType_PlaneCollider:        return { ICON_LC_LAYERS, UI::Accent::Physics };
                case CompType_HeightFieldCollider:  return { ICON_LC_MOUNTAIN_SNOW, UI::Accent::Physics };
                case CompType_CharacterController:  return { ICON_LC_PERSON_STANDING, UI::Accent::Physics };
                case CompType_PhysicsJoint:         return { ICON_LC_ANCHOR, UI::Accent::Physics };
                case CompType_AudioSource:          return { ICON_LC_VOLUME_2, UI::Accent::Audio };
                case CompType_ParticleSystem:       return { ICON_LC_SPARKLES, UI::Accent::Audio };
                case CompType_Script:               return { ICON_LC_FILE_CODE_2, UI::Accent::ScriptCS };
                case CompType_LuaScript:            return { ICON_LC_FILE_CODE, UI::Accent::ScriptLua };
                case CompType_Prefab:               return { ICON_LC_PACKAGE, UI::Accent::Mesh };
                case CompType_NetworkIdentity:      return { ICON_LC_NETWORK, UI::Accent::Network };
                default:                            return { ICON_LC_SLIDERS_HORIZONTAL, t.mutedForeground };
            }
        }

        NodeVisual GetComponentVisualByName(std::string_view name)
        {
            if (name == "Transform")            return GetComponentVisual(CompType_Transform);
            if (name == "Rendering")            return GetComponentVisual(CompType_Rendering);
            if (name == "Network Identity")     return GetComponentVisual(CompType_NetworkIdentity);
            if (name == "Prefab")               return GetComponentVisual(CompType_Prefab);
            if (name == "Widget")               return GetComponentVisual(CompType_Widget);
            if (name == "World Environment")    return GetComponentVisual(CompType_WorldEnvironment);
            if (name == "Directional Light")    return GetComponentVisual(CompType_DirectionalLight);
            if (name == "Point Light")          return GetComponentVisual(CompType_PointLight);
            if (name == "Spot Light")           return GetComponentVisual(CompType_SpotLight);
            if (name == "Irradiance Volume")    return GetComponentVisual(CompType_IrradianceVolume);
            if (name == "Reflection Probe")     return GetComponentVisual(CompType_ReflectionProbe);
            if (name == "Sprite 2D")            return GetComponentVisual(CompType_Sprite2D);
            if (name == "Animator 2D")          return GetComponentVisual(CompType_Animator2D);
            if (name == "Point Light 2D")       return GetComponentVisual(CompType_PointLight2D);
            if (name == "Circle 2D")            return GetComponentVisual(CompType_Circle2D);
            if (name == "Static Mesh")          return GetComponentVisual(CompType_StaticMesh);
            if (name == "Skeletal Mesh")        return GetComponentVisual(CompType_SkeletalMesh);
            if (name == "Camera")               return GetComponentVisual(CompType_Camera);
            if (name == "Rigid Body 2D")        return GetComponentVisual(CompType_Rigidbody2D);
            if (name == "Box Collider 2D")      return GetComponentVisual(CompType_BoxCollider2D);
            if (name == "Circle Collider 2D")   return GetComponentVisual(CompType_CircleCollider2D);
            if (name == "Rigid Body")           return GetComponentVisual(CompType_Rigidbody);
            if (name == "Box Collider")         return GetComponentVisual(CompType_BoxCollider);
            if (name == "Sphere Collider")      return GetComponentVisual(CompType_SphereCollider);
            if (name == "Capsule Collider")     return GetComponentVisual(CompType_CapsuleCollider);
            if (name == "Character Controller") return GetComponentVisual(CompType_CharacterController);
            if (name == "Mesh Collider")        return GetComponentVisual(CompType_MeshCollider);
            if (name == "Plane Collider")       return GetComponentVisual(CompType_PlaneCollider);
            if (name == "HeightField Collider") return GetComponentVisual(CompType_HeightFieldCollider);
            if (name == "Physics Joint")        return GetComponentVisual(CompType_PhysicsJoint);
            if (name == "Text")                 return GetComponentVisual(CompType_Text);
            if (name == "Audio Source")         return GetComponentVisual(CompType_AudioSource);
            if (name == "C# Script")            return GetComponentVisual(CompType_Script);
            if (name == "Lua Script")           return GetComponentVisual(CompType_LuaScript);
            if (name == "Terrain")              return GetComponentVisual(CompType_Terrain);
            if (name == "Particle System")      return GetComponentVisual(CompType_ParticleSystem);
            return GetComponentVisual(CompType_Invalid);
        }

        NodeVisual GetEntityVisual(Entity entity, bool isOpen)
        {
            const auto &idComp = entity.GetComponent<IDComponent>();
            if (idComp.IsInType(EntityType_Folder))             return { isOpen ? ICON_LC_FOLDER_OPEN : ICON_LC_FOLDER, UI::Accent::Folder };
            if (entity.HasComponent<CameraComponent>())         return GetComponentVisual(CompType_Camera);
            if (entity.HasComponent<DirectionalLightComponent>()) return GetComponentVisual(CompType_DirectionalLight);
            if (entity.HasComponent<PointLightComponent>())     return GetComponentVisual(CompType_PointLight);
            if (entity.HasComponent<SpotLightComponent>())      return GetComponentVisual(CompType_SpotLight);
            if (entity.HasComponent<PointLight2DComponent>())   return GetComponentVisual(CompType_PointLight2D);
            if (entity.HasComponent<WorldEnvironment>())        return GetComponentVisual(CompType_WorldEnvironment);
            if (entity.HasComponent<IrradianceVolumeComponent>()) return GetComponentVisual(CompType_IrradianceVolume);
            if (entity.HasComponent<ReflectionProbeComponent>()) return GetComponentVisual(CompType_ReflectionProbe);
            if (entity.HasComponent<TerrainComponent>())        return GetComponentVisual(CompType_Terrain);
            if (entity.HasComponent<ParticleSystemComponent>()) return GetComponentVisual(CompType_ParticleSystem);
            if (entity.HasComponent<AudioSourceComponent>())    return GetComponentVisual(CompType_AudioSource);
            if (entity.HasComponent<SkeletalMeshComponent>())   return GetComponentVisual(CompType_SkeletalMesh);
            if (entity.HasComponent<StaticMeshComponent>())     return GetComponentVisual(CompType_StaticMesh);
            if (entity.HasComponent<WidgetComponent>())         return GetComponentVisual(CompType_Widget);
            if (entity.HasComponent<Sprite2DComponent>())       return GetComponentVisual(CompType_Sprite2D);
            if (entity.HasComponent<Circle2DComponent>())       return GetComponentVisual(CompType_Circle2D);
            if (entity.HasComponent<TextComponent>())           return GetComponentVisual(CompType_Text);
            if (idComp.IsInType(EntityType_Prefab) || entity.HasComponent<PrefabComponent>())
                return { ICON_LC_PACKAGE, UI::Accent::Mesh };
            return { ICON_LC_CIRCLE_DOT, UI::GetTheme().mutedForeground };
        }

        bool EntityHasCompType(Entity entity, CompType type)
        {
            switch (type)
            {
                case CompType_Camera:              return entity.HasComponent<CameraComponent>();
                case CompType_Sprite2D:            return entity.HasComponent<Sprite2DComponent>();
                case CompType_Animator2D:          return entity.HasComponent<Animator2DComponent>();
                case CompType_Circle2D:            return entity.HasComponent<Circle2DComponent>();
                case CompType_PointLight2D:        return entity.HasComponent<PointLight2DComponent>();
                case CompType_DirectionalLight:    return entity.HasComponent<DirectionalLightComponent>();
                case CompType_PointLight:          return entity.HasComponent<PointLightComponent>();
                case CompType_SpotLight:           return entity.HasComponent<SpotLightComponent>();
                case CompType_IrradianceVolume:    return entity.HasComponent<IrradianceVolumeComponent>();
                case CompType_ReflectionProbe:     return entity.HasComponent<ReflectionProbeComponent>();
                case CompType_Text:                return entity.HasComponent<TextComponent>();
                case CompType_Widget:              return entity.HasComponent<WidgetComponent>();
                case CompType_Rigidbody2D:         return entity.HasComponent<Rigidbody2DComponent>();
                case CompType_BoxCollider2D:       return entity.HasComponent<BoxCollider2DComponent>();
                case CompType_CircleCollider2D:    return entity.HasComponent<CircleCollider2DComponent>();
                case CompType_SkeletalMesh:        return entity.HasComponent<SkeletalMeshComponent>();
                case CompType_StaticMesh:          return entity.HasComponent<StaticMeshComponent>();
                case CompType_Rigidbody:           return entity.HasComponent<RigidbodyComponent>();
                case CompType_BoxCollider:         return entity.HasComponent<BoxColliderComponent>();
                case CompType_SphereCollider:      return entity.HasComponent<SphereColliderComponent>();
                case CompType_CapsuleCollider:     return entity.HasComponent<CapsuleColliderComponent>();
                case CompType_CharacterController: return entity.HasComponent<CharacterControllerComponent>();
                case CompType_MeshCollider:        return entity.HasComponent<MeshColliderComponent>();
                case CompType_PlaneCollider:       return entity.HasComponent<PlaneColliderComponent>();
                case CompType_Terrain:             return entity.HasComponent<TerrainComponent>();
                case CompType_HeightFieldCollider: return entity.HasComponent<HeightFieldColliderComponent>();
                case CompType_Prefab:              return entity.HasComponent<PrefabComponent>();
                case CompType_AudioSource:         return entity.HasComponent<AudioSourceComponent>();
                case CompType_WorldEnvironment:    return entity.HasComponent<WorldEnvironment>();
                case CompType_Script:              return entity.HasComponent<ScriptComponent>();
                case CompType_LuaScript:           return entity.HasComponent<LuaScriptComponent>();
                case CompType_ParticleSystem:      return entity.HasComponent<ParticleSystemComponent>();
                case CompType_PhysicsJoint:        return entity.HasComponent<PhysicsJointComponent>();
                case CompType_NetworkIdentity:     return entity.HasComponent<NetworkIdentityComponent>();
                default:                           return false;
            }
        }
    }

    UUID ScenePanel::m_TrackingSelectedEntity = UUID(0);

    ScenePanel::ScenePanel(const char *windowTitle, EditorLayer *editor)
        : IPanel(windowTitle, editor)
        , m_SceneFocused(false)
        , m_Scene(nullptr)
        , m_SceneFocusCooldown(0)
    {
        Application* app = Application::GetInstance();

        const uint32_t width = app->GetCreateInfo().width;
        const uint32_t height = app->GetCreateInfo().height;

        m_EditorCamera = EditorCamera("ScenePanel-Editor Camera");

        m_EditorCamera.SetTarget(glm::vec3(0.0f));
        m_EditorCamera.SetDistance(24.0f);
        m_EditorCamera.yaw = glm::radians(45.0f);
        m_EditorCamera.pitch = glm::radians(25.0f);
        m_EditorCamera.farPlane = 1000.0f;

        m_EditorCamera.UpdateSphericalPosition();
        m_EditorCamera.UpdateView();
        m_EditorCamera.UpdateProjection(width, height);
        m_EditorCamera.SetNavigationMode(EditorCamera::NavigationMode::Orbit);

        m_EditorCamera2D = m_EditorCamera;
        m_EditorCamera3D = m_EditorCamera;
        m_EditorCamera2D->SetNavigationMode(EditorCamera::NavigationMode::Mode2D);
        m_EditorCamera3D->SetNavigationMode(EditorCamera::NavigationMode::Orbit);

        // Load icons
        TextureCreateInfo createInfo;
        createInfo.mipLevels = 1;
        createInfo.format = nri::Format::RGBA8_UNORM;
        createInfo.initialLayout = nri::Layout::SHADER_RESOURCE;
        createInfo.initialAccess = nri::AccessBits::SHADER_RESOURCE;
        createInfo.keepInitialState = true;
        createInfo.bindless = false;

        m_Icons["camera"] = Texture::Create("resources/ui/world/ic_world_camera.png", createInfo, nullptr);
        m_Icons["lighting"] = Texture::Create("resources/ui/world/ic_world_lighting.png", createInfo, nullptr);
    }

    ScenePanel::~ScenePanel()
    {
        if (m_EditorLayer)
        {
            m_EditorLayer->m_ScenePanel = nullptr;
        }
    }

    void ScenePanel::SetActiveScene(Scene *scene)
    {
        if (m_Scene == scene)
            return;

        m_Scene = scene;
        m_SelectedEntities.clear();
        m_TrackingSelectedEntity = UUID(0);
        m_RootEntities.clear();
        m_LastEntityCount = 0;
        m_HierarchyDirty = true;
        m_PendingHierarchyDrop.reset();
        m_PendingDeleteUUIDs.clear();
        m_PendingGroupSelectionInFolder = false;
        m_PendingUngroupFolderUUID = UUID(0);
        m_RenamingEntityUUID = UUID(0);
        m_FocusRenameInput = false;
        m_LastAutoExpandedSelection = UUID(0);

        // A scene swap can happen while a gizmo drag is active.
        // Drop those entity-specific interaction handles before the next panel
        // frame can resolve them against the new scene.
        m_Data.isGizmoManipulating = false;
        m_Data.isGizmoBeingUse = false;
        m_Gizmo.ResetInteraction();
        m_ViewportData.wantMouseDragging = false;
        m_SceneFocused = false;
    }

    int ScenePanel::GetMaxParentDepth(Entity entity)
    {
        int depth = 0;
        Entity current = entity;
        while (current.HasComponent<IDComponent>() && current.GetComponent<IDComponent>().parent != UUID(0))
        {
            current = SceneManager::GetEntity(m_Scene, current.GetComponent<IDComponent>().parent);
            depth++;
        }
        return depth;
    }

    void ScenePanel::SortHierarchy(std::vector<Entity>& entities)
    {
        std::sort(entities.begin(), entities.end(), [](Entity a, Entity b)
        {
            return a.GetComponent<IDComponent>().name < b.GetComponent<IDComponent>().name;
        });

        for (auto& entity : entities)
        {
            IDComponent& id = entity.GetComponent<IDComponent>();
            if (!id.children.empty())
            {
                std::vector<Entity> children;
                for (auto childUuid : id.children)
                    children.push_back(SceneManager::GetEntity(m_Scene, childUuid));

                SortHierarchy(children);

                id.children.clear();
                for (auto& child : children)
                    id.children.push_back(child.GetUUID());
            }
        }
    }

    void ScenePanel::OnGuiRender()
    {
        IGN_PROFILE_FUNCTION();

        if (m_Scene)
        {
            {
                IGN_PROFILE_SCOPE("ScenePanel::RenderInspector");
                RenderInspector();
            }

            {
                IGN_PROFILE_SCOPE("ScenePanel::RenderHierarchy");
                RenderHierarchy();
            }
        }

        {
            IGN_PROFILE_SCOPE("ScenePanel::RenderSceneGameViewport");
            RenderSceneGameViewport();
        }

        {
            IGN_PROFILE_SCOPE("ScenePanel::RenderSceneEditViewport");
            RenderSceneEditViewport();
        }
    }

    void ScenePanel::OnUpdate(float deltaTime)
    {
        const bool hasSeparateGameplayView = m_EditorLayer && m_EditorLayer->GetState().gameplayViewportWindow && m_Data.sceneViewportGameplayVisible;
        if (m_Scene && (!m_Scene->IsPlaying() || (hasSeparateGameplayView && !m_SceneFocused)))
        {
            UpdateCameraInput(deltaTime);
        }
    }

    void ScenePanel::RenderHierarchy()
    {
        IGN_PROFILE_FUNCTION();
        const auto &theme = UI::GetTheme();
        const float scale = std::max(UI::GetDpiScale(), 1.0f);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f * scale, 8.0f * scale));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f * scale, 6.0f * scale));
        ImGui::Begin("Hierarchy");

        if (!m_Scene || !m_Scene->registry)
        {
            ImGui::End();
            ImGui::PopStyleVar(2);
            return;
        }

        auto assetManager = AssetManager::GetInstance();
        const size_t currentEntityCount = m_Scene->registry->storage<entt::entity>().size();
        if (m_HierarchyDirty || currentEntityCount != m_LastEntityCount || m_Scene != m_LastSyncedScene || (++m_HierarchySyncFrameCounter >= 60u))
        {
            SceneManager::SyncHierarchyOrder(m_Scene);
            m_HierarchyDirty = false;
            m_LastEntityCount = currentEntityCount;
            m_LastSyncedScene = m_Scene;
            m_HierarchySyncFrameCounter = 0;
        }

        // Matched-height toolbar: + Folder, + Entity popup, and Search filter
        const float barH = std::round(26.0f * scale);
        if (UI::Button("Folder", UI::ButtonVariant::Outline, UI::ButtonSize::Small, ImVec2(0.0f, barH), ICON_LC_FOLDER_PLUS))
        {
            Entity selected = GetSelectedEntity();
            Entity targetParent = (selected.IsValid() && selected.GetComponent<IDComponent>().IsInType(EntityType_Folder))
                ? selected
                : Entity{};
            CreateFolderInHierarchy(targetParent);
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        {
            ImGui::SetTooltip("Create a new Folder (nests under selected folder if any)");
        }

        ImGui::SameLine(0.0f, 4.0f * scale);
        if (UI::IconButton("##hier_create_btn", ICON_LC_PLUS, UI::ButtonVariant::Secondary, UI::ButtonSize::IconSm, "Create Entity"))
        {
            UI::OpenPopupMenu("##hier_toolbar_create_popup");
        }
        if (UI::BeginPopupMenu("##hier_toolbar_create_popup"))
        {
            UI::ContextMenuLabel("Create Entity");
            UI::ContextMenuSeparator();
            ScopedCommandTransaction tx("Create Entity");
            ShowEntityContextMenu();
            UI::EndContextMenu();
        }

        ImGui::SameLine(0.0f, 6.0f * scale);
        UI::TextField( "##hierarchy_search", m_HierarchySearchBuffer, sizeof(m_HierarchySearchBuffer), "Search hierarchy...",
            ImVec2(-1.0f, barH), ICON_LC_SEARCH, true, ImGuiInputTextFlags_EscapeClearsAll | ImGuiInputTextFlags_NoHorizontalScroll);

        // Keyboard shortcuts when Hierarchy is focused
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::GetIO().WantTextInput && !m_Scene->IsRunning())
        {
            if (ImGui::IsKeyPressed(ImGuiKey_F2, false))
            {
                if (Entity selected = GetSelectedEntity(); selected.IsValid())
                {
                    m_RenamingEntityUUID = selected.GetUUID();
                    std::memset(m_RenameBuffer, 0, sizeof(m_RenameBuffer));
                    std::strncpy(m_RenameBuffer, selected.GetName().c_str(), sizeof(m_RenameBuffer) - 1);
                    m_FocusRenameInput = true;
                }
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) && !m_SelectedEntities.empty())
            {
                m_PendingDeleteUUIDs = GetOrderedSelectedUUIDs();
            }
        }

        constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersOuter;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, UI::ToImVec4(theme.card));
        ImGui::PushStyleColor(ImGuiCol_Border, UI::ToImVec4(theme.border));
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4.0f * scale, 1.0f * scale));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f * scale, 1.0f * scale));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f * scale, 3.0f * scale));

        if (ImGui::BeginTable("entity_hierarchy_table", 1, tableFlags))
        {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_NoReorder | ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            const auto sceneName = m_EditorLayer->IsInPrefabIsolation() ? "Prefab" : assetManager->GetAssetDisplayName(m_Scene->handle);
            const std::string rootLabel = std::string("     ") + sceneName;
            const ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth
                | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_LabelSpanAllColumns | ImGuiTreeNodeFlags_DefaultOpen;

            ImGui::PushStyleColor(ImGuiCol_Header, UI::ToImVec4(theme.secondary));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, UI::ToImVec4(theme.secondaryHover));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, UI::ToImVec4(theme.secondaryActive));
            const bool sceneRootOpened = ImGui::TreeNodeEx(sceneName.c_str(), treeFlags, "%s", rootLabel.c_str());
            const ImVec2 sceneRectMin = ImGui::GetItemRectMin();
            const ImVec2 sceneRectMax = ImGui::GetItemRectMax();
            ImGui::PopStyleColor(3);

            {
                const float rowH = (std::max)(1.0f, sceneRectMax.y - sceneRectMin.y);
                const float iconX = sceneRectMin.x + ImGui::GetTreeNodeToLabelSpacing();
                const char *sceneIcon = m_EditorLayer->IsInPrefabIsolation() ? ICON_LC_PACKAGE : ICON_LC_LAYERS;
                const glm::vec4 sceneIconCol = m_EditorLayer->IsInPrefabIsolation() ? UI::Accent::Mesh : UI::Accent::Environment;
                {
                    UI::ScopedFont smallFont(UI::FontVariant::Small);
                    const ImVec2 icSz = ImGui::CalcTextSize(sceneIcon);
                    ImGui::GetWindowDrawList()->AddText(
                        ImVec2(iconX, sceneRectMin.y + std::round((rowH - icSz.y) * 0.5f)),
                        UI::ToU32(sceneIconCol),
                        sceneIcon);
                }
            }

            if (sceneRootOpened)
            {
                auto acceptRootDrop = [&]()
                {
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_ENTITY_SOURCE_ITEM))
                        {
                            const size_t count = payload->DataSize / sizeof(UUID);
                            const auto droppedUUIDs = static_cast<const UUID *>(payload->Data);
                            PendingHierarchyDrop drop;
                            drop.sourceUUIDs.assign(droppedUUIDs, droppedUUIDs + count);
                            drop.targetUUID = UUID(0);
                            drop.position = HierarchyDropPosition::Inside;
                            drop.toRoot = true;
                            m_PendingHierarchyDrop = std::move(drop);
                        }
                        else if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                        {
                            if (payload->DataSize == sizeof(AssetHandle))
                            {
                                AssetHandle handle = *static_cast<const AssetHandle *>(payload->Data);
                                AssetMetaData metadata = assetManager->GetMetaData(handle);
                                if (metadata.type == AssetType::Prefab)
                                {
                                    Ref<Prefab> prefab = assetManager->GetAsset<Prefab>(handle);
                                    if (prefab)
                                    {
                                        Entity instantiated = SceneManager::InstantiatePrefab(m_Scene, prefab);
                                        if (instantiated.IsValid())
                                        {
                                            if (!m_Scene->IsRunning())
                                            {
                                                CommandManager::AddCommand(CreateScope<EntityCreateCommand>(m_Scene, instantiated, "Instantiate Prefab"));
                                            }
                                            SetSelectedEntity(instantiated);
                                        }
                                    }
                                }
                            }
                        }

                        ImGui::EndDragDropTarget();
                    }
                };

                acceptRootDrop();

                const std::string filterLower = stringutils::ToLower(std::string(m_HierarchySearchBuffer));

                const auto &rootOrder = m_Scene->rootEntityOrder;
                for (size_t i = 0; i < rootOrder.size(); ++i)
                {
                    Entity entity = SceneManager::GetEntity(m_Scene, rootOrder[i]);
                    if (entity.IsValid() && MatchesHierarchyFilter(entity, filterLower))
                    {
                        RenderEntityNode(entity, filterLower);
                    }
                }

                // Only trigger one-shot auto-expansion when selection changes so folders remain collapsible.
                m_LastAutoExpandedSelection = m_TrackingSelectedEntity;

                // Extra root drop zone at the bottom of the hierarchy list
                ImGui::Dummy(ImVec2(-1.0f, 36.0f));
                acceptRootDrop();

                ImGui::TreePop();
            }

            // Context menu on empty hierarchy area
            if (UI::BeginWindowContextMenu("create_entity_context", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
            {
                UI::ContextMenuLabel("Hierarchy");
                UI::ContextMenuSeparator();
                if (UI::ContextMenuItem("Folder", nullptr, ICON_LC_FOLDER_PLUS, false, true, UI::Accent::Folder))
                {
                    ScopedCommandTransaction tx("Create Folder");
                    CreateFolderInHierarchy({});
                }
                UI::ContextMenuSeparator();
                {
                    ScopedCommandTransaction tx("Create Entity");
                    ShowEntityContextMenu();
                }
                UI::EndContextMenu();
            }

            ImGui::EndTable();
        }

        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);

        // Deferred hierarchy modifications executed safely outside tree iteration
        if (m_PendingHierarchyDrop.has_value())
        {
            const PendingHierarchyDrop drop = *m_PendingHierarchyDrop;
            m_PendingHierarchyDrop.reset();

            std::vector<UUID> draggedTopLevel = FilterTopLevelUUIDs(m_Scene, drop.sourceUUIDs);
            const auto visualOrder = BuildHierarchyVisualOrderMap(m_Scene);
            std::stable_sort(draggedTopLevel.begin(), draggedTopLevel.end(), [&visualOrder](UUID a, UUID b)
            {
                const auto itA = visualOrder.find(a);
                const auto itB = visualOrder.find(b);
                const size_t ordA = itA != visualOrder.end() ? itA->second : (std::numeric_limits<size_t>::max)();
                const size_t ordB = itB != visualOrder.end() ? itB->second : (std::numeric_limits<size_t>::max)();
                return ordA < ordB;
            });

            std::vector<HierarchyMoveEntry> moveEntries;
            if (drop.toRoot)
            {
                for (UUID srcUUID : draggedTopLevel)
                {
                    Entity srcEntity = SceneManager::GetEntity(m_Scene, srcUUID);
                    if (!srcEntity.IsValid())
                        continue;

                    const UUID oldParent = srcEntity.GetComponent<IDComponent>().parent;
                    const size_t oldIndex = SceneManager::GetSiblingIndex(m_Scene, srcUUID);
                    if (SceneManager::MoveEntity(m_Scene, srcUUID, UUID(0), (std::numeric_limits<size_t>::max)()))
                    {
                        const size_t newIndex = SceneManager::GetSiblingIndex(m_Scene, srcUUID);
                        if (oldParent != UUID(0) || oldIndex != newIndex)
                        {
                            moveEntries.push_back({ srcUUID, oldParent, oldIndex, UUID(0), newIndex });
                        }
                    }
                }
            }
            else if (drop.targetUUID != UUID(0))
            {
                Entity targetEntity = SceneManager::GetEntity(m_Scene, drop.targetUUID);
                if (targetEntity.IsValid())
                {
                    UUID prevAnchor = drop.targetUUID;
                    HierarchyDropPosition curPos = drop.position;
                    for (size_t idx = 0; idx < draggedTopLevel.size(); ++idx)
                    {
                        const UUID srcUUID = draggedTopLevel[idx];
                        if (srcUUID == drop.targetUUID || SceneManager::IsParent(m_Scene, drop.targetUUID, srcUUID))
                            continue;

                        Entity srcEntity = SceneManager::GetEntity(m_Scene, srcUUID);
                        if (!srcEntity.IsValid())
                            continue;

                        const UUID oldParent = srcEntity.GetComponent<IDComponent>().parent;
                        const size_t oldIndex = SceneManager::GetSiblingIndex(m_Scene, srcUUID);

                        bool moved = false;
                        if (curPos == HierarchyDropPosition::Below && idx > 0)
                        {
                            moved = SceneManager::ReorderRelativeTo(m_Scene, srcUUID, prevAnchor, HierarchyDropPosition::Below);
                        }
                        else if (curPos == HierarchyDropPosition::Above && idx > 0)
                        {
                            moved = SceneManager::ReorderRelativeTo(m_Scene, srcUUID, prevAnchor, HierarchyDropPosition::Below);
                        }
                        else
                        {
                            moved = SceneManager::ReorderRelativeTo(m_Scene, srcUUID, drop.targetUUID, curPos);
                        }

                        if (moved)
                        {
                            const UUID newParent = srcEntity.GetComponent<IDComponent>().parent;
                            const size_t newIndex = SceneManager::GetSiblingIndex(m_Scene, srcUUID);
                            if (oldParent != newParent || oldIndex != newIndex)
                            {
                                moveEntries.push_back({ srcUUID, oldParent, oldIndex, newParent, newIndex });
                            }
                            prevAnchor = srcUUID;
                        }
                    }
                }
            }

            if (!moveEntries.empty() && !m_Scene->IsRunning())
            {
                const std::string cmdName = (moveEntries.size() > 1) ? "Reorder Entities" : "Reorder Entity";
                CommandManager::AddCommand(CreateScope<EntityHierarchyMoveCommand>(m_Scene, std::move(moveEntries), cmdName));
            }
            SetHierarchyDirty();
        }

        if (!m_PendingDeleteUUIDs.empty())
        {
            std::vector<UUID> toDelete = FilterTopLevelUUIDs(m_Scene, m_PendingDeleteUUIDs);
            m_PendingDeleteUUIDs.clear();

            if (!toDelete.empty())
            {
                ScopedCommandTransaction tx(toDelete.size() > 1 ? "Delete Entities" : "Delete Entity");
                ClearSelection(true);
                for (UUID uuid : toDelete)
                {
                    Entity e = SceneManager::GetEntity(m_Scene, uuid);
                    if (e.IsValid())
                    {
                        DestroyEntity(e);
                    }
                }
            }
        }

        if (m_PendingGroupSelectionInFolder)
        {
            m_PendingGroupSelectionInFolder = false;
            GroupSelectedEntitiesInFolder();
        }

        if (m_PendingUngroupFolderUUID != UUID(0))
        {
            const UUID folderUUID = m_PendingUngroupFolderUUID;
            m_PendingUngroupFolderUUID = UUID(0);

            Entity folderEntity = SceneManager::GetEntity(m_Scene, folderUUID);
            if (folderEntity.IsValid())
            {
                const IDComponent folderID = folderEntity.GetComponent<IDComponent>();
                const UUID targetParent = folderID.parent;
                const size_t folderIndex = SceneManager::GetSiblingIndex(m_Scene, folderUUID);

                ScopedCommandTransaction tx("Ungroup Folder");
                std::vector<HierarchyMoveEntry> moveEntries;
                size_t insertCursor = folderIndex + 1;
                for (UUID childUUID : folderID.children)
                {
                    Entity childEntity = SceneManager::GetEntity(m_Scene, childUUID);
                    if (!childEntity.IsValid())
                        continue;

                    const size_t oldIdx = SceneManager::GetSiblingIndex(m_Scene, childUUID);
                    if (SceneManager::MoveEntity(m_Scene, childUUID, targetParent, insertCursor))
                    {
                        const size_t newIdx = SceneManager::GetSiblingIndex(m_Scene, childUUID);
                        moveEntries.push_back({ childUUID, folderUUID, oldIdx, targetParent, newIdx });
                        insertCursor = newIdx + 1;
                    }
                }

                if (!moveEntries.empty() && !m_Scene->IsRunning())
                {
                    CommandManager::AddCommand(CreateScope<EntityHierarchyMoveCommand>(m_Scene, std::move(moveEntries), "Ungroup Folder Children"));
                }

                ClearSelection(true);
                DestroyEntity(folderEntity);
            }
        }

        ImGui::End();
        ImGui::PopStyleVar(2);
    }

    Entity ScenePanel::ShowEntityContextMenu()
    {
        Entity entity = {};
        if (UI::ContextMenuItem("Empty Entity", nullptr, ICON_LC_CIRCLE_DOT))
        {
            entity = SetSelectedEntity(SceneManager::CreateEmptyEntity(m_Scene, "Empty"));
        }
        if (UI::ContextMenuItem("Camera", nullptr, ICON_LC_CAMERA, false, true, UI::Accent::Camera))
        {
            entity = SetSelectedEntity(SceneManager::CreateCamera(m_Scene, "Camera"));
        }
        if (UI::ContextMenuItem("Widget", nullptr, ICON_LC_LAYOUT_TEMPLATE, false, true, UI::Accent::Widget))
        {
            entity = SetSelectedEntity(SceneManager::CreateEmptyEntity(m_Scene, "Widget"));
            if (entity.IsValid() && !entity.HasComponent<WidgetComponent>())
            {
                entity.AddComponent<WidgetComponent>();
            }
        }

        if (UI::ContextMenuBeginSubMenu("2D", ICON_LC_IMAGE, true, UI::Accent::Material))
        {
            if (UI::ContextMenuItem("Sprite", nullptr, ICON_LC_IMAGE, false, true, UI::Accent::Material))
            {
                entity = SetSelectedEntity(SceneManager::CreateSprite(m_Scene, "Sprite"));
            }
            if (UI::ContextMenuItem("Circle", nullptr, ICON_LC_CIRCLE, false, true, UI::Accent::Material))
            {
                entity = SetSelectedEntity(SceneManager::CreateCircle(m_Scene, "Circle"));
            }
            if (UI::ContextMenuItem("Point Light", nullptr, ICON_LC_LIGHTBULB, false, true, UI::Accent::Folder))
            {
                entity = SetSelectedEntity(SceneManager::CreatePointLight2D(m_Scene, "Point Light 2D"));
            }
            UI::ContextMenuEndSubMenu();
        }

        if (UI::ContextMenuBeginSubMenu("3D", ICON_LC_BOX, true, UI::Accent::Mesh))
        {
            if (UI::ContextMenuBeginSubMenu("Primitives", ICON_LC_BOXES, true, UI::Accent::Mesh))
            {
                if (UI::ContextMenuItem("Cube", nullptr, ICON_LC_BOX, false, true, UI::Accent::Mesh))
                    entity = SetSelectedEntity(SceneManager::CreatePrimitive(m_Scene, EMeshType::CUBE, "Cube"));
                if (UI::ContextMenuItem("Sphere", nullptr, ICON_LC_CIRCLE, false, true, UI::Accent::Mesh))
                    entity = SetSelectedEntity(SceneManager::CreatePrimitive(m_Scene, EMeshType::SPHERE, "Sphere"));
                if (UI::ContextMenuItem("Icosphere", nullptr, ICON_LC_GLOBE, false, true, UI::Accent::Mesh))
                    entity = SetSelectedEntity(SceneManager::CreatePrimitive(m_Scene, EMeshType::ICOSPHERE, "Icosphere"));
                if (UI::ContextMenuItem("Capsule", nullptr, ICON_LC_PILL, false, true, UI::Accent::Mesh))
                    entity = SetSelectedEntity(SceneManager::CreatePrimitive(m_Scene, EMeshType::CAPSULE, "Capsule"));
                if (UI::ContextMenuItem("Cylinder", nullptr, ICON_LC_CYLINDER, false, true, UI::Accent::Mesh))
                    entity = SetSelectedEntity(SceneManager::CreatePrimitive(m_Scene, EMeshType::CYLINDER, "Cylinder"));
                if (UI::ContextMenuItem("Plane", nullptr, ICON_LC_SQUARE, false, true, UI::Accent::Mesh))
                    entity = SetSelectedEntity(SceneManager::CreatePrimitive(m_Scene, EMeshType::PLANE, "Plane"));
                if (UI::ContextMenuItem("Cone", nullptr, ICON_LC_CONE, false, true, UI::Accent::Mesh))
                    entity = SetSelectedEntity(SceneManager::CreatePrimitive(m_Scene, EMeshType::CONE, "Cone"));
                if (UI::ContextMenuItem("Torus", nullptr, ICON_LC_TORUS, false, true, UI::Accent::Mesh))
                    entity = SetSelectedEntity(SceneManager::CreatePrimitive(m_Scene, EMeshType::TORUS, "Torus"));
                UI::ContextMenuEndSubMenu();
            }
            UI::ContextMenuSeparator();
            if (UI::ContextMenuItem("Skeletal Mesh", nullptr, ICON_LC_BONE, false, true, UI::Accent::Animation))
            {
                entity = SetSelectedEntity(SceneManager::CreateEmptyEntity(m_Scene, "Skeletal Mesh"));
                if (entity.IsValid() && !entity.HasComponent<SkeletalMeshComponent>())
                {
                    entity.AddComponent<SkeletalMeshComponent>();
                }
            }
            if (UI::ContextMenuItem("Static Mesh", nullptr, ICON_LC_BOX, false, true, UI::Accent::Mesh))
            {
                entity = SetSelectedEntity(SceneManager::CreateEmptyEntity(m_Scene, "Static Mesh"));
                if (entity.IsValid() && !entity.HasComponent<StaticMeshComponent>())
                {
                    entity.AddComponent<StaticMeshComponent>();
                }
            }
            UI::ContextMenuSeparator();
            if (UI::ContextMenuItem("Directional Light", nullptr, ICON_LC_SUN, false, true, UI::Accent::Folder))
            {
                entity = SetSelectedEntity(SceneManager::CreateEmptyEntity(m_Scene, "Directional Light"));
                if (entity.IsValid() && !entity.HasComponent<DirectionalLightComponent>())
                {
                    entity.AddComponent<DirectionalLightComponent>();
                }
            }
            if (UI::ContextMenuItem("Point Light", nullptr, ICON_LC_LIGHTBULB, false, true, UI::Accent::Folder))
            {
                entity = SetSelectedEntity(SceneManager::CreateEmptyEntity(m_Scene, "Point Light"));
                if (entity.IsValid() && !entity.HasComponent<PointLightComponent>())
                {
                    entity.AddComponent<PointLightComponent>();
                }
            }
            if (UI::ContextMenuItem("Spot Light", nullptr, ICON_LC_FLASHLIGHT, false, true, UI::Accent::Folder))
            {
                entity = SetSelectedEntity(SceneManager::CreateEmptyEntity(m_Scene, "Spot Light"));
                if (entity.IsValid() && !entity.HasComponent<SpotLightComponent>())
                {
                    entity.AddComponent<SpotLightComponent>();
                }
            }
            UI::ContextMenuSeparator();
            if (UI::ContextMenuItem("World Environment", nullptr, ICON_LC_GLOBE, false, true, UI::Accent::Environment))
            {
                entity = SetSelectedEntity(SceneManager::CreateWorldEnvironment(m_Scene, "World Environment"));
            }
            if (UI::ContextMenuItem("Irradiance Volume", nullptr, ICON_LC_BOXES, false, true, UI::Accent::Environment))
            {
                entity = SetSelectedEntity(SceneManager::CreateEmptyEntity(m_Scene, "Irradiance Volume"));
                if (entity.IsValid() && !entity.HasComponent<IrradianceVolumeComponent>())
                {
                    entity.AddComponent<IrradianceVolumeComponent>();
                }
            }
            if (UI::ContextMenuItem("Reflection Probe", nullptr, ICON_LC_RADAR, false, true, UI::Accent::Camera))
            {
                entity = SetSelectedEntity(SceneManager::CreateEmptyEntity(m_Scene, "Reflection Probe"));
                if (entity.IsValid() && !entity.HasComponent<ReflectionProbeComponent>())
                {
                    entity.AddComponent<ReflectionProbeComponent>();
                }
            }
            if (UI::ContextMenuItem("Terrain", nullptr, ICON_LC_MOUNTAIN, false, true, UI::Accent::Environment))
            {
                entity = SetSelectedEntity(SceneManager::CreateEmptyEntity(m_Scene, "Terrain"));
                if (entity.IsValid() && !entity.HasComponent<TerrainComponent>())
                {
                    entity.AddComponent<TerrainComponent>();
                }
            }
            if (UI::ContextMenuItem("Physics Joint", nullptr, ICON_LC_ANCHOR, false, true, UI::Accent::Physics))
            {
                entity = SetSelectedEntity(SceneManager::CreateEmptyEntity(m_Scene, "Physics Joint"));
                if (entity.IsValid())
                {
                    if (!entity.HasComponent<RigidbodyComponent>())
                        entity.AddComponent<RigidbodyComponent>();
                    if (!entity.HasComponent<PhysicsJointComponent>())
                        entity.AddComponent<PhysicsJointComponent>();
                }
            }
            UI::ContextMenuEndSubMenu();
        }

        if (UI::ContextMenuBeginSubMenu("Effects", ICON_LC_SPARKLES, true, UI::Accent::Audio))
        {
            if (UI::ContextMenuItem("Smoke Emitter", nullptr, ICON_LC_WIND, false, true, UI::Accent::Audio))
            {
                entity = SetSelectedEntity(SceneManager::CreateEmptyEntity(m_Scene, "Smoke Particle"));
                if (entity.IsValid() && !entity.HasComponent<ParticleSystemComponent>())
                {
                    auto &comp = entity.AddComponent<ParticleSystemComponent>();
                    comp.settings = ParticleEmitterSettings::CreateSmoke();
                }
            }
            if (UI::ContextMenuItem("Fire Emitter", nullptr, ICON_LC_FLAME, false, true, UI::Accent::Audio))
            {
                entity = SetSelectedEntity(SceneManager::CreateEmptyEntity(m_Scene, "Fire Particle"));
                if (entity.IsValid() && !entity.HasComponent<ParticleSystemComponent>())
                {
                    auto &comp = entity.AddComponent<ParticleSystemComponent>();
                    comp.settings = ParticleEmitterSettings::CreateFire();
                }
            }
            if (UI::ContextMenuItem("Spark Emitter", nullptr, ICON_LC_ZAP, false, true, UI::Accent::Folder))
            {
                entity = SetSelectedEntity(SceneManager::CreateEmptyEntity(m_Scene, "Spark Particle"));
                if (entity.IsValid() && !entity.HasComponent<ParticleSystemComponent>())
                {
                    auto &comp = entity.AddComponent<ParticleSystemComponent>();
                    comp.settings = ParticleEmitterSettings::CreateSpark();
                }
            }
            if (UI::ContextMenuItem("Particle System", nullptr, ICON_LC_SPARKLES, false, true, UI::Accent::Audio))
            {
                entity = SetSelectedEntity(SceneManager::CreateEmptyEntity(m_Scene, "Particle System"));
                if (entity.IsValid() && !entity.HasComponent<ParticleSystemComponent>())
                {
                    entity.AddComponent<ParticleSystemComponent>();
                }
            }
            UI::ContextMenuEndSubMenu();
        }

        if (entity.IsValid())
        {
            SetHierarchyDirty();
        }

        return entity;
    }

    bool ScenePanel::HasSelectedDescendant(Entity current) const
    {
        if (!current.IsValid())
            return false;

        const IDComponent &currentID = current.GetComponent<IDComponent>();
        for (size_t i = 0; i < currentID.children.size(); ++i)
        {
            const UUID childUuid = currentID.children[i];
            if (m_SelectedEntities.contains(childUuid))
                return true;

            Entity child = SceneManager::GetEntity(m_Scene, childUuid);
            if (child.IsValid() && HasSelectedDescendant(child))
                return true;
        }

        return false;
    }

    void ScenePanel::RenderEntityNode(Entity entity, const std::string &filterLower)
    {
        IGN_PROFILE_FUNCTION();
        if (!entity.IsValid())
        {
            return;
        }

        static UUID s_LastAutoScrolledTarget = UUID(0);
        if (m_TrackingSelectedEntity == UUID(0))
        {
            s_LastAutoScrolledTarget = UUID(0);
        }

        const auto &theme = UI::GetTheme();
        auto &idComp = entity.GetComponent<IDComponent>();
        bool isDeleting = false;
        const bool isPrefab = idComp.IsInType(EntityType_Prefab);
        const bool isFolder = idComp.IsInType(EntityType_Folder);
        const bool isSelected = m_SelectedEntities.contains(entity.GetUUID());
        const bool isRenaming = (m_RenamingEntityUUID == idComp.uuid);
        const bool isEntityVisible = !entity.HasComponent<RenderingComponent>() || entity.GetComponent<RenderingComponent>().visible;

        if (m_TrackingSelectedEntity != UUID(0) && m_LastAutoExpandedSelection != m_TrackingSelectedEntity && HasSelectedDescendant(entity))
        {
            ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        }

        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        const ImGuiTreeNodeFlags flags = (isSelected ? ImGuiTreeNodeFlags_Selected : 0)
            | (!idComp.HasChild() ? ImGuiTreeNodeFlags_Leaf : 0)
            | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen
            | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_LabelSpanAllColumns;

        const auto imguiPushId = static_cast<intptr_t>(static_cast<uint64_t>(static_cast<uint32_t>(entity)));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, UI::ToImVec4(theme.secondary));
        ImGui::PushStyleColor(ImGuiCol_Header, UI::ToImVec4(theme.secondaryHover));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, UI::ToImVec4(theme.secondaryActive));

        std::string nodeLabel;
        if (isRenaming)
        {
            nodeLabel = fmt::format("##rename_node_{}", static_cast<uint64_t>(idComp.uuid));
        }
        else if (isFolder)
        {
            nodeLabel = fmt::format("     {}{}", idComp.name, idComp.children.empty() ? std::string() : fmt::format(" ({})", idComp.children.size()));
            ImGui::PushStyleColor(ImGuiCol_Text, UI::ToImVec4(UI::Accent::Folder));
        }
        else
        {
            nodeLabel = fmt::format("     {}", idComp.name);
            glm::vec4 textCol = isSelected ? theme.primary : (isPrefab ? UI::Accent::Mesh : theme.foreground);
            if (!isEntityVisible)
            {
                textCol = UI::WithAlpha(textCol, 0.45f);
            }
            ImGui::PushStyleColor(ImGuiCol_Text, UI::ToImVec4(textCol));
        }

        const bool opened = ImGui::TreeNodeEx((void *)imguiPushId, flags, "%s", nodeLabel.c_str());

        if (!isRenaming)
        {
            ImGui::PopStyleColor();
        }

        const ImVec2 itemRectMin = ImGui::GetItemRectMin();
        const ImVec2 itemRectMax = ImGui::GetItemRectMax();

        if (isSelected && entity.GetUUID() == m_TrackingSelectedEntity && s_LastAutoScrolledTarget != m_TrackingSelectedEntity)
        {
            if (!ImGui::IsItemVisible())
            {
                ImGui::SetScrollHereY(0.5f);
            }
            s_LastAutoScrolledTarget = m_TrackingSelectedEntity;
        }

        ImGui::PopStyleColor(3);

        // Draw crisp left selection indicator bar and semantic colored Lucide icon
        if (!isRenaming)
        {
            ImDrawList *dl = ImGui::GetWindowDrawList();
            if (isSelected)
            {
                dl->AddRectFilled(
                    ImVec2(itemRectMin.x, itemRectMin.y + 1.0f),
                    ImVec2(itemRectMin.x + 2.0f, itemRectMax.y - 1.0f),
                    UI::ToU32(theme.primary));
            }

            const NodeVisual vis = GetEntityVisual(entity, opened);
            const float rowHeight = (std::max)(1.0f, itemRectMax.y - itemRectMin.y);
            const float iconX = itemRectMin.x + ImGui::GetTreeNodeToLabelSpacing();
            const glm::vec4 iconCol = isEntityVisible ? vis.color : UI::WithAlpha(vis.color, 0.45f);
            {
                UI::ScopedFont smallFont(UI::FontVariant::Small);
                const ImVec2 icSz = ImGui::CalcTextSize(vis.icon);
                const float iconY = itemRectMin.y + std::round((rowHeight - icSz.y) * 0.5f);
                dl->AddText(ImVec2(iconX, iconY), UI::ToU32(iconCol), vis.icon);
            }
        }

        if (!m_Scene->IsRunning())
        {
            ImGui::PushID((int)imguiPushId);
            if (UI::BeginContextMenu(idComp.name.c_str()))
            {
                if (UI::ContextMenuBeginSubMenu("Create Child", ICON_LC_CIRCLE_PLUS, true, UI::Accent::Mesh))
                {
                    if (UI::ContextMenuItem("Folder", nullptr, ICON_LC_FOLDER_PLUS, false, true, UI::Accent::Folder))
                    {
                        ScopedCommandTransaction tx("Create Child Folder");
                        CreateFolderInHierarchy(entity);
                    }
                    UI::ContextMenuSeparator();

                    ScopedCommandTransaction tx("Create Child Entity");
                    if (const Entity e = ShowEntityContextMenu())
                    {
                        SceneManager::MoveEntity(m_Scene, e.GetUUID(), entity.GetUUID());
                        SetSelectedEntity(e);
                    }

                    UI::ContextMenuEndSubMenu();
                }

                UI::ContextMenuSeparator();

                if (UI::ContextMenuItem("Rename", "F2", ICON_LC_PENCIL))
                {
                    m_RenamingEntityUUID = idComp.uuid;
                    std::memset(m_RenameBuffer, 0, sizeof(m_RenameBuffer));
                    std::strncpy(m_RenameBuffer, idComp.name.c_str(), sizeof(m_RenameBuffer) - 1);
                    m_FocusRenameInput = true;
                }

                if (UI::ContextMenuItem("Duplicate", "Ctrl+D", ICON_LC_COPY, false, true, UI::Accent::Camera))
                {
                    if (!m_SelectedEntities.contains(idComp.uuid))
                    {
                        SetSelectedEntity(entity);
                    }
                    DuplicateSelectedEntity();
                }

                if (UI::ContextMenuItem("Group Selected in New Folder", nullptr, ICON_LC_FOLDER_INPUT, false, true, UI::Accent::Folder))
                {
                    if (!m_SelectedEntities.contains(idComp.uuid))
                    {
                        SetSelectedEntity(entity);
                    }
                    m_PendingGroupSelectionInFolder = true;
                }

                if (isFolder && UI::ContextMenuItem("Ungroup Children", nullptr, ICON_LC_FOLDER_OUTPUT, false, true, UI::Accent::Folder))
                {
                    m_PendingUngroupFolderUUID = idComp.uuid;
                }

                UI::ContextMenuSeparator();

                std::vector<UUID> deleteCandidates = GetOrderedSelectedUUIDs();
                if (std::find(deleteCandidates.begin(), deleteCandidates.end(), idComp.uuid) == deleteCandidates.end())
                {
                    deleteCandidates.push_back(idComp.uuid);
                }
                const auto deletString = std::format("Delete ({})", deleteCandidates.size());
                if (UI::ContextMenuItem(deletString.c_str(), "Del", ICON_LC_TRASH_2, true))
                {
                    m_PendingDeleteUUIDs = std::move(deleteCandidates);
                    isDeleting = true;
                }

                UI::EndContextMenu();
            }
            ImGui::PopID();
        }

        if (!isDeleting && !isRenaming)
        {
            // drag and drop source
            if (isPrefab == false && ImGui::BeginDragDropSource())
            {
                std::vector<UUID> selectedUUIDs;
                selectedUUIDs.reserve(m_SelectedEntities.size() + 1);
                selectedUUIDs.push_back(entity.GetUUID());

                if (m_SelectedEntities.size() > 1)
                {
                    for (auto &[uuid, selectedEntity] : m_SelectedEntities)
                    {
                        if (selectedEntity.IsValid() && uuid != entity.GetUUID())
                            selectedUUIDs.push_back(uuid);
                    }
                }

                ImGui::SetDragDropPayload(DND_PAYLOAD_ENTITY_SOURCE_ITEM,
                    selectedUUIDs.data(),
                    selectedUUIDs.size() * sizeof(UUID));

                ImGui::BeginTooltip();
                if (selectedUUIDs.size() > 1)
                {
                    ImGui::Text("%s (+%zu more)", idComp.name.c_str(), selectedUUIDs.size() - 1);
                }
                else
                {
                    ImGui::Text("%s", idComp.name.c_str());
                }
                ImGui::EndTooltip();

                ImGui::EndDragDropSource();
            }

            // 3-zone target drop (Above / Inside / Below)
            if (isPrefab == false && ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_ENTITY_SOURCE_ITEM,
                    ImGuiDragDropFlags_AcceptPeekOnly | ImGuiDragDropFlags_AcceptNoDrawDefaultRect))
                {
                    const float itemH = (std::max)(1.0f, itemRectMax.y - itemRectMin.y);
                    const float relY = (ImGui::GetMousePos().y - itemRectMin.y) / itemH;

                    HierarchyDropPosition dropPos = HierarchyDropPosition::Inside;
                    if (relY < 0.25f)
                        dropPos = HierarchyDropPosition::Above;
                    else if (relY > 0.75f)
                        dropPos = HierarchyDropPosition::Below;

                    ImDrawList *drawList = ImGui::GetWindowDrawList();
                    const ImU32 dropCol = UI::ToU32(UI::Accent::Camera);
                    if (dropPos == HierarchyDropPosition::Above)
                    {
                        drawList->AddLine(ImVec2(itemRectMin.x, itemRectMin.y), ImVec2(itemRectMax.x, itemRectMin.y), dropCol, 2.0f);
                    }
                    else if (dropPos == HierarchyDropPosition::Below)
                    {
                        drawList->AddLine(ImVec2(itemRectMin.x, itemRectMax.y), ImVec2(itemRectMax.x, itemRectMax.y), dropCol, 2.0f);
                    }
                    else
                    {
                        drawList->AddRectFilled(itemRectMin, itemRectMax, UI::ToU32(UI::WithAlpha(UI::Accent::Camera, 0.14f)));
                        drawList->AddRect(itemRectMin, itemRectMax, dropCol, 0.0f, 0, 1.5f);
                    }

                    if (payload->IsDelivery())
                    {
                        const size_t count = payload->DataSize / sizeof(UUID);
                        const auto droppedUUIDs = static_cast<const UUID *>(payload->Data);
                        PendingHierarchyDrop drop;
                        drop.sourceUUIDs.assign(droppedUUIDs, droppedUUIDs + count);
                        drop.targetUUID = idComp.uuid;
                        drop.position = dropPos;
                        drop.toRoot = false;
                        m_PendingHierarchyDrop = std::move(drop);
                    }
                }
                else if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                {
                    if (payload->DataSize == sizeof(AssetHandle))
                    {
                        AssetHandle handle = *static_cast<const AssetHandle *>(payload->Data);
                        auto am = AssetManager::GetInstance();
                        AssetMetaData metadata = am ? am->GetMetaData(handle) : AssetMetaData();
                        if (metadata.type == AssetType::Prefab)
                        {
                            Ref<Prefab> prefab = am ? am->GetAsset<Prefab>(handle) : nullptr;
                            if (prefab)
                            {
                                Entity instantiated = SceneManager::InstantiatePrefab(m_Scene, prefab, entity);
                                if (instantiated.IsValid())
                                {
                                    if (!m_Scene->IsRunning())
                                    {
                                        CommandManager::AddCommand(CreateScope<EntityCreateCommand>(m_Scene, instantiated, "Instantiate Prefab"));
                                    }
                                    SetSelectedEntity(instantiated);
                                    SetHierarchyDirty();
                                }
                            }
                        }
                    }
                }

                ImGui::EndDragDropTarget();
            }

            if (ImGui::IsItemHovered(ImGuiMouseButton_Left)
                && ImGui::IsMouseReleased(ImGuiMouseButton_Left)
                && !ImGui::IsMouseDragging(ImGuiMouseButton_Left)
                && !ImGui::IsItemToggledOpen())
            {
                SetSelectedEntity(entity);
            }
        }

        // Inline rename input overlay
        if (isRenaming)
        {
            ImGui::SameLine();
            if (m_FocusRenameInput)
            {
                ImGui::SetKeyboardFocusHere();
                m_FocusRenameInput = false;
            }
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            const bool enterPressed = ImGui::InputText(
                "##inline_entity_rename",
                m_RenameBuffer,
                sizeof(m_RenameBuffer),
                ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
            const bool cancelPressed = ImGui::IsKeyPressed(ImGuiKey_Escape, false);
            const bool deactivated = ImGui::IsItemDeactivatedAfterEdit() || ImGui::IsItemDeactivated();

            if (cancelPressed)
            {
                m_RenamingEntityUUID = UUID(0);
            }
            else if (enterPressed || deactivated)
            {
                const std::string newName(m_RenameBuffer);
                if (!newName.empty() && newName != idComp.name)
                {
                    CommandManager::ExecuteCommand(CreateScope<EntityRenameCommand>(m_Scene, idComp.uuid, idComp.name, newName));
                }
                m_RenamingEntityUUID = UUID(0);
            }
        }

        if (opened)
        {
            if (!isDeleting)
            {
                const auto &children = entity.GetComponent<IDComponent>().children;
                for (size_t i = 0; i < children.size(); ++i)
                {
                    Entity childEntity = SceneManager::GetEntity(m_Scene, children[i]);
                    if (childEntity.IsValid() && MatchesHierarchyFilter(childEntity, filterLower))
                    {
                        RenderEntityNode(childEntity, filterLower);
                    }
                }
            }

            ImGui::TreePop();
        }
    }

    void ScenePanel::RenderInspector()
    {
        IGN_PROFILE_FUNCTION();
        const auto &theme = UI::GetTheme();
        const float scale = std::max(UI::GetDpiScale(), 1.0f);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f * scale, 8.0f * scale));
        ImGui::Begin("Inspector");

        Entity selectedEntity = GetSelectedEntity();
        if (selectedEntity.IsValid())
        {
            UI::ScopedCompactInspector compactInspector;
            auto project = m_EditorLayer ? m_EditorLayer->GetActiveProject().get() : nullptr;
            auto assetManager = AssetManager::GetInstance();

            // Compact Entity Header
            auto &idComp = selectedEntity.GetComponent<IDComponent>();
            const NodeVisual entityVis = GetEntityVisual(selectedEntity, true);
            const float headerH = std::round(26.0f * scale);

            {
                const ImVec2 badgeMin = ImGui::GetCursorScreenPos();
                const ImVec2 badgeMax(badgeMin.x + headerH, badgeMin.y + headerH);
                ImGui::Dummy(ImVec2(headerH, headerH));
                ImDrawList *dl = ImGui::GetWindowDrawList();
                dl->AddRectFilled(badgeMin, badgeMax, UI::ToU32(theme.secondary));
                dl->AddRect(badgeMin, badgeMax, UI::ToU32(theme.border), 0.0f, 0, 1.0f);
                {
                    UI::ScopedFont smallFont(UI::FontVariant::Small);
                    const ImVec2 icSz = ImGui::CalcTextSize(entityVis.icon);
                    dl->AddText(
                        ImVec2(badgeMin.x + std::round((headerH - icSz.x) * 0.5f), badgeMin.y + std::round((headerH - icSz.y) * 0.5f)),
                        UI::ToU32(entityVis.color),
                        entityVis.icon);
                }
            }

            ImGui::SameLine(0.0f, 6.0f * scale);

            char buffer[255] = {};
            std::strncpy(buffer, idComp.name.c_str(), sizeof(buffer) - 1);
            const bool isFolderEntity = idComp.IsInType(EntityType_Folder);
            const float addBtnW = isFolderEntity ? 0.0f : std::round(66.0f * scale);
            const float nameFieldW = isFolderEntity
                ? -1.0f
                : std::max(60.0f, ImGui::GetContentRegionAvail().x - addBtnW - 6.0f * scale);

            if (UI::TextField("##label", buffer, sizeof(buffer), "Entity name...", ImVec2(nameFieldW, headerH), nullptr, false, ImGuiInputTextFlags_EnterReturnsTrue))
            {
                std::string oldName = idComp.name;
                std::string newName(buffer);
                if (!newName.empty() && newName != oldName)
                {
                    CommandManager::ExecuteCommand(CreateScope<EntityRenameCommand>(m_Scene, idComp.uuid, oldName, newName));
                }
            }

            if (isFolderEntity)
            {
                ImGui::Spacing();
                ImGui::SeparatorText("Folder");
                const size_t totalDescendants = CountDescendantsRecursive(m_Scene, selectedEntity);
                ImGui::Text("Direct Children: %zu", idComp.children.size());
                ImGui::Text("Total Descendants: %zu", totalDescendants);
                ImGui::Spacing();
                if (UI::Button("Ungroup Children", UI::ButtonVariant::Outline, UI::ButtonSize::Small, ImVec2(-1.0f, headerH), ICON_LC_FOLDER_OUTPUT))
                {
                    m_PendingUngroupFolderUUID = idComp.uuid;
                }
            }
            else
            {
                ImGui::SameLine(0.0f, 6.0f * scale);

                if (UI::Button("Add", UI::ButtonVariant::Primary, UI::ButtonSize::Small, ImVec2(addBtnW, headerH), ICON_LC_PLUS))
                {
                    ImGui::OpenPopup("##add_component_context");
                }

            // transform component
            RenderComponent<TransformComponent>("Transform", selectedEntity, [&]()
            {
                auto &comp = selectedEntity.GetComponent<TransformComponent>();

                // Snapshot before the user starts dragging.
                // IsItemActivated() fires on the FIRST click frame before any value changes,
                // so it must be checked AFTER the widget call, unconditionally.
                static TransformComponent s_TransformBefore;

                UI::State translationState = UI::DrawVec3Control("Translation", comp.local.translation, 0.025f, 0.0f);
                if (translationState.isItemActivated)            s_TransformBefore = comp;
                if (translationState.isItemEdited)               comp.dirty = true;
                if (translationState.isItemDeactivatedAfterEdit) CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<TransformComponent>>(m_Scene, selectedEntity.GetUUID(), s_TransformBefore, comp));

                static UUID s_RotationEditEntity = UUID(0);
                static glm::vec3 s_RotationEditEuler = glm::vec3(0.0f);
                static bool s_RotationEditing = false;

                if (s_RotationEditEntity != selectedEntity.GetUUID())
                {
                    s_RotationEditEntity = selectedEntity.GetUUID();
                    s_RotationEditEuler = eulerAngles(comp.local.rotation);
                    s_RotationEditing = false;
                }

                if (!s_RotationEditing)
                {
                    s_RotationEditEuler = eulerAngles(comp.local.rotation);
                }

                UI::State rotationState = UI::DrawVec3Control("Rotation", s_RotationEditEuler, 0.025f, 0.0f);
                if (rotationState.isItemActivated)            s_TransformBefore = comp;
                if (rotationState.isItemActivated)            s_RotationEditing = true;
                if (rotationState.isItemEdited)               { comp.local.rotation = glm::quat(s_RotationEditEuler); comp.dirty = true; }
                if (rotationState.isItemDeactivatedAfterEdit) { CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<TransformComponent>>(m_Scene, selectedEntity.GetUUID(), s_TransformBefore, comp)); s_RotationEditing = false; }

                UI::State scaleState = UI::DrawVec3Control("Scale", comp.local.scale, 0.025f, 1.0f);
                if (scaleState.isItemActivated)            s_TransformBefore = comp;
                if (scaleState.isItemEdited)               comp.dirty = true;
                if (scaleState.isItemDeactivatedAfterEdit) CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<TransformComponent>>(m_Scene, selectedEntity.GetUUID(), s_TransformBefore, comp));
            }, false); // false: not allowed to remove the component

            // Rendering component
            RenderComponent<RenderingComponent>("Rendering", selectedEntity, [&]()
            {
                auto &comp = selectedEntity.GetComponent<RenderingComponent>();
                UI::DrawSwitch("Visible", &comp.visible);
                UI::DrawSwitch("Cast Shadow", &comp.castShadow);
                UI::DrawSwitch("Receive Shadow", &comp.receiveShadow);
                UI::DrawSwitch("Contribute GI", &comp.contributeGI);
                UI::DrawSwitch("Receive GI", &comp.receiveGI);

                const char *mobilityLabels[] = { "Static", "Movable" };
                int mobilityIdx = static_cast<int>(comp.mobility);
                if (UI::DrawComboBox("Mobility", mobilityLabels, IM_ARRAYSIZE(mobilityLabels), &mobilityIdx))
                {
                    comp.mobility = static_cast<EntityMobility>(std::clamp(mobilityIdx, 0, 1));
                    if (m_Scene) m_Scene->SetDirtyFlag(true);
                }
            }, false); // false: not allowed to remove the component

            // Network Identity component
            RenderComponent<NetworkIdentityComponent>("Network Identity", selectedEntity, [&]()
            {
                auto &comp = selectedEntity.GetComponent<NetworkIdentityComponent>();

                // Prefab / Template Identifier
                UI::DrawTextControl("Prefab", comp.prefab, "Prefab name...", [&]()
                {
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                        {
                            if (payload->DataSize == sizeof(AssetHandle))
                            {
                                AssetHandle handle = *static_cast<const AssetHandle *>(payload->Data);
                                AssetMetaData metadata = assetManager->GetMetaData(handle);
                                if (metadata.type == AssetType::Prefab)
                                {
                                    comp.prefab = metadata.filepath.stem().string();
                                }
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }
                });

                UI::DrawSwitch("Is Local", &comp.isLocalPlayer);
                UI::DrawSwitch("Replicate", &comp.replicate);

                if (ImGui::CollapsingHeader("Replicated Components", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto DrawCompMaskCheckbox = [ & ](const char *label, NetCompFlag flag)
                    {
                        bool enabled = (comp.syncComponentsMask & flag) != 0;
                        if (UI::DrawSwitch(label, &enabled))
                        {
                            if (enabled) comp.syncComponentsMask |= flag;
                            else comp.syncComponentsMask &= ~flag;
                            comp.MarkDirty(flag);
                        }
                    };

                    DrawCompMaskCheckbox("Point Light", NetComp_PointLight);
                    DrawCompMaskCheckbox("Directional Light", NetComp_DirectionalLight);
                    DrawCompMaskCheckbox("Spot Light", NetComp_SpotLight);
                    DrawCompMaskCheckbox("Sprite 2D", NetComp_Sprite2D);
                    DrawCompMaskCheckbox("Circle 2D", NetComp_Circle2D);
                    DrawCompMaskCheckbox("Text", NetComp_Text);
                    DrawCompMaskCheckbox("Static Mesh", NetComp_StaticMesh);
                    DrawCompMaskCheckbox("Audio Source", NetComp_AudioSource);
                    DrawCompMaskCheckbox("Rigidbody", NetComp_Rigidbody);
                    DrawCompMaskCheckbox("Camera", NetComp_Camera);
                    DrawCompMaskCheckbox("Character Controller", NetComp_CharacterController);

                    if (UI::Button("Sync Components to Clients", UI::ButtonVariant::Outline, UI::ButtonSize::Small, ImVec2(-1.0f, 22.0f), ICON_LC_REFRESH_CW))
                    {
                        comp.MarkDirty(comp.syncComponentsMask);
                    }
                }
            });

            RenderComponent<PrefabComponent>("Prefab", selectedEntity, [ & ]()
            {
                auto &c = selectedEntity.GetComponent<PrefabComponent>();
                const bool isPrefabValid = c.prefabHandle != AssetHandle(0);
                std::string prefabName = isPrefabValid ? assetManager->GetAssetDisplayName(c.prefabHandle) : "Drag Prefab Asset Here";

                UI::DrawButtonWithColumn("Prefab Asset", prefabName.c_str(), nullptr, [ & ]()
                {
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                        {
                            if (payload->DataSize == sizeof(AssetHandle))
                            {
                                AssetHandle handle = *static_cast<const AssetHandle *>(payload->Data);
                                AssetMetaData metadata = assetManager->GetMetaData(handle);
                                if (metadata.type == AssetType::Prefab)
                                {
                                    c.prefabHandle = handle;
                                    SceneManager::SyncAllPrefabInstances(m_Scene, handle);
                                }
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }

                    if (c.prefabHandle != AssetHandle(0))
                    {
                        ImGui::SameLine(0.0f, 4.0f);
                        if (UI::IconButton("##ClearPrefab", ICON_LC_X, UI::ButtonVariant::Outline, UI::ButtonSize::IconSm, "Clear Prefab"))
                        {
                            c.prefabHandle = AssetHandle(0);
                        }
                    }
                });

                if (UI::Button("Apply", UI::ButtonVariant::Primary, UI::ButtonSize::Small, ImVec2(0.0f, 22.0f), ICON_LC_CHECK))
                {
                    SceneManager::ApplyPrefabChanges(selectedEntity, m_Scene, m_EditorLayer ? m_EditorLayer->GetActiveProject().get() : nullptr);
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                {
                    ImGui::SetTooltip("Save local changes to .ixprefab file and sync all scene instances.");
                }

                ImGui::SameLine(0.0f, 4.0f);
                if (UI::Button("Revert", UI::ButtonVariant::Outline, UI::ButtonSize::Small, ImVec2(0.0f, 22.0f), ICON_LC_ROTATE_CCW))
                {
                    if (isPrefabValid)
                    {
                        SceneManager::SyncAllPrefabInstances(m_Scene, c.prefabHandle);
                    }
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                {
                    ImGui::SetTooltip("Discard local changes and reload from .ixprefab asset.");
                }

                ImGui::SameLine(0.0f, 4.0f);
                if (UI::Button("Unpack", UI::ButtonVariant::Outline, UI::ButtonSize::Small, ImVec2(0.0f, 22.0f), ICON_LC_UNLINK))
                {
                    selectedEntity.RemoveComponent<PrefabComponent>();
                    SetSelectedEntity(selectedEntity);
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                {
                    ImGui::SetTooltip("Break link to prefab asset and turn entity into normal scene object.");
                }

                ImGui::SameLine(0.0f, 4.0f);
                if (UI::Button("Open", UI::ButtonVariant::Secondary, UI::ButtonSize::Small, ImVec2(0.0f, 22.0f), ICON_LC_EXTERNAL_LINK))
                {
                    if (isPrefabValid && m_EditorLayer)
                    {
                        m_EditorLayer->EnterPrefabIsolation(c.prefabHandle);
                    }
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                {
                    ImGui::SetTooltip("Open prefab asset in Isolation Mode editor.");
                }
            });

            RenderComponent<WidgetComponent>("Widget", selectedEntity, [ & ]()
            {
                auto &c = selectedEntity.GetComponent<WidgetComponent>();

                const bool isWidgetLoaded = c.widgetHandle != AssetHandle(0);
                std::string label = isWidgetLoaded ? assetManager->GetAssetDisplayName(c.widgetHandle) : "Drag Here";
                UI::DrawButtonWithColumn("Widget", label.c_str(), nullptr, [ &, this ]()
                {
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                        {
                            LOG_ASSERT(payload->DataSize == sizeof(AssetHandle), "WRONG ITEM, that should be an asset handle");
                            AssetHandle handle = *static_cast<AssetHandle *>(payload->Data);
                            AssetMetaData metadata = assetManager->GetMetaData(handle);
                            if (metadata.type == AssetType::Widget)
                            {
                                c.widgetHandle = handle;
                                c.dirty = true;
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }

                    if (isWidgetLoaded)
                    {
                        ImGui::SameLine(0.0f, 4.0f);
                        if (UI::IconButton("##ClearWidget", ICON_LC_X, UI::ButtonVariant::Outline, UI::ButtonSize::IconSm, "Clear Widget"))
                        {
                            c.widgetHandle = AssetHandle(0);
                            c.dirty = true;
                        }
                    }
                });
            });

            RenderComponent<WorldEnvironment>("World Environment", selectedEntity, [ & ]()
            {
                auto &c = selectedEntity.GetComponent<WorldEnvironment>();

                const char *skyTypes[] = { "HDRI", "Procedural Sky" };
                int currentType = static_cast<int>(c.skyType);
                if (UI::DrawComboBox("Sky Type", skyTypes, 2, &currentType))
                {
                    c.skyType = static_cast<SkyType>(currentType);
                    c.dirtyEnvironment = true;
                }

                UI::DrawSwitch("IBL Environment Lighting", &c.enableIBL);
                UI::DrawFloatControl("Exposure", &c.exposure, 0.025f, 0.1f, 4.0f);
                UI::DrawFloatControl("Gamma", &c.gamma, 0.025f, 1.4f, 2.8f);
                UI::DrawFloatControl("Ambient", &c.ambient, 0.025f, 0.0f, FLT_MAX);

                // Fog
                UI::DrawFloatControl("Fog Density", &c.fogDensity, 0.01f, 0.0f, FLT_MAX);
                if (c.fogDensity > 0.0f)
                {
                    UI::DrawColorVec4("Fog Color", c.fogColor);
                    UI::DrawFloatControl("Fog Start", &c.fogStart, 0.1f, 0.0f, FLT_MAX);
                    UI::DrawFloatControl("Fog End", &c.fogEnd, 0.1f, 0.0f, FLT_MAX);
                }

                if (c.skyType == SkyType::HDRI)
                {
                    const bool hasHDR = c.hdrHandle != AssetHandle(0);
                    std::string buttonLabel = hasHDR ? assetManager->GetAssetDisplayName(c.hdrHandle) : "Drag Here";
                    UI::DrawButtonWithColumn("HDR", buttonLabel.c_str(), nullptr, [ &c, assetManager, this, &hasHDR ]()
                    {
                        if (ImGui::BeginDragDropTarget())
                        {
                            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                            {
                                LOG_ASSERT(payload->DataSize == sizeof(AssetHandle), "WRONG ITEM, that should be an asset handle");
                                AssetHandle handle = *static_cast<AssetHandle *>(payload->Data);
                                AssetMetaData metadata = assetManager->GetMetaData(handle);
                                if (metadata.type == AssetType::Texture && metadata.filepath.extension() == ".hdr")
                                {
                                    c.hdrHandle = handle;
                                    c.dirtyEnvironment = true;
                                    c.gpuInitialized = false;
                                }
                            }
                            ImGui::EndDragDropTarget();
                        }

                        if (hasHDR)
                        {
                            ImGui::SameLine(0.0f, 4.0f);
                            if (UI::IconButton("##ClearHDR", ICON_LC_X, UI::ButtonVariant::Outline, UI::ButtonSize::IconSm, "Clear HDR"))
                            {
                                c.hdrHandle = AssetHandle(0);
                                c.dirtyEnvironment = true;
                                c.gpuInitialized = false;
                            }
                        }
                    });
                }
                else if (c.skyType == SkyType::ProceduralSky)
                {
                    if (ImGui::TreeNodeEx("Atmosphere Parameters", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        auto &atmo = c.atmosphereParams;
                        bool modified = false;

                        ImGui::TextDisabled("Scattering values use 1e-3 per km units.");
                        modified |= UI::DrawVec3Control("Rayleigh Scattering (x10^-3 / km)", atmo.rayleighScattering);
                        modified |= UI::DrawVec3Control("Mie Scattering (x10^-3 / km)", atmo.mieScattering);
                        modified |= UI::DrawFloatControl("Rayleigh Scale Height (km)", &atmo.rayleighDensityH, 0.025f, 0.1f, 50.0f);
                        modified |= UI::DrawFloatControl("Mie Scale Height (km)", &atmo.mieDensityH, 0.025f, 0.1f, 20.0f);
                        modified |= UI::DrawFloatControl("Mie Anisotropy (g)", &atmo.mieG, 0.01f, 0.0f, 0.99f);
                        modified |= UI::DrawFloatControl("Planet Radius (km)", &atmo.planetRadius, 10.0f, 100.0f, 100000.0f);
                        modified |= UI::DrawFloatControl("Atmosphere Radius (km)", &atmo.atmosphereRadius, 10.0f, 100.0f, 100000.0f);

                        if (UI::DrawColorVec3("Ground Albedo", atmo.groundAlbedo))
                        {
                            modified = true;
                        }

                        if (UI::Button("Reset to Earth Default", UI::ButtonVariant::Outline, UI::ButtonSize::Small, ImVec2(-1.0f, 22.0f), ICON_LC_ROTATE_CCW))
                        {
                            atmo = AtmosphereParams();
                            modified = true;
                        }

                        if (modified && c.environment && c.environment->GetProceduralSky())
                        {
                            c.environment->GetProceduralSky()->MarkDirty();
                        }

                        ImGui::TreePop();
                    }
                }
            });

            RenderComponent<DirectionalLightComponent>("Directional Light", selectedEntity, [ & ]()
            {
                auto &c = selectedEntity.GetComponent<DirectionalLightComponent>();
                auto &tr = selectedEntity.GetComponent<TransformComponent>();

                UI::DrawColorVec4("Color", c.color);
                UI::DrawFloatControl("Intensity", &c.intensity, 0.01f, 0.0f, 100.0f);
                UI::DrawFloatControl("Angular Radius", &c.angularRadius, 0.01f, 0.0f, 45.0f);

                const glm::vec3 sunDirection = glm::normalize(tr.world.rotation * glm::vec3(0.0f, 0.0f, 1.0f));
                const float azimuth = std::atan2(sunDirection.x, sunDirection.z);
                const float elevation = std::asin(glm::clamp(sunDirection.y, -1.0f, 1.0f));

                ImGui::TextDisabled("Azimuth: %.3f rad", azimuth);
                ImGui::TextDisabled("Elevation: %.3f rad", elevation);

                ImGui::SeparatorText("Shadow");
                UI::DrawSwitch("Cascade Shadow", &c.cascadeShadow);
                UI::DrawSwitch("Bake Indirect", &c.bakeIndirect);

                ImGui::BeginDisabled(!c.cascadeShadow);
                {
                    UI::DrawSwitch("Soft Shadow Maps", &c.softShadows);
                    UI::DrawFloatControl("Strength", &c.shadowStrength, 0.01f, 0.0f, 1.0f);
                    UI::DrawFloatControl("Min Bias", &c.shadowMinBias, 0.0001f, 0.0f, 0.1f);
                    UI::DrawFloatControl("Max Bias", &c.shadowMaxBias, 0.0001f, 0.0f, 0.1f);
                    UI::DrawFloatControl("PCF Radius", &c.pcfRadius, 0.01f, 0.0f, 8.0f);
                    UI::DrawFloatControl("Shadow Distance", &c.shadowDistance, 1.0f, 10.0f, 2000.0f);

                    static const char *resolutionLabels[] =
                    {
                        "Low - 512px",
                        "Medium - 1024px",
                        "High - 2048px",
                        "Ultra - 4096px",
                        "Ultimate - 8192px"
                    };

                    UI::DrawComboBox("Resolution", resolutionLabels, IM_ARRAYSIZE(resolutionLabels), &c.shadowResolution);

                    // if (auto sceneRenderer = m_Scene->GetSceneRenderer())
                    // {
                    //     auto shadowMap = sceneRenderer->GetCascadedShadowMapDepthTexture();
                    //     if (shadowMap)
                    //     {
                    //         ImTextureID texId = UI::GetTextureID(shadowMap);
                    //         ImGui::Image(texId, { 256, 256 });
                    //     }
                    // }
                }
                ImGui::EndDisabled();
            });

            RenderComponent<PointLightComponent>("Point Light", selectedEntity, [ & ]()
            {
                auto &c = selectedEntity.GetComponent<PointLightComponent>();

                UI::DrawSwitch("Enabled", &c.enabled);
                UI::DrawSwitch("Bake Indirect", &c.bakeIndirect);
                UI::DrawColorVec4("Color", c.color);
                UI::DrawFloatControl("Intensity", &c.intensity, 0.01f, 0.0f, 100.0f);
                UI::DrawFloatControl("Range", &c.range, 0.1f, 0.0f, 1000.0f);

                ImGui::SeparatorText("Attenuation");
                UI::DrawFloatControl("Constant", &c.constantAttenuation, 0.01f, 0.0f, 10.0f);
                UI::DrawFloatControl("Linear", &c.linearAttenuation, 0.001f, 0.0f, 10.0f);
                UI::DrawFloatControl("Quadratic", &c.quadraticAttenuation, 0.0001f, 0.0f, 10.0f);

                ImGui::SeparatorText("Shadow");
                UI::DrawSwitch("Cast Shadow", &c.castShadow);
                ImGui::BeginDisabled(!c.castShadow);
                UI::DrawSwitch("Bake Static Shadow", &c.bakeStaticShadow);
                UI::DrawSwitch("Soft Shadows", &c.softShadows);
                UI::DrawFloatControl("Shadow Strength", &c.shadowStrength, 0.01f, 0.0f, 1.0f);
                UI::DrawFloatControl("Shadow Bias", &c.shadowBias, 0.0005f, 0.0f, 0.1f);
                UI::DrawFloatControl("PCF Radius", &c.pcfRadius, 0.001f, 0.0f, 0.15f);
                ImGui::EndDisabled();
            });

            RenderComponent<SpotLightComponent>("Spot Light", selectedEntity, [ & ]()
            {
                auto &c = selectedEntity.GetComponent<SpotLightComponent>();

                UI::DrawSwitch("Enabled", &c.enabled);
                UI::DrawSwitch("Bake Indirect", &c.bakeIndirect);
                UI::DrawColorVec4("Color", c.color);
                UI::DrawFloatControl("Intensity", &c.intensity, 0.01f, 0.0f, 100.0f);
                UI::DrawFloatControl("Range", &c.range, 0.1f, 0.0f, 1000.0f);

                ImGui::SeparatorText("Attenuation");
                UI::DrawFloatControl("Constant", &c.constantAttenuation, 0.01f, 0.0f, 10.0f);
                UI::DrawFloatControl("Linear", &c.linearAttenuation, 0.001f, 0.0f, 10.0f);
                UI::DrawFloatControl("Quadratic", &c.quadraticAttenuation, 0.0001f, 0.0f, 10.0f);

                ImGui::SeparatorText("Cone angles");
                UI::DrawFloatControl("Inner Angle", &c.innerConeAngle, 0.1f, 0.0f, 90.0f);
                UI::DrawFloatControl("Outer Angle", &c.outerConeAngle, 0.1f, 0.0f, 90.0f);

                ImGui::SeparatorText("Shadow");
                UI::DrawSwitch("Cast Shadow", &c.castShadow);
                ImGui::BeginDisabled(!c.castShadow);
                UI::DrawSwitch("Bake Static Shadow", &c.bakeStaticShadow);
                UI::DrawSwitch("Soft Shadows", &c.softShadows);
                UI::DrawFloatControl("Shadow Strength", &c.shadowStrength, 0.01f, 0.0f, 1.0f);
                UI::DrawFloatControl("Shadow Bias", &c.shadowBias, 0.0005f, 0.0f, 0.1f);
                UI::DrawFloatControl("PCF Radius", &c.pcfRadius, 0.001f, 0.0f, 0.15f);
                ImGui::EndDisabled();
            });

            RenderComponent<IrradianceVolumeComponent>("Irradiance Volume", selectedEntity, [ & ]()
            {
                auto &c = selectedEntity.GetComponent<IrradianceVolumeComponent>();
                const auto &tr = selectedEntity.GetComponent<TransformComponent>();

                UI::DrawVec3Control("Half Extents", c.extents, 0.1f, 10.0f);
                UI::DrawFloatControl("Probe Spacing (m)", &c.spacing, 0.05f, 0.25f, 16.0f);
                UI::DrawIntControl("Bounces", &c.bounces, 1, 1, 4);
                UI::DrawIntControl("Capture Resolution", &c.captureResolution, 1, 16, 128);
                UI::DrawIntControl("SH Order (1-2)", &c.shOrder, 1, 1, 2);

                const char *modeNames[] = { "Off", "Fast", "Leak Resistant" };
                int modeIdx = std::clamp(c.runtimeMode, 0, 2);
                if (UI::DrawComboBox("Runtime Mode", modeNames, 3, &modeIdx))
                {
                    c.runtimeMode = modeIdx;
                }
                UI::DrawSwitch("Sky Visibility Split", &c.skyVisibilitySplit);

                UI::DrawFloatControl("Normal Bias", &c.normalBias, 0.01f, 0.0f, 2.0f);
                UI::DrawFloatControl("View Bias", &c.viewBias, 0.01f, 0.0f, 2.0f);
                UI::DrawFloatControl("Edge Fade (m)", &c.edgeFade, 0.05f, 0.0f, 32.0f);
                UI::DrawFloatControl("Intensity", &c.intensity, 0.05f, 0.0f, 10.0f);
                UI::DrawIntControl("Priority", &c.priority, 1, -100, 100);

                const glm::vec3 halfExt = glm::max(glm::abs(c.extents * tr.world.scale), glm::vec3(0.5f));
                const float safeSpacing = std::max(c.spacing, 0.25f);
                const glm::ivec3 estDims = glm::clamp(
                    glm::ivec3(glm::ceil((halfExt * 2.0f) / safeSpacing)) + glm::ivec3(1),
                    glm::ivec3(2, 2, 2),
                    glm::ivec3(64, 32, 64));
                ImGui::Text("Probe Grid: %dx%dx%d (%d probes)",
                    estDims.x, estDims.y, estDims.z, estDims.x * estDims.y * estDims.z);
                ImGui::TextWrapped("Baked File: %s", c.bakedAssetPath.empty() ? "(not baked)" : c.bakedAssetPath.c_str());
                const uint64_t currentBakeHash = ComputeSceneBakeHash(m_Scene);
                const bool bakeOutOfDate = c.bakeHash == 0 || c.bakeHash != currentBakeHash;
                ImGui::TextColored(bakeOutOfDate ? ImVec4(1.0f, 0.55f, 0.2f, 1.0f) : ImVec4(0.35f, 0.9f, 0.4f, 1.0f),
                    bakeOutOfDate ? "GI out of date" : "GI up to date");
            });

            RenderComponent<ReflectionProbeComponent>("Reflection Probe", selectedEntity, [ & ]()
            {
                auto &c = selectedEntity.GetComponent<ReflectionProbeComponent>();
                UI::DrawVec3Control("Influence Extents", c.influenceExtents, 0.1f, 5.0f);
                UI::DrawVec3Control("Projection Extents", c.projectionExtents, 0.1f, 5.0f);
                if (UI::Button("Fit Projection to Influence", UI::ButtonVariant::Outline, UI::ButtonSize::Small, ImVec2(-1.0f, 22.0f)))
                {
                    c.projectionExtents = c.influenceExtents;
                }
                UI::DrawVec3Control("Capture Offset", c.captureOffset, 0.05f, 0.0f);
                UI::DrawFloatControl("Blend Distance (m)", &c.blendDistance, 0.05f, 0.0f, 32.0f);
                UI::DrawFloatControl("Exclude Self Radius (m)", &c.excludeRadius, 0.05f, 0.0f, 10.0f);
                UI::DrawIntControl("Resolution", &c.resolution, 16, 32, 256);
                UI::DrawIntControl("Priority", &c.priority, 1, -100, 100);
                UI::DrawFloatControl("Intensity", &c.intensity, 0.05f, 0.0f, 10.0f);
                UI::DrawSwitch("Box Projection", &c.boxProjection);
                UI::DrawSwitch("Normalize To GI", &c.normalize);
                if (c.normalize)
                {
                    UI::DrawFloatControl("Normalization Strength", &c.normalizationStrength, 0.05f, 0.0f, 1.0f);
                }
                ImGui::TextWrapped("Baked Cube: %s", c.bakedAssetPath.empty() ? "(not baked)" : c.bakedAssetPath.c_str());

                bool hasBakedFile = false;
                if (!c.bakedAssetPath.empty())
                {
                    std::filesystem::path resolved(c.bakedAssetPath);
                    if (resolved.is_relative() && m_Scene && m_Scene->GetProject())
                    {
                        resolved = m_Scene->GetProject()->GetDirectory() / resolved;
                    }
                    std::error_code ec;
                    hasBakedFile = std::filesystem::exists(resolved, ec);
                }
                const uint64_t sceneHash = ComputeSceneBakeHash(m_Scene);
                const bool upToDate = hasBakedFile && c.bakeHash != 0 && c.bakeHash == sceneHash;
                ImGui::TextColored(upToDate ? ImVec4(0.35f, 0.9f, 0.4f, 1.0f) : ImVec4(1.0f, 0.55f, 0.2f, 1.0f),
                    upToDate ? "Reflection probe up to date" : (hasBakedFile ? "Reflection probe out of date" : "Reflection probe not baked"));
            });

            RenderComponent<Sprite2DComponent>("Sprite 2D", selectedEntity, [ & ]()
            {
                auto &c = selectedEntity.GetComponent<Sprite2DComponent>();

                static Sprite2DComponent s_Sprite2DBefore;

                // Material 2D
                bool isMat2dLoaded = c.materialHandle != AssetHandle(0);
                std::string mat2dLabel = isMat2dLoaded ? assetManager->GetAssetDisplayName(c.materialHandle) : "Drag Here";
                UI::DrawButtonWithColumn("Material", mat2dLabel.c_str(), nullptr, [ &c, &selectedEntity, &isMat2dLoaded, assetManager, this ]()
                {
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                        {
                            LOG_ASSERT(payload->DataSize == sizeof(AssetHandle), "WRONG ITEM, that should be an asset handle");
                            AssetHandle handle = *static_cast<AssetHandle *>(payload->Data);
                            AssetType type = assetManager->GetAssetType(handle);
                            if (type == AssetType::Material2D)
                            {
                                Sprite2DComponent before = c;
                                c.materialHandle = handle;
                                CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<Sprite2DComponent>>(m_Scene, selectedEntity.GetUUID(), before, c));
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }

                    if (isMat2dLoaded)
                    {
                        ImGui::SameLine(0.0f, 4.0f);
                        if (UI::IconButton("##ClearMat2D", ICON_LC_X, UI::ButtonVariant::Outline, UI::ButtonSize::IconSm, "Clear Material 2D"))
                        {
                            Sprite2DComponent before = c;
                            c.materialHandle = AssetHandle(0);
                            CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<Sprite2DComponent>>(m_Scene, selectedEntity.GetUUID(), before, c));
                        }
                    }
                });

                // Get material 2d
                Ref<Material2D> mat2d = nullptr;
                if (isMat2dLoaded)
                {
                    assetManager->GetAsset<Material2D>(c.materialHandle);
                }

                if (!isMat2dLoaded)
                {
                    // Texture on sprite 2d
                    const bool isTextureLoaded = c.handle != AssetHandle(0);
                    const std::string textureLabel = isTextureLoaded ? assetManager->GetAssetDisplayName(c.handle) : "Drag Here";
                    UI::DrawButtonWithColumn("Texture", textureLabel.c_str(), nullptr, [ &c, &isTextureLoaded, this, assetManager, &selectedEntity ]()
                    {
                        if (ImGui::BeginDragDropTarget())
                        {
                            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                            {
                                LOG_ASSERT(payload->DataSize == sizeof(AssetHandle), "WRONG ITEM, that should be an asset handle");
                                AssetHandle handle = *static_cast<AssetHandle *>(payload->Data);
                                AssetType type = assetManager->GetAssetType(handle);
                                if (type == AssetType::Texture)
                                {
                                    c.handle = handle;
                                }
                            }
                            else if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_SPRITE_SHEET_ITEM))
                            {
                                if (payload->Data && payload->DataSize == sizeof(SpriteSheetSpritePayload))
                                {
                                    auto dropped = *static_cast<const SpriteSheetSpritePayload *>(payload->Data);
                                    std::swap(dropped.uv0.y, dropped.uv1.y);

                                    Sprite2DComponent before = c;
                                    c.handle = dropped.textureHandle;
                                    c.uv0 = dropped.uv0;
                                    c.uv1 = dropped.uv1;
                                    c.materialHandle = AssetHandle(0);
                                    CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<Sprite2DComponent>>(m_Scene, selectedEntity.GetUUID(), before, c));
                                }
                            }

                            ImGui::EndDragDropTarget();
                        }

                        if (isTextureLoaded)
                        {
                            ImGui::SameLine(0.0f, 4.0f);
                            if (UI::IconButton("##ClearTexture", ICON_LC_X, UI::ButtonVariant::Outline, UI::ButtonSize::IconSm, "Clear Texture"))
                            {
                                c.handle = AssetHandle(0);
                            }
                        }
                    });

                    UI::State tilingState = UI::DrawVec2Control("Tiling", c.tilingFactor, 0.025f, 1.0f);
                    if (tilingState.isItemActivated)            s_Sprite2DBefore = c;
                    if (tilingState.isItemDeactivatedAfterEdit)
                        CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<Sprite2DComponent>>(m_Scene,
                            selectedEntity.GetUUID(), s_Sprite2DBefore, c));

                    auto colorState = UI::DrawColorVec4("Color", c.color);
                    if (colorState.isItemActivated) s_Sprite2DBefore = c;
                    if (colorState.isItemDeactivatedAfterEdit)
                        CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<Sprite2DComponent>>(m_Scene,
                            selectedEntity.GetUUID(), s_Sprite2DBefore, c));
                }

                UI::State uv0State = UI::DrawVec2Control("UV0", c.uv0, 0.001f);
                if (uv0State.isItemActivated)            s_Sprite2DBefore = c;
                if (uv0State.isItemDeactivatedAfterEdit)
                    CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<Sprite2DComponent>>(m_Scene,
                        selectedEntity.GetUUID(), s_Sprite2DBefore, c));

                UI::State uv1State = UI::DrawVec2Control("UV1", c.uv1, 0.001f);
                if (uv1State.isItemActivated)            s_Sprite2DBefore = c;
                if (uv1State.isItemDeactivatedAfterEdit)
                    CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<Sprite2DComponent>>(m_Scene,
                        selectedEntity.GetUUID(), s_Sprite2DBefore, c));

                UI::State flipXState = UI::DrawSwitch("Flip X", &c.flipX);
                if (flipXState.isItemActivated)            s_Sprite2DBefore = c;
                if (flipXState.isItemDeactivatedAfterEdit)
                    CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<Sprite2DComponent>>(m_Scene,
                        selectedEntity.GetUUID(), s_Sprite2DBefore, c));

                UI::State flipYState = UI::DrawSwitch("Flip Y", &c.flipY);
                if (flipYState.isItemActivated)            s_Sprite2DBefore = c;
                if (flipYState.isItemDeactivatedAfterEdit)
                    CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<Sprite2DComponent>>(m_Scene,
                        selectedEntity.GetUUID(), s_Sprite2DBefore, c));
            });

            RenderComponent<Animator2DComponent>("Animator 2D", selectedEntity, [ & ]()
            {
                auto &c = selectedEntity.GetComponent<Animator2DComponent>();

                bool isAnimatorLoaded = c.controllerHandle != AssetHandle(0);
                std::string animDropLabel = isAnimatorLoaded ? assetManager->GetAssetDisplayName(c.controllerHandle) : "Drop Here";
                UI::DrawButtonWithColumn("Controller", animDropLabel.c_str(), nullptr, [ &c ]()
                {
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                        {
                            if (payload->Data && payload->DataSize == sizeof(AssetHandle))
                            {
                                AssetHandle handle = *static_cast<AssetHandle *>(payload->Data);
                                if (handle != AssetHandle(0))
                                {
                                    c.controllerHandle = handle;
                                }
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }
                });

                if (isAnimatorLoaded)
                {
                    Ref<AnimatorController2D> animCtrl = assetManager->GetAsset<AnimatorController2D>(c.controllerHandle);
                    if (animCtrl && !animCtrl->states.empty())
                    {
                        std::vector<const char *> stateLabels;
                        stateLabels.reserve(animCtrl->states.size());

                        int currentStateIndex = 0;
                        for (size_t i = 0; i < animCtrl->states.size(); ++i)
                        {
                            stateLabels.push_back(animCtrl->states[i].name.c_str());
                            if (animCtrl->states[i].name == c.currentStateName)
                            {
                                currentStateIndex = static_cast<int>(i);
                            }
                        }

                        if (UI::DrawComboBox("Current State", stateLabels.data(), static_cast<int>(stateLabels.size()), &currentStateIndex))
                        {
                            c.currentStateName = animCtrl->states[static_cast<size_t>(currentStateIndex)].name;
                        }
                    }

                }
            });

            RenderComponent<PointLight2DComponent>("Point Light 2D", selectedEntity, [ & ]()
            {
                auto &c = selectedEntity.GetComponent<PointLight2DComponent>();
                UI::DrawSwitch("Enabled", &c.enabled);
                UI::DrawColorVec4("Color", c.color);
                UI::DrawFloatControl("Radius", &c.radius, 0.025f, 0.0f, 10000.0f);
                UI::DrawFloatControl("Intensity", &c.intensity, 0.025f, 0.0f, 10000.0f);
            });

            RenderComponent<Circle2DComponent>("Circle 2D", selectedEntity, [ & ]()
            {
                auto &c = selectedEntity.GetComponent<Circle2DComponent>();

                static Circle2DComponent compBefore;

                UI::State colorState = UI::DrawColorVec4("Color", c.color);
                if (colorState.isItemActivated)
                    compBefore = c;

                if (colorState.isItemDeactivatedAfterEdit)
                    CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<Circle2DComponent>>(m_Scene, selectedEntity.GetUUID(), compBefore, c));
            });

            RenderComponent<StaticMeshComponent>("Static Mesh", selectedEntity, [ & ]()
            {
                auto &c = selectedEntity.GetComponent<StaticMeshComponent>();
                Ref<Project> project = assetManager->LockActiveProject();

                auto drawAssetSlotWithThumbnail = [&](const char *label, AssetHandle handle, const char *emptyLabel,
                    const char *fallbackIcon, const glm::vec4 &accentColor,
                    const std::function<void(AssetHandle)> &onDrop, const std::function<void()> &onClear,
                    const char *clearTooltip, const char *uniqueSuffix)
                {
                    const auto &theme = UI::GetTheme();
                    const float scale = std::max(UI::GetDpiScale(), 1.0f);
                    const float thumbSize = std::round(36.0f * scale);

                    ImGui::PushID(uniqueSuffix);
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted(label);
                    ImGui::SameLine(UI::defColWidth);

                    const ImVec2 wellMin = ImGui::GetCursorScreenPos();
                    const ImVec2 wellMax(wellMin.x + thumbSize, wellMin.y + thumbSize);
                    ImDrawList *drawList = ImGui::GetWindowDrawList();

                    ImGui::InvisibleButton("##thumb_btn", ImVec2(thumbSize, thumbSize));
                    const bool thumbHovered = ImGui::IsItemHovered();
                    if (thumbHovered && handle != AssetHandle(0) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    {
                        AssetMetaData meta = assetManager->GetMetaData(handle);
                        if (meta.type != AssetType::Invalid)
                        {
                            Application::SubmitToMainThread([handle, meta]()
                            {
                                SignalBus::Emit(AssetEditorOpenSignal{ handle, meta });
                            });
                        }
                    }

                    drawList->AddRectFilled(wellMin, wellMax, UI::ToU32(theme.inputBg), 2.0f);
                    drawList->AddRect(wellMin, wellMax, UI::ToU32(thumbHovered ? theme.primary : theme.border), 2.0f, 0, 1.0f);

                    Ref<Texture> thumb = ContentBrowserPanel::GetAssetThumbnail(handle, assetManager, project.get());
                    if (thumb && thumb->GetShaderResourceView())
                    {
                        const float pad = 2.0f;
                        drawList->AddImage(UI::GetTextureID(thumb),
                            ImVec2(wellMin.x + pad, wellMin.y + pad),
                            ImVec2(wellMax.x - pad, wellMax.y - pad));
                    }
                    else
                    {
                        const ImVec2 icSz = ImGui::CalcTextSize(fallbackIcon);
                        drawList->AddText(
                            ImVec2(wellMin.x + (thumbSize - icSz.x) * 0.5f, wellMin.y + (thumbSize - icSz.y) * 0.5f),
                            UI::ToU32(accentColor), fallbackIcon);
                    }
                    drawList->AddRectFilled(ImVec2(wellMin.x, wellMax.y - 2.0f), wellMax, UI::ToU32(accentColor, 0.85f));

                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                        {
                            LOG_ASSERT(payload->DataSize == sizeof(AssetHandle), "WRONG ITEM, that should be an asset handle");
                            onDrop(*static_cast<AssetHandle *>(payload->Data));
                        }
                        ImGui::EndDragDropTarget();
                    }

                    ImGui::SameLine(0.0f, 8.0f);
                    ImGui::BeginGroup();
                    const std::string displayName = (handle != AssetHandle(0)) ? assetManager->GetAssetDisplayName(handle) : emptyLabel;
                    ImGui::Button(displayName.c_str(), ImVec2(std::max(120.0f, ImGui::GetContentRegionAvail().x - (handle != AssetHandle(0) ? 30.0f : 4.0f)), thumbSize));
                    if (ImGui::IsItemHovered() && handle != AssetHandle(0) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    {
                        AssetMetaData meta = assetManager->GetMetaData(handle);
                        if (meta.type != AssetType::Invalid)
                        {
                            Application::SubmitToMainThread([handle, meta]()
                            {
                                SignalBus::Emit(AssetEditorOpenSignal{ handle, meta });
                            });
                        }
                    }
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                        {
                            LOG_ASSERT(payload->DataSize == sizeof(AssetHandle), "WRONG ITEM, that should be an asset handle");
                            onDrop(*static_cast<AssetHandle *>(payload->Data));
                        }
                        ImGui::EndDragDropTarget();
                    }
                    if (handle != AssetHandle(0))
                    {
                        ImGui::SameLine(0.0f, 4.0f);
                        if (UI::IconButton("##ClearSlot", ICON_LC_X, UI::ButtonVariant::Outline, UI::ButtonSize::IconSm, clearTooltip))
                        {
                            onClear();
                        }
                    }
                    ImGui::EndGroup();
                    ImGui::PopID();
                };

                drawAssetSlotWithThumbnail("Static Mesh Asset", c.handle, "Drag Mesh Here",
                    ICON_LC_BOX, UI::Accent::Mesh,
                    [&](AssetHandle handle)
                    {
                        AssetMetaData metadata = assetManager->GetMetaData(handle);
                        if (metadata.type == AssetType::Mesh || metadata.type == AssetType::StaticMesh)
                        {
                            metadata.type = AssetType::StaticMesh;
                            assetManager->AssignMetaData(handle, metadata);
                            assetManager->UnloadAsset(handle);
                            c.handle = handle;
                            if (selectedEntity.HasComponent<MeshColliderComponent>())
                            {
                                auto &mc = selectedEntity.GetComponent<MeshColliderComponent>();
                                mc.vertices.clear();
                                mc.indices.clear();
                                mc.dirty = true;
                            }
                        }
                    },
                    [&]()
                    {
                        c.handle = AssetHandle(0);
                        if (selectedEntity.HasComponent<MeshColliderComponent>())
                        {
                            auto &mc = selectedEntity.GetComponent<MeshColliderComponent>();
                            mc.vertices.clear();
                            mc.indices.clear();
                            mc.dirty = true;
                        }
                    },
                    "Clear Static Mesh", "StaticMeshSlot");

                if (c.handle != AssetHandle(0))
                {
                    Ref<StaticMesh> sm = assetManager->GetAsset<StaticMesh>(c.handle);
                    if (sm)
                    {
                        if (ImGui::CollapsingHeader("Materials", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            const auto &instances = sm->GetMeshInstances();
                            for (size_t i = 0; i < instances.size(); ++i)
                            {
                                const auto &instance = instances[i];
                                std::string submeshName = instance->GetName();
                                if (submeshName.empty())
                                    submeshName = "Submesh " + std::to_string(i);
                                else
                                    submeshName = std::format("{} ({})", submeshName, i);

                                const AssetHandle matHandle = instance->GetMaterialAssetHandle();
                                const std::string slotId = std::format("StaticMat_{}", i);
                                drawAssetSlotWithThumbnail(submeshName.c_str(), matHandle, "Default Material",
                                    ICON_LC_PALETTE, UI::Accent::Material,
                                    [&](AssetHandle handle)
                                    {
                                        AssetMetaData metadata = assetManager->GetMetaData(handle);
                                        if (metadata.type == AssetType::Material)
                                        {
                                            instance->SetMaterial(handle);
                                            sm->SetDirtyFlag(true);
                                        }
                                    },
                                    [&]()
                                    {
                                        instance->SetMaterial(AssetHandle(0));
                                        sm->SetDirtyFlag(true);
                                    },
                                    "Reset to Default Material", slotId.c_str());
                            }
                        }
                    }
                }
            });

            RenderComponent<SkeletalMeshComponent>("Skeletal Mesh", selectedEntity, [ & ]()
            {
                auto &c = selectedEntity.GetComponent<SkeletalMeshComponent>();
                Ref<Project> project = assetManager->LockActiveProject();

                auto drawSkelSlotWithThumbnail = [&](const char *label, AssetHandle handle, const char *emptyLabel,
                    const char *fallbackIcon, const glm::vec4 &accentColor,
                    const std::function<void(AssetHandle)> &onDrop, const std::function<void()> &onClear,
                    const char *clearTooltip, const char *uniqueSuffix)
                {
                    const auto &theme = UI::GetTheme();
                    const float scale = std::max(UI::GetDpiScale(), 1.0f);
                    const float thumbSize = std::round(36.0f * scale);

                    ImGui::PushID(uniqueSuffix);
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted(label);
                    ImGui::SameLine(UI::defColWidth);

                    const ImVec2 wellMin = ImGui::GetCursorScreenPos();
                    const ImVec2 wellMax(wellMin.x + thumbSize, wellMin.y + thumbSize);
                    ImDrawList *drawList = ImGui::GetWindowDrawList();

                    ImGui::InvisibleButton("##thumb_btn", ImVec2(thumbSize, thumbSize));
                    const bool thumbHovered = ImGui::IsItemHovered();
                    if (thumbHovered && handle != AssetHandle(0) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    {
                        AssetMetaData meta = assetManager->GetMetaData(handle);
                        if (meta.type != AssetType::Invalid)
                        {
                            Application::SubmitToMainThread([handle, meta]()
                            {
                                SignalBus::Emit(AssetEditorOpenSignal{ handle, meta });
                            });
                        }
                    }

                    drawList->AddRectFilled(wellMin, wellMax, UI::ToU32(theme.inputBg), 2.0f);
                    drawList->AddRect(wellMin, wellMax, UI::ToU32(thumbHovered ? theme.primary : theme.border), 2.0f, 0, 1.0f);

                    Ref<Texture> thumb = ContentBrowserPanel::GetAssetThumbnail(handle, assetManager, project.get());
                    if (thumb && thumb->GetShaderResourceView())
                    {
                        const float pad = 2.0f;
                        drawList->AddImage(UI::GetTextureID(thumb),
                            ImVec2(wellMin.x + pad, wellMin.y + pad),
                            ImVec2(wellMax.x - pad, wellMax.y - pad));
                    }
                    else
                    {
                        const ImVec2 icSz = ImGui::CalcTextSize(fallbackIcon);
                        drawList->AddText(
                            ImVec2(wellMin.x + (thumbSize - icSz.x) * 0.5f, wellMin.y + (thumbSize - icSz.y) * 0.5f),
                            UI::ToU32(accentColor), fallbackIcon);
                    }
                    drawList->AddRectFilled(ImVec2(wellMin.x, wellMax.y - 2.0f), wellMax, UI::ToU32(accentColor, 0.85f));

                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                        {
                            LOG_ASSERT(payload->DataSize == sizeof(AssetHandle), "WRONG ITEM, that should be an asset handle");
                            onDrop(*static_cast<AssetHandle *>(payload->Data));
                        }
                        ImGui::EndDragDropTarget();
                    }

                    ImGui::SameLine(0.0f, 8.0f);
                    ImGui::BeginGroup();
                    const std::string displayName = (handle != AssetHandle(0)) ? assetManager->GetAssetDisplayName(handle) : emptyLabel;
                    ImGui::Button(displayName.c_str(), ImVec2(std::max(120.0f, ImGui::GetContentRegionAvail().x - (handle != AssetHandle(0) ? 30.0f : 4.0f)), thumbSize));
                    if (ImGui::IsItemHovered() && handle != AssetHandle(0) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    {
                        AssetMetaData meta = assetManager->GetMetaData(handle);
                        if (meta.type != AssetType::Invalid)
                        {
                            Application::SubmitToMainThread([handle, meta]()
                            {
                                SignalBus::Emit(AssetEditorOpenSignal{ handle, meta });
                            });
                        }
                    }
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                        {
                            LOG_ASSERT(payload->DataSize == sizeof(AssetHandle), "WRONG ITEM, that should be an asset handle");
                            onDrop(*static_cast<AssetHandle *>(payload->Data));
                        }
                        ImGui::EndDragDropTarget();
                    }
                    if (handle != AssetHandle(0))
                    {
                        ImGui::SameLine(0.0f, 4.0f);
                        if (UI::IconButton("##ClearSlot", ICON_LC_X, UI::ButtonVariant::Outline, UI::ButtonSize::IconSm, clearTooltip))
                        {
                            onClear();
                        }
                    }
                    ImGui::EndGroup();
                    ImGui::PopID();
                };

                drawSkelSlotWithThumbnail("Skeletal Mesh Asset", c.handle, "Drag Mesh Here",
                    ICON_LC_BOX, UI::Accent::Mesh,
                    [&](AssetHandle handle)
                    {
                        AssetMetaData metadata = assetManager->GetMetaData(handle);
                        if (metadata.type == AssetType::Mesh || metadata.type == AssetType::SkeletalMesh)
                        {
                            metadata.type = AssetType::SkeletalMesh;
                            assetManager->AssignMetaData(handle, metadata);
                            assetManager->UnloadAsset(handle);
                            c.handle = handle;
                            if (selectedEntity.HasComponent<MeshColliderComponent>())
                            {
                                auto &mc = selectedEntity.GetComponent<MeshColliderComponent>();
                                mc.vertices.clear();
                                mc.indices.clear();
                                mc.dirty = true;
                            }
                        }
                    },
                    [&]()
                    {
                        c.handle = AssetHandle(0);
                        if (selectedEntity.HasComponent<MeshColliderComponent>())
                        {
                            auto &mc = selectedEntity.GetComponent<MeshColliderComponent>();
                            mc.vertices.clear();
                            mc.indices.clear();
                            mc.dirty = true;
                        }
                    },
                    "Clear Skeletal Mesh", "SkelMeshSlot");

                if (c.handle != AssetHandle(0))
                {
                    Ref<SkeletalMesh> sm = assetManager->GetAsset<SkeletalMesh>(c.handle);
                    if (sm)
                    {
                        if (ImGui::CollapsingHeader("Materials", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            const auto &instances = sm->GetMeshInstances();
                            for (size_t i = 0; i < instances.size(); ++i)
                            {
                                const auto &instance = instances[i];
                                std::string submeshName = instance->GetName();
                                if (submeshName.empty())
                                    submeshName = "Submesh " + std::to_string(i);
                                else
                                    submeshName = std::format("{} ({})", submeshName, i);

                                const AssetHandle matHandle = instance->GetMaterialAssetHandle();
                                const std::string slotId = std::format("SkelMat_{}", i);
                                drawSkelSlotWithThumbnail(submeshName.c_str(), matHandle, "Default Material",
                                    ICON_LC_PALETTE, UI::Accent::Material,
                                    [&](AssetHandle handle)
                                    {
                                        AssetMetaData metadata = assetManager->GetMetaData(handle);
                                        if (metadata.type == AssetType::Material)
                                        {
                                            instance->SetMaterial(handle);
                                            sm->SetDirtyFlag(true);
                                        }
                                    },
                                    [&]()
                                    {
                                        instance->SetMaterial(AssetHandle(0));
                                        sm->SetDirtyFlag(true);
                                    },
                                    "Reset to Default Material", slotId.c_str());
                            }
                        }

                        // Animator
                        bool isAnimatorLoaded = c.runtimeAnimatorHandle != AssetHandle(0);
                        std::string buttonLabel = isAnimatorLoaded ? assetManager->GetAssetDisplayName(c.runtimeAnimatorHandle) : "Drag Here";
                        UI::DrawButtonWithColumn("Animator", buttonLabel.c_str(), nullptr, [ &c, this, &sm, &isAnimatorLoaded, assetManager ]()
                        {
                            if (ImGui::BeginDragDropTarget())
                            {
                                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                                {
                                    LOG_ASSERT(payload->DataSize == sizeof(AssetHandle), "WRONG ITEM, that should be an asset handle");
                                    AssetHandle handle = *static_cast<AssetHandle *>(payload->Data);
                                    AssetMetaData metadata = assetManager->GetMetaData(handle);

                                    if (metadata.type == AssetType::AnimatorController)
                                    {
                                        metadata.type = AssetType::AnimatorController;
                                        assetManager->AssignMetaData(handle, metadata);
                                        assetManager->UnloadAsset(handle);
                                        sm->SetAnimator(handle);
                                        c.runtimeAnimatorHandle = handle;
                                        c.currentStateName.clear();
                                        c.stateElapsed = 0.0f;
                                        c.stateNormalized = 0.0f;
                                        c.runtimeParams.clear();
                                    }
                                }

                                ImGui::EndDragDropTarget();
                            }

                            if (isAnimatorLoaded)
                            {
                                ImGui::SameLine(0.0f, 4.0f);
                                if (UI::IconButton("##ClearAnimator", ICON_LC_X, UI::ButtonVariant::Outline, UI::ButtonSize::IconSm, "Clear Animator"))
                                {
                                    sm->SetAnimator(AssetHandle(0)); // reset animator
                                    c.runtimeAnimatorHandle = AssetHandle(0);
                                    c.currentStateName.clear();
                                    c.stateElapsed = 0.0f;
                                    c.stateNormalized = 0.0f;
                                    c.runtimeParams.clear();

                                    isAnimatorLoaded = false;
                                }
                            }
                        });

                        if (isAnimatorLoaded)
                        {
                            if (UI::DrawSwitch("Unique", &c.uniqueAnimator))
                            {
                                c.currentStateName.clear();
                                c.stateElapsed = 0.0f;
                                c.stateNormalized = 0.0f;
                                c.runtimeParams.clear();
                            }
                        }

                        // SOCKET SYSTEM
                        // Render Socket Attachments UI
                        if (sm->GetSkeletonHandle() != AssetHandle(0))
                        {
                            Ref<Skeleton> skeleton = assetManager->GetAsset<Skeleton>(sm->GetSkeletonHandle());
                            if (skeleton && !skeleton->sockets.empty())
                            {
                                if (ImGui::CollapsingHeader("Socket Attachments", ImGuiTreeNodeFlags_DefaultOpen))
                                {
                                    for (const JointSocket &socket : skeleton->sockets)
                                    {
                                        AssetHandle attachedMeshHandle = AssetHandle(0);
                                        auto it = c.socketAttachments.find(socket.name);
                                        if (it != c.socketAttachments.end())
                                        {
                                            attachedMeshHandle = it->second;
                                        }

                                        bool isAttached = attachedMeshHandle != AssetHandle(0);
                                        const std::string socketMeshLabel = assetManager->GetAssetDisplayName(attachedMeshHandle);

                                        UI::DrawButtonWithColumn(socket.name.c_str(), socketMeshLabel.c_str(), nullptr, [ this, &c, socketName = socket.name, assetManager, isAttached, &selectedEntity ]()
                                        {
                                            if (ImGui::BeginDragDropTarget())
                                            {
                                                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                                                {
                                                    LOG_ASSERT(payload->DataSize == sizeof(AssetHandle), "WRONG ITEM, that should be an asset handle");
                                                    AssetHandle handle = *static_cast<AssetHandle *>(payload->Data);
                                                    AssetMetaData metadata = assetManager->GetMetaData(handle);

                                                    if (metadata.type == AssetType::Mesh)
                                                    {
                                                        SkeletalMeshComponent before = c;
                                                        c.socketAttachments[socketName] = handle;
                                                        CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<SkeletalMeshComponent>>(m_Scene, selectedEntity.GetUUID(), before, c));
                                                    }
                                                }
                                                ImGui::EndDragDropTarget();
                                            }

                                            if (isAttached)
                                            {
                                                ImGui::SameLine(0.0f, 4.0f);
                                                const std::string clearSockId = std::format("##ClearSocket_{}", socketName);
                                                if (UI::IconButton(clearSockId.c_str(), ICON_LC_X, UI::ButtonVariant::Outline, UI::ButtonSize::IconSm, "Clear Socket Mesh"))
                                                {
                                                    SkeletalMeshComponent before = c;
                                                    c.socketAttachments.erase(socketName);
                                                    CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<SkeletalMeshComponent>>(m_Scene, selectedEntity.GetUUID(), before, c));
                                                }
                                            }
                                        });
                                    }
                                }
                            }
                        }
                    }
                }
            });

            RenderComponent<CameraComponent>("Camera", selectedEntity, [ & ]()
            {
                auto &c = selectedEntity.GetComponent<CameraComponent>();
                static CameraComponent s_CameraBefore;

                // Projection
                {
                    static const char *projectionTypeStr[] = { "Orthographic", "Perspective" };
                    auto projectionIdx = static_cast<int>(c.camera.projectionType);
                    if (UI::DrawComboBox("Projection", projectionTypeStr, IM_ARRAYSIZE(projectionTypeStr), &projectionIdx))
                    {
                        c.camera.projectionType = static_cast<ProjectionType>(projectionIdx);
                        c.dirty = true;
                    }
                }

                // Aspect Ratio
                {
                    static const char *aspectRatioLabels[] = { "Free", "16:9", "16:10", "4:3", "21:9", "1:1" };
                    auto aspectRatioIndex = static_cast<int>(c.camera.GetAspectRatioPreset());
                    if (UI::DrawComboBox("Aspect Ratio", aspectRatioLabels, IM_ARRAYSIZE(aspectRatioLabels), &aspectRatioIndex))
                    {
                        c.camera.SetAspectRatioPreset(static_cast<SceneCamera::AspectRatioPreset>(aspectRatioIndex));
                        c.dirty = true;
                    }
                }

                if (c.camera.projectionType == ProjectionType::Perspective)
                {
                    UI::State fovState = UI::DrawFloatControl("Fov", &c.camera.fov, 0.025f, 0.0f, FLT_MAX);
                    if (fovState.isItemActivated)
                        s_CameraBefore = c;
                    if (fovState.isItemEdited)
                        c.dirty = true;
                    if (fovState.isItemDeactivatedAfterEdit)
                        CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<CameraComponent>>(m_Scene, selectedEntity.GetUUID(), s_CameraBefore, c));
                }
                else
                {
                    UI::State orthoState = UI::DrawFloatControl("Ortho Size", &c.camera.orthoSize, 0.025f, 0.0f, FLT_MAX);
                    if (orthoState.isItemActivated)
                        s_CameraBefore = c;
                    if (orthoState.isItemEdited)
                        c.dirty = true;
                    if (orthoState.isItemDeactivatedAfterEdit)
                        CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<CameraComponent>>(m_Scene, selectedEntity.GetUUID(), s_CameraBefore, c));
                }

                UI::State nearState = UI::DrawFloatControl("Near", &c.camera.nearPlane, 0.025f, 0.0f, FLT_MAX);
                if (nearState.isItemActivated)
                    s_CameraBefore = c;
                if (nearState.isItemEdited)
                    c.dirty = true;
                if (nearState.isItemDeactivatedAfterEdit)
                    CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<CameraComponent>>(m_Scene, selectedEntity.GetUUID(), s_CameraBefore, c));

                UI::State farState = UI::DrawFloatControl("Far", &c.camera.farPlane, 0.025f, 0.0f, FLT_MAX);
                if (farState.isItemActivated)
                    s_CameraBefore = c;
                if (farState.isItemEdited)
                    c.dirty = true;
                if (farState.isItemDeactivatedAfterEdit)
                    CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<CameraComponent>>(m_Scene, selectedEntity.GetUUID(), s_CameraBefore, c));

                {
                    CameraComponent before = c;
                    if (UI::DrawSwitch("Primary", &c.primary).isItemEdited)
                    {
                        c.dirty = true;
                        CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<CameraComponent>>(m_Scene, selectedEntity.GetUUID(), before, c));
                    }
                }

                if (ImGui::TreeNodeEx("Render Properties", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto &pp = c.camera.postProcessing;

                    if (ImGui::TreeNodeEx("Anti aliasing"))
                    {
                        if (ImGui::TreeNodeEx("TAA", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            c.dirty |= UI::DrawSwitch("Enable", &pp.taaProperties.enable);
                            c.dirty |= UI::DrawFloatControl("Blend Factor", &pp.taaProperties.blendFactor, 0.025f, 0.01f, 1.0f, 1.0f);
                            ImGui::TreePop();
                        }

                        if (ImGui::TreeNodeEx("MSAA", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            c.dirty |= UI::DrawSwitch("Enable", &pp.msaaProperties.enable);
                            c.dirty |= UI::DrawIntControl("Samples", &pp.msaaProperties.sampleCount, 1, 1, 16);
                            ImGui::TreePop();
                        }

                        ImGui::TreePop();
                    }

                    if (ImGui::TreeNodeEx("Render scale"))
                    {
                        c.dirty |= UI::DrawFloatControl("Factor", &pp.renderScale, 0.025f, 0.25f, 1.0f, 1.0f);
                        if (UI::DrawButtonWithColumn("", "Apply"))
                        {
                            m_EditorLayer->m_State.gameplayRequestToResize = true;
                            m_EditorLayer->m_State.editorRequestToResize = true;
                        }
                        ImGui::TreePop();
                    }

                    if (ImGui::TreeNodeEx("Post Processing"))
                    {
                        // Tonemapping & Color correction
                        if (ImGui::TreeNodeEx("Color Correction", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            const char *tonemapModes[] = { "Reinhard", "Uncharted 2", "ACES Filmic" };
                            int currentTonemap = static_cast<int>(pp.tonemapMode);
                            if (UI::DrawComboBox("Tonemap Mode", tonemapModes, std::size(tonemapModes), &currentTonemap))
                            {
                                pp.tonemapMode = static_cast<TonemapMode>(currentTonemap);
                                c.dirty = true;
                            }
                            // TODO: Add these controls back in when we have a proper color grading system
                            // c.dirty |= UI::DrawFloatControl("Exposure", &pp.exposure, 0.025f, 0.0f, 10.0f, 1.0f);
                            // c.dirty |= UI::DrawFloatControl("Gamma", &pp.gamma, 0.025f, 0.1f, 5.0f, 2.2f);

                            ImGui::TreePop();
                        }

                        // Depth of Field
                        c.dirty |= UI::DrawSwitch("Enable DOF", &c.camera.lens.enabledDOF).isItemEdited;
                        ImGui::BeginDisabled(!c.camera.lens.enabledDOF);
                        {
                            c.dirty |= UI::DrawFloatControl("Focal Length", &c.camera.lens.focalLength, 0.1f, 0.0f, FLT_MAX).isItemEdited;
                            c.dirty |= UI::DrawFloatControl("Focal Distance", &c.camera.lens.focalDistance, 0.05f, 0.0f, FLT_MAX).isItemEdited;
                            c.dirty |= UI::DrawFloatControl("fStop", &c.camera.lens.fStop, 0.05f, 0.0f, FLT_MAX).isItemEdited;
                            c.dirty |= UI::DrawFloatControl("Focus Range", &c.camera.lens.focusRange, 0.05f, 0.0f, FLT_MAX).isItemEdited;
                            c.dirty |= UI::DrawFloatControl("Blur Amount", &c.camera.lens.blurAmount, 0.05f, 0.0f, FLT_MAX).isItemEdited;
                        }
                        ImGui::EndDisabled();

                        // Bloom
                        c.dirty |= UI::DrawSwitch("Enable Bloom", &pp.enableBloom).isItemEdited;
                        ImGui::BeginDisabled(!pp.enableBloom);
                        {
                            c.dirty |= UI::DrawFloatControl("Bloom Intensity", &pp.bloomIntensity, 0.01f, 0.0f, 100.0f).isItemEdited;
                            c.dirty |= UI::DrawFloatControl("Bloom Threshold", &pp.bloomThreshold, 0.01f, 0.0f, 10.0f).isItemEdited;
                            c.dirty |= UI::DrawFloatControl("Bloom Knee", &pp.bloomKnee, 0.01f, 0.0f, 10.0f).isItemEdited;
                            c.dirty |= UI::DrawFloatControl("Bloom Radius", &pp.bloomRadius, 0.01f, 0.0f, 10.0f).isItemEdited;
                            c.dirty |= UI::DrawIntControl("Bloom Iterations", &pp.bloomIterations, 1.0f, 1, 16).isItemEdited;
                        }
                        ImGui::EndDisabled();

                        // Vignette
                        c.dirty |= UI::DrawSwitch("Enable Vignette", &pp.enableVignette).isItemEdited;
                        ImGui::BeginDisabled(!pp.enableVignette);
                        {
                            c.dirty |= UI::DrawColorVec3("Vignette Color", pp.vignetteColor).isItemEdited;
                            c.dirty |= UI::DrawFloatControl("Vignette Radius", &pp.vignetteRadius, 0.01f, 0.0f, 10.0f).isItemEdited;
                            c.dirty |= UI::DrawFloatControl("Vignette Softness", &pp.vignetteSoftness, 0.01f, 0.0f, 10.0f).isItemEdited;
                            c.dirty |= UI::DrawFloatControl("Vignette Intensity", &pp.vignetteIntensity, 0.01f, 0.0f, 10.0f).isItemEdited;
                        }
                        ImGui::EndDisabled();

                        // Chromatic Aberration
                        c.dirty |= UI::DrawSwitch("Enable Chromatic Aberration", &pp.enableChromAb).isItemEdited;
                        ImGui::BeginDisabled(!pp.enableChromAb);
                        {
                            c.dirty |= UI::DrawFloatControl("Chromatic Aberration Amount", &pp.chromAbAmount, 0.0001f, 0.0f, 0.1f).isItemEdited;
                            c.dirty |= UI::DrawFloatControl("Chromatic Aberration Radial", &pp.chromAbRadial, 0.01f, 0.0f, 10.0f).isItemEdited;
                        }
                        ImGui::EndDisabled();

                        // Ambient Occlusion (GTAO / Legacy SSAO)
                        c.dirty |= UI::DrawSwitch("Enable Ambient Occlusion", &pp.enableSSAO).isItemEdited;
                        ImGui::BeginDisabled(!pp.enableSSAO);
                        {
                            static const char *kAOModes[] = { "Off", "Legacy SSAO", "GTAO" };
                            int aoModeIdx = static_cast<int>(pp.aoMode);
                            if (UI::DrawComboBox("AO Mode", kAOModes, IM_ARRAYSIZE(kAOModes), &aoModeIdx))
                            {
                                pp.aoMode = static_cast<AOMode>(std::clamp(aoModeIdx, 0, 2));
                                c.dirty = true;
                            }
                            c.dirty |= UI::DrawFloatControl("AO Radius", &pp.aoRadius, 0.01f, 0.05f, 2.5f).isItemEdited;
                            c.dirty |= UI::DrawFloatControl("AO Intensity", &pp.aoIntensity, 0.05f, 0.0f, 3.0f).isItemEdited;
                            c.dirty |= UI::DrawFloatControl("AO Power", &pp.aoPower, 0.025f, 0.1f, 5.0f).isItemEdited;
                        }
                        ImGui::EndDisabled();

                        ImGui::TreePop();
                    }

                    ImGui::TreePop();
                }

                if (c.dirty && m_Data.sceneViewportGameplayVisible)
                {
                    c.camera.UpdateView();
                    c.camera.UpdateProjection(
                        static_cast<uint32_t>(globals::GEditor::GameViewport.max.x),
                        static_cast<uint32_t>(globals::GEditor::GameViewport.max.y));
                    c.dirty = false;
                }
            });

            RenderComponent<Rigidbody2DComponent>("Rigid Body 2D", selectedEntity, [ & ]()
            {
                auto &c = selectedEntity.GetComponent<Rigidbody2DComponent>();

                static std::array<const char *, 3> bodyTypeStr = { "Static", "Kinematic", "Dynamic" };
                int bodyTypeIndex = static_cast<int>(c.bodyType);
                if (UI::DrawComboBox("Body Type", bodyTypeStr.data(), static_cast<int>(bodyTypeStr.size()), &bodyTypeIndex))
                {
                    c.bodyType = static_cast<physics::BodyType>(bodyTypeIndex);
                }

                UI::DrawVec2Control("Linear Vel", c.linearVelocity, 0.025f);
                UI::DrawFloatControl("Angular Vel", &c.angularVelocity, 0.025f, FLT_MIN, FLT_MAX);
                UI::DrawFloatControl("Gravity", &c.gravityScale, 0.025f, FLT_MIN, FLT_MAX);
                UI::DrawFloatControl("Linear Damping", &c.linearDamping, 0.025f, 0.0f, FLT_MAX);
                UI::DrawFloatControl("Angular Damping", &c.angularDamping, 0.025f, 0.0f, FLT_MAX);
                UI::DrawSwitch("Awake", &c.isAwake);
                UI::DrawSwitch("Enabled", &c.isEnabled);
                UI::DrawSwitch("Enable Sleeping", &c.enableSleeping);
                UI::DrawSwitch("Fixed Rotation", &c.fixedRotation);

                if (!c.fixedRotation)
                {
                    UI::DrawSwitch("Fast Rotation", &c.allowFastRotation);
                }
            });

            RenderComponent<BoxCollider2DComponent>("Box Collider 2D", selectedEntity, [ & ]()
            {
                auto &c = selectedEntity.GetComponent<BoxCollider2DComponent>();
                c.dirty = UI::DrawVec2Control("Size", c.size, 0.025f, 1.0f);
                c.dirty |= UI::DrawVec2Control("Offset", c.offset, 0.025f);
                c.dirty |= UI::DrawFloatControl("Restitution", &c.restitution, 0.025f, 0.0f, FLT_MAX);
                c.dirty |= UI::DrawFloatControl("Friction", &c.friction, 0.025f, 0.0f, FLT_MAX);
                c.dirty |= UI::DrawFloatControl("Density", &c.density, 0.025f);
                c.dirty |= UI::DrawSwitch("Is Sensor", &c.isSensor);
            });

            RenderComponent<CircleCollider2DComponent>("Circle Collider 2D", selectedEntity, [ & ]()
            {
                auto &cc = selectedEntity.GetComponent<CircleCollider2DComponent>();
                cc.dirty = UI::DrawVec2Control("Center", cc.center, 0.025f);
                cc.dirty |= UI::DrawFloatControl("Radius", &cc.radius, 0.025f, 0.0f, FLT_MAX);
                cc.dirty |= UI::DrawFloatControl("Restitution", &cc.restitution, 0.025f, 0.0f, FLT_MAX);
                cc.dirty |= UI::DrawFloatControl("Friction", &cc.friction, 0.025f, 0.0f, FLT_MAX);
                cc.dirty |= UI::DrawFloatControl("Density", &cc.density, 0.025f);
                cc.dirty |= UI::DrawSwitch("Is Sensor", &cc.isSensor);
            });

            RenderComponent<RigidbodyComponent>("Rigid Body", selectedEntity, [ & ]()
            {
                auto &c = selectedEntity.GetComponent<RigidbodyComponent>();

                static std::array<const char *, 3> bodyTypeLabels = { "Static", "Kinematic", "Dynamic" };
                static std::array<const char *, 2> motionTypeLabels = { "Discrete", "LinearCast" };

                if (ImGui::TreeNodeEx("Physics Properties", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    int bodyTypeIndex = static_cast<int>(c.bodyType);
                    if (UI::DrawComboBox("Body Type", bodyTypeLabels.data(), static_cast<int>(bodyTypeLabels.size()), &bodyTypeIndex))
                    {
                        c.bodyType = static_cast<physics::BodyType>(bodyTypeIndex);
                        c.dirty = true;
                    }

                    int motionTypeIndex = static_cast<int>(c.motionQuality);
                    if (UI::DrawComboBox("Motion Quality", motionTypeLabels.data(), static_cast<int>(motionTypeLabels.size()), &motionTypeIndex))
                    {
                        c.motionQuality = static_cast<physics::MotionQuality>(motionTypeIndex);
                        c.dirty = true;
                    }

                    if (auto assetManager = AssetManager::GetInstance())
                    {
                        if (auto project = assetManager->LockActiveProject())
                        {
                            const auto &physSettings = project->GetInfo().physicsSettings;
                            std::vector<const char *> layerLabels;
                            layerLabels.reserve(physSettings.layerCount);
                            for (uint32_t i = 0; i < physSettings.layerCount; ++i)
                            {
                                layerLabels.push_back(physSettings.layerNames[i].c_str());
                            }

                            int layerIndex = static_cast<int>(c.layer);
                            if (layerIndex >= static_cast<int>(physSettings.layerCount))
                            {
                                layerIndex = 0;
                            }

                            if (UI::DrawComboBox("Physics Layer", layerLabels.data(), static_cast<int>(layerLabels.size()), &layerIndex))
                            {
                                c.layer = static_cast<uint32_t>(layerIndex);
                                c.dirty = true;
                            }
                        }
                    }

                    c.dirty |= UI::DrawFloatControl("Gravity Factor", &c.gravityFactor, 0.0025f, FLT_MIN, FLT_MAX, 1.0f);
                    c.dirty |= UI::DrawFloatControl("Mass", &c.mass, 0.0025f, FLT_MIN, FLT_MAX, 1.0f);
                    c.dirty |= UI::DrawFloatControl("Friction", &c.friction, 0.025f);
                    c.dirty |= UI::DrawFloatControl("Restitution", &c.restitution, 0.025f);

                    ImGui::TreePop();
                }

                if (ImGui::TreeNodeEx("Velocities", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    c.dirty |= UI::DrawVec3Control("Linear Velocity", c.linearVelocity, 0.025f);
                    c.dirty |= UI::DrawVec3Control("Angular Velocity", c.angularVelocity, 0.025f);
                    c.dirty |= UI::DrawFloatControl("Max Linear Velocity", &c.maxLinearVelocity, 0.025f, 0.0f, FLT_MAX);
                    c.dirty |= UI::DrawFloatControl("Max Angular Velocity", &c.maxAngularVelocity, 0.025f, 0.0f, FLT_MAX);

                    if (ImGui::TreeNodeEx("Damping", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        c.dirty |= UI::DrawFloatControl("Linear Damping", &c.linearDamping, 0.0025f, 0.0f, FLT_MAX);
                        c.dirty |= UI::DrawFloatControl("Angular Damping", &c.angularDamping, 0.0025f, 0.0f, FLT_MAX);
                        ImGui::TreePop();
                    }

                    ImGui::TreePop();
                }

                if (ImGui::TreeNodeEx("Constraints"))
                {
                    c.dirty |= UI::DrawCheckbox3("Translation Lock XYZ", &c.moveX, &c.moveY, &c.moveZ);
                    c.dirty |= UI::DrawCheckbox3("Rotation Lock XYZ", &c.rotateX, &c.rotateY, &c.rotateZ);

                    ImGui::TreePop();
                }

                c.dirty |= UI::DrawSwitch("Is Sensor", &c.isSensor);
                c.dirty |= UI::DrawSwitch("Use Gravity", &c.useGravity);
                c.dirty |= UI::DrawSwitch("Allow sleeping", &c.allowSleeping);
                c.dirty |= UI::DrawSwitch("Retain Acceleration", &c.retainAcceleration);
                c.dirty |= UI::DrawSwitch("Apply Gyroscopic Force", &c.applyGyroscopicForce);
            });

            RenderComponent<BoxColliderComponent>("Box Collider", selectedEntity, [ & ]()
            {
                auto &c = selectedEntity.GetComponent<BoxColliderComponent>();
                c.dirty |= UI::DrawVec3Control("Center", c.center, 0.025f, 0.0f);
                c.dirty |= UI::DrawVec3Control("Scale", c.scale, 0.025f, 1.0f);
                // TODO: Collider Edit from UI (TOGGLES)
            });

            RenderComponent<SphereColliderComponent>("Sphere Collider", selectedEntity, [ & ]()
            {
                auto &c = selectedEntity.GetComponent<SphereColliderComponent>();
                c.dirty |= UI::DrawVec3Control("Center", c.center, 0.025f, 0.0f);
                c.dirty |= UI::DrawFloatControl("Radius", &c.radius, 0.025f, 0.01f, 10000.0f, 1.0f);

                // TODO: Collider Edit from UI (TOGGLES)
            });

            RenderComponent<CapsuleColliderComponent>("Capsule Collider", selectedEntity, [ & ]()
            {
                auto &c = selectedEntity.GetComponent<CapsuleColliderComponent>();
                c.dirty |= UI::DrawVec3Control("Center", c.center, 0.025f, 0.0f);
                c.dirty |= UI::DrawFloatControl("Radius", &c.radius, 0.025f, 0.01f, 10000.0f, 1.0f);
                c.dirty |= UI::DrawFloatControl("Height", &c.height, 0.025f, 0.01f, 10000.0f, 1.0f);

                // TODO: Collider Edit from UI (TOGGLES)
            });

            RenderComponent<CharacterControllerComponent>("Character Controller", selectedEntity, [ & ]()
            {
                auto &c = selectedEntity.GetComponent<CharacterControllerComponent>();

                c.dirty |= UI::DrawVec3Control("Center", c.center, 0.025f, 0.0f);
                c.dirty |= UI::DrawFloatControl("Radius", &c.radius, 0.025f, 0.01f, 100.0f, 0.5f);
                c.dirty |= UI::DrawFloatControl("Height", &c.height, 0.025f, 0.01f, 100.0f, 2.0f);
                c.dirty |= UI::DrawFloatControl("Max Step Height", &c.maxStepHeight, 0.01f, 0.0f, 10.0f, 0.4f);
                c.dirty |= UI::DrawFloatControl("Max Slope Angle", &c.maxSlopeAngle, 0.5f, 0.0f, 90.0f, 45.0f);
                c.dirty |= UI::DrawFloatControl("Mass", &c.mass, 0.1f, 0.01f, 10000.0f, 80.0f);
                c.dirty |= UI::DrawFloatControl("Friction", &c.friction, 0.025f, 0.0f, 1.0f, 0.2f);
                c.dirty |= UI::DrawFloatControl("Gravity Factor", &c.gravityFactor, 0.025f, 0.0f, 10.0f, 1.0f);
                c.dirty |= UI::DrawVec3Control("Up Vector", c.up, 0.025f, 0.0f);
                c.dirty |= UI::DrawVec3Control("Linear Velocity", c.linearVelocity, 0.025f, 0.0f);

                if (auto charCtrl = c.character.lock())
                {
                    ImGui::Text("Ground State: %s", charCtrl->IsOnGround() ? "On Ground" : "In Air");
                }
            });

            RenderComponent<MeshColliderComponent>("Mesh Collider", selectedEntity, [ & ]()
            {
                auto &c = selectedEntity.GetComponent<MeshColliderComponent>();
                c.dirty |= UI::DrawVec3Control("Center", c.center, 0.025f, 0.0f);
                c.dirty |= UI::DrawSwitch("Convex", &c.convex);
                ImGui::Text("Vertices: %zu", c.vertices.size());
                ImGui::Text("Indices: %zu", c.indices.size());
                if (UI::Button("Rebuild From Mesh", UI::ButtonVariant::Outline, UI::ButtonSize::Small, ImVec2(0.0f, 22.0f), ICON_LC_REFRESH_CW))
                {
                    c.vertices.clear();
                    c.indices.clear();
                    if (selectedEntity.HasComponent<StaticMeshComponent>())
                    {
                        const auto &smc = selectedEntity.GetComponent<StaticMeshComponent>();
                        if (smc.handle != AssetHandle(0))
                        {
                            if (auto sm = assetManager->GetAsset<StaticMesh>(smc.handle))
                            {
                                sm->ExtractMeshData(c.vertices, c.indices);
                            }
                        }
                    }
                    else if (selectedEntity.HasComponent<SkeletalMeshComponent>())
                    {
                        const auto &skc = selectedEntity.GetComponent<SkeletalMeshComponent>();
                        if (skc.handle != AssetHandle(0))
                        {
                            if (auto skm = assetManager->GetAsset<SkeletalMesh>(skc.handle))
                            {
                                skm->ExtractMeshData(c.vertices, c.indices);
                            }
                        }
                    }
                    c.dirty = true;
                }
                ImGui::SameLine(0.0f, 4.0f);
                if (UI::Button("Clear Mesh Data", UI::ButtonVariant::Outline, UI::ButtonSize::Small, ImVec2(0.0f, 22.0f), ICON_LC_X))
                {
                    c.dirty = true;
                    c.vertices.clear();
                    c.indices.clear();
                }
            });

            RenderComponent<PlaneColliderComponent>("Plane Collider", selectedEntity, [ & ]()
            {
                auto &c = selectedEntity.GetComponent<PlaneColliderComponent>();
                c.dirty |= UI::DrawVec3Control("Scale", c.scale, 0.025f, 1.0f);
                // TODO: Collider Edit from UI (TOGGLES)
            });

            RenderComponent<HeightFieldColliderComponent>("HeightField Collider", selectedEntity, [ & ]()
            {
                auto &c = selectedEntity.GetComponent<HeightFieldColliderComponent>();
                c.dirty |= UI::DrawVec3Control("Center", c.center, 0.025f, 0.0f);
                c.dirty |= UI::DrawVec3Control("Scale", c.scale, 0.025f, 1.0f);
                ImGui::Text("Sample Count: %u", c.sampleCount);
                ImGui::Text("Heights Size: %zu", c.heights.size());
            });

            RenderComponent<PhysicsJointComponent>("Physics Joint", selectedEntity, [ & ]()
            {
                auto &c = selectedEntity.GetComponent<PhysicsJointComponent>();
                auto &d = c.desc;

                if (!selectedEntity.HasComponent<RigidbodyComponent>())
                {
                    ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "Requires a Rigid Body component on this entity.");
                }

                // Connected Entity drag & drop field
                std::string connectedLabel = "World (Fixed)";
                if (c.connectedEntity != UUID(0))
                {
                    Entity targetEnt = SceneManager::GetEntity(m_Scene, c.connectedEntity);
                    if (targetEnt.IsValid())
                    {
                        connectedLabel = targetEnt.GetName();
                    }
                    else
                    {
                        connectedLabel = std::format("Missing ({})", static_cast<uint64_t>(c.connectedEntity));
                    }
                }

                UI::DrawButtonWithColumn("Connected Entity", connectedLabel.c_str(), nullptr, [ &, this ]()
                {
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_ENTITY_SOURCE_ITEM))
                        {
                            const UUID droppedUuid = *static_cast<const UUID *>(payload->Data);
                            if (droppedUuid != selectedEntity.GetUUID())
                            {
                                PhysicsJointComponent beforeSnap = c;
                                c.connectedEntity = droppedUuid;
                                c.dirty = true;
                                m_Scene->SetDirtyFlag(true);
                                if (!m_Scene->IsRunning())
                                {
                                    UUID entityUuid = selectedEntity.GetUUID();
                                    CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<PhysicsJointComponent>>(m_Scene, entityUuid, beforeSnap, c));
                                }
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }
                });

                if (c.connectedEntity != UUID(0))
                {
                    if (UI::Button("Clear Connected Entity", UI::ButtonVariant::Outline, UI::ButtonSize::Small, ImVec2(-1.0f, 22.0f), ICON_LC_UNLINK))
                    {
                        PhysicsJointComponent beforeSnap = c;
                        c.connectedEntity = UUID(0);
                        c.dirty = true;
                        m_Scene->SetDirtyFlag(true);
                        if (!m_Scene->IsRunning())
                        {
                            UUID entityUuid = selectedEntity.GetUUID();
                            CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<PhysicsJointComponent>>(m_Scene, entityUuid, beforeSnap, c));
                        }
                    }
                }

                // Joint Type
                static std::array<const char *, 6> jointTypes = { "Fixed", "Distance", "Point", "Hinge", "Slider", "Cone" };
                int selectedType = static_cast<int>(d.type);
                if (UI::DrawComboBox("Joint Type", jointTypes.data(), static_cast<int>(jointTypes.size()), &selectedType))
                {
                    d.type = static_cast<physics::JointType>(selectedType);
                    c.dirty = true;
                }

                // Space
                static std::array<const char *, 2> jointSpaces = { "Local", "World" };
                int selectedSpace = static_cast<int>(d.space);
                if (UI::DrawComboBox("Space", jointSpaces.data(), static_cast<int>(jointSpaces.size()), &selectedSpace))
                {
                    d.space = static_cast<physics::JointSpace>(selectedSpace);
                    c.dirty = true;
                }

                c.dirty |= UI::DrawSwitch("Enabled", &d.enabled);
                c.dirty |= UI::DrawSwitch("Auto Configure Connected", &d.autoConfigureConnected);

                if (auto rtJoint = c.runtimeJoint.lock())
                {
                    if (rtJoint->IsBroken())
                    {
                        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Status: Broken");
                    }
                }

                // Anchor & Axis A
                c.dirty |= UI::DrawVec3Control("Anchor A", d.localAnchorA, 0.025f, 0.0f);
                if (d.type == physics::JointType::Fixed || d.type == physics::JointType::Hinge || d.type == physics::JointType::Slider || d.type == physics::JointType::Cone)
                {
                    c.dirty |= UI::DrawVec3Control("Axis A", d.localAxisA, 0.025f, 0.0f);
                }
                if (d.type == physics::JointType::Fixed || d.type == physics::JointType::Hinge || d.type == physics::JointType::Slider)
                {
                    c.dirty |= UI::DrawVec3Control("Normal A", d.localNormalA, 0.025f, 0.0f);
                }

                // Anchor & Axis B
                if (!d.autoConfigureConnected || d.type == physics::JointType::Distance)
                {
                    c.dirty |= UI::DrawVec3Control("Anchor B", d.localAnchorB, 0.025f, 0.0f);
                }
                if (!d.autoConfigureConnected)
                {
                    if (d.type == physics::JointType::Fixed || d.type == physics::JointType::Hinge || d.type == physics::JointType::Slider || d.type == physics::JointType::Cone)
                    {
                        c.dirty |= UI::DrawVec3Control("Axis B", d.localAxisB, 0.025f, 0.0f);
                    }
                    if (d.type == physics::JointType::Fixed || d.type == physics::JointType::Hinge || d.type == physics::JointType::Slider)
                    {
                        c.dirty |= UI::DrawVec3Control("Normal B", d.localNormalB, 0.025f, 0.0f);
                    }
                }

                // Type-specific parameters
                if (d.type == physics::JointType::Distance)
                {
                    ImGui::SeparatorText("Distance Settings");
                    c.dirty |= UI::DrawFloatControl("Min Distance", &d.minDistance, 0.025f, 0.0f, 10000.0f);
                    c.dirty |= UI::DrawFloatControl("Max Distance (-1 = Auto)", &d.maxDistance, 0.025f, -1.0f, 10000.0f, -1.0f);
                    c.dirty |= UI::DrawSwitch("Enable Spring", &d.enableSpring);
                    ImGui::BeginDisabled(!d.enableSpring);
                    {
                        c.dirty |= UI::DrawFloatControl("Spring Frequency (Hz)", &d.springFrequency, 0.05f, 0.0f, 100.0f, 2.0f);
                        c.dirty |= UI::DrawFloatControl("Spring Damping", &d.springDamping, 0.025f, 0.0f, 50.0f, 0.5f);
                    }
                    ImGui::EndDisabled();
                }
                else if (d.type == physics::JointType::Hinge)
                {
                    ImGui::SeparatorText("Hinge Limits & Spring");

                    c.dirty |= UI::DrawSwitch("Enable Limits", &d.enableLimits);
                    ImGui::BeginDisabled(!d.enableLimits);
                    {
                        c.dirty |= UI::DrawFloatControl("Min Angle (deg)", &d.limitsMin, 0.5f, -180.0f, 0.0f, -45.0f);
                        c.dirty |= UI::DrawFloatControl("Max Angle (deg)", &d.limitsMax, 0.5f, 0.0f, 180.0f, 45.0f);
                    }
                    ImGui::EndDisabled();

                    c.dirty |= UI::DrawSwitch("Enable Spring", &d.enableSpring);
                    ImGui::BeginDisabled(!d.enableSpring);
                    {
                        c.dirty |= UI::DrawFloatControl("Spring Frequency (Hz)", &d.springFrequency, 0.05f, 0.0f, 100.0f, 2.0f);
                        c.dirty |= UI::DrawFloatControl("Spring Damping", &d.springDamping, 0.025f, 0.0f, 50.0f, 0.5f);
                    }
                    ImGui::EndDisabled();
                    c.dirty |= UI::DrawFloatControl("Max Friction Torque", &d.maxFriction, 0.1f, 0.0f, 100000.0f);

                    ImGui::SeparatorText("Hinge Motor");
                    static std::array<const char *, 3> motorStates = { "Off", "Velocity", "Position" };
                    int selectedMotor = static_cast<int>(d.motorState);
                    if (UI::DrawComboBox("Motor Mode", motorStates.data(), static_cast<int>(motorStates.size()), &selectedMotor))
                    {
                        d.motorState = static_cast<physics::JointMotorState>(selectedMotor);
                        c.dirty = true;
                    }
                    if (d.motorState == physics::JointMotorState::Velocity)
                    {
                        c.dirty |= UI::DrawFloatControl("Target Velocity (deg/s)", &d.motorTargetVelocity, 1.0f, -10000.0f, 10000.0f);
                    }
                    else if (d.motorState == physics::JointMotorState::Position)
                    {
                        c.dirty |= UI::DrawFloatControl("Target Angle (deg)", &d.motorTargetPosition, 0.5f, -180.0f, 180.0f);
                        c.dirty |= UI::DrawFloatControl("Motor Frequency (Hz)", &d.motorFrequency, 0.05f, 0.0f, 100.0f, 2.0f);
                        c.dirty |= UI::DrawFloatControl("Motor Damping", &d.motorDamping, 0.025f, 0.0f, 50.0f, 1.0f);
                    }
                    if (d.motorState != physics::JointMotorState::Off)
                    {
                        c.dirty |= UI::DrawFloatControl("Max Motor Torque", &d.maxMotorForce, 1.0f, 0.0f, 1000000.0f, 1000.0f);
                    }
                }
                else if (d.type == physics::JointType::Slider)
                {
                    ImGui::SeparatorText("Slider Limits & Spring");

                    c.dirty |= UI::DrawSwitch("Enable Limits", &d.enableLimits);
                    ImGui::BeginDisabled(!d.enableLimits);
                    {
                        c.dirty |= UI::DrawFloatControl("Min Limit (m)", &d.limitsMin, 0.05f, -10000.0f, 0.0f, -1.0f);
                        c.dirty |= UI::DrawFloatControl("Max Limit (m)", &d.limitsMax, 0.05f, 0.0f, 10000.0f, 1.0f);
                    }
                    ImGui::EndDisabled();

                    c.dirty |= UI::DrawSwitch("Enable Spring", &d.enableSpring);
                    ImGui::BeginDisabled(!d.enableSpring);
                    {
                        c.dirty |= UI::DrawFloatControl("Spring Frequency (Hz)", &d.springFrequency, 0.05f, 0.0f, 100.0f, 2.0f);
                        c.dirty |= UI::DrawFloatControl("Spring Damping", &d.springDamping, 0.025f, 0.0f, 50.0f, 0.5f);
                    }
                    ImGui::EndDisabled();

                    c.dirty |= UI::DrawFloatControl("Max Friction Force", &d.maxFriction, 0.1f, 0.0f, 100000.0f);

                    ImGui::SeparatorText("Slider Motor");
                    static std::array<const char *, 3> motorStates = { "Off", "Velocity", "Position" };
                    int selectedMotor = static_cast<int>(d.motorState);
                    if (UI::DrawComboBox("Motor Mode", motorStates.data(), static_cast<int>(motorStates.size()), &selectedMotor))
                    {
                        d.motorState = static_cast<physics::JointMotorState>(selectedMotor);
                        c.dirty = true;
                    }
                    if (d.motorState == physics::JointMotorState::Velocity)
                    {
                        c.dirty |= UI::DrawFloatControl("Target Velocity (m/s)", &d.motorTargetVelocity, 0.1f, -10000.0f, 10000.0f);
                    }
                    else if (d.motorState == physics::JointMotorState::Position)
                    {
                        c.dirty |= UI::DrawFloatControl("Target Position (m)", &d.motorTargetPosition, 0.05f, -10000.0f, 10000.0f);
                        c.dirty |= UI::DrawFloatControl("Motor Frequency (Hz)", &d.motorFrequency, 0.05f, 0.0f, 100.0f, 2.0f);
                        c.dirty |= UI::DrawFloatControl("Motor Damping", &d.motorDamping, 0.025f, 0.0f, 50.0f, 1.0f);
                    }
                    if (d.motorState != physics::JointMotorState::Off)
                    {
                        c.dirty |= UI::DrawFloatControl("Max Motor Force", &d.maxMotorForce, 1.0f, 0.0f, 1000000.0f, 1000.0f);
                    }
                }
                else if (d.type == physics::JointType::Cone)
                {
                    ImGui::SeparatorText("Cone Settings");
                    c.dirty |= UI::DrawFloatControl("Half Cone Angle (deg)", &d.halfConeAngle, 0.5f, 0.0f, 180.0f, 45.0f);
                }

                ImGui::SeparatorText("Breaking Thresholds");
                c.dirty |= UI::DrawFloatControl("Break Force (0 = Unbreakable)", &d.breakForce, 1.0f, 0.0f, 10000000.0f);
                c.dirty |= UI::DrawFloatControl("Break Torque (0 = Unbreakable)", &d.breakTorque, 1.0f, 0.0f, 10000000.0f);
            });

            RenderComponent<TextComponent>("Text", selectedEntity, [ & ]()
            {
                auto &c = selectedEntity.GetComponent<TextComponent>();

                const bool isFontLoaded = c.fontHandle != AssetHandle(0);
                std::string fontLabel = isFontLoaded ? "Font Loaded" : "Drag Here";
                UI::DrawButtonWithColumn("Font", fontLabel.c_str(), nullptr, [ &, this ]()
                {
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                        {
                            LOG_ASSERT(payload->DataSize == sizeof(AssetHandle), "WRONG ITEM, that should be an asset handle");
                            AssetHandle handle = *static_cast<AssetHandle *>(payload->Data);
                            if (assetManager->GetAssetType(handle) == AssetType::Font)
                            {
                                c.fontHandle = handle;
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }

                    if (isFontLoaded)
                    {
                        ImGui::SameLine(0.0f, 4.0f);
                        if (UI::IconButton("##ClearTextFont", ICON_LC_X, UI::ButtonVariant::Outline, UI::ButtonSize::IconSm, "Clear Font"))
                        {
                            c.fontHandle = AssetHandle(0);
                        }
                    }
                });

                const bool isMaterialLoaded = c.material2dHandle != AssetHandle(0);
                std::string materialLabel = isMaterialLoaded ? "Material Loaded" : "Drag Here";
                UI::DrawButtonWithColumn("Material", materialLabel.c_str(), nullptr, [ &c, this, assetManager, &isMaterialLoaded ]()
                {
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                        {
                            LOG_ASSERT(payload->DataSize == sizeof(AssetHandle), "WRONG ITEM, that should be an asset handle");
                            AssetHandle handle = *static_cast<AssetHandle *>(payload->Data);
                            if (assetManager->GetAssetType(handle) == AssetType::Material2D)
                            {
                                c.material2dHandle = handle;
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }

                    if (isMaterialLoaded)
                    {
                        ImGui::SameLine(0.0f, 4.0f);
                        if (UI::IconButton("##ClearTextMaterial", ICON_LC_X, UI::ButtonVariant::Outline, UI::ButtonSize::IconSm, "Clear Material"))
                        {
                            c.material2dHandle = AssetHandle(0);
                        }
                    }
                });

                UI::TextArea("##text_component_area", c.text, "Text content...", ImVec2(-1.0f, 68.0f));

                UI::DrawColorVec4("Color", c.color);
                UI::DrawFloatControl("Font Size", &c.fontSize, 0.25f, 4.0f, 256.0f);
                UI::DrawFloatControl("Kerning", &c.kerning, 0.001f, -10.0f, 10.0f);
                UI::DrawFloatControl("Line Spacing", &c.lineSpacing, 0.001f, -10.0f, 10.0f);
                UI::DrawSwitch("Screen Space", &c.screenSpace);
                UI::DrawSwitch("Billboard", &c.billboard);
                UI::DrawSwitch("Center Align", &c.centerAlign);
            });

            RenderComponent<AudioSourceComponent>("Audio Source", selectedEntity, [ & ]()
            {
                auto &c = selectedEntity.GetComponent<AudioSourceComponent>();

                const bool isLoaded = c.handle != AssetHandle(0);
                std::string label = isLoaded ? assetManager->GetAssetDisplayName(c.handle) : "Drag Here";

                UI::DrawButtonWithColumn("Audio", label.c_str(), nullptr, [ &c, this, &isLoaded, assetManager ]()
                {
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                        {
                            if (payload->DataSize == sizeof(AssetHandle))
                            {
                                AssetHandle handle = *static_cast<AssetHandle *>(payload->Data);
                                if (handle != AssetHandle(0))
                                {
                                    AssetMetaData metadata = assetManager->GetMetaData(handle);
                                    if (metadata.type == AssetType::Audio)
                                    {
                                        c.handle = handle;
                                        Ref<FmodSound> sound = assetManager->GetAsset<FmodSound>(handle);
                                    }
                                }
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }

                    if (isLoaded)
                    {
                        ImGui::SameLine(0.0f, 4.0f);
                        if (UI::IconButton("##ClearAudio", ICON_LC_X, UI::ButtonVariant::Outline, UI::ButtonSize::IconSm, "Clear Audio"))
                        {
                            c.handle = AssetHandle(0);
                        }
                    }
                });

                if (isLoaded)
                {
                    if (Ref<FmodSound> sound = assetManager->GetAsset<FmodSound>(c.handle))
                    {
                        std::string soundId = std::format("##{}", (uint64_t)c.handle);

                        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                        ImGui::BeginGroup();
                        if (UI::Button("Play", UI::ButtonVariant::Primary, UI::ButtonSize::Small, ImVec2(0.0f, 22.0f), ICON_LC_PLAY))
                        {
                            sound->Stop();
                            sound->Play();
                            sound->SetVolume(c.volume);
                            sound->SetPitch(c.pitch);
                            sound->SetPan(c.pan);
                            sound->SetMode(c.loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);
                        }

                        ImGui::SameLine(0.0f, 4.0f);
                        if (UI::Button("Stop", UI::ButtonVariant::Outline, UI::ButtonSize::Small, ImVec2(0.0f, 22.0f), ICON_LC_SQUARE))
                        {
                            sound->Stop();
                        }

                        const bool isPlaying = sound->IsPlaying();
                        const bool isPaused = sound->IsPaused();

                        ImGui::SameLine(0.0f, 4.0f);
                        const char *pauseLabel = !isPaused ? "Pause" : "Resume";
                        const char *pauseIcon = !isPaused ? ICON_LC_PAUSE : ICON_LC_PLAY;
                        if (UI::Button(pauseLabel, UI::ButtonVariant::Secondary, UI::ButtonSize::Small, ImVec2(0.0f, 22.0f), pauseIcon, false, !isPlaying && !isPaused))
                        {
                            if (sound->IsPaused())
                                sound->Resume();
                            else if (isPlaying)
                                sound->Pause();
                        }

                        ImGui::EndGroup();

                        if (UI::DrawFloatControl("Volume", &c.volume, 0.001f, 0.0f, 1.0f, 1.0f))
                        {
                            sound->SetVolume(c.volume);
                        }

                        if (UI::DrawFloatControl("Pitch", &c.pitch, 0.001f, 0.0f, 5.0f, 1.0f))
                        {
                            sound->SetPitch(c.pitch);
                        }
                        if (UI::DrawFloatControl("Pan", &c.pan, 0.001f, -1.0f, 1.0f, 0.0f))
                        {
                            sound->SetPan(c.pan);
                        }

                        if (UI::DrawSwitch("Play On Start", &c.playOnStart))
                        {
                        }

                        if (UI::DrawSwitch("Loop", &c.loop))
                        {
                            if (c.loop)
                                sound->SetMode(FMOD_LOOP_NORMAL);
                            else
                                sound->SetMode(FMOD_LOOP_OFF);
                        }

                        bool dspChainChanged = false;

                        if (UI::Button("Add DSP", UI::ButtonVariant::Outline, UI::ButtonSize::Small, ImVec2(0.0f, 22.0f), ICON_LC_PLUS))
                        {
                            UI::OpenPopupMenu("##add_audio_dsp_popup");
                        }

                        if (UI::BeginPopupMenu("##add_audio_dsp_popup"))
                        {
                            UI::ContextMenuLabel("Audio DSP Effects");
                            UI::ContextMenuSeparator();
                            if (UI::ContextMenuItem("Reverb", nullptr, ICON_LC_AUDIO_LINES, false, true, UI::Accent::Audio))
                            {
                                AudioSourceComponent::DspSettings dsp;
                                dsp.type = AudioSourceComponent::DspType::Reverb;
                                c.dsps.push_back(dsp);
                                dspChainChanged = true;
                            }
                            if (UI::ContextMenuItem("Distortion", nullptr, ICON_LC_ZAP, false, true, UI::Accent::Audio))
                            {
                                AudioSourceComponent::DspSettings dsp;
                                dsp.type = AudioSourceComponent::DspType::Distortion;
                                c.dsps.push_back(dsp);
                                dspChainChanged = true;
                            }
                            if (UI::ContextMenuItem("Chorus", nullptr, ICON_LC_AUDIO_WAVEFORM, false, true, UI::Accent::Audio))
                            {
                                AudioSourceComponent::DspSettings dsp;
                                dsp.type = AudioSourceComponent::DspType::Chorus;
                                c.dsps.push_back(dsp);
                                dspChainChanged = true;
                            }
                            if (UI::ContextMenuItem("Compressor", nullptr, ICON_LC_SLIDERS_HORIZONTAL, false, true, UI::Accent::Audio))
                            {
                                AudioSourceComponent::DspSettings dsp;
                                dsp.type = AudioSourceComponent::DspType::Compressor;
                                c.dsps.push_back(dsp);
                                dspChainChanged = true;
                            }
                            if (UI::ContextMenuItem("Delay", nullptr, ICON_LC_TIMER, false, true, UI::Accent::Audio))
                            {
                                AudioSourceComponent::DspSettings dsp;
                                dsp.type = AudioSourceComponent::DspType::Delay;
                                c.dsps.push_back(dsp);
                                dspChainChanged = true;
                            }

                            UI::EndContextMenu();
                        }

                        if (!c.dsps.empty())
                        {
                            ImGui::SeparatorText("DSP Chain");

                            int removeIndex = -1;
                            ImGuiListClipper clipper;
                            clipper.Begin(static_cast<int>(c.dsps.size()));
                            while (clipper.Step())
                            {
                                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
                                {
                                    auto &dsp = c.dsps[static_cast<size_t>(i)];
                                    ImGui::PushID(i);

                                    const char *dspTypeName = "DSP";
                                    switch (dsp.type)
                                    {
                                    case AudioSourceComponent::DspType::Reverb: dspTypeName = "Reverb"; break;
                                    case AudioSourceComponent::DspType::Distortion: dspTypeName = "Distortion"; break;
                                    case AudioSourceComponent::DspType::Chorus: dspTypeName = "Chorus"; break;
                                    case AudioSourceComponent::DspType::Compressor: dspTypeName = "Compressor"; break;
                                    case AudioSourceComponent::DspType::Delay: dspTypeName = "Delay"; break;
                                    }

                                    const std::string header = std::format("{} ##{}", dspTypeName, i);
                                    if (ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                                    {
                                        dspChainChanged |= UI::DrawSwitch("Enabled", &dsp.enabled);
                                        if (UI::Button("Remove DSP", UI::ButtonVariant::Destructive, UI::ButtonSize::Small, ImVec2(0.0f, 22.0f), ICON_LC_TRASH_2))
                                        {
                                            removeIndex = i;
                                        }

                                        switch (dsp.type)
                                        {
                                        case AudioSourceComponent::DspType::Reverb:
                                            dspChainChanged |= UI::DrawFloatControl("Decay Time", &dsp.reverbDecayTime, 1.0f, 100.0f, 2000.0f);
                                            dspChainChanged |= UI::DrawFloatControl("Early Delay", &dsp.reverbEarlyDelay, 0.1f, 0.0f, 300.0f);
                                            dspChainChanged |= UI::DrawFloatControl("Late Delay", &dsp.reverbLateDelay, 0.1f, 0.0f, 300.0f);
                                            dspChainChanged |= UI::DrawFloatControl("HF Reference", &dsp.reverbHighFrequencyReference, 1.0f, 20.0f, 20000.0f);
                                            dspChainChanged |= UI::DrawFloatControl("Diffusion", &dsp.reverbDiffusion, 0.1f, 10.0f, 100.0f);
                                            dspChainChanged |= UI::DrawFloatControl("Density", &dsp.reverbDensity, 0.1f, 10.0f, 100.0f);
                                            dspChainChanged |= UI::DrawFloatControl("Low Shelf Gain", &dsp.reverbLowShelfGain, 1.0f, 20.0f, 1000.0f);
                                            dspChainChanged |= UI::DrawFloatControl("High Cut", &dsp.reverbHighCut, 1.0f, 20.0f, 20000.0f);
                                            dspChainChanged |= UI::DrawFloatControl("Dry Level", &dsp.reverbDryLevel, 0.1f, -80.0f, 20.0f);
                                            dspChainChanged |= UI::DrawFloatControl("Wet Level", &dsp.reverbWetLevel, 0.1f, -80.0f, 20.0f);
                                            break;
                                        case AudioSourceComponent::DspType::Distortion:
                                            dspChainChanged |= UI::DrawFloatControl("Distortion", &dsp.distortionLevel, 0.01f, 0.0f, 1.0f);
                                            break;
                                        case AudioSourceComponent::DspType::Chorus:
                                            dspChainChanged |= UI::DrawFloatControl("Mix", &dsp.chorusMix, 0.1f, 0.0f, 100.0f);
                                            dspChainChanged |= UI::DrawFloatControl("Rate", &dsp.chorusRate, 0.01f, 0.0f, 20.0f);
                                            dspChainChanged |= UI::DrawFloatControl("Depth", &dsp.chorusDepth, 0.1f, 0.0f, 100.0f);
                                            break;
                                        case AudioSourceComponent::DspType::Compressor:
                                            dspChainChanged |= UI::DrawFloatControl("Threshold", &dsp.compressorThreshold, 0.1f, -60.0f, 0.0f);
                                            dspChainChanged |= UI::DrawFloatControl("Ratio", &dsp.compressorRatio, 0.01f, 0.0f, 5.0f);
                                            dspChainChanged |= UI::DrawFloatControl("Release", &dsp.compressorRelease, 1.0f, 10.0f, 5000.0f);
                                            dspChainChanged |= UI::DrawFloatControl("Gain Makeup", &dsp.compressorGainMakeup, 0.1f, -30.0f, 30.0f);
                                            dspChainChanged |= UI::DrawSwitch("Use Sidechain", &dsp.compressorUseSidechain);
                                            break;
                                        case AudioSourceComponent::DspType::Delay:
                                            dspChainChanged |= UI::DrawFloatControl("Delay (ms)", &dsp.delayMs, 1.0f, 0.0f, 10000.0f);
                                            dspChainChanged |= UI::DrawFloatControl("Feedback", &dsp.delayFeedback, 0.1f, 0.0f, 100.0f);
                                            break;
                                        }
                                    }

                                    ImGui::PopID();
                                }
                            }

                            if (removeIndex >= 0 && removeIndex < static_cast<int>(c.dsps.size()))
                            {
                                c.dsps.erase(c.dsps.begin() + removeIndex);
                                dspChainChanged = true;
                            }
                        }

                        if (dspChainChanged)
                        {
                            sound->ClearDsps(true);

                            for (const auto &dspSettings : c.dsps)
                            {
                                FMOD::DSP *dsp = nullptr;
                                FMOD_DSP_TYPE dspType = FMOD_DSP_TYPE_UNKNOWN;

                                switch (dspSettings.type)
                                {
                                case AudioSourceComponent::DspType::Reverb: dspType = FMOD_DSP_TYPE_SFXREVERB; break;
                                case AudioSourceComponent::DspType::Distortion: dspType = FMOD_DSP_TYPE_DISTORTION; break;
                                case AudioSourceComponent::DspType::Chorus: dspType = FMOD_DSP_TYPE_CHORUS; break;
                                case AudioSourceComponent::DspType::Compressor: dspType = FMOD_DSP_TYPE_COMPRESSOR; break;
                                case AudioSourceComponent::DspType::Delay: dspType = FMOD_DSP_TYPE_ECHO; break;
                                }

                                if (dspType == FMOD_DSP_TYPE_UNKNOWN)
                                {
                                    continue;
                                }

                                if (FmodAudio::GetFmodSystem()->createDSPByType(dspType, &dsp) != FMOD_OK || !dsp)
                                {
                                    continue;
                                }

                                switch (dspSettings.type)
                                {
                                case AudioSourceComponent::DspType::Reverb:
                                    dsp->setParameterFloat(FMOD_DSP_SFXREVERB_DECAYTIME, dspSettings.reverbDecayTime);
                                    dsp->setParameterFloat(FMOD_DSP_SFXREVERB_EARLYDELAY, dspSettings.reverbEarlyDelay);
                                    dsp->setParameterFloat(FMOD_DSP_SFXREVERB_LATEDELAY, dspSettings.reverbLateDelay);
                                    dsp->setParameterFloat(FMOD_DSP_SFXREVERB_HFREFERENCE, dspSettings.reverbHighFrequencyReference);
                                    dsp->setParameterFloat(FMOD_DSP_SFXREVERB_DIFFUSION, dspSettings.reverbDiffusion);
                                    dsp->setParameterFloat(FMOD_DSP_SFXREVERB_DENSITY, dspSettings.reverbDensity);
                                    dsp->setParameterFloat(FMOD_DSP_SFXREVERB_LOWSHELFFREQUENCY, dspSettings.reverbLowShelfGain);
                                    dsp->setParameterFloat(FMOD_DSP_SFXREVERB_HIGHCUT, dspSettings.reverbHighCut);
                                    dsp->setParameterFloat(FMOD_DSP_SFXREVERB_DRYLEVEL, dspSettings.reverbDryLevel);
                                    dsp->setParameterFloat(FMOD_DSP_SFXREVERB_WETLEVEL, dspSettings.reverbWetLevel);
                                    break;
                                case AudioSourceComponent::DspType::Distortion:
                                    dsp->setParameterFloat(FMOD_DSP_DISTORTION_LEVEL, dspSettings.distortionLevel);
                                    break;
                                case AudioSourceComponent::DspType::Chorus:
                                    dsp->setParameterFloat(FMOD_DSP_CHORUS_MIX, dspSettings.chorusMix);
                                    dsp->setParameterFloat(FMOD_DSP_CHORUS_RATE, dspSettings.chorusRate);
                                    dsp->setParameterFloat(FMOD_DSP_CHORUS_DEPTH, dspSettings.chorusDepth);
                                    break;
                                case AudioSourceComponent::DspType::Compressor:
                                    dsp->setParameterFloat(FMOD_DSP_COMPRESSOR_THRESHOLD, dspSettings.compressorThreshold);
                                    dsp->setParameterFloat(FMOD_DSP_COMPRESSOR_RATIO, dspSettings.compressorRatio);
                                    dsp->setParameterFloat(FMOD_DSP_COMPRESSOR_RELEASE, dspSettings.compressorRelease);
                                    dsp->setParameterFloat(FMOD_DSP_COMPRESSOR_GAINMAKEUP, dspSettings.compressorGainMakeup);
                                    dsp->setParameterBool(FMOD_DSP_COMPRESSOR_USESIDECHAIN, dspSettings.compressorUseSidechain);
                                    break;
                                case AudioSourceComponent::DspType::Delay:
                                    dsp->setParameterFloat(FMOD_DSP_ECHO_DELAY, dspSettings.delayMs);
                                    dsp->setParameterFloat(FMOD_DSP_ECHO_FEEDBACK, dspSettings.delayFeedback);
                                    break;
                                }

                                dsp->setActive(dspSettings.enabled);
                                sound->AddDsp(dsp);
                            }

                        }
                    }
                }
            });

            RenderComponent<ScriptComponent>("C# Script", selectedEntity, [ & ]()
            {
                auto &c = selectedEntity.GetComponent<ScriptComponent>();
                ScriptEngine *scriptEngine = ScriptEngine::GetInstance();

                bool scriptClassExist = scriptEngine->IsEntityClassExists(c.className);
                if (!scriptClassExist)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
                }

                const bool isRunning = m_Scene && m_Scene->IsRunning();
                const auto &scriptStorage = scriptEngine->GetEntityScriptClassStorage();
                std::string currentScriptClasses = c.className;

                if (!scriptStorage.empty())
                {
                    std::vector<const char *> scriptClassLabels;
                    scriptClassLabels.reserve(scriptStorage.size());

                    int scriptClassIndex = 0;
                    for (size_t i = 0; i < scriptStorage.size(); ++i)
                    {
                        scriptClassLabels.push_back(scriptStorage[i].c_str());
                        if (scriptStorage[i] == currentScriptClasses)
                        {
                            scriptClassIndex = static_cast<int>(i);
                        }
                    }

                    ImGui::BeginDisabled(isRunning);
                    if (UI::DrawComboBox("Script Class", scriptClassLabels.data(), static_cast<int>(scriptClassLabels.size()), &scriptClassIndex))
                    {
                        currentScriptClasses = scriptStorage[static_cast<size_t>(scriptClassIndex)];
                        c.className = currentScriptClasses;
                    }
                    if (UI::Button("Detach Script", UI::ButtonVariant::Outline, UI::ButtonSize::Small, ImVec2(-1.0f, 22.0f), ICON_LC_UNLINK))
                        c.className = "Detached";
                    ImGui::EndDisabled();
                }

                const bool detached = c.className == "Detached";
                if (scriptClassExist && !detached)
                {
                    Ref<ScriptClass> scriptClass = scriptEngine->GetEntityClassByName(c.className);
                    if (scriptClass)
                    {
                        const uint64_t instanceId = selectedEntity.GetUUID();
                        auto classRegisteredInstanceField = scriptClass->GetInstanceFieldsById(instanceId);

                        if (!classRegisteredInstanceField)
                        {
                            std::unordered_map<std::string, ScriptInstanceField> emptyInstanceFields;
                            scriptClass->InsertInstanceFields(instanceId, emptyInstanceFields);
                            classRegisteredInstanceField = scriptClass->GetInstanceFieldsById(instanceId);
                        }

                        if (classRegisteredInstanceField)
                        {
                            for (const auto &name : scriptClass->GetOrderedFieldNames())
                            {
                                const auto &field = scriptClass->GetFields().at(name);
                                ImGui::PushID(name.c_str());

                                ScriptInstanceField dummy;
                                dummy.field = field;
                                auto it = classRegisteredInstanceField->find(name);
                                if (it != classRegisteredInstanceField->end())
                                {
                                    dummy = it->second;
                                }
                                else if (scriptClass->GetDefaultFields().contains(name))
                                {
                                    dummy = scriptClass->GetDefaultFields().at(name);
                                }
                                else if (scriptEngine->GetScriptHost())
                                {
                                    char buffer[64] = { 0 };
                                    if (scriptEngine->GetScriptHost()->GetInstanceFieldValue(instanceId, name, buffer, sizeof(buffer)))
                                    {
                                        dummy.SetValueRaw(buffer, sizeof(buffer));
                                    }
                                }

                                switch (field.Type)
                                {
                                case ScriptFieldType::Bool:
                                {
                                    auto data = dummy.GetValue<bool>();
                                    if (UI::DrawSwitch(name.c_str(), &data))
                                    {
                                        dummy.SetValue<bool>(data);
                                        (*classRegisteredInstanceField)[name] = dummy;
                                        if (c.runtimeScriptInstance)
                                            c.runtimeScriptInstance->SetFieldValue<bool>(name, data);
                                    }
                                    break;
                                }
                                case ScriptFieldType::String:
                                {
                                    auto data = dummy.GetValue<std::string>();
                                    char buffer[256];
                                    memset(buffer, 0, sizeof(buffer));
                                    strncpy(buffer, data.c_str(), sizeof(buffer) - 1);

                                    ImGui::BeginColumns(name.c_str(), 2);
                                    ImGui::SetColumnWidth(0, 100.0f);
                                    ImGui::Text("%s", name.c_str());
                                    ImGui::NextColumn();

                                    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                                    if (ImGui::InputText("##StringControl", buffer, sizeof(buffer)))
                                    {
                                        std::string newData(buffer);
                                        dummy.SetValue<std::string>(newData);
                                        (*classRegisteredInstanceField)[name] = dummy;
                                        if (c.runtimeScriptInstance)
                                            c.runtimeScriptInstance->SetFieldValue<std::string>(name, newData);
                                    }
                                    ImGui::PopItemWidth();
                                    ImGui::EndColumns();
                                    break;
                                }
                                case ScriptFieldType::Float:
                                {
                                    auto data = dummy.GetValue<float>();
                                    bool changed = (field.uiType == FieldUIType::Slider)
                                        ? UI::DrawSliderFloat(name.c_str(), &data, field.minValue, field.maxValue)
                                        : UI::DrawFloatControl(name.c_str(), &data, field.speed, -FLT_MAX, FLT_MAX);
                                    if (changed)
                                    {
                                        dummy.SetValue<float>(data);
                                        (*classRegisteredInstanceField)[name] = dummy;
                                        if (c.runtimeScriptInstance)
                                            c.runtimeScriptInstance->SetFieldValue<float>(name, data);
                                    }
                                    break;
                                }
                                case ScriptFieldType::Int:
                                {
                                    auto data = dummy.GetValue<int>();
                                    bool changed = (field.uiType == FieldUIType::Slider)
                                        ? UI::DrawSliderInt(name.c_str(), &data, static_cast<int>(field.minValue), static_cast<int>(field.maxValue))
                                        : UI::DrawIntControl(name.c_str(), &data, 1.0f, INT_MIN, INT_MAX);
                                    if (changed)
                                    {
                                        dummy.SetValue<int>(data);
                                        (*classRegisteredInstanceField)[name] = dummy;
                                        if (c.runtimeScriptInstance)
                                            c.runtimeScriptInstance->SetFieldValue<int>(name, data);
                                    }
                                    break;
                                }
                                case ScriptFieldType::UInt:
                                {
                                    auto val = dummy.GetValue<uint32_t>();
                                    int data = static_cast<int>(val);
                                    bool changed = (field.uiType == FieldUIType::Slider)
                                        ? UI::DrawSliderInt(name.c_str(), &data, static_cast<int>(field.minValue), static_cast<int>(field.maxValue))
                                        : UI::DrawIntControl(name.c_str(), &data, 1.0f, 0, INT_MAX);
                                    if (changed)
                                    {
                                        val = static_cast<uint32_t>(data);
                                        dummy.SetValue<uint32_t>(val);
                                        (*classRegisteredInstanceField)[name] = dummy;
                                        if (c.runtimeScriptInstance)
                                            c.runtimeScriptInstance->SetFieldValue<uint32_t>(name, val);
                                    }
                                    break;
                                }
                                case ScriptFieldType::Byte:
                                {
                                    auto val = dummy.GetValue<uint8_t>();
                                    int data = static_cast<int>(val);
                                    bool changed = (field.uiType == FieldUIType::Slider)
                                        ? UI::DrawSliderInt(name.c_str(), &data, static_cast<int>(field.minValue), static_cast<int>(field.maxValue))
                                        : UI::DrawIntControl(name.c_str(), &data, 1.0f, 0, 255);
                                    if (changed)
                                    {
                                        val = static_cast<uint8_t>(data);
                                        dummy.SetValue<uint8_t>(val);
                                        (*classRegisteredInstanceField)[name] = dummy;
                                        if (c.runtimeScriptInstance)
                                            c.runtimeScriptInstance->SetFieldValue<uint8_t>(name, val);
                                    }
                                    break;
                                }
                                case ScriptFieldType::SByte:
                                {
                                    auto val = dummy.GetValue<int8_t>();
                                    int data = static_cast<int>(val);
                                    bool changed = (field.uiType == FieldUIType::Slider)
                                        ? UI::DrawSliderInt(name.c_str(), &data, static_cast<int>(field.minValue), static_cast<int>(field.maxValue))
                                        : UI::DrawIntControl(name.c_str(), &data, 1.0f, -128, 127);
                                    if (changed)
                                    {
                                        val = static_cast<int8_t>(data);
                                        dummy.SetValue<int8_t>(val);
                                        (*classRegisteredInstanceField)[name] = dummy;
                                        if (c.runtimeScriptInstance)
                                            c.runtimeScriptInstance->SetFieldValue<int8_t>(name, val);
                                    }
                                    break;
                                }
                                case ScriptFieldType::Short:
                                {
                                    auto val = dummy.GetValue<int16_t>();
                                    int data = static_cast<int>(val);
                                    bool changed = (field.uiType == FieldUIType::Slider)
                                        ? UI::DrawSliderInt(name.c_str(), &data, static_cast<int>(field.minValue), static_cast<int>(field.maxValue))
                                        : UI::DrawIntControl(name.c_str(), &data, 1.0f, SHRT_MIN, SHRT_MAX);
                                    if (changed)
                                    {
                                        val = static_cast<int16_t>(data);
                                        dummy.SetValue<int16_t>(val);
                                        (*classRegisteredInstanceField)[name] = dummy;
                                        if (c.runtimeScriptInstance)
                                            c.runtimeScriptInstance->SetFieldValue<int16_t>(name, val);
                                    }
                                    break;
                                }
                                case ScriptFieldType::UShort:
                                {
                                    auto val = dummy.GetValue<uint16_t>();
                                    int data = static_cast<int>(val);
                                    bool changed = (field.uiType == FieldUIType::Slider)
                                        ? UI::DrawSliderInt(name.c_str(), &data, static_cast<int>(field.minValue), static_cast<int>(field.maxValue))
                                        : UI::DrawIntControl(name.c_str(), &data, 1.0f, 0, USHRT_MAX);
                                    if (changed)
                                    {
                                        val = static_cast<uint16_t>(data);
                                        dummy.SetValue<uint16_t>(val);
                                        (*classRegisteredInstanceField)[name] = dummy;
                                        if (c.runtimeScriptInstance)
                                            c.runtimeScriptInstance->SetFieldValue<uint16_t>(name, val);
                                    }
                                    break;
                                }
                                case ScriptFieldType::Long:
                                {
                                    auto val = dummy.GetValue<int64_t>();
                                    int data = static_cast<int>(val);
                                    bool changed = (field.uiType == FieldUIType::Slider)
                                        ? UI::DrawSliderInt(name.c_str(), &data, static_cast<int>(field.minValue), static_cast<int>(field.maxValue))
                                        : UI::DrawIntControl(name.c_str(), &data, 1.0f, INT_MIN, INT_MAX);
                                    if (changed)
                                    {
                                        val = static_cast<int64_t>(data);
                                        dummy.SetValue<int64_t>(val);
                                        (*classRegisteredInstanceField)[name] = dummy;
                                        if (c.runtimeScriptInstance)
                                            c.runtimeScriptInstance->SetFieldValue<int64_t>(name, val);
                                    }
                                    break;
                                }
                                case ScriptFieldType::ULong:
                                {
                                    auto val = dummy.GetValue<uint64_t>();
                                    int data = static_cast<int>(val);
                                    bool changed = (field.uiType == FieldUIType::Slider)
                                        ? UI::DrawSliderInt(name.c_str(), &data, static_cast<int>(field.minValue), static_cast<int>(field.maxValue))
                                        : UI::DrawIntControl(name.c_str(), &data, 1.0f, 0, INT_MAX);
                                    if (changed)
                                    {
                                        val = static_cast<uint64_t>(data);
                                        dummy.SetValue<uint64_t>(val);
                                        (*classRegisteredInstanceField)[name] = dummy;
                                        if (c.runtimeScriptInstance)
                                            c.runtimeScriptInstance->SetFieldValue<uint64_t>(name, val);
                                    }
                                    break;
                                }
                                case ScriptFieldType::Enum:
                                {
                                    auto data = dummy.GetValue<int>();
                                    const auto &enumNames = field.EnumNames;
                                    const auto &enumValues = field.EnumValues;

                                    int selectedIndex = -1;
                                    for (int i = 0; i < (int)enumValues.size(); ++i)
                                    {
                                        if (enumValues[i] == data)
                                        {
                                            selectedIndex = i;
                                            break;
                                        }
                                    }

                                    std::vector<const char *> labels;
                                    for (const auto &n : enumNames)
                                        labels.push_back(n.c_str());

                                    if (UI::DrawComboBox(name.c_str(), labels.data(), (int)labels.size(), &selectedIndex))
                                    {
                                        data = enumValues[selectedIndex];
                                        dummy.SetValue<int>(data);
                                        (*classRegisteredInstanceField)[name] = dummy;
                                        if (c.runtimeScriptInstance)
                                            c.runtimeScriptInstance->SetFieldValue<int>(name, data);
                                    }
                                    break;
                                }
                                case ScriptFieldType::Double:
                                {
                                    auto dData = dummy.GetValue<double>();
                                    float data = static_cast<float>(dData);
                                    bool changed = (field.uiType == FieldUIType::Slider)
                                        ? UI::DrawSliderFloat(name.c_str(), &data, field.minValue, field.maxValue)
                                        : UI::DrawFloatControl(name.c_str(), &data, field.speed, -FLT_MAX, FLT_MAX);
                                    if (changed)
                                    {
                                        dData = static_cast<double>(data);
                                        dummy.SetValue<double>(dData);
                                        (*classRegisteredInstanceField)[name] = dummy;
                                        if (c.runtimeScriptInstance)
                                            c.runtimeScriptInstance->SetFieldValue<double>(name, dData);
                                    }
                                    break;
                                }
                                case ScriptFieldType::Vector2:
                                {
                                    auto data = dummy.GetValue<glm::vec2>();
                                    if (UI::DrawVec2Control(name.c_str(), data, field.speed))
                                    {
                                        dummy.SetValue<glm::vec2>(data);
                                        (*classRegisteredInstanceField)[name] = dummy;
                                        if (c.runtimeScriptInstance)
                                            c.runtimeScriptInstance->SetFieldValue<glm::vec2>(name, data);
                                    }
                                    break;
                                }
                                case ScriptFieldType::Vector3:
                                {
                                    auto data = dummy.GetValue<glm::vec3>();
                                    if (UI::DrawVec3Control(name.c_str(), data, field.speed))
                                    {
                                        dummy.SetValue<glm::vec3>(data);
                                        (*classRegisteredInstanceField)[name] = dummy;
                                        if (c.runtimeScriptInstance)
                                            c.runtimeScriptInstance->SetFieldValue<glm::vec3>(name, data);
                                    }
                                    break;
                                }
                                case ScriptFieldType::Vector4:
                                {
                                    auto data = dummy.GetValue<glm::vec4>();
                                    if (UI::DrawVec4Control(name.c_str(), data, field.speed))
                                    {
                                        dummy.SetValue<glm::vec4>(data);
                                        (*classRegisteredInstanceField)[name] = dummy;
                                        if (c.runtimeScriptInstance)
                                            c.runtimeScriptInstance->SetFieldValue<glm::vec4>(name, data);
                                    }
                                    break;
                                }
                                case ScriptFieldType::Quat:
                                {
                                    auto data = dummy.GetValue<glm::quat>();
                                    glm::vec4 vec = { data.x, data.y, data.z, data.w };
                                    if (UI::DrawVec4Control(name.c_str(), vec, field.speed))
                                    {
                                        data = { vec.w, vec.x, vec.y, vec.z };
                                        dummy.SetValue<glm::quat>(data);
                                        (*classRegisteredInstanceField)[name] = dummy;
                                        if (c.runtimeScriptInstance)
                                            c.runtimeScriptInstance->SetFieldValue<glm::quat>(name, data);
                                    }
                                    break;
                                }
                                case ScriptFieldType::Color:
                                {
                                    auto data = dummy.GetValue<glm::vec4>();
                                    if (UI::DrawColorVec4(name.c_str(), data))
                                    {
                                        dummy.SetValue<glm::vec4>(data);
                                        (*classRegisteredInstanceField)[name] = dummy;
                                        if (c.runtimeScriptInstance)
                                            c.runtimeScriptInstance->SetFieldValue<glm::vec4>(name, data);
                                    }
                                    break;
                                }
                                case ScriptFieldType::Entity:
                                case ScriptFieldType::Asset:
                                {
                                    auto handle = dummy.GetValue<uint64_t>();
                                    std::string label = "Drag Here";

                                    if (field.Type == ScriptFieldType::Entity)
                                    {
                                        if (handle)
                                        {
                                            Entity e = SceneManager::GetEntity(m_Scene, UUID(handle));
                                            if (e)
                                                label = e.GetName();
                                        }
                                    }
                                    else if (field.Type == ScriptFieldType::Asset)
                                    {
                                        if (handle)
                                        {
                                            label = assetManager->GetAssetDisplayName(AssetHandle(handle));
                                        }
                                    }

                                    ImGui::BeginDisabled(isRunning);
                                    UI::DrawButtonWithColumn(name.c_str(), label.c_str(), nullptr, [ this, &name, &dummy, &classRegisteredInstanceField, &c, handle, assetManager, &field ]()
                                    {
                                        if (ImGui::BeginDragDropTarget())
                                        {
                                            if (field.Type == ScriptFieldType::Entity)
                                            {
                                                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_ENTITY_SOURCE_ITEM))
                                                {
                                                    const size_t count = payload->DataSize / sizeof(UUID);
                                                    auto UUIDs = static_cast<UUID *>(payload->Data);

                                                    // TODO: Support multiple entities drag and drop

                                                    // Get first entity from the payload
                                                    if (count > 0)
                                                    {
                                                        UUID entityUUID = UUIDs[0];
                                                        Entity src { SceneManager::GetEntity(m_Scene, entityUUID) };
                                                        if (src)
                                                        {
                                                            uint64_t id = (uint64_t)src.GetUUID();
                                                            dummy.SetValue<uint64_t>(id);
                                                            (*classRegisteredInstanceField)[name] = dummy;
                                                            if (c.runtimeScriptInstance)
                                                                c.runtimeScriptInstance->SetFieldValue<uint64_t>(name, id);
                                                        }
                                                    }
                                                }
                                            }
                                            else if (field.Type == ScriptFieldType::Asset)
                                            {
                                                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                                                {
                                                    LOG_ASSERT(payload->DataSize == sizeof(AssetHandle), "WRONG ASSET ITEM");
                                                    if (payload->DataSize == sizeof(AssetHandle))
                                                    {
                                                        AssetHandle h = *static_cast<AssetHandle *>(payload->Data);
                                                        uint64_t id = (uint64_t)h;
                                                        dummy.SetValue<uint64_t>(id);
                                                        (*classRegisteredInstanceField)[name] = dummy;
                                                        if (c.runtimeScriptInstance)
                                                            c.runtimeScriptInstance->SetFieldValue<uint64_t>(name, id);
                                                    }
                                                }
                                            }
                                            ImGui::EndDragDropTarget();
                                        }

                                        if (ImGui::IsItemHovered())
                                        {
                                            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                                            {
                                                uint64_t id = 0;
                                                dummy.SetValue<uint64_t>(id);
                                                (*classRegisteredInstanceField)[name] = dummy;
                                                if (c.runtimeScriptInstance)
                                                    c.runtimeScriptInstance->SetFieldValue<uint64_t>(name, id);
                                            }

                                            ImGui::BeginTooltip();
                                            if (handle)
                                            {
                                                if (field.Type == ScriptFieldType::Entity)
                                                {
                                                    Entity e = SceneManager::GetEntity(m_Scene, UUID(handle));
                                                    if (e) ImGui::Text("Entity Name : %s\nEntity ID : %llu", e.GetName().c_str(), handle);
                                                    else ImGui::Text("Invalid Entity");
                                                }
                                                else
                                                {
                                                    AssetMetaData metadata = assetManager->GetMetaData(AssetHandle(handle));
                                                    ImGui::Text("Asset Name : %s\nAsset ID : %llu\nType : %s", assetManager->GetAssetDisplayName(AssetHandle(handle)).c_str(), handle, AssetTypeToString(metadata.type).c_str());
                                                }
                                            }
                                            else
                                            {
                                                ImGui::Text(field.Type == ScriptFieldType::Entity ? "Empty Entity" : "Empty Asset");
                                            }
                                            ImGui::EndTooltip();
                                        }
                                    });
                                    if (handle)
                                    {
                                        ImGui::SameLine(0.0f, 4.0f);
                                        if (UI::IconButton("##ClearCSScriptField", ICON_LC_X, UI::ButtonVariant::Outline, UI::ButtonSize::IconSm, "Clear"))
                                        {
                                            dummy.SetValue<uint64_t>(0);
                                            (*classRegisteredInstanceField)[name] = dummy;
                                            if (c.runtimeScriptInstance)
                                            {
                                                c.runtimeScriptInstance->SetFieldValue<uint64_t>(name, 0);
                                            }
                                        }
                                    }
                                    ImGui::EndDisabled();

                                    break;
                                }
                                }
                                ImGui::PopID();
                            }
                        }

                    }
                }

                if (!scriptClassExist)
                {
                    ImGui::PopStyleColor();
                }
            });

            RenderComponent<LuaScriptComponent>("Lua Script", selectedEntity, [ & ]()
            {
                auto &component = selectedEntity.GetComponent<LuaScriptComponent>();
                const bool isRunning = m_Scene && m_Scene->IsRunning();

                std::string label = "Drag Lua Script Here";
                const bool isLoaded = component.scriptHandle != AssetHandle(0);
                if (isLoaded)
                {
                    std::string assetName = assetManager->GetAssetDisplayName(component.scriptHandle);
                    if (!assetName.empty())
                        label = assetName;
                    else if (!component.scriptPath.empty())
                        label = component.scriptPath.filename().generic_string();
                }
                else if (!component.scriptPath.empty())
                {
                    label = component.scriptPath.filename().generic_string();
                }

                UI::DrawButtonWithColumn("Script Asset", label.c_str(), nullptr, [ & ]()
                {
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                        {
                            if (payload->DataSize == sizeof(AssetHandle))
                            {
                                AssetHandle handle = *static_cast<const AssetHandle *>(payload->Data);
                                AssetMetaData metadata = assetManager->GetMetaData(handle);
                                if (metadata.type == AssetType::LuaScript || metadata.filepath.extension() == ".lua")
                                {
                                    component.scriptHandle = handle;
                                    component.scriptPath = metadata.filepath;
                                    component.fields.clear();
                                    m_Scene->SetDirtyFlag(true);

                                    if (isRunning && m_Scene->GetLuaScriptRuntime())
                                    {
                                        m_Scene->GetLuaScriptRuntime()->CreateInstance(selectedEntity.GetUUID(), component.scriptPath, component.scriptHandle);
                                    }
                                }
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }

                    if (isLoaded)
                    {
                        ImGui::SameLine(0.0f, 4.0f);
                        if (UI::IconButton("##ClearLuaScript", ICON_LC_X, UI::ButtonVariant::Outline, UI::ButtonSize::IconSm, "Clear Script"))
                        {
                            component.scriptHandle = AssetHandle(0);
                            component.scriptPath.clear();
                            component.fields.clear();
                            m_Scene->SetDirtyFlag(true);

                            if (isRunning && m_Scene->GetLuaScriptRuntime())
                            {
                                m_Scene->GetLuaScriptRuntime()->DestroyInstance(selectedEntity.GetUUID());
                            }
                        }
                    }
                });

                if (component.scriptHandle != AssetHandle(0))
                {
                    ImGui::TextDisabled("Handle: %llu", static_cast<unsigned long long>(static_cast<uint64_t>(component.scriptHandle)));
                }
                if (!component.scriptPath.empty())
                {
                    ImGui::TextDisabled("Path: %s", component.scriptPath.generic_string().c_str());
                }

                // Resolve script filepath for metadata inspection
                std::filesystem::path resolvedScriptPath = component.scriptPath;
                if (component.scriptHandle != AssetHandle(0) && assetManager)
                {
                    const auto &meta = assetManager->GetMetaData(component.scriptHandle);
                    if (!meta.filepath.empty())
                        resolvedScriptPath = meta.filepath;
                }
                if (!resolvedScriptPath.empty() && !resolvedScriptPath.is_absolute() && m_Scene && m_Scene->GetProject())
                {
                    Project *proj = m_Scene->GetProject();
                    const std::filesystem::path dirCandidate = (proj->GetDirectory() / resolvedScriptPath).lexically_normal();
                    resolvedScriptPath = std::filesystem::exists(dirCandidate)
                        ? dirCandidate
                        : proj->GetProjectFilepath(resolvedScriptPath);
                }

                // Render exposed Lua fields (from runtime instance if playing, else cached Edit-mode inspection)
                std::vector<LuaFieldDesc> inspectedFields;
                const std::vector<LuaFieldDesc> *luaFieldsPtr = nullptr;
                if (isRunning && m_Scene->GetLuaScriptRuntime())
                {
                    luaFieldsPtr = m_Scene->GetLuaScriptRuntime()->GetInstanceFields(selectedEntity.GetUUID());
                }
                if (!luaFieldsPtr && !resolvedScriptPath.empty())
                {
                    inspectedFields = lua::Script::GetOrInspectFields(resolvedScriptPath);
                    luaFieldsPtr = &inspectedFields;
                }

                if (luaFieldsPtr && !luaFieldsPtr->empty())
                {
                    ImGui::SeparatorText("Fields");
                    for (const LuaFieldDesc &desc : *luaFieldsPtr)
                    {
                        ImGui::PushID(desc.name.c_str());

                        LuaFieldValue curValue = desc.defaultValue;
                        if (auto it = component.fields.find(desc.name); it != component.fields.end())
                        {
                            if (IsLuaFieldValueCompatible(desc.type, it->second.value))
                                curValue = it->second.value;
                            else if (desc.type == ScriptFieldType::Float && std::holds_alternative<int32_t>(it->second.value))
                                curValue = static_cast<float>(std::get<int32_t>(it->second.value));
                            else if (desc.type == ScriptFieldType::Int && std::holds_alternative<float>(it->second.value))
                                curValue = static_cast<int32_t>(std::get<float>(it->second.value));
                        }
                        if (isRunning && m_Scene->GetLuaScriptRuntime())
                        {
                            LuaFieldValue liveValue;
                            if (m_Scene->GetLuaScriptRuntime()->GetFieldValue(selectedEntity.GetUUID(), desc.name, liveValue))
                                curValue = liveValue;
                        }
                        curValue = ClampLuaFieldValue(desc, curValue);

                        auto commitValue = [ & ](const LuaFieldValue &newValue)
                        {
                            const LuaFieldValue clamped = ClampLuaFieldValue(desc, newValue);
                            component.fields[desc.name] = LuaSerializedField { desc.type, clamped };
                            m_Scene->SetDirtyFlag(true);
                            if (isRunning && m_Scene->GetLuaScriptRuntime())
                            {
                                m_Scene->GetLuaScriptRuntime()->SetFieldValue(selectedEntity.GetUUID(), desc.name, clamped);
                            }
                        };

                        switch (desc.type)
                        {
                        case ScriptFieldType::Bool:
                        {
                            bool data = std::holds_alternative<bool>(curValue) ? std::get<bool>(curValue) : false;
                            if (UI::DrawSwitch(desc.name.c_str(), &data))
                                commitValue(data);
                            break;
                        }
                        case ScriptFieldType::Int:
                        {
                            int data = std::holds_alternative<int32_t>(curValue) ? std::get<int32_t>(curValue) : 0;
                            const int minV = desc.hasMin ? static_cast<int>(desc.minValue) : INT_MIN;
                            const int maxV = desc.hasMax ? static_cast<int>(desc.maxValue) : INT_MAX;
                            const int defV = std::holds_alternative<int32_t>(desc.defaultValue) ? std::get<int32_t>(desc.defaultValue) : 0;
                            bool changed = (desc.uiType == FieldUIType::Slider)
                                ? UI::DrawSliderInt(desc.name.c_str(), &data, desc.hasMin ? minV : 0, desc.hasMax ? maxV : 100)
                                : UI::DrawIntControl(desc.name.c_str(), &data, std::max(1.0f, desc.speed), minV, maxV, defV);
                            if (changed)
                                commitValue(static_cast<int32_t>(data));
                            break;
                        }
                        case ScriptFieldType::Float:
                        {
                            float data = std::holds_alternative<float>(curValue) ? std::get<float>(curValue) : 0.0f;
                            const float minV = desc.hasMin ? desc.minValue : -FLT_MAX;
                            const float maxV = desc.hasMax ? desc.maxValue : FLT_MAX;
                            const float defV = std::holds_alternative<float>(desc.defaultValue) ? std::get<float>(desc.defaultValue) : 0.0f;
                            bool changed = (desc.uiType == FieldUIType::Slider)
                                ? UI::DrawSliderFloat(desc.name.c_str(), &data, desc.hasMin ? minV : 0.0f, desc.hasMax ? maxV : 1.0f)
                                : UI::DrawFloatControl(desc.name.c_str(), &data, desc.speed, minV, maxV, defV);
                            if (changed)
                                commitValue(data);
                            break;
                        }
                        case ScriptFieldType::String:
                        {
                            std::string data = std::holds_alternative<std::string>(curValue) ? std::get<std::string>(curValue) : std::string();
                            if (UI::DrawTextControl(desc.name.c_str(), data))
                            {
                                commitValue(data);
                            }
                            break;
                        }
                        case ScriptFieldType::Vector2:
                        {
                            glm::vec2 data = std::holds_alternative<glm::vec2>(curValue) ? std::get<glm::vec2>(curValue) : glm::vec2(0.0f);
                            if (UI::DrawVec2Control(desc.name.c_str(), data, desc.speed))
                                commitValue(data);
                            break;
                        }
                        case ScriptFieldType::Vector3:
                        {
                            glm::vec3 data = std::holds_alternative<glm::vec3>(curValue) ? std::get<glm::vec3>(curValue) : glm::vec3(0.0f);
                            if (UI::DrawVec3Control(desc.name.c_str(), data, desc.speed))
                                commitValue(data);
                            break;
                        }
                        case ScriptFieldType::Vector4:
                        {
                            glm::vec4 data = std::holds_alternative<glm::vec4>(curValue) ? std::get<glm::vec4>(curValue) : glm::vec4(0.0f);
                            if (UI::DrawVec4Control(desc.name.c_str(), data, desc.speed))
                                commitValue(data);
                            break;
                        }
                        case ScriptFieldType::Color:
                        {
                            glm::vec4 data = std::holds_alternative<glm::vec4>(curValue) ? std::get<glm::vec4>(curValue) : glm::vec4(1.0f);
                            if (UI::DrawColorVec4(desc.name.c_str(), data))
                                commitValue(data);
                            break;
                        }
                        case ScriptFieldType::Enum:
                        {
                            if (!desc.enumNames.empty())
                            {
                                int selectedIndex = 0;
                                if (std::holds_alternative<std::string>(curValue))
                                {
                                    const std::string &curStr = std::get<std::string>(curValue);
                                    for (int i = 0; i < static_cast<int>(desc.enumNames.size()); ++i)
                                    {
                                        if (desc.enumNames[static_cast<size_t>(i)] == curStr)
                                        {
                                            selectedIndex = i;
                                            break;
                                        }
                                    }
                                }
                                else if (std::holds_alternative<int32_t>(curValue))
                                {
                                    const int idx = std::get<int32_t>(curValue) - 1;
                                    if (idx >= 0 && idx < static_cast<int>(desc.enumNames.size()))
                                        selectedIndex = idx;
                                }

                                std::vector<const char *> labels;
                                labels.reserve(desc.enumNames.size());
                                for (const auto &opt : desc.enumNames)
                                    labels.push_back(opt.c_str());

                                if (UI::DrawComboBox(desc.name.c_str(), labels.data(), static_cast<int>(labels.size()), &selectedIndex))
                                {
                                    if (std::holds_alternative<int32_t>(desc.defaultValue))
                                        commitValue(static_cast<int32_t>(selectedIndex + 1));
                                    else
                                        commitValue(desc.enumNames[static_cast<size_t>(selectedIndex)]);
                                }
                            }
                            break;
                        }
                        case ScriptFieldType::Entity:
                        case ScriptFieldType::Asset:
                        {
                            const uint64_t handle = std::holds_alternative<uint64_t>(curValue) ? std::get<uint64_t>(curValue) : 0ULL;
                            std::string refLabel = desc.type == ScriptFieldType::Entity ? "Drag Entity Here" : "Drag Asset Here";
                            if (handle != 0)
                            {
                                if (desc.type == ScriptFieldType::Entity)
                                {
                                    Entity refEntity = SceneManager::GetEntity(m_Scene, UUID(handle));
                                    refLabel = refEntity ? refEntity.GetName() : "Missing Entity";
                                }
                                else if (assetManager)
                                {
                                    refLabel = assetManager->GetAssetDisplayName(AssetHandle(handle));
                                }
                            }

                            UI::DrawButtonWithColumn(desc.name.c_str(), refLabel.c_str(), nullptr, [ & ]()
                            {
                                if (ImGui::BeginDragDropTarget())
                                {
                                    if (desc.type == ScriptFieldType::Entity)
                                    {
                                        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_ENTITY_SOURCE_ITEM))
                                        {
                                            const size_t count = payload->DataSize / sizeof(UUID);
                                            auto *uuids = static_cast<const UUID *>(payload->Data);
                                            if (count > 0)
                                            {
                                                Entity src = SceneManager::GetEntity(m_Scene, uuids[0]);
                                                if (src)
                                                    commitValue(static_cast<uint64_t>(src.GetUUID()));
                                            }
                                        }
                                    }
                                    else if (assetManager)
                                    {
                                        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                                        {
                                            if (payload->DataSize == sizeof(AssetHandle))
                                            {
                                                const AssetHandle h = *static_cast<const AssetHandle *>(payload->Data);
                                                bool accept = true;
                                                if (!desc.assetTypeFilter.empty())
                                                {
                                                    const AssetMetaData meta = assetManager->GetMetaData(h);
                                                    const std::string actual = stringutils::ToLower(AssetTypeToString(meta.type));
                                                    const std::string expected = stringutils::ToLower(desc.assetTypeFilter);
                                                    accept = (actual == expected);
                                                }
                                                if (accept)
                                                    commitValue(static_cast<uint64_t>(h));
                                            }
                                        }
                                    }
                                    ImGui::EndDragDropTarget();
                                }

                                if (handle != 0)
                                {
                                    ImGui::SameLine(0.0f, 4.0f);
                                    if (UI::IconButton("##ClearLuaRef", ICON_LC_X, UI::ButtonVariant::Outline, UI::ButtonSize::IconSm, "Clear"))
                                        commitValue(uint64_t(0));
                                }
                            });
                            break;
                        }
                        default:
                            break;
                        }

                        if (!desc.tooltip.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
                        {
                            ImGui::SetTooltip("%s", desc.tooltip.c_str());
                        }

                        if (UI::BeginContextMenu("##LuaFieldResetCtx"))
                        {
                            if (UI::ContextMenuItem("Reset to Default", nullptr, ICON_LC_ROTATE_CCW))
                            {
                                component.fields.erase(desc.name);
                                m_Scene->SetDirtyFlag(true);
                                if (isRunning && m_Scene->GetLuaScriptRuntime())
                                {
                                    m_Scene->GetLuaScriptRuntime()->SetFieldValue(selectedEntity.GetUUID(), desc.name, desc.defaultValue);
                                }
                            }
                            UI::EndContextMenu();
                        }

                        ImGui::PopID();
                    }
                }

                if (UI::Button("Reload Script", UI::ButtonVariant::Outline, UI::ButtonSize::Small, ImVec2(-1.0f, 22.0f), ICON_LC_REFRESH_CW))
                {
                    lua::Script::InvalidateFieldCache(resolvedScriptPath);
                    if (isRunning && m_Scene->GetLuaScriptRuntime())
                    {
                        m_Scene->GetLuaScriptRuntime()->ReloadInstance(selectedEntity.GetUUID());
                    }
                    else if (component.scriptHandle != AssetHandle(0) && assetManager)
                    {
                        auto script = assetManager->GetAsset<lua::Script>(component.scriptHandle);
                        if (script)
                            script->Reload();
                    }
                }
            });

            RenderComponent<TerrainComponent>("Terrain", selectedEntity, [ & ]()
            {
                auto &c = selectedEntity.GetComponent<TerrainComponent>();
                TerrainComponent compBefore = c;

                bool rebuildGPU = false;
                const bool hasExternalHeightmap = c.heightmapHandle != AssetHandle(0);

                if (ImGui::TreeNodeEx("Heightmap", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    std::string label = assetManager->GetAssetDisplayName(c.heightmapHandle);
                    if (label.empty())
                    {
                        label = "None (Embedded / Procedural)";
                    }

                    UI::DrawButtonWithColumn("Heightmap Data / Texture", label.c_str(), nullptr, [ &, this ]()
                    {
                        if (ImGui::BeginDragDropTarget())
                        {
                            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                            {
                                LOG_ASSERT(payload->DataSize == sizeof(AssetHandle), "WRONG ITEM, that should be an asset handle");
                                AssetHandle handle = *static_cast<AssetHandle *>(payload->Data);
                                AssetMetaData metadata = assetManager->GetMetaData(handle);
                                if (metadata.type == AssetType::Terrain)
                                {
                                    c.heightmapHandle = handle;
                                    auto asset = assetManager->GetAsset<TerrainData>(handle);
                                    if (asset)
                                    {
                                        c.data = asset;
                                        c.resolution = asset->resolution;
                                        c.worldSize = asset->worldSize;
                                        c.maxHeight = asset->maxHeight;
                                    }
                                    rebuildGPU = true;
                                }
                                else if (metadata.type == AssetType::Texture)
                                {
                                    c.heightmapHandle = handle;
                                    if (!c.data) c.data = CreateRef<TerrainData>();
                                    c.data->LoadFromTexture(handle);
                                    c.resolution = c.data->resolution;
                                    rebuildGPU = true;
                                }
                            }
                            ImGui::EndDragDropTarget();
                        }

                        if (hasExternalHeightmap)
                        {
                            ImGui::SameLine(0.0f, 4.0f);
                            if (UI::IconButton("##ClearHeightmap", ICON_LC_X, UI::ButtonVariant::Outline, UI::ButtonSize::IconSm, "Clear Heightmap"))
                            {
                                c.heightmapHandle = AssetHandle(0);
                                rebuildGPU = true;
                            }
                        }
                    });

                    ImGui::TreePop();
                }

                if (ImGui::TreeNodeEx("Properties", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    if (hasExternalHeightmap)
                    {
                        ImGui::BeginDisabled();
                    }

                    int res = static_cast<int>(c.resolution);
                    if (UI::DrawIntControl("Resolution", &res, 1, 16, 2048))
                    {
                        c.resolution = static_cast<uint32_t>(std::max(16, res));
                        if (!c.data)
                        {
                            c.data = CreateRef<TerrainData>();
                        }
                        c.data->InitFlat(c.resolution, c.worldSize, c.maxHeight);
                        rebuildGPU = true;
                    }

                    if (hasExternalHeightmap)
                    {
                        ImGui::EndDisabled();
                    }

                    if (UI::DrawFloatControl("World Size", &c.worldSize, 1.0f, 1.0f, 10000.0f))
                    {
                        if (c.data)
                        {
                            c.data->worldSize = c.worldSize;
                        }
                        rebuildGPU = true;
                    }

                    if (UI::DrawFloatControl("Max Height", &c.maxHeight, 1.0f, 0.0f, 5000.0f))
                    {
                        if (c.data)
                        {
                            c.data->maxHeight = c.maxHeight;
                        }
                        rebuildGPU = true;
                    }

                    int chunkCount = static_cast<int>(c.chunkCount);
                    if (UI::DrawIntControl("Chunk Count", &chunkCount, 1, 1, 64))
                    {
                        c.chunkCount = static_cast<uint32_t>(std::max(1, chunkCount));
                        rebuildGPU = true;
                    }

                    int lodLevels = static_cast<int>(c.lodLevels);
                    if (UI::DrawIntControl("LOD Levels", &lodLevels, 1, 1, 10))
                    {
                        c.lodLevels = static_cast<uint32_t>(std::max(1, lodLevels));
                        rebuildGPU = true;
                    }

                    if (hasExternalHeightmap)
                    {
                        ImGui::BeginDisabled();
                    }

                    if (UI::Button("Rebuild Flat Terrain", UI::ButtonVariant::Outline, UI::ButtonSize::Small, ImVec2(-1.0f, 22.0f), ICON_LC_REFRESH_CW))
                    {
                        if (!c.data)
                        {
                            c.data = CreateRef<TerrainData>();
                        }
                        c.data->InitFlat(c.resolution, c.worldSize, c.maxHeight);
                        rebuildGPU = true;
                    }

                    UI::DrawFloatControl("Noise Frequency", &c.noiseFrequency, 0.05f, 0.001f, 5.0f);
                    UI::DrawIntControl("Noise Seed", &c.noiseSeed, 1, 0, 99999);

                    if (UI::Button("Generate Noise Terrain", UI::ButtonVariant::Secondary, UI::ButtonSize::Small, ImVec2(-1.0f, 22.0f), ICON_LC_SPARKLES))
                    {
                        TerrainBuilder::GenerateProcedural(c, c.noiseFrequency, c.noiseSeed);
                        rebuildGPU = true;
                    }

                    if (hasExternalHeightmap)
                    {
                        ImGui::EndDisabled();
                    }

                    ImGui::TreePop();
                }

                if (ImGui::TreeNodeEx("Renderer", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    const bool isMaterialLoaded = c.materialHandle != AssetHandle(0);
                    std::string label = assetManager->GetAssetDisplayName(c.materialHandle);
                    UI::DrawButtonWithColumn("Material", label.c_str(), nullptr, [ &, this ]()
                    {
                        if (ImGui::BeginDragDropTarget())
                        {
                            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                            {
                                LOG_ASSERT(payload->DataSize == sizeof(AssetHandle), "WRONG ITEM, that should be an asset handle");
                                AssetHandle handle = *static_cast<AssetHandle *>(payload->Data);
                                AssetMetaData metadata = assetManager->GetMetaData(handle);
                                if (metadata.type == AssetType::Material)
                                {
                                    c.materialHandle = handle;
                                    c.dirty = true;
                                }
                            }
                            ImGui::EndDragDropTarget();
                        }

                        if (isMaterialLoaded)
                        {
                            ImGui::SameLine(0.0f, 4.0f);
                            if (UI::IconButton("##ClearMaterial", ICON_LC_X, UI::ButtonVariant::Outline, UI::ButtonSize::IconSm, "Clear Material"))
                            {
                                c.materialHandle = AssetHandle(0);
                                c.dirty = true;
                            }
                        }
                    });

                    ImGui::TreePop();
                }

                if (rebuildGPU)
                {
                    c.gpuInitialized = false;
                    TerrainComponent compBeforeCmd = compBefore;
                    TerrainComponent compAfterCmd = c;
                    compBeforeCmd.ReleaseGPU();
                    compAfterCmd.ReleaseGPU();
                    CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<TerrainComponent>>(m_Scene, selectedEntity.GetUUID(), compBeforeCmd, compAfterCmd));
                }
            });

            RenderComponent<ParticleSystemComponent>("Particle System", selectedEntity, [ & ]()
            {
                auto &c = selectedEntity.GetComponent<ParticleSystemComponent>();

                // Playback & Controls
                UI::DrawSwitch("Playing", &c.isPlaying);
                if (UI::Button("Reset", UI::ButtonVariant::Outline, UI::ButtonSize::Small, ImVec2(0.0f, 22.0f), ICON_LC_ROTATE_CCW))
                {
                    c.system = nullptr;
                }

                ImGui::SameLine(0.0f, 6.0f);
                if (UI::Button("Smoke", UI::ButtonVariant::Outline, UI::ButtonSize::Small, ImVec2(0.0f, 22.0f), ICON_LC_WIND))
                {
                    c.settings = ParticleEmitterSettings::CreateSmoke();
                    c.system = nullptr;
                }
                ImGui::SameLine(0.0f, 4.0f);
                if (UI::Button("Fire", UI::ButtonVariant::Outline, UI::ButtonSize::Small, ImVec2(0.0f, 22.0f), ICON_LC_FLAME))
                {
                    c.settings = ParticleEmitterSettings::CreateFire();
                    c.system = nullptr;
                }
                ImGui::SameLine(0.0f, 4.0f);
                if (UI::Button("Spark", UI::ButtonVariant::Outline, UI::ButtonSize::Small, ImVec2(0.0f, 22.0f), ICON_LC_ZAP))
                {
                    c.settings = ParticleEmitterSettings::CreateSpark();
                    c.system = nullptr;
                }

                ImGui::Spacing();
                ImGui::SeparatorText("Emitter");

                int typeIdx = static_cast<int>(c.settings.type);
                static const char *emitterTypeLabels[] = { "Smoke", "Fire", "Spark", "Custom" };
                if (UI::DrawComboBox("Emitter Type", emitterTypeLabels, IM_ARRAYSIZE(emitterTypeLabels), &typeIdx))
                {
                    c.settings.type = static_cast<ParticleEmitterType>(typeIdx);
                }

                int blendIdx = static_cast<int>(c.settings.blendMode);
                static const char *blendModeLabels[] = { "Alpha", "Additive" };
                if (UI::DrawComboBox("Blend Mode", blendModeLabels, IM_ARRAYSIZE(blendModeLabels), &blendIdx))
                {
                    c.settings.blendMode = static_cast<ParticleBlendMode>(blendIdx);
                }

                int maxP = static_cast<int>(c.settings.maxParticles);
                if (UI::DrawIntControl("Max Particles", &maxP, 100, 10, 200000))
                {
                    c.settings.maxParticles = static_cast<uint32_t>(std::max(10, maxP));
                    c.system = nullptr;
                }

                UI::DrawFloatControl("Emission Rate", &c.settings.emissionRate, 1.0f, 0.0f, 10000.0f);
                UI::DrawFloatControl("Spawn Radius", &c.settings.spawnRadius, 0.01f, 0.0f, 50.0f);

                ImGui::Spacing();
                ImGui::SeparatorText("Lifetime");
                UI::DrawFloatControl("Lifetime Min", &c.settings.lifetimeMin, 0.05f, 0.01f, 60.0f);
                UI::DrawFloatControl("Lifetime Max", &c.settings.lifetimeMax, 0.05f, 0.01f, 60.0f);
                if (c.settings.lifetimeMax < c.settings.lifetimeMin)
                {
                    c.settings.lifetimeMax = c.settings.lifetimeMin;
                }

                ImGui::Spacing();
                ImGui::SeparatorText("Physics & Motion");
                UI::DrawVec3Control("Velocity Min", c.settings.velocityMin, 0.05f);
                UI::DrawVec3Control("Velocity Max", c.settings.velocityMax, 0.05f);
                UI::DrawVec3Control("Acceleration", c.settings.acceleration, 0.05f);
                UI::DrawFloatControl("Drag", &c.settings.drag, 0.01f, 0.0f, 10.0f);
                UI::DrawFloatControl("Velocity Align", &c.settings.velocityAlignment, 0.02f, 0.0f, 1.0f);

                ImGui::Spacing();
                ImGui::SeparatorText("Size & Rotation");
                UI::DrawVec2Control("Start Size", c.settings.startSize, 0.02f);
                UI::DrawVec2Control("End Size", c.settings.endSize, 0.02f);
                UI::DrawFloatControl("Rot Speed Min", &c.settings.rotationSpeedMin, 0.05f, -30.0f, 30.0f);
                UI::DrawFloatControl("Rot Speed Max", &c.settings.rotationSpeedMax, 0.05f, -30.0f, 30.0f);

                ImGui::Spacing();
                ImGui::SeparatorText("Color");
                UI::DrawColorVec4("Start Color", c.settings.startColor);
                UI::DrawColorVec4("End Color", c.settings.endColor);

                ImGui::Spacing();
                ImGui::SeparatorText("Texture (Optional)");
                const bool isTextureLoaded = c.settings.textureHandle != AssetHandle(0);
                const std::string textureLabel = isTextureLoaded ? assetManager->GetAssetDisplayName(c.settings.textureHandle) : "Drag Texture Here";
                UI::DrawButtonWithColumn("Texture", textureLabel.c_str(), nullptr, [&c, &isTextureLoaded, assetManager]()
                {
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                        {
                            LOG_ASSERT(payload->DataSize == sizeof(AssetHandle), "WRONG ITEM, that should be an asset handle");
                            AssetHandle handle = *static_cast<const AssetHandle *>(payload->Data);
                            AssetType type = assetManager->GetAssetType(handle);
                            if (type == AssetType::Texture)
                            {
                                c.settings.textureHandle = handle;
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }

                    if (isTextureLoaded)
                    {
                        ImGui::SameLine(0.0f, 4.0f);
                        if (UI::IconButton("##ClearParticleTex", ICON_LC_X, UI::ButtonVariant::Outline, UI::ButtonSize::IconSm, "Clear Texture"))
                        {
                            c.settings.textureHandle = AssetHandle(0);
                        }
                    }
                });
            });

            static bool focusComponentInput = true;

            ImGui::PushStyleColor(ImGuiCol_PopupBg, UI::ToImVec4(theme.popover));
            ImGui::PushStyleColor(ImGuiCol_Border, UI::ToImVec4(theme.border));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f * scale, 8.0f * scale));
            ImGui::SetNextWindowSize(ImVec2(std::round(260.0f * scale), 0.0f), ImGuiCond_Always);
            if (ImGui::BeginPopup("##add_component_context", ImGuiWindowFlags_NoDecoration))
            {
                static char buffer[256] = { 0 };
                static std::string compNameFilterResultStr;

                std::set<std::pair<std::string, CompType>> filteredCompName;

                if (focusComponentInput)
                {
                    ImGui::SetKeyboardFocusHere();
                    focusComponentInput = false;
                }

                UI::TextField(
                    "##component_name",
                    buffer,
                    sizeof(buffer),
                    "Search components...",
                    ImVec2(-1.0f, std::round(26.0f * scale)),
                    ICON_LC_SEARCH,
                    true,
                    ImGuiInputTextFlags_EscapeClearsAll | ImGuiInputTextFlags_NoHorizontalScroll);
                compNameFilterResultStr = std::string(buffer);

                if (!compNameFilterResultStr.empty())
                {
                    std::string search = stringutils::ToLower(compNameFilterResultStr);
                    for (const auto &[strName, type] : s_ComponentsName)
                    {
                        std::string nameLower = stringutils::ToLower(strName);
                        if (nameLower.find(search) != std::string::npos)
                        {
                            filteredCompName.insert({ strName, type });
                        }
                    }
                }

                auto addCompFunc = [this](Entity entity, CompType type, const std::string &compName) -> void
                {
                    auto addTyped = [&](auto dummy)
                    {
                        using CompT = std::decay_t<decltype(dummy)>;
                        if (entity.HasComponent<CompT>())
                            return;

                        if (m_Scene && !m_Scene->IsRunning())
                        {
                            CommandManager::ExecuteCommand(CreateScope<ComponentAddCommand<CompT>>(m_Scene, entity.GetUUID(), compName));
                        }
                        else
                        {
                            entity.AddComponent<CompT>();
                        }
                    };

                    switch (type)
                    {
                    case CompType_Camera: addTyped(CameraComponent{}); break;
                    case CompType_Sprite2D: addTyped(Sprite2DComponent{}); break;
                    case CompType_Animator2D: addTyped(Animator2DComponent{}); break;
                    case CompType_Circle2D: addTyped(Circle2DComponent{}); break;
                    case CompType_PointLight2D: addTyped(PointLight2DComponent{}); break;
                    case CompType_DirectionalLight: addTyped(DirectionalLightComponent{}); break;
                    case CompType_PointLight: addTyped(PointLightComponent{}); break;
                    case CompType_SpotLight: addTyped(SpotLightComponent{}); break;
                    case CompType_IrradianceVolume: addTyped(IrradianceVolumeComponent{}); break;
                    case CompType_ReflectionProbe: addTyped(ReflectionProbeComponent{}); break;
                    case CompType_Text: addTyped(TextComponent{}); break;
                    case CompType_Widget: addTyped(WidgetComponent{}); break;
                    case CompType_Rigidbody2D: addTyped(Rigidbody2DComponent{}); break;
                    case CompType_BoxCollider2D: addTyped(BoxCollider2DComponent{}); break;
                    case CompType_CircleCollider2D: addTyped(CircleCollider2DComponent{}); break;
                    case CompType_SkeletalMesh: addTyped(SkeletalMeshComponent{}); break;
                    case CompType_StaticMesh: addTyped(StaticMeshComponent{}); break;
                    case CompType_Rigidbody: addTyped(RigidbodyComponent{}); break;
                    case CompType_BoxCollider: addTyped(BoxColliderComponent{}); break;
                    case CompType_SphereCollider: addTyped(SphereColliderComponent{}); break;
                    case CompType_CapsuleCollider: addTyped(CapsuleColliderComponent{}); break;
                    case CompType_CharacterController: addTyped(CharacterControllerComponent{}); break;
                    case CompType_MeshCollider: addTyped(MeshColliderComponent{}); break;
                    case CompType_PlaneCollider: addTyped(PlaneColliderComponent{}); break;
                    case CompType_Terrain: addTyped(TerrainComponent{}); break;
                    case CompType_HeightFieldCollider: addTyped(HeightFieldColliderComponent{}); break;
                    case CompType_Prefab: addTyped(PrefabComponent{}); break;
                    case CompType_AudioSource: addTyped(AudioSourceComponent{}); break;
                    case CompType_WorldEnvironment: addTyped(WorldEnvironment{}); break;
                    case CompType_Script: addTyped(ScriptComponent{}); break;
                    case CompType_LuaScript: addTyped(LuaScriptComponent{}); break;
                    case CompType_ParticleSystem: addTyped(ParticleSystemComponent{}); break;
                    case CompType_PhysicsJoint: addTyped(PhysicsJointComponent{}); break;
                    case CompType_NetworkIdentity: addTyped(NetworkIdentityComponent{}); break;
                    default: break;
                    }
                };

                UI::ContextMenuSeparator();

                ImGui::BeginChildEx("##component_list", ImGui::GetCurrentWindow()->GetID("##component_list"),
                    ImVec2(-1, std::round(240.0f * scale)), ImGuiChildFlags_None, ImGuiWindowFlags_None);

                auto renderCompChoice = [&](const std::string &strName, CompType type)
                {
                    const NodeVisual compVis = GetComponentVisual(type);
                    const bool alreadyPresent = EntityHasCompType(selectedEntity, type);
                    if (UI::ContextMenuItem(strName.c_str(), alreadyPresent ? "Added" : nullptr, compVis.icon, false, !alreadyPresent, compVis.color))
                    {
                        addCompFunc(Entity{ selectedEntity, m_Scene }, type, strName);
                        buffer[0] = '\0';
                        ImGui::CloseCurrentPopup();
                    }
                };

                if (compNameFilterResultStr.empty())
                {
                    for (const auto &[strName, type] : s_ComponentsName)
                    {
                        renderCompChoice(strName, type);
                    }
                }
                else
                {
                    for (const auto &[strName, type] : filteredCompName)
                    {
                        renderCompChoice(strName, type);
                    }
                }

                ImGui::EndChild();

                ImGui::EndPopup();
            }
            else
            {
                focusComponentInput = true;
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
            } // end else (!Folder)
        }

        ImGui::End();
        ImGui::PopStyleVar();
    }

    void ScenePanel::RenderSceneEditViewport()
    {
        IGN_PROFILE_FUNCTION();
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

        if (m_Scene && m_Scene->IsDirty())
        {
            windowFlags |= ImGuiWindowFlags_UnsavedDocument;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
        m_Data.sceneViewportEditorVisible = ImGui::Begin("Viewport", nullptr, windowFlags);
        if (m_Data.sceneViewportEditorVisible)
        {
            SceneRenderer *activeSceneRenderer = nullptr;
            if (m_Scene)
            {
                activeSceneRenderer = m_Scene->GetSceneRenderer();

                // During Play (when no separate Game window is open), render to the Editor Viewport using the EditorPlayCamera.
                // When the separate Game viewport is open or during Stop/Simulate, keep the Editor Viewport on the EditorCamera.
                const bool hasSeparateGameplayView = m_EditorLayer && m_EditorLayer->GetState().gameplayViewportWindow && m_Data.sceneViewportGameplayVisible;
                ICamera *editorViewCamera = nullptr;
                if (m_Scene->IsPlaying() && !hasSeparateGameplayView)
                {
                    editorViewCamera = m_EditorLayer->GetEditorPlayCamera();
                }
                else
                {
                    editorViewCamera = (ICamera *)&m_EditorCamera;
                }

                auto target = activeSceneRenderer->GetRenderTarget(editorViewCamera);

                const ImGuiWindow *window = ImGui::GetCurrentWindow();

                m_IsFocused = ImGui::IsWindowFocused();
                m_IsHovered = ImGui::IsWindowHovered();

                // Calculating Scene Viewport location (0px edge-to-edge padding)
                const ImVec2 &canvasPos = ImGui::GetCursorScreenPos();
                const ImVec2 &canvasSize = ImGui::GetContentRegionAvail();

                globals::GEditor::EditorViewport.min = { canvasPos.x, canvasPos.y };
                globals::GEditor::EditorViewport.max = { canvasSize.x, canvasSize.y };

                // When not playing/simulating, always clear focus state and restore OS cursor
                const bool isPlayOrSimulate = m_Scene->IsRunning();
                const CursorMode activeCursorMode = InputSystem::GetCursorMode();
                const CursorMode requestedCursorMode = InputSystem::GetRequestedCursorMode();
                const bool cursorLockedOrHidden = (activeCursorMode == CursorMode::Disabled || activeCursorMode == CursorMode::Hidden);
                const bool scriptWantsMouseControl = (requestedCursorMode == CursorMode::Disabled || requestedCursorMode == CursorMode::Hidden);

                if (!isPlayOrSimulate)
                {
                    m_SceneFocused = false;
                    m_SceneFocusCooldown = 0;
                    if (activeCursorMode != CursorMode::Normal || requestedCursorMode != CursorMode::Normal || InputSystem::IsCursorForceUnlocked())
                    {
                        InputSystem::ResetCursorState();
                    }
                }
                else
                {
                    if (m_SceneFocusCooldown > 0)
                    {
                        m_SceneFocusCooldown--;
                    }
                    m_SceneFocused = cursorLockedOrHidden && !InputSystem::IsCursorForceUnlocked();
                }

                // Mouse position in screen space
                const ImVec2 &mousePos = ImGui::GetMousePos();
                m_ViewportData.mousePos = { mousePos.x - canvasPos.x, mousePos.y - canvasPos.y };

                if (target)
                {
                    // Render scene texture edge-to-edge across the viewport
                    ImTextureID editorViewImage = UI::GetTextureID(target->compositeRT->GetColorAttachment(0));
                    ImGui::Image(editorViewImage, canvasSize);
                }

                bool imageHovered = ImGui::IsItemHovered();

                if (m_EditorLayer && m_EditorLayer->IsInPrefabIsolation())
                {
                    ImGui::SetCursorScreenPos(ImVec2(canvasPos.x + 12.0f, canvasPos.y + 12.0f));
                    if (UI::Button("Back to Scene", UI::ButtonVariant::Secondary, UI::ButtonSize::Small, ImVec2(0.0f, 26.0f), ICON_LC_ARROW_LEFT))
                        m_EditorLayer->ExitPrefabIsolation(true);
                    if (ImGui::IsItemHovered())
                        imageHovered = false;
                }

                const bool mouseDown = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
                const bool mouseDoubleDown = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

                ImDrawList *drawList = ImGui::GetWindowDrawList();

                if (isPlayOrSimulate && cursorLockedOrHidden)
                {
                    const char *bannerText = "SCENE FOCUSED - Shift + F1 to release";
                    UI::DrawBannerText(bannerText, { 124.0f, 200.0f });
                }

                // Click inside the viewport while playing/simulating to re-apply the game script's
                // requested cursor lock/hide mode (if not scripted, do nothing).
                if (!m_Data.sceneViewportGameplayVisible && isPlayOrSimulate && scriptWantsMouseControl
                    && (InputSystem::IsCursorForceUnlocked() || !m_SceneFocused) && m_SceneFocusCooldown == 0)
                {
                    if (imageHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    {
                        m_SceneFocused = true;
                        m_SceneFocusCooldown = 10;
                        InputSystem::SetCursorForceUnlocked(false);
                    }
                }

                Entity clickedIconEntity = {};
                // Draw editor icons (cameras, directional lights, etc.)
                if (!m_Scene->IsPlaying())
                {
                    glm::mat4 viewProjection = editorViewCamera->GetProjection() * editorViewCamera->GetView();
                    Rect viewportRect = { globals::GEditor::EditorViewport.min, globals::GEditor::EditorViewport.min + globals::GEditor::EditorViewport.max };

                    // Camera icons & frustum outlines
                    auto cameraViewReg = m_Scene->registry->view<TransformComponent, RenderingComponent, CameraComponent>();
                    for (entt::entity e : cameraViewReg)
                    {
                        const auto &[tr, rc] = m_Scene->registry->get<TransformComponent, RenderingComponent>(e);
                        if (!rc.visible)
                            continue;

                        const glm::mat4 world = tr.world.GetMatrix();
                        const glm::vec3 worldPos = glm::vec3(world[3]);

                        ImVec2 screenPos;
                        if (Math::ProjectWorldToScreen(worldPos, viewProjection, viewportRect, screenPos))
                        {
                            Ref<Texture> texture = m_Icons["camera"];
                            if (texture && texture->GetHandle())
                            {
                                const float size = 36.0f;
                                ImVec2 iconMin = { screenPos.x - size * 0.5f, screenPos.y - size * 0.5f };
                                ImVec2 iconMax = { screenPos.x + size * 0.5f, screenPos.y + size * 0.5f };
                                drawList->AddImage(UI::GetTextureID(texture), iconMin, iconMax);

                                if (imageHovered && (mouseDown || mouseDoubleDown) && !m_Gizmo.IsManipulating() && !m_Gizmo.IsHovered())
                                {
                                    if (mousePos.x >= iconMin.x && mousePos.x <= iconMax.x &&
                                        mousePos.y >= iconMin.y && mousePos.y <= iconMax.y)
                                    {
                                        clickedIconEntity = Entity{ e, m_Scene };
                                    }
                                }
                            }
                        }

                        auto &cc = m_Scene->registry->get<CameraComponent>(e);
                        if (cc.camera.GetAspectRatioPreset() != SceneCamera::AspectRatioPreset::Free)
                        {
                            glm::mat4 camViewProj = cc.camera.GetProjection() * glm::inverse(world);
                            Frustum frustum(camViewProj);
                            auto edges = frustum.GetEdges();
                            for (const auto &edge : edges)
                            {
                                ImVec2 screenStart, screenEnd;
                                if (Math::ProjectWorldToScreen(edge.first, viewProjection, viewportRect, screenStart)
                                    && Math::ProjectWorldToScreen(edge.second, viewProjection, viewportRect, screenEnd))
                                {
                                    drawList->AddLine(screenStart, screenEnd, IM_COL32(255, 255, 255, 128), 1.0f);
                                }
                            }
                        }
                    }

                    // Light icons & direction vectors
                    auto dirLightReg = m_Scene->registry->view<TransformComponent, RenderingComponent, DirectionalLightComponent>();
                    for (entt::entity e : dirLightReg)
                    {
                        const auto &[tr, rc] = m_Scene->registry->get<TransformComponent, RenderingComponent>(e);
                        if (!rc.visible)
                            continue;

                        auto &lc = m_Scene->registry->get<DirectionalLightComponent>(e);

                        const glm::mat4 world = tr.world.GetMatrix();
                        const glm::vec3 worldPos = glm::vec3(world[3]);

                        ImVec2 screenPos;
                        if (Math::ProjectWorldToScreen(worldPos, viewProjection, viewportRect, screenPos))
                        {
                            Ref<Texture> texture = m_Icons["lighting"];
                            if (texture && texture->GetHandle())
                            {
                                const float size = 36.0f;
                                ImVec2 iconMin = { screenPos.x - size * 0.5f, screenPos.y - size * 0.5f };
                                ImVec2 iconMax = { screenPos.x + size * 0.5f, screenPos.y + size * 0.5f };
                                ImU32 col = IM_COL32(
                                    static_cast<int>(lc.color.r * 255.0f),
                                    static_cast<int>(lc.color.g * 255.0f),
                                    static_cast<int>(lc.color.b * 255.0f),
                                    static_cast<int>(lc.color.a * 255.0f)
                                );
                                drawList->AddImage(UI::GetTextureID(texture), iconMin, iconMax, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), col);

                                if (imageHovered && (mouseDown || mouseDoubleDown) && !m_Gizmo.IsManipulating() && !m_Gizmo.IsHovered())
                                {
                                    if (mousePos.x >= iconMin.x && mousePos.x <= iconMax.x &&
                                        mousePos.y >= iconMin.y && mousePos.y <= iconMax.y)
                                    {
                                        clickedIconEntity = Entity{ e, m_Scene };
                                    }
                                }
                            }
                        }

                        const glm::vec3 direction = glm::normalize(tr.world.rotation * glm::vec3(0.0f, 0.0f, 1.0f));
                        ImVec2 screenStart, screenEnd;
                        if (Math::ProjectWorldToScreen(worldPos, viewProjection, viewportRect, screenStart)
                            && Math::ProjectWorldToScreen(worldPos - direction * 5.0f, viewProjection, viewportRect, screenEnd))
                        {
                            ImU32 col = IM_COL32(
                                static_cast<int>(lc.color.r * 255.0f),
                                static_cast<int>(lc.color.g * 255.0f),
                                static_cast<int>(lc.color.b * 255.0f),
                                static_cast<int>(lc.color.a * 255.0f)
                            );
                            drawList->AddLine(screenStart, screenEnd, col, 1.5f);
                        }
                    }
                }

                // UI Input
                if (activeSceneRenderer)
                {
                    const auto localMouseX = static_cast<uint32_t>(std::max(m_ViewportData.mousePos.x, 0.0f));
                    const auto localMouseY = static_cast<uint32_t>(std::max(m_ViewportData.mousePos.y, 0.0f));
                    activeSceneRenderer->SetEditorWidgetMousePosition(localMouseX, localMouseY, imageHovered);
                    activeSceneRenderer->SetCameraWidgetMousePosition(&m_EditorCamera, localMouseX, localMouseY, imageHovered);
                }

                // Mouse picking from viewport object-id attachment (on mouse down only)
                if (!m_Scene->IsPlaying())
                {
                    if (imageHovered && (mouseDown || mouseDoubleDown) && !m_Gizmo.IsManipulating() && !m_Gizmo.IsHovered())
                    {
                        if (clickedIconEntity.IsValid())
                        {
                            Entity targetSelection = clickedIconEntity;
                            if (!mouseDoubleDown && !ImGui::IsKeyDown(ImGuiKey_LeftShift))
                            {
                                const auto parent = clickedIconEntity.GetParentUUID();
                                if (parent != UUID(0))
                                {
                                    if (Entity parentEntity = SceneManager::GetEntity(m_Scene, parent);
                                        parentEntity.IsValid() && !parentEntity.GetComponent<IDComponent>().IsInType(EntityType_Folder))
                                    {
                                        targetSelection = parentEntity;
                                    }
                                }
                            }

                            if (targetSelection == GetSelectedEntity())
                            {
                                // If the target selection is already selected, and the user is not holding Shift, deselect it.
                                if (!m_EditorLayer->GetState().multiSelect)
                                {
                                    SetSelectedEntity(Entity{});
                                    SetGizmoOperation(GizmoOperation::NONE);
                                }
                            }
                            else
                            {
                                SetSelectedEntity(targetSelection);
                                if (m_Gizmo.GetOperation() != GizmoOperation::NONE)
                                {
                                    m_Data.blockGizmoUntilMouseRelease = true;
                                    m_Gizmo.ResetInteraction();
                                }
                            }
                        }
                        else // invalid entity
                        {
                            Ref<Texture> objectIdTexture = (target->msaaSampleCount > 1)
                                ? (target->objectIdResolveRT ? target->objectIdResolveRT->GetColorAttachment(0) : nullptr)
                                : target->sceneRT->GetColorAttachment(1);
                            if (objectIdTexture && objectIdTexture->GetHandle() && objectIdTexture->GetCreateInfo().sampleCount <= 1)
                            {
                                const int texWidth = objectIdTexture->GetWidth();
                                const int texHeight = objectIdTexture->GetHeight();

                                if (canvasSize.x > 0.0f && canvasSize.y > 0.0f && texWidth > 0 && texHeight > 0)
                                {
                                    const int pixelX = std::clamp(static_cast<int>((m_ViewportData.mousePos.x / canvasSize.x) * static_cast<float>(texWidth)), 0, texWidth - 1);
                                    const int pixelY = std::clamp(static_cast<int>((m_ViewportData.mousePos.y / canvasSize.y) * static_cast<float>(texHeight)), 0, texHeight - 1);

                                    const uint32_t pickedObjectId = objectIdTexture->ReadPixel(pixelX, pixelY);
                                    {
                                        Entity pickedEntity = {};
                                        if (pickedObjectId != 0xFFFFFFFFu)
                                        {
                                            m_Scene->registry->view<IDComponent>().each([&](const entt::entity e, const auto &id)
                                            {
                                                if (pickedEntity.IsValid())
                                                    return;

                                                const auto objectId = static_cast<uint32_t>(static_cast<uint64_t>(id.uuid));
                                                if (objectId == pickedObjectId)
                                                {
                                                    pickedEntity = Entity{ e, m_Scene };
                                                }
                                            });
                                        }

                                        if (pickedEntity.IsValid())
                                        {
                                            Entity targetSelection = pickedEntity;

                                            // Single click: prefer selecting the direct parent group first (skip Folders).
                                            // Double click: select the exact clicked entity.
                                            if (!mouseDoubleDown && !ImGui::IsKeyDown(ImGuiKey_LeftShift))
                                            {
                                                const auto parent = pickedEntity.GetParentUUID();
                                                if (parent != UUID(0))
                                                {
                                                    if (Entity parentEntity = SceneManager::GetEntity(m_Scene, parent);
                                                        parentEntity.IsValid() && !parentEntity.GetComponent<IDComponent>().IsInType(EntityType_Folder))
                                                    {
                                                        targetSelection = parentEntity;
                                                    }
                                                }
                                            }

                                            if (targetSelection == GetSelectedEntity())
                                            {
                                                // If the target selection is already selected, and the user is not holding Shift, deselect it.
                                                if (!m_EditorLayer->GetState().multiSelect)
                                                {
                                                    SetSelectedEntity(Entity{});
                                                    SetGizmoOperation(GizmoOperation::NONE);
                                                }
                                            }
                                            else
                                            {
                                                SetSelectedEntity(targetSelection);
                                                if (m_Gizmo.GetOperation() != GizmoOperation::NONE)
                                                {
                                                    m_Data.blockGizmoUntilMouseRelease = true;
                                                    m_Gizmo.ResetInteraction();
                                                }
                                            }
                                        }
                                        else if (!m_EditorLayer->GetState().multiSelect)
                                        {
                                            SetSelectedEntity(Entity{});
                                            SetGizmoOperation(GizmoOperation::NONE);
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Viewport drag and drop
                    // TODO: Implement drag drop for meshes
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                        {
                            if (payload->DataSize == sizeof(AssetHandle))
                            {
                                auto *handle = static_cast<AssetHandle *>(payload->Data);
                                if (handle && *handle != AssetHandle(0))
                                {
                                    AssetMetaData metadata = AssetManager::GetInstance()->GetMetaData(*handle);
                                    if (metadata.type == AssetType::Scene)
                                    {
                                        const auto &filepath = m_EditorLayer->GetActiveProject()->GetProjectFilepath(metadata.filepath);
                                        Application::SubmitToMainThread([this, file = filepath]()
                                        {
                                            SignalBus::Emit<FileImportPayload>(
                                                FileImportPayload{
                                                    .type = ImportType::Open,
                                                    .status = FileStatus::Success,
                                                    .metadata = AssetMetaData(file, AssetType::Scene),
                                                    .userData = nullptr
                                                }
                                            );
                                        });
                                        // m_EditorLayer->OpenScene(filepath);
                                    }
                                    else if (metadata.type == AssetType::Prefab)
                                    {
                                        Ref<Prefab> prefab = AssetManager::GetInstance()->GetAsset<Prefab>(*handle);
                                        if (prefab)
                                        {
                                            Entity instantiated = Prefab::Instantiate(prefab, m_Scene);
                                            if (instantiated.IsValid())
                                            {
                                                if (!m_Scene->IsRunning())
                                                {
                                                    CommandManager::AddCommand(CreateScope<EntityCreateCommand>(m_Scene, instantiated, "Instantiate Prefab"));
                                                }
                                                SetSelectedEntity(instantiated);
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        ImGui::EndDragDropTarget();
                    }

                    // Begin GIZMO Manipulation
                    auto view = m_EditorCamera.GetView();
                    auto &projection = m_EditorCamera.GetProjection();

                    // Draw orientation gizmo for both Perspective and 3D/2.5D Orthographic views (not Mode2D)
                    if (m_EditorCamera.GetNavigationMode() != EditorCamera::NavigationMode::Mode2D)
                    {
                        constexpr float orientationSize = 80.0f;
                        ImGuiOrientation::internal::config.mSize = orientationSize;

                        constexpr float orientationPadding = 25.0f;
                        ImGuiOrientation::config.axisLengthScale = 0.25f;
                        ImGuiOrientation::SetRect
                        (
                            globals::GEditor::EditorViewport.max.x + globals::GEditor::EditorViewport.min.x - orientationSize - orientationPadding,
                            globals::GEditor::EditorViewport.min.y + orientationPadding
                        );

                        if (ImGuiOrientation::DrawGizmo(ImGui::GetWindowDrawList(), (float *const)glm::value_ptr(view), glm::value_ptr(projection), 100.0f))
                        {
                            m_EditorCamera.SetOrientationFromView(view);
                            m_EditorCamera3D = m_EditorCamera;
                        }
                    }

                    // Setup gizmo
                    GizmoInfo gizmoInfo;
                    gizmoInfo.cameraView = view;
                    gizmoInfo.cameraProjection = projection;
                    gizmoInfo.cameraPosition = m_EditorCamera.position;
                    gizmoInfo.cameraForward = m_EditorCamera.GetForwardDirection();
                    gizmoInfo.cameraType = m_EditorCamera.projectionType;
                    gizmoInfo.viewRect = Rect(globals::GEditor::EditorViewport.min, globals::GEditor::EditorViewport.min + globals::GEditor::EditorViewport.max);
                    gizmoInfo.isSnapping = ImGui::GetIO().KeyCtrl;
                    switch (m_Gizmo.GetOperation())
                    {
                        default:
                        case GizmoOperation::TRANSLATE:
                            gizmoInfo.snapValue = m_ViewportData.snapValues[0];
                            break;
                        case GizmoOperation::ROTATE:
                            gizmoInfo.snapValue = m_ViewportData.snapValues[1];
                            break;
                        case GizmoOperation::SCALE:
                        case GizmoOperation::BOUND_SIZING_2D:
                        case GizmoOperation::BOUND_SIZING_3D:
                            gizmoInfo.snapValue = m_ViewportData.snapValues[2];
                            break;
                    }

                    m_Gizmo.SetInfo(gizmoInfo);

                    static std::unordered_map<UUID, TransformComponent> initialTransforms;
                    static glm::vec3 initialPivot(0.0f);
                    static AABB initialSharedBounds(glm::vec3(0.0f), glm::vec3(1.0f));

                    const bool wasManipulating = m_Data.isGizmoManipulating;
                    if (!wasManipulating && !m_SelectedEntities.empty())
                    {
                        initialTransforms.clear();
                        for (const auto &[uuid, entity] : m_SelectedEntities)
                        {
                            initialTransforms[uuid] = entity.GetTransform();
                        }
                        if (m_SelectedEntities.size() > 1)
                        {
                            ComputeSharedGizmoPivotAndBounds(this, m_SelectedEntities, initialPivot, initialSharedBounds);
                        }
                    }

                    bool isManipulatingNow = false;

                    if (m_Data.blockGizmoUntilMouseRelease)
                    {
                        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                        {
                            m_Data.blockGizmoUntilMouseRelease = false;
                        }
                        m_Data.isGizmoManipulating = false;
                        m_Data.isGizmoBeingUse = false;
                    }
                    else if (m_SelectedEntities.size() > 1)
                    {
                        glm::mat4 gizmoTransform = glm::translate(glm::mat4(1.0f), initialPivot);
                        glm::mat4 manipulatedTransform = gizmoTransform;

                        isManipulatingNow = m_Gizmo.Manipulate(manipulatedTransform, initialSharedBounds);
                        m_Data.isGizmoManipulating = isManipulatingNow;
                        m_Data.isGizmoBeingUse = isManipulatingNow || m_Gizmo.IsHovered();

                        if (isManipulatingNow)
                        {
                            const glm::mat4 gizmoDelta = glm::inverse(gizmoTransform) * manipulatedTransform;

                            glm::vec3 deltaTranslation, deltaScale, deltaRotation;
                            Math::DecomposeTransformEuler(gizmoDelta, deltaTranslation, deltaRotation, deltaScale);
                            const GizmoOperation op = m_Gizmo.GetOperation();
                            const bool applyScale = (op == GizmoOperation::SCALE || op == GizmoOperation::BOUND_SIZING_2D || op == GizmoOperation::BOUND_SIZING_3D);

                            const glm::mat4 toPivot = glm::translate(glm::mat4(1.0f), -initialPivot);
                            const glm::mat4 fromPivot = glm::translate(glm::mat4(1.0f), initialPivot);
                            const glm::mat4 noScaleDelta = Math::RemoveScale(gizmoDelta);

                            for (auto &[uuid, entity] : m_SelectedEntities)
                            {
                                auto itInit = initialTransforms.find(uuid);
                                if (itInit == initialTransforms.end())
                                    continue;

                                auto &tr = entity.GetTransform();
                                const auto &initialTransform = itInit->second;
                                const glm::mat4 initialWorldMatrix = initialTransform.world.GetMatrix();

                                const glm::vec3 newWorldPos = glm::vec3(fromPivot * gizmoDelta * toPivot * glm::vec4(initialTransform.world.translation, 1.0f));
                                glm::mat4 newWorldMatrix = fromPivot * noScaleDelta * toPivot * initialWorldMatrix;
                                newWorldMatrix[3] = glm::vec4(newWorldPos, 1.0f);

                                glm::vec3 newTranslation, newRotationEuler, newScale;
                                Math::DecomposeTransformEuler(newWorldMatrix, newTranslation, newRotationEuler, newScale);

                                if (entity.GetParentUUID() != UUID(0))
                                {
                                    Entity parent = SceneManager::GetEntity(m_Scene, entity.GetParentUUID());
                                    const auto &parentTr = parent.GetTransform();
                                    const glm::mat4 parentWorld = parentTr.world.GetMatrix();
                                    const glm::mat4 localMatrix = glm::inverse(parentWorld) * newWorldMatrix;

                                    glm::vec3 localTranslation, localEuler, localScale;
                                    Math::DecomposeTransformEuler(localMatrix, localTranslation, localEuler, localScale);
                                    tr.local.translation = localTranslation;
                                    tr.local.rotation = glm::quat(localEuler);
                                    tr.local.scale = applyScale ? (initialTransform.local.scale * deltaScale) : initialTransform.local.scale;
                                }
                                else
                                {
                                    tr.local.translation = newTranslation;
                                    tr.local.rotation = glm::quat(newRotationEuler);
                                    tr.local.scale = applyScale ? (initialTransform.local.scale * deltaScale) : initialTransform.local.scale;
                                }

                                tr.dirty = true;

                                if (isPlayOrSimulate)
                                {
                                    Application::SubmitToMainThread([scene = m_Scene, entity, translation = tr.local.translation, rotation = tr.local.rotation]() mutable {
                                        if (entity.IsValid())
                                        {
                                            if (entity.HasComponent<RigidbodyComponent>())
                                            {
                                                auto &rb = entity.GetComponent<RigidbodyComponent>();
                                                rb.isGizmoDragging = true;
                                                if (auto dynActor = rb.dynamicActor.lock())
                                                {
                                                    dynActor->SetPosition(translation, true);
                                                    dynActor->SetRotation(rotation, true);
                                                    dynActor->SetLinearVelocity(glm::vec3(0.0f));
                                                    dynActor->SetAngularVelocity(glm::vec3(0.0f));
                                                }
                                                else if (auto statActor = rb.staticActor.lock())
                                                {
                                                    statActor->SetPosition(translation, true);
                                                    statActor->SetRotation(rotation, true);
                                                }
                                            }
                                            if (entity.HasComponent<Rigidbody2DComponent>())
                                            {
                                                auto &rb2D = entity.GetComponent<Rigidbody2DComponent>();
                                                rb2D.isGizmoDragging = true;
                                                if (scene->GetPhysics2D() && scene->GetPhysics2D()->IsValidBody(rb2D.bodyId))
                                                {
                                                    scene->GetPhysics2D()->SetPosition(rb2D.bodyId, translation);
                                                    scene->GetPhysics2D()->SetRotation(rb2D.bodyId, glm::eulerAngles(rotation).z);
                                                    scene->GetPhysics2D()->SetLinearVelocity(rb2D.bodyId, glm::vec2(0.0f));
                                                    scene->GetPhysics2D()->SetAngularVelocity(rb2D.bodyId, 0.0f);
                                                    scene->GetPhysics2D()->SetAwake(rb2D.bodyId, true);
                                                }
                                            }
                                        }
                                    });
                                }
                            }
                        }

                        // Commit commands when the multi-entity gizmo is released
                        if (!isManipulatingNow && wasManipulating)
                        {
                            if (isPlayOrSimulate)
                            {
                                for (auto &[uuid, entity] : m_SelectedEntities)
                                {
                                    Application::SubmitToMainThread([scene = m_Scene, entity]() mutable {
                                        if (entity.IsValid())
                                        {
                                            if (entity.HasComponent<RigidbodyComponent>())
                                                entity.GetComponent<RigidbodyComponent>().isGizmoDragging = false;
                                            if (entity.HasComponent<Rigidbody2DComponent>())
                                                entity.GetComponent<Rigidbody2DComponent>().isGizmoDragging = false;
                                        }
                                    });
                                }
                            }

                            std::vector<ComponentPropertyBatchCommand<TransformComponent>::Entry> entries;
                            for (auto &[uuid, entity] : m_SelectedEntities)
                            {
                                if (auto it = initialTransforms.find(uuid); it != initialTransforms.end())
                                {
                                    entries.push_back({ uuid, it->second, entity.GetTransform() });
                                }
                            }

                            if (!entries.empty())
                            {
                                CommandManager::AddCommand(CreateScope<ComponentPropertyBatchCommand<TransformComponent>>(m_Scene, std::move(entries)));
                            }
                        }
                    }
                    else if (Entity entity = GetSelectedEntity())
                    {
                        auto &tr = entity.GetTransform();
                        glm::mat4 transformMatrix = tr.world.GetMatrix();
                        const AABB localBounds = ComputeEntityLocalBounds(entity);

                        isManipulatingNow = m_Gizmo.Manipulate(transformMatrix, localBounds);
                        m_Data.isGizmoManipulating = isManipulatingNow;
                        m_Data.isGizmoBeingUse = isManipulatingNow || m_Gizmo.IsHovered();

                        if (isManipulatingNow)
                        {
                            const glm::vec3 preservedLocalScale = tr.local.scale;
                            glm::vec3 translation, rotation, scale;
                            Math::DecomposeTransformEuler(transformMatrix, translation, rotation, scale);
                            const GizmoOperation op = m_Gizmo.GetOperation();
                            const bool applyScale = (op == GizmoOperation::SCALE || op == GizmoOperation::BOUND_SIZING_2D || op == GizmoOperation::BOUND_SIZING_3D);

                            if (entity.GetParentUUID() != UUID(0))
                            {
                                Entity parent = SceneManager::GetEntity(m_Scene, entity.GetParentUUID());
                                const auto &parentTr = parent.GetTransform();
                                const glm::mat4 parentWorld = parentTr.world.GetMatrix();
                                const glm::mat4 localMatrix = glm::inverse(parentWorld) * transformMatrix;

                                glm::vec3 localTranslation, localEuler, localScale;
                                Math::DecomposeTransformEuler(localMatrix, localTranslation, localEuler, localScale);
                                tr.local.translation = localTranslation;
                                tr.local.rotation = glm::quat(localEuler);
                                tr.local.scale = applyScale ? localScale : preservedLocalScale;
                            }
                            else
                            {
                                tr.local.translation = translation;
                                tr.local.rotation = glm::quat(rotation);
                                tr.local.scale = applyScale ? scale : preservedLocalScale;
                            }
                            tr.dirty = true;

                            if (isPlayOrSimulate)
                            {
                                Application::SubmitToMainThread([scene = m_Scene, entity, translation = tr.local.translation, rotation = tr.local.rotation]() mutable {
                                    if (entity.IsValid())
                                    {
                                        if (entity.HasComponent<RigidbodyComponent>())
                                        {
                                            auto &rb = entity.GetComponent<RigidbodyComponent>();
                                            rb.isGizmoDragging = true;
                                            if (auto dynActor = rb.dynamicActor.lock())
                                            {
                                                dynActor->SetPosition(translation, true);
                                                dynActor->SetRotation(rotation, true);
                                                dynActor->SetLinearVelocity(glm::vec3(0.0f));
                                                dynActor->SetAngularVelocity(glm::vec3(0.0f));
                                            }
                                            else if (auto statActor = rb.staticActor.lock())
                                            {
                                                statActor->SetPosition(translation, true);
                                                statActor->SetRotation(rotation, true);
                                            }
                                        }
                                        if (entity.HasComponent<Rigidbody2DComponent>())
                                        {
                                            auto &rb2D = entity.GetComponent<Rigidbody2DComponent>();
                                            rb2D.isGizmoDragging = true;
                                            if (scene->GetPhysics2D() && scene->GetPhysics2D()->IsValidBody(rb2D.bodyId))
                                            {
                                                scene->GetPhysics2D()->SetPosition(rb2D.bodyId, translation);
                                                scene->GetPhysics2D()->SetRotation(rb2D.bodyId, glm::eulerAngles(rotation).z);
                                                scene->GetPhysics2D()->SetLinearVelocity(rb2D.bodyId, glm::vec2(0.0f));
                                                scene->GetPhysics2D()->SetAngularVelocity(rb2D.bodyId, 0.0f);
                                                scene->GetPhysics2D()->SetAwake(rb2D.bodyId, true);
                                            }
                                        }
                                    }
                                });
                            }
                        }

                        // Commit a single command when the gizmo is released (single entity)
                        if (!isManipulatingNow && wasManipulating)
                        {
                            if (isPlayOrSimulate)
                            {
                                Application::SubmitToMainThread([scene = m_Scene, entity]() mutable {
                                    if (entity.IsValid())
                                    {
                                        if (entity.HasComponent<RigidbodyComponent>())
                                            entity.GetComponent<RigidbodyComponent>().isGizmoDragging = false;
                                        if (entity.HasComponent<Rigidbody2DComponent>())
                                            entity.GetComponent<Rigidbody2DComponent>().isGizmoDragging = false;
                                    }
                                });
                            }

                            if (auto it = initialTransforms.find(entity.GetUUID()); it != initialTransforms.end())
                            {
                                CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<TransformComponent>>(m_Scene, entity.GetUUID(), it->second, entity.GetTransform()));
                            }
                        }
                    }
                    else
                    {
                        m_Data.isGizmoManipulating = false;
                        m_Data.isGizmoBeingUse = false;
                    }
                }
            }
            else
            {
                UI::DrawCenteredText("No Scene");
            }
        }

        ImGui::End();
        ImGui::PopStyleVar(2);
    }

    void ScenePanel::RenderSceneGameViewport()
    {
        IGN_PROFILE_FUNCTION();
        if (m_EditorLayer->GetState().gameplayViewportWindow)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
            m_Data.sceneViewportGameplayVisible = ImGui::Begin("Game", &m_EditorLayer->GetState().gameplayViewportWindow,
                ImGuiWindowFlags_NoMouseInputs | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

            if (m_Data.sceneViewportGameplayVisible)
            {
                // Preview camera (0px edge-to-edge padding)
                if (m_Scene)
                {
                    auto activeSceneRenderer = m_Scene->GetSceneRenderer();

                    // Calculating Scene Viewport location across entire panel
                    const ImVec2 &canvasPos = ImGui::GetCursorScreenPos();
                    const ImVec2 &canvasSize = ImGui::GetContentRegionAvail();

                    if (Entity cameraEntity = m_Scene->GetPrimaryCamera())
                    {
                        auto &cc = cameraEntity.GetComponent<CameraComponent>();
                        ICamera *camera = &cc.camera;

                        ImVec2 baseImagePos = canvasPos;
                        ImVec2 baseImageSize = canvasSize;

                        const float safeCanvasW = glm::max(canvasSize.x, 1.0f);
                        const float safeCanvasH = glm::max(canvasSize.y, 1.0f);
                        const float canvasAspect = safeCanvasW / safeCanvasH;

                        // Calculate the ImGui Image Canvas Aspect Ratio
                        float targetAspect = canvasAspect;
                        if (!cc.camera.IsFreeAspect())
                        {
                            targetAspect = glm::max(cc.camera.GetAspectRatioValue(), 0.0001f);
                        }

                        if (canvasAspect > targetAspect)
                        {
                            baseImageSize.x = safeCanvasH * targetAspect;
                            baseImagePos.x += (safeCanvasW - baseImageSize.x) * 0.5f;
                        }
                        else
                        {
                            baseImageSize.y = safeCanvasW / targetAspect;
                            baseImagePos.y += (safeCanvasH - baseImageSize.y) * 0.5f;
                        }

                        // Calculate the ImGui Image Size by baseImageSize * gamePreviewZoom
                        m_Data.gamePreviewZoom = glm::clamp(m_Data.gamePreviewZoom, 0.25f, 4.0f);
                        ImVec2 imageSize = { baseImageSize.x * m_Data.gamePreviewZoom, baseImageSize.y * m_Data.gamePreviewZoom };

                        // Panning constraint
                        const float maxPanX = glm::max((imageSize.x - baseImageSize.x) * 0.5f, 0.0f);
                        const float maxPanY = glm::max((imageSize.y - baseImageSize.y) * 0.5f, 0.0f);
                        m_Data.gamePreviewPan.x = glm::clamp(m_Data.gamePreviewPan.x, -maxPanX, maxPanX);
                        m_Data.gamePreviewPan.y = glm::clamp(m_Data.gamePreviewPan.y, -maxPanY, maxPanY);

                        // Calculate the ImGui Image Position by the Panning Position
                        ImVec2 imagePos = { baseImagePos.x + (baseImageSize.x - imageSize.x) * 0.5f + m_Data.gamePreviewPan.x, baseImagePos.y + (baseImageSize.y - imageSize.y) * 0.5f + m_Data.gamePreviewPan.y };

                        // ImGui Global Window Cursor Position
                        const ImVec2 cursor = ImGui::GetMousePos();

                        // Check if the Image is containing the Cursor
                        const bool imageHovered = cursor.x >= imagePos.x && cursor.x <= imagePos.x + imageSize.x  // X Bounds
                                               && cursor.y >= imagePos.y && cursor.y <= imagePos.y + imageSize.y; // Y Bounds

                        ImDrawList *drawList = ImGui::GetWindowDrawList();
                        auto target = activeSceneRenderer->GetRenderTarget(camera);

                        if (target)
                        {
                            {

                                // Click inside the gameplay viewport while playing/simulating to re-apply the game
                                // script's requested cursor lock/hide mode (if not scripted, do nothing).
                                const CursorMode requestedMode = InputSystem::GetRequestedCursorMode();
                                const bool scriptWantsMouseControl = (requestedMode == CursorMode::Disabled || requestedMode == CursorMode::Hidden);
                                if (m_Scene->IsRunning() && scriptWantsMouseControl
                                    && (InputSystem::IsCursorForceUnlocked() || !m_SceneFocused) && m_SceneFocusCooldown == 0)
                                {
                                    if (imageHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                                    {
                                        m_SceneFocused = true;
                                        m_SceneFocusCooldown = 10;
                                        InputSystem::SetCursorForceUnlocked(false);
                                    }
                                }

                                uint32_t localMouseX = 0;
                                uint32_t localMouseY = 0;

                                // We need to store the Game-play Mouse Position if only the Image is hovered
                                if (imageHovered && target->widgetRT)
                                {
                                    const float u = std::clamp((cursor.x - imagePos.x) / std::max(imageSize.x, 1.0f), 0.0f, 1.0f);
                                    const float v = std::clamp((cursor.y - imagePos.y) / std::max(imageSize.y, 1.0f), 0.0f, 1.0f);
                                    localMouseX = static_cast<uint32_t>(u * static_cast<float>(std::max(target->widgetRT->GetWidth(), 1u)));
                                    localMouseY = static_cast<uint32_t>(v * static_cast<float>(std::max(target->widgetRT->GetHeight(), 1u)));

                                    float rx = u * baseImageSize.x;
                                    float ry = v * baseImageSize.y;
                                    InputSystem::SetGameplayMousePosition(rx, ry, true);
                                }
                                else
                                {
                                    // Otherwise set it to Zero and and Disable it
                                    InputSystem::SetGameplayMousePosition(0.0f, 0.0f, false);
                                }

                                // Set the Widget Mouse Position
                                activeSceneRenderer->SetGameplayWidgetMousePosition(localMouseX, localMouseY, imageHovered);
                                activeSceneRenderer->SetCameraWidgetMousePosition(camera, localMouseX, localMouseY, imageHovered);
                            }

                            if (ImGui::IsWindowFocused() && ImGui::IsWindowHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
                            {
                                const ImVec2 delta = ImGui::GetIO().MouseDelta;
                                m_Data.gamePreviewPan += glm::vec2(delta.x, delta.y);
                            }

                            globals::GEditor::GameViewport.min = { baseImagePos.x, baseImagePos.y };
                            globals::GEditor::GameViewport.max = { baseImageSize.x, baseImageSize.y };

                            ImTextureID gameplayViewImaage = UI::GetTextureID(target->compositeRT->GetColorAttachment(0));
                            drawList->PushClipRect(baseImagePos, ImVec2(baseImagePos.x + baseImageSize.x, baseImagePos.y + baseImageSize.y), true);
                            drawList->AddImage(gameplayViewImaage, imagePos, ImVec2(imagePos.x + imageSize.x, imagePos.y + imageSize.y));
                            drawList->PopClipRect();
                        }

                        ImGui::SetCursorScreenPos(baseImagePos);
                        ImGui::InvisibleButton("##GamePreviewCanvas", baseImageSize);
                    }
                    else
                    {
                        ImGui::Text("No Camera");
                    }
                }
                else
                {
                    UI::DrawCenteredText("No Scene");
                }
            }
            ImGui::End();
            ImGui::PopStyleVar(2);
        }
    }

    void ScenePanel::RenderToolbar()
    {
        // TOOLBAR:
        constexpr ImVec2 buttonSize = { 28.0f, 28.0f };
        static std::array<const char *, 3> kCameraModeLabels = { "Orbit", "Fly", "2D" };
        int cameraModeIndex = 0;
        switch (m_EditorCamera.GetNavigationMode())
        {
            case EditorCamera::NavigationMode::Fly: cameraModeIndex = 1; break;
            case EditorCamera::NavigationMode::Mode2D: cameraModeIndex = 2; break;
            default: cameraModeIndex = 0; break;
        }

        ImGui::BeginDisabled(m_Scene == nullptr);

        ImGui::SetNextItemWidth(96.0f);
        if (ImGui::Combo("##camera_mode", &cameraModeIndex, kCameraModeLabels.data(), static_cast<int>(kCameraModeLabels.size())))
        {
            const auto mode = cameraModeIndex == 0 ? EditorCamera::NavigationMode::Orbit :
                (cameraModeIndex == 1 ? EditorCamera::NavigationMode::Fly : EditorCamera::NavigationMode::Mode2D);
            const auto previousMode = m_EditorCamera.GetNavigationMode();

            if (previousMode == EditorCamera::NavigationMode::Mode2D)
            {
                m_EditorCamera2D = m_EditorCamera;
            }
            else
            {
                m_EditorCamera3D = m_EditorCamera;
            }

            if (mode == EditorCamera::NavigationMode::Mode2D)
            {
                if (m_EditorCamera2D)
                {
                    m_EditorCamera = *m_EditorCamera2D;
                }
            }
            else
            {
                if (m_EditorCamera3D)
                {
                    m_EditorCamera = *m_EditorCamera3D;
                }
            }

            m_EditorCamera.UpdateView();
            m_EditorCamera.SetNavigationMode(mode);
        }

        ImGui::SameLine();
        if (m_EditorCamera.GetNavigationMode() == EditorCamera::NavigationMode::Mode2D)
        {
            ImGui::TextUnformatted("Pan Snap");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80.0f);
            ImGui::DragFloat("##CameraPanSnap", &m_ViewportData.panSnapValue, 0.05f, 0.0f, 100.0f);
            m_EditorCamera.SetPanSnapValue(m_ViewportData.panSnapValue);
        }
        else
        {
            m_EditorCamera.SetPanSnapValue(0.0f);
            static constexpr std::array<const char *, 8> kViewPresetLabels = {
                "Perspective", "2.5D Ortho", "Top", "Bottom", "Front", "Back", "Left", "Right"
            };
            int viewPresetIndex = static_cast<int>(m_EditorCamera.GetViewPreset());
            if (viewPresetIndex < 0 || viewPresetIndex >= static_cast<int>(kViewPresetLabels.size()))
            {
                viewPresetIndex = 0;
            }
            ImGui::SetNextItemWidth(112.0f);
            if (ImGui::Combo("##view_preset", &viewPresetIndex, kViewPresetLabels.data(), static_cast<int>(kViewPresetLabels.size())))
            {
                m_EditorCamera.SetViewPreset(static_cast<EditorCamera::EditorViewPreset>(viewPresetIndex));
                m_EditorCamera3D = m_EditorCamera;
            }
        }

        ImGui::SameLine();

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));

        if (UI::IconButton("Picking", ICON_LC_MOUSE_POINTER_2, UI::ButtonVariant::Ghost, UI::ButtonSize::IconMd, "Pick Entity", m_Data.gizmoOp == GizmoOperation::NONE))
            SetGizmoOperation(GizmoOperation::NONE);

        ImGui::SameLine();
        if (UI::IconButton("Translate", ICON_LC_MOVE_3D, UI::ButtonVariant::Ghost, UI::ButtonSize::IconMd, "Translate Entity", m_Data.gizmoOp == GizmoOperation::TRANSLATE))
            SetGizmoOperation(GizmoOperation::TRANSLATE);

        ImGui::SameLine();
        if (UI::IconButton("Rotate", ICON_LC_ROTATE_3D, UI::ButtonVariant::Ghost, UI::ButtonSize::IconMd, "Rotate Entity", m_Data.gizmoOp == GizmoOperation::ROTATE))
            SetGizmoOperation(GizmoOperation::ROTATE);

        ImGui::SameLine();
        if (UI::IconButton("Scale", ICON_LC_SCALE_3D, UI::ButtonVariant::Ghost, UI::ButtonSize::IconMd, "Scale Entity", m_Data.gizmoOp == GizmoOperation::SCALE))
            SetGizmoOperation(GizmoOperation::SCALE);

        {
            ImGui::SameLine();
            if (UI::IconButton("2D Bounds", ICON_LC_SQUARE, UI::ButtonVariant::Ghost, UI::ButtonSize::IconMd, "2D Bounds", m_Data.gizmoOp == GizmoOperation::BOUND_SIZING_2D))
                SetGizmoOperation(GizmoOperation::BOUND_SIZING_2D);
        }

        if (m_EditorCamera.GetNavigationMode() != EditorCamera::NavigationMode::Mode2D)
        {
            ImGui::SameLine();
            if (UI::IconButton("3D Bounds", ICON_LC_CUBOID, UI::ButtonVariant::Ghost, UI::ButtonSize::IconMd, "3D Bounds", m_Data.gizmoOp == GizmoOperation::BOUND_SIZING_3D))
                SetGizmoOperation(GizmoOperation::BOUND_SIZING_3D);
        }

        ImGui::SameLine();
        ImGui::Spacing();
        ImGui::SameLine();

        const bool isLocal = m_Gizmo.GetMode() == GizmoMode::LOCAL;
        if (UI::Button(isLocal ? "Local" : "World", isLocal ? UI::ButtonVariant::Ghost : UI::ButtonVariant::Primary, UI::ButtonSize::IconMd, ImVec2(0.0f, 0.0f), isLocal ? ICON_LC_MOVE_3D : ICON_LC_GLOBE))
            SetGizmoMode(isLocal ? GizmoMode::WORLD : GizmoMode::LOCAL);

        ImGui::PopStyleVar(2);

        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        switch (m_Gizmo.GetOperation())
        {
        default:
        case GizmoOperation::TRANSLATE:
            ImGui::DragFloat("##GizmoSnapping", &m_ViewportData.snapValues[0], 0.05f, 0.0f, 100.0f, "Snap %.2f");
            break;
        case GizmoOperation::ROTATE:
            ImGui::DragFloat("##GizmoSnapping", &m_ViewportData.snapValues[1], 0.05f, 0.0f, 100.0f, "Snap %.2f");
            break;
        case GizmoOperation::SCALE:
        case GizmoOperation::BOUND_SIZING_2D:
        case GizmoOperation::BOUND_SIZING_3D:
            ImGui::DragFloat("##GizmoSnapping", &m_ViewportData.snapValues[2], 0.05f, 0.0f, 100.0f, "Snap %.2f");
            break;
        }
        ImGui::SameLine();

        const bool isScenePlaying = m_Scene && m_Scene->IsPlaying();
        const bool isSceneReadyToPlay = m_Scene && m_Scene->IsReadyToPlay();
        const bool canClickPlay = m_Scene && (isScenePlaying || isSceneReadyToPlay);

        ImGui::SameLine();
        ImGui::BeginDisabled(!canClickPlay);
        if (UI::IconButton("##play_button", isScenePlaying ? ICON_LC_SQUARE : ICON_LC_PLAY, UI::ButtonVariant::Ghost, UI::ButtonSize::IconMd, "Play Scene", isSceneReadyToPlay))
        {
            if (isScenePlaying)
            {
                Application::SubmitToMainThread([this]()
                {
                    m_EditorLayer->OnSceneStop();
                    m_SceneFocused = false;
                });
#if _WIN32
                HWND hwnd = Application::GetInstance()->GetWindow()->GetNativeWindow();
                COLORREF rgbRed = 0x00E86071;
                DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &rgbRed, sizeof(rgbRed));
#endif
            }
            else
            {
                Application::SubmitToMainThread([this]()
                {
                    m_EditorLayer->OnScenePlay();
                    m_SceneFocused = true;
                });
#if _WIN32
                HWND hwnd = Application::GetInstance()->GetWindow()->GetNativeWindow();
                COLORREF rgbRed = 0x000000AB;
                DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &rgbRed, sizeof(rgbRed));
#endif
            }
        }
        ImGui::EndDisabled();
        if (!canClickPlay && m_Scene && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            const SceneAssetLoadProgress progress = m_Scene->GetAssetLoadProgress();
            ImGui::SetTooltip("Waiting for scene assets to load (%zu/%zu)...", progress.readyAssets, progress.totalAssets);
        }

        const bool isSceneSimulate = m_Scene && m_Scene->IsSimulating();
        const bool canClickSimulate = m_Scene && (isSceneSimulate || isSceneReadyToPlay);

        ImGui::SameLine();
        ImGui::BeginDisabled(!canClickSimulate);
        if (UI::IconButton("##simulate_button", isSceneSimulate ? ICON_LC_SQUARE : ICON_LC_COG, UI::ButtonVariant::Ghost, UI::ButtonSize::IconMd, "Simulate Scene", isSceneReadyToPlay))
        {
            if (isSceneSimulate)
            {
                Application::SubmitToMainThread([this]()
                {
                    m_EditorLayer->OnSceneStop();
                    m_SceneFocused = false;
                });
#if _WIN32
                HWND hwnd = Application::GetInstance()->GetWindow()->GetNativeWindow();
                COLORREF rgbRed = 0x00E86071;
                DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &rgbRed, sizeof(rgbRed));
#endif
            }
            else
            {
                Application::SubmitToMainThread([this]()
                    {
                        m_EditorLayer->OnSceneSimulate();
                        m_SceneFocused = true;
                    });
#if _WIN32
                HWND hwnd = Application::GetInstance()->GetWindow()->GetNativeWindow();
                COLORREF rgbRed = 0x000000AB;
                DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &rgbRed, sizeof(rgbRed));
#endif
            }
        }
        ImGui::EndDisabled();
        if (!canClickSimulate && m_Scene && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            const SceneAssetLoadProgress progress = m_Scene->GetAssetLoadProgress();
            ImGui::SetTooltip("Waiting for scene assets to load (%zu/%zu)...", progress.readyAssets, progress.totalAssets);
        }

        ImGui::SameLine();
        ImGui::Spacing();
        ImGui::SameLine();
        if (UI::Button("Scalability", UI::ButtonVariant::Outline, UI::ButtonSize::IconMd, ImVec2(0.0f, 0.0f), ICON_LC_SLIDERS_HORIZONTAL))
        {
            ImGui::OpenPopup("##ScalabilityPopup");
        }

        if (ImGui::BeginPopup("##ScalabilityPopup"))
        {
            if (auto sr = m_EditorLayer ? m_EditorLayer->GetSceneRenderer() : nullptr)
            {
                auto &rs = sr->sceneRenderSettings;
                auto &sc = rs.scalability;
                bool changed = false;

                static constexpr const char *kLevelLabels[] = { "LOW", "MEDIUM", "HIGH", "ULTRA" };
                auto drawLevelRow = [&](const char *label, ScalabilityLevel &level, bool isOverall)
                {
                    ImGui::PushID(label);
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted(label);
                    ImGui::SameLine(155.0f);
                    for (int i = 0; i < 4; ++i)
                    {
                        if (i > 0) ImGui::SameLine(0.0f, 4.0f);
                        const bool active = (static_cast<int>(level) == i);
                        if (UI::Button(kLevelLabels[i], active ? UI::ButtonVariant::Primary : UI::ButtonVariant::Outline, UI::ButtonSize::Small))
                        {
                            const auto newLevel = static_cast<ScalabilityLevel>(i);
                            if (isOverall)
                            {
                                rs.ApplyOverallScalability(newLevel);
                            }
                            else
                            {
                                level = newLevel;
                                rs.ApplyIndividualScalability();
                            }
                            changed = true;
                        }
                    }
                    ImGui::PopID();
                };

                drawLevelRow("Overall Scalability", sc.overall, true);
                ImGui::Separator();
                drawLevelRow("Shadows", sc.shadows, false);
                drawLevelRow("Global Illumination", sc.globalIllumination, false);
                drawLevelRow("Ambient Occlusion", sc.ambientOcclusion, false);
                drawLevelRow("Reflections", sc.reflections, false);
                drawLevelRow("Resolution Scale", sc.resolutionScale, false);
                drawLevelRow("Post Processing", sc.postProcessing, false);

                ImGui::Separator();
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Anti-Aliasing");
                ImGui::SameLine(155.0f);
                {
                    struct AAOption { const char *label; SampleMethod method; };
                    static constexpr AAOption kAAOptions[] = {
                        { "NONE", SampleMethod::NONE },
                        { "MSAA", SampleMethod::MSAA },
                        { "FXAA", SampleMethod::FXAA },
                        { "TAA",  SampleMethod::TAA  },
                    };
                    for (int i = 0; i < 4; ++i)
                    {
                        if (i > 0) ImGui::SameLine(0.0f, 4.0f);
                        const bool active = (rs.sampleMethod == kAAOptions[i].method);
                        if (UI::Button(kAAOptions[i].label, active ? UI::ButtonVariant::Primary : UI::ButtonVariant::Outline, UI::ButtonSize::Small))
                        {
                            sr->SetSampleMethod(kAAOptions[i].method);
                            changed = true;
                        }
                    }
                }

                if (rs.sampleMethod == SampleMethod::MSAA)
                {
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted("MSAA Samples");
                    ImGui::SameLine(155.0f);
                    static constexpr int kMSAASamples[] = { 2, 4, 8 };
                    static constexpr const char *kMSAALabels[] = { "2x", "4x", "8x" };
                    for (int i = 0; i < 3; ++i)
                    {
                        if (i > 0) ImGui::SameLine(0.0f, 4.0f);
                        const bool active = (rs.msaaProperties.sampleCount == kMSAASamples[i]);
                        if (UI::Button(kMSAALabels[i], active ? UI::ButtonVariant::Primary : UI::ButtonVariant::Outline, UI::ButtonSize::Small))
                        {
                            rs.msaaProperties.sampleCount = kMSAASamples[i];
                            changed = true;
                        }
                    }
                }
                else if (rs.sampleMethod == SampleMethod::FXAA)
                {
                    ImGui::SetNextItemWidth(220.0f);
                    if (ImGui::SliderFloat("Subpixel Quality", &rs.fxaaProperties.subpixQuality, 0.0f, 1.0f, "%.2f"))
                        changed = true;
                    ImGui::SetNextItemWidth(220.0f);
                    if (ImGui::SliderFloat("Edge Threshold", &rs.fxaaProperties.edgeThreshold, 0.063f, 0.333f, "%.3f"))
                        changed = true;
                    ImGui::SetNextItemWidth(220.0f);
                    if (ImGui::SliderFloat("Min Threshold", &rs.fxaaProperties.edgeThresholdMin, 0.015f, 0.0833f, "%.4f"))
                        changed = true;
                }
                else if (rs.sampleMethod == SampleMethod::TAA)
                {
                    ImGui::SetNextItemWidth(220.0f);
                    if (ImGui::SliderFloat("TAA Blend Factor", &rs.taaProperties.blendFactor, 0.02f, 1.0f, "%.2f"))
                        changed = true;
                }

                if (changed)
                {
                    m_EditorLayer->m_State.editorRequestToResize = true;
                    m_EditorLayer->m_State.gameplayRequestToResize = true;
                    if (auto proj = m_EditorLayer->GetActiveProject())
                    {
                        rs.SerializeToFile(proj->GetDirectory() / "GameSettings.ign");
                    }
                }
            }
            ImGui::EndPopup();
        }

        ImGui::EndDisabled();
    }

    AABB ScenePanel::ComputeEntityLocalBounds(Entity entity) const
    {
        if (!entity.IsValid())
        {
            return AABB(glm::vec3(0.0f), glm::vec3(1.0f));
        }

        AssetManager *am = AssetManager::GetInstance();
        if (entity.HasComponent<StaticMeshComponent>() && am)
        {
            const auto &smc = entity.GetComponent<StaticMeshComponent>();
            if (smc.handle != AssetHandle(0))
            {
                if (Ref<StaticMesh> mesh = am->GetAsset<StaticMesh>(smc.handle))
                {
                    const AABB &aabb = (glm::length(mesh->localAABB.GetSize()) > 1e-4f)
                        ? mesh->localAABB
                        : mesh->CalculateLocalAABB();
                    if (glm::length(aabb.GetSize()) > 1e-4f)
                    {
                        return aabb;
                    }
                }
            }
        }

        if (entity.HasComponent<SkeletalMeshComponent>() && am)
        {
            const auto &skc = entity.GetComponent<SkeletalMeshComponent>();
            if (skc.handle != AssetHandle(0))
            {
                if (Ref<SkeletalMesh> mesh = am->GetAsset<SkeletalMesh>(skc.handle))
                {
                    const AABB &aabb = (glm::length(mesh->localAABB.GetSize()) > 1e-4f)
                        ? mesh->localAABB
                        : mesh->CalculateLocalAABB();
                    if (glm::length(aabb.GetSize()) > 1e-4f)
                    {
                        return aabb;
                    }
                }
            }
        }

        if (entity.HasComponent<Sprite2DComponent>() || entity.HasComponent<Circle2DComponent>() || entity.HasComponent<TextComponent>())
        {
            return AABB::FromMinMax(glm::vec3(-0.5f, -0.5f, 0.0f), glm::vec3(0.5f, 0.5f, 0.0f));
        }

        return AABB(glm::vec3(0.0f), glm::vec3(1.0f));
    }

    void ScenePanel::SubmitGizmoForRender(SceneRenderer *renderer, ICamera *camera)
    {
        if (!renderer)
        {
            return;
        }

        if (!m_Scene || m_Scene->IsPlaying() || m_SelectedEntities.empty() || m_Data.gizmoOp == GizmoOperation::NONE || !camera)
        {
            renderer->ClearGizmoDrawList();
            return;
        }

        Rect viewRect(globals::GEditor::EditorViewport.min, globals::GEditor::EditorViewport.min + globals::GEditor::EditorViewport.max);
        if (viewRect.GetSize().x <= 1.0f || viewRect.GetSize().y <= 1.0f)
        {
            const glm::uvec2 vpSize = camera->GetViewportSize();
            viewRect = Rect(glm::vec2(0.0f), glm::vec2(static_cast<float>(std::max(1u, vpSize.x)), static_cast<float>(std::max(1u, vpSize.y))));
        }

        GizmoInfo gizmoInfo;
        gizmoInfo.cameraView = camera->GetView();
        gizmoInfo.cameraProjection = camera->GetProjection();
        gizmoInfo.cameraPosition = camera->position;
        gizmoInfo.cameraForward = camera->GetForwardDirection();
        gizmoInfo.cameraType = camera->projectionType;
        gizmoInfo.viewRect = viewRect;
        if (ImGui::GetCurrentContext() != nullptr)
        {
            gizmoInfo.isSnapping = ImGui::GetIO().KeyCtrl;
        }

        switch (m_Gizmo.GetOperation())
        {
        default:
        case GizmoOperation::TRANSLATE: gizmoInfo.snapValue = m_ViewportData.snapValues[0]; break;
        case GizmoOperation::ROTATE:    gizmoInfo.snapValue = m_ViewportData.snapValues[1]; break;
        case GizmoOperation::SCALE:
        case GizmoOperation::BOUND_SIZING_2D:
        case GizmoOperation::BOUND_SIZING_3D:
            gizmoInfo.snapValue = m_ViewportData.snapValues[2];
            break;
        }

        m_Gizmo.SetInfo(gizmoInfo);

        if (m_SelectedEntities.size() > 1)
        {
            glm::vec3 pivot(0.0f);
            AABB sharedBounds(glm::vec3(0.0f), glm::vec3(1.0f));
            ComputeSharedGizmoPivotAndBounds(this, m_SelectedEntities, pivot, sharedBounds);
            m_Gizmo.BuildDrawList(glm::translate(glm::mat4(1.0f), pivot), sharedBounds);
        }
        else if (Entity entity = GetSelectedEntity(); entity.IsValid())
        {
            m_Gizmo.BuildDrawList(entity.GetTransform().world.GetMatrix(), ComputeEntityLocalBounds(entity));
        }
        else
        {
            renderer->ClearGizmoDrawList();
            return;
        }

        renderer->SetGizmoDrawList(m_Gizmo.GetDrawList());
    }

    template<typename T, typename UIFunction>
    void ScenePanel::RenderComponent(const std::string &name, Entity entity, UIFunction uiFunction, bool allowedToRemove)
    {
        if (entity.HasComponent<T>())
        {
            const auto &theme = UI::GetTheme();
            const float scale = std::max(UI::GetDpiScale(), 1.0f);
            const NodeVisual vis = GetComponentVisualByName(name);

            T &comp = entity.GetComponent<T>();
            UUID compID = comp.GetCompID();

            ImGui::PushID(static_cast<int>(compID));

            ImGuiStorage *storage = ImGui::GetStateStorage();
            const ImGuiID openId = ImGui::GetID("comp_open");
            bool open = storage->GetBool(openId, true);

            const float headerH = std::round(26.0f * scale);
            const float optsW = std::round(24.0f * scale);
            const float availW = ImGui::GetContentRegionAvail().x;
            const ImVec2 headerMin = ImGui::GetCursorScreenPos();
            const ImVec2 headerMax(headerMin.x + availW, headerMin.y + headerH);
            const float toggleW = std::max(40.0f, availW - optsW - 2.0f * scale);

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
            const bool toggled = ImGui::InvisibleButton("##comp_toggle", ImVec2(toggleW, headerH));
            const bool headerHovered = ImGui::IsItemHovered();
            if (headerHovered)
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

            if (toggled)
            {
                open = !open;
                storage->SetBool(openId, open);
            }

            ImGui::SameLine(0.0f, 2.0f * scale);
            if (UI::IconButton("##comp_opts", ICON_LC_ELLIPSIS, UI::ButtonVariant::Ghost, UI::ButtonSize::IconSm, "Component Options"))
            {
                UI::OpenPopupMenu("comp_settings");
            }
            ImGui::PopStyleVar();

            ImDrawList *dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(headerMin, headerMax, UI::ToU32(headerHovered ? theme.secondary : theme.inputBg));
            dl->AddRect(headerMin, headerMax, UI::ToU32(headerHovered ? theme.borderStrong : theme.border), 0.0f, 0, 1.0f);
            dl->AddRectFilled(headerMin, ImVec2(headerMin.x + 2.5f, headerMax.y), UI::ToU32(vis.color));

            float drawX = headerMin.x + std::round(8.0f * scale);
            const char *chevron = open ? ICON_LC_CHEVRON_DOWN : ICON_LC_CHEVRON_RIGHT;
            {
                UI::ScopedFont tinyFont(UI::FontVariant::Small);
                const ImVec2 chevSz = ImGui::CalcTextSize(chevron);
                dl->AddText(ImVec2(drawX, headerMin.y + std::round((headerH - chevSz.y) * 0.5f)), UI::ToU32(theme.mutedForeground), chevron);
                drawX += std::round(16.0f * scale);

                const ImVec2 icSz = ImGui::CalcTextSize(vis.icon);
                dl->AddText(ImVec2(drawX, headerMin.y + std::round((headerH - icSz.y) * 0.5f)), UI::ToU32(vis.color), vis.icon);
                drawX += std::round(20.0f * scale);
            }
            {
                UI::ScopedFont medFont(UI::FontVariant::Medium);
                const ImVec2 lblSz = ImGui::CalcTextSize(name.c_str());
                const ImVec4 clipRect(drawX, headerMin.y, headerMax.x - optsW - 4.0f, headerMax.y);
                dl->AddText(nullptr, 0.0f, ImVec2(drawX, headerMin.y + std::round((headerH - lblSz.y) * 0.5f)), UI::ToU32(theme.foreground), name.c_str(), nullptr, 0.0f, &clipRect);
            }

            bool componentRemoved = false;

            if (UI::BeginPopupMenu("comp_settings"))
            {
                UI::ContextMenuLabel(name.c_str());
                UI::ContextMenuSeparator();
                if (UI::ContextMenuItem("Remove Component", nullptr, ICON_LC_TRASH_2, true, allowedToRemove))
                    componentRemoved = true;
                UI::EndContextMenu();
            }

            if (open)
            {
                ImGui::Dummy(ImVec2(0.0f, 2.0f * scale));
                uiFunction();
                ImGui::Dummy(ImVec2(0.0f, 4.0f * scale));
            }
            else
            {
                ImGui::Dummy(ImVec2(0.0f, 2.0f * scale));
            }

            if (componentRemoved)
            {
                if (m_Scene && !m_Scene->IsRunning())
                {
                    CommandManager::ExecuteCommand(CreateScope<ComponentRemoveCommand<T>>(m_Scene, entity.GetUUID(), entity.GetComponent<T>(), name));
                }
                else
                {
                    entity.RemoveComponent<T>();
                }
            }

            ImGui::PopID();
        }
    }

    void ScenePanel::OnEvent(Event &event)
    {
        EventDispatcher dispatcher(event);

        // Dispatch key pressed first to check for unfocus hotkey
        dispatcher.Dispatch<KeyPressedEvent>(BIND_CLASS_EVENT_FN(ScenePanel::OnKeyPressedEvent));

        if (m_Scene && m_SceneFocused && m_Scene->IsPlaying())
        {
            event.Handled = true;
            return;
        }

        dispatcher.Dispatch<MouseScrolledEvent>(BIND_CLASS_EVENT_FN(ScenePanel::OnMouseScrolledEvent));
        dispatcher.Dispatch<MouseMovedEvent>(BIND_CLASS_EVENT_FN(ScenePanel::OnMouseMovedEvent));
        dispatcher.Dispatch<JoystickConnectionEvent>(BIND_CLASS_EVENT_FN(ScenePanel::OnJoystickConnectionEvent));
    }

    bool ScenePanel::OnKeyPressedEvent(KeyPressedEvent &event)
    {
        const bool shiftPressed = InputSystem::IsModifierPressed(KeyMod::Shift)
            || InputSystem::IsModifierPressed(KeyMod::LeftShift)
            || InputSystem::IsModifierPressed(KeyMod::RightShift);

        if (m_Scene && m_Scene->IsRunning() && event.GetKeyCode() == Key::F1 && shiftPressed)
        {
            m_SceneFocused = false;
            m_SceneFocusCooldown = 10;
            InputSystem::SetCursorForceUnlocked(true);
            LOG_INFO("Scene play/simulate cursor force-unlocked (Shift + F1)");
            return true;
        }
        return false;
    }

    bool ScenePanel::OnMouseScrolledEvent(MouseScrolledEvent &event)
    {
        if (m_IsHovered)
        {
            m_EditorCamera.mouse.scroll = { event.GetXOffset(), event.GetYOffset() };
        }

        return false;
    }

    bool ScenePanel::OnMouseMovedEvent(MouseMovedEvent &event)
    {
        return false;
    }

    bool ScenePanel::OnJoystickConnectionEvent(JoystickConnectionEvent &event)
    {
        LOG_INFO(event.ToString());

        return false;
    }

    void ScenePanel::SetGizmoOperation(GizmoOperation op)
    {
        if (m_EditorCamera.GetNavigationMode() == EditorCamera::NavigationMode::Mode2D && op == GizmoOperation::BOUND_SIZING_3D)
        {
            op = GizmoOperation::BOUND_SIZING_2D;
        }

        m_Data.gizmoOp = op;
        m_Gizmo.SetOperation(op);
    }

    void ScenePanel::SetGizmoMode(GizmoMode mode)
    {
        m_Gizmo.SetMode(mode);
    }

    void ScenePanel::UpdateCameraInput(float deltaTime)
    {
        for (const Ref<Joystick> &j : JoystickManager::GetConnectedJoystick())
        {
            const glm::vec2 &camViewAxis = j->GetRightAxis();
            const glm::vec2 &camMoveAxis = j->GetLeftAxis();
            const glm::vec2 &l2r2 = j->GetTriggerAxis();

            // m_EditorCamera.yaw += deltaTime * camViewAxis.x;
            // m_EditorCamera.pitch += deltaTime * camViewAxis.y;

            // m_Camera.position += m_Camera.GetForwardDirection() * deltaTime * m_CameraData.moveSpeed * -camMoveAxis.y;
            // m_Camera.position += m_Camera.GetRightDirection() * deltaTime * m_CameraData.moveSpeed * camMoveAxis.x;

            // LOG_INFO(j->ToString());
        }

        m_EditorCamera.UpdateMouseState();
        if (m_IsHovered && !m_Gizmo.IsManipulating())
        {
            switch (m_EditorCamera.GetNavigationMode())
            {
            case EditorCamera::NavigationMode::Fly:
                m_EditorCamera.HandleFly(deltaTime);
                m_EditorCamera.HandlePan(deltaTime);
                m_EditorCamera.HandleZoom(deltaTime);
                break;
            case EditorCamera::NavigationMode::Mode2D:
                m_EditorCamera.HandlePan(deltaTime);
                m_EditorCamera.HandleZoom(deltaTime);
                break;
            case EditorCamera::NavigationMode::Orbit:
            default:
                m_EditorCamera.HandleOrbit(deltaTime);
                m_EditorCamera.HandlePan(deltaTime);
                m_EditorCamera.HandleZoom(deltaTime);
                break;
            }
        }
        m_EditorCamera.ApplyInertia(deltaTime);
        m_EditorCamera.UpdateCameraPosition(deltaTime);
        m_EditorCamera.UpdateView();
    }

    void ScenePanel::DestroyEntity(Entity entity)
    {
        CommandManager::ExecuteCommand(CreateScope<EntityDestroyCommand>(m_Scene, entity));
        SetHierarchyDirty();
    }

    bool ScenePanel::MatchesHierarchyFilter(Entity entity, const std::string &filterLower) const
    {
        if (filterLower.empty())
            return true;

        if (!entity.IsValid())
            return false;

        if (stringutils::ToLower(entity.GetName()).find(filterLower) != std::string::npos)
            return true;

        const IDComponent &idComp = entity.GetComponent<IDComponent>();
        for (UUID childUUID : idComp.children)
        {
            Entity child = SceneManager::GetEntity(m_Scene, childUUID);
            if (child.IsValid() && MatchesHierarchyFilter(child, filterLower))
                return true;
        }
        return false;
    }

    std::vector<UUID> ScenePanel::GetOrderedSelectedUUIDs() const
    {
        std::vector<UUID> uuids;
        uuids.reserve(m_SelectedEntities.size());
        for (const auto &[uuid, e] : m_SelectedEntities)
        {
            if (uuid != UUID(0) && e.IsValid())
            {
                uuids.push_back(uuid);
            }
        }

        const auto visualOrder = BuildHierarchyVisualOrderMap(m_Scene);
        std::stable_sort(uuids.begin(), uuids.end(), [&visualOrder](UUID a, UUID b)
        {
            const auto itA = visualOrder.find(a);
            const auto itB = visualOrder.find(b);
            const size_t ordA = itA != visualOrder.end() ? itA->second : (std::numeric_limits<size_t>::max)();
            const size_t ordB = itB != visualOrder.end() ? itB->second : (std::numeric_limits<size_t>::max)();
            return ordA < ordB;
        });

        return uuids;
    }

    void ScenePanel::ApplySelectionInternal(const std::vector<UUID> &entityUUIDs, UUID trackingUUID)
    {
        m_SelectedEntities.clear();

        SceneRenderer *activeSceneRenderer = m_Scene ? m_Scene->GetSceneRenderer() : nullptr;
        if (activeSceneRenderer)
        {
            activeSceneRenderer->ClearSelectedEntities();
        }

        if (m_Scene)
        {
            for (UUID uuid : entityUUIDs)
            {
                Entity e = SceneManager::GetEntity(m_Scene, uuid);
                if (e.IsValid())
                {
                    m_SelectedEntities[uuid] = e;
                    if (activeSceneRenderer)
                    {
                        activeSceneRenderer->SetSelectedEntity(e);
                    }
                }
            }
        }

        if (m_SelectedEntities.empty())
        {
            m_TrackingSelectedEntity = UUID(0);
            SetGizmoOperation(GizmoOperation::NONE);
        }
        else if (trackingUUID != UUID(0) && m_SelectedEntities.contains(trackingUUID))
        {
            m_TrackingSelectedEntity = trackingUUID;
        }
        else
        {
            m_TrackingSelectedEntity = m_SelectedEntities.begin()->first;
        }
    }

    void ScenePanel::SetSelectedEntities(const std::vector<UUID> &entityUUIDs, UUID trackingUUID, bool recordUndo)
    {
        std::vector<UUID> filteredNew;
        filteredNew.reserve(entityUUIDs.size());
        std::unordered_set<UUID> newSet;

        if (m_Scene)
        {
            for (UUID uuid : entityUUIDs)
            {
                if (uuid == UUID(0) || newSet.contains(uuid))
                    continue;

                if (SceneManager::GetEntity(m_Scene, uuid).IsValid())
                {
                    newSet.insert(uuid);
                    filteredNew.push_back(uuid);
                }
            }
        }

        const UUID resolvedTracking = (trackingUUID != UUID(0) && newSet.contains(trackingUUID))
            ? trackingUUID
            : (filteredNew.empty() ? UUID(0) : filteredNew.front());

        std::vector<UUID> oldUUIDs = GetOrderedSelectedUUIDs();
        const UUID oldTracking = m_TrackingSelectedEntity;

        std::unordered_set<UUID> oldSet(oldUUIDs.begin(), oldUUIDs.end());
        if (oldSet == newSet && oldTracking == resolvedTracking)
            return;

        ApplySelectionInternal(filteredNew, resolvedTracking);

        bool recordSelectionPref = true;
        if (m_EditorLayer)
        {
            if (auto activeProject = m_EditorLayer->GetActiveProject())
            {
                recordSelectionPref = activeProject->GetInfo().editorSettings.recordSelectionUndo;
            }
        }

        const bool shouldRecord = recordUndo && recordSelectionPref && m_Scene && !m_Scene->IsRunning() && !CommandManager::IsExecutingUndoRedo();
        if (shouldRecord)
        {
            std::string cmdName = "Clear Selection";
            if (filteredNew.size() == 1)
            {
                Entity e = SceneManager::GetEntity(m_Scene, filteredNew.front());
                cmdName = e.IsValid() ? ("Select " + e.GetName()) : "Select Entity";
            }
            else if (filteredNew.size() > 1)
            {
                cmdName = std::format("Select {} Entities", filteredNew.size());
            }

            const size_t extraMem = (oldUUIDs.size() + filteredNew.size()) * sizeof(UUID);
            ScenePanel *panel = this;
            Scene *expectedScene = m_Scene;

            CommandManager::AddCommand(CreateScope<LambdaCommand>(
                std::move(cmdName),
                [panel, expectedScene, filteredNew, resolvedTracking]()
                {
                    if (panel && panel->m_Scene == expectedScene)
                    {
                        panel->ApplySelectionInternal(filteredNew, resolvedTracking);
                    }
                },
                [panel, expectedScene, oldUUIDs, oldTracking]()
                {
                    if (panel && panel->m_Scene == expectedScene)
                    {
                        panel->ApplySelectionInternal(oldUUIDs, oldTracking);
                    }
                },
                extraMem));
        }
    }

    void ScenePanel::ClearSelection(bool recordUndo)
    {
        SetSelectedEntities({}, UUID(0), recordUndo);
    }

    Entity ScenePanel::SetSelectedEntity(Entity entity)
    {
        if (!entity.IsValid())
        {
            ClearSelection(true);
            return {};
        }

        const UUID targetUUID = entity.GetUUID();
        std::vector<UUID> nextUUIDs;
        UUID nextTracking = targetUUID;

        if (m_EditorLayer && m_EditorLayer->GetState().multiSelect)
        {
            nextUUIDs = GetOrderedSelectedUUIDs();
            auto it = std::find(nextUUIDs.begin(), nextUUIDs.end(), targetUUID);
            if (it != nextUUIDs.end())
            {
                nextUUIDs.erase(it);
                nextTracking = nextUUIDs.empty() ? UUID(0) : nextUUIDs.front();
            }
            else
            {
                nextUUIDs.push_back(targetUUID);
                nextTracking = targetUUID;
            }
        }
        else
        {
            nextUUIDs.push_back(targetUUID);
            nextTracking = targetUUID;
        }

        SetSelectedEntities(nextUUIDs, nextTracking, true);
        return m_SelectedEntities.contains(targetUUID) ? entity : GetSelectedEntity();
    }

    Entity ScenePanel::GetSelectedEntity()
    {
        if (m_TrackingSelectedEntity != UUID(0))
        {
            auto it = m_SelectedEntities.find(m_TrackingSelectedEntity);
            if (it != m_SelectedEntities.end() && it->second.IsValid())
                return it->second;
        }
        return m_SelectedEntities.empty() ? Entity{} : m_SelectedEntities.begin()->second;
    }

    Entity ScenePanel::CreateFolderInHierarchy(Entity parent)
    {
        if (!m_Scene)
            return {};

        ScopedCommandTransaction tx("Create Folder");
        Entity folder = SceneManager::CreateFolder(m_Scene, "Folder");
        if (folder.IsValid())
        {
            if (parent.IsValid())
            {
                SceneManager::MoveEntity(m_Scene, folder.GetUUID(), parent.GetUUID());
            }
            SetSelectedEntity(folder);
            SetHierarchyDirty();
        }
        return folder;
    }

    void ScenePanel::GroupSelectedEntitiesInFolder()
    {
        if (!m_Scene)
            return;

        std::vector<UUID> selected = FilterTopLevelUUIDs(m_Scene, GetOrderedSelectedUUIDs());
        if (selected.empty())
            return;

        ScopedCommandTransaction tx("Group in Folder");

        Entity firstSelected = SceneManager::GetEntity(m_Scene, selected.front());
        const UUID targetParent = firstSelected.IsValid() ? firstSelected.GetComponent<IDComponent>().parent : UUID(0);
        const size_t insertIdx = firstSelected.IsValid() ? SceneManager::GetSiblingIndex(m_Scene, selected.front()) : 0;

        Entity folder = SceneManager::CreateFolder(m_Scene, "Folder");
        if (!folder.IsValid())
            return;

        const UUID folderUUID = folder.GetUUID();
        {
            const UUID folderOldParent = folder.GetComponent<IDComponent>().parent;
            const size_t folderOldIdx = SceneManager::GetSiblingIndex(m_Scene, folderUUID);
            if (SceneManager::MoveEntity(m_Scene, folderUUID, targetParent, insertIdx))
            {
                const size_t folderNewIdx = SceneManager::GetSiblingIndex(m_Scene, folderUUID);
                if (!m_Scene->IsRunning() && (folderOldParent != targetParent || folderOldIdx != folderNewIdx))
                {
                    std::vector<HierarchyMoveEntry> entry = { { folderUUID, folderOldParent, folderOldIdx, targetParent, folderNewIdx } };
                    CommandManager::AddCommand(CreateScope<EntityHierarchyMoveCommand>(m_Scene, std::move(entry), "Place Group Folder"));
                }
            }
        }

        std::vector<HierarchyMoveEntry> moveEntries;
        for (UUID uuid : selected)
        {
            if (uuid == folderUUID || SceneManager::IsParent(m_Scene, folderUUID, uuid))
                continue;

            Entity e = SceneManager::GetEntity(m_Scene, uuid);
            if (!e.IsValid())
                continue;

            const UUID oldParent = e.GetComponent<IDComponent>().parent;
            const size_t oldIdx = SceneManager::GetSiblingIndex(m_Scene, uuid);
            if (SceneManager::MoveEntity(m_Scene, uuid, folderUUID, (std::numeric_limits<size_t>::max)()))
            {
                const size_t newIdx = SceneManager::GetSiblingIndex(m_Scene, uuid);
                moveEntries.push_back({ uuid, oldParent, oldIdx, folderUUID, newIdx });
            }
        }

        if (!moveEntries.empty() && !m_Scene->IsRunning())
        {
            CommandManager::AddCommand(CreateScope<EntityHierarchyMoveCommand>(m_Scene, std::move(moveEntries), "Group Entities"));
        }

        SetSelectedEntity(folder);
        SetHierarchyDirty();
    }

    void ScenePanel::DuplicateSelectedEntity()
    {
        if (!m_Scene)
            return;

        std::vector<UUID> toDuplicate = FilterTopLevelUUIDs(m_Scene, GetOrderedSelectedUUIDs());
        if (toDuplicate.empty())
            return;

        ScopedCommandTransaction tx(toDuplicate.size() > 1 ? "Duplicate Entities" : "Duplicate Entity");

        std::vector<UUID> duplicatedUUIDs;
        duplicatedUUIDs.reserve(toDuplicate.size());
        for (UUID uuid : toDuplicate)
        {
            Entity entity = SceneManager::GetEntity(m_Scene, uuid);
            if (!entity.IsValid())
                continue;

            Entity dup = SceneManager::DuplicateEntity(m_Scene, entity, true, true);
            if (dup.IsValid())
            {
                duplicatedUUIDs.push_back(dup.GetUUID());
            }
        }

        if (!duplicatedUUIDs.empty())
        {
            SetSelectedEntities(duplicatedUUIDs, duplicatedUUIDs.front(), true);
        }
        SetHierarchyDirty();
    }
}
