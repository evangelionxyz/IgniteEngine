// Copyright (c) 2026 Evangelion Manuhutu

#include "pch.hpp"
#include "scene_panel.hpp"
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
#include "ignite/graphics/ui/widget.hpp"
#include "ignite/math/math.hpp"
#include "ignite/math/transform.hpp"
#include "ignite/math/frustum.hpp"
#include "ignite/scripting/script_engine.hpp"
#include "ignite/scripting/script_field.hpp"
#include "ignite/scripting/script_instance.hpp"
#include "ignite/animation/animator/animator_controller.hpp"
#include "ignite/asset/asset_importer.hpp"
#include "ignite/core/profiler/profiler.hpp"
#include "ignite/scene/entity.hpp"
#include "ignite/scene/prefab.hpp"
#include "ignite/scene/sprite_sheet.hpp"
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
#include <string>
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
        const std::array<glm::vec2, 4> kBoundsCorners =
        {
            glm::vec2(-0.5f, -0.5f),
            glm::vec2( 0.5f, -0.5f),
            glm::vec2( 0.5f,  0.5f),
            glm::vec2(-0.5f,  0.5f)
        };
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

        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();
        nvrhi::CommandListHandle cmd = device->createCommandList();
        cmd->open();

        // Load icons
        TextureCreateInfo createInfo;
        createInfo.mipLevels = 1;
        createInfo.format = nvrhi::Format::RGBA8_UNORM;
        createInfo.initialState = nvrhi::ResourceStates::ShaderResource;
        createInfo.keepInitialState = true;
        createInfo.bindless = false;

        m_Icons["checker128"] = Texture::Create("resources/ui/checker-128px.jpg", createInfo, cmd);

        m_Icons["transform_world"] = Texture::Create("resources/ui/editor/ic_editor_world_transform.png", createInfo, cmd);
        m_Icons["transform_local"] = Texture::Create("resources/ui/editor/ic_editor_local_transform.png", createInfo, cmd);
        m_Icons["picking"] = Texture::Create("resources/ui/editor/ic_editor_picking.png", createInfo, cmd);
        m_Icons["translate"] = Texture::Create("resources/ui/editor/ic_editor_translate.png", createInfo, cmd);
        m_Icons["scale"] = Texture::Create("resources/ui/editor/ic_editor_scale.png", createInfo, cmd);
        m_Icons["rotate"] = Texture::Create("resources/ui/editor/ic_editor_rotate.png", createInfo, cmd);

        m_Icons["play"] = Texture::Create("resources/ui/editor/ic_editor_play.png", createInfo, cmd);
        m_Icons["stop"] = Texture::Create("resources/ui/editor/ic_editor_stop.png", createInfo, cmd);
        m_Icons["pause"] = Texture::Create("resources/ui/editor/ic_editor_pause.png", createInfo, cmd);
        m_Icons["simulate"] = Texture::Create("resources/ui/editor/ic_editor_simulate.png", createInfo, cmd);
        m_Icons["stepping"] = Texture::Create("resources/ui/editor/ic_editor_stepping.png", createInfo, cmd);

        m_Icons["camera"] = Texture::Create("resources/ui/world/ic_world_camera.png", createInfo, cmd);
        m_Icons["lighting"] = Texture::Create("resources/ui/world/ic_world_lighting.png", createInfo, cmd);

        cmd->close();
        device->executeCommandList(cmd);
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
        if (m_Scene && !m_Scene->IsPlaying())
        {
            UpdateCameraInput(deltaTime);
        }
    }

    void ScenePanel::RenderHierarchy()
    {
        IGN_PROFILE_FUNCTION();
        ImGui::Begin("Hierarchy");

        auto assetManager = AssetManager::GetInstance();

        const ImGuiTableFlags tableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_NoClip | ImGuiTableFlags_PadOuterX
            | ImGuiTableFlags_NoPadInnerX | ImGuiTableFlags_NoPadOuterX | ImGuiTableFlags_NoBordersInBodyUntilResize | ImGuiTableFlags_Resizable;

        if (ImGui::BeginTable("entity_hierarchy_table", 1, tableFlags))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, { 0.000f, 0.245f, 0.409f, 1.000f });
            ImGui::PushStyleColor(ImGuiCol_FrameBg, { 0.000f, 0.000f, 0.000f, 0.620f });
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, { 0.000f, 0.243f, 0.408f, 1.000f });
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 2.0f, 0.0f });

            // Root tree node
            
            const auto sceneName = m_EditorLayer->IsInPrefabIsolation() ? "Prefab" : assetManager->GetAssetDisplayName(m_Scene->handle);
            const ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_Selected | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth
                | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_LabelSpanAllColumns | ImGuiTreeNodeFlags_DefaultOpen;

            if (ImGui::TreeNodeEx(sceneName.c_str(), treeFlags))
            {
                // target drop
                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_ENTITY_SOURCE_ITEM))
                    {
                        const size_t count = payload->DataSize / sizeof(UUID);
                        const auto droppedUUIDs = static_cast<const UUID *>(payload->Data);

                        for (size_t i = 0; i < count; ++i)
                        {
                            const UUID uuid = droppedUUIDs[i];
                            Entity droppedEntity = SceneManager::GetEntity(m_Scene, uuid);

                            if (!droppedEntity)
                                continue;

                            // check if src entity has parent
                            auto &idComp = droppedEntity.GetComponent<IDComponent>();
                            if (idComp.parent != UUID(0))
                            {
                                UUID oldParent = idComp.parent;
                                // current parent should be removed
                                Entity parent = SceneManager::GetEntity(m_Scene, idComp.parent);
                                parent.GetComponent<IDComponent>().RemoveChild(idComp.uuid);
                                idComp.parent = UUID(0);

                                // Record for undo — re-parenting to root (UUID 0)
                                CommandManager::AddCommand(CreateScope<EntityReparentCommand>(
                                    m_Scene, droppedEntity.GetUUID(), oldParent, UUID(0)));
                            }
                        }
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
                                        SetSelectedEntity(instantiated);
                                    }
                                }
                            }
                        }
                    }

                    ImGui::EndDragDropTarget();
                }

                std::vector<Entity> rootEntities;
                m_Scene->registry->view<IDComponent>().each([&](const entt::entity e, const auto &id)
                    {
                        if (id.parent == UUID(0))
                            rootEntities.emplace_back(e, m_Scene);
                    });

                for (const Entity &entity : rootEntities)
                    RenderEntityNode(entity);

                // Add some extra space at the bottom
                ImGui::Dummy(ImVec2(-1.0f, 32.0f));

                ImGui::TreePop();
            }

            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);

            // Context menu for creating entities
            if (ImGui::BeginPopupContextWindow("create_entity_context", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
            {
                ShowEntityContextMenu();
                ImGui::EndPopup();
            }

            ImGui::EndTable();
        }
        ImGui::End();
    }

    Entity ScenePanel::ShowEntityContextMenu()
    {
        Entity entity = {};
        if (ImGui::MenuItem("Empty"))
        {
            entity = SetSelectedEntity(SceneManager::CreateEmptyEntity(m_Scene, "Empty"));
        }
        if (ImGui::MenuItem("Camera"))
        {
            entity = SetSelectedEntity(SceneManager::CreateCamera(m_Scene, "Camera"));
        }
        if (ImGui::MenuItem("Widget"))
        {
            entity = SetSelectedEntity(SceneManager::CreateEmptyEntity(m_Scene, "Widget"));
            if (entity.IsValid() && !entity.HasComponent<WidgetComponent>())
            {
                entity.AddComponent<WidgetComponent>();
            }
        }

        if (ImGui::BeginMenu("2D"))
        {
            if (ImGui::MenuItem("Sprite"))
            {
                entity = SetSelectedEntity(SceneManager::CreateSprite(m_Scene, "Sprite"));
            }
            if (ImGui::MenuItem("Circle"))
            {
                entity = SetSelectedEntity(SceneManager::CreateCircle(m_Scene, "Circle"));
            }
            if (ImGui::MenuItem("Point Light"))
            {
                entity = SetSelectedEntity(SceneManager::CreatePointLight2D(m_Scene, "Point Light 2D"));
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("3D"))
        {
            if (ImGui::MenuItem("Skeletal Mesh"))
            {
                entity = SetSelectedEntity(SceneManager::CreateEmptyEntity(m_Scene, "Skeletal Mesh"));
                if (entity.IsValid() && !entity.HasComponent<SkeletalMeshComponent>())
                {
                    entity.AddComponent<SkeletalMeshComponent>();
                }
            }
            if (ImGui::MenuItem("Static Mesh"))
            {
                entity = SetSelectedEntity(SceneManager::CreateEmptyEntity(m_Scene, "Static Mesh"));
                if (entity.IsValid() && !entity.HasComponent<StaticMeshComponent>())
                {
                    entity.AddComponent<StaticMeshComponent>();
                }
            }
            if (ImGui::MenuItem("Directional Light"))
            {
                entity = SetSelectedEntity(SceneManager::CreateEmptyEntity(m_Scene, "Directional Light"));
                if (entity.IsValid() && !entity.HasComponent<DirectionalLightComponent>())
                {
                    entity.AddComponent<DirectionalLightComponent>();
                }
            }
            if (ImGui::MenuItem("Point Light"))
            {
                entity = SetSelectedEntity(SceneManager::CreateEmptyEntity(m_Scene, "Point Light"));
                if (entity.IsValid() && !entity.HasComponent<PointLightComponent>())
                {
                    entity.AddComponent<PointLightComponent>();
                }
            }
            if (ImGui::MenuItem("Spot Light"))
            {
                entity = SetSelectedEntity(SceneManager::CreateEmptyEntity(m_Scene, "Spot Light"));
                if (entity.IsValid() && !entity.HasComponent<SpotLightComponent>())
                {
                    entity.AddComponent<SpotLightComponent>();
                }
            }
            if (ImGui::MenuItem("World Environment"))
            {
                entity = SetSelectedEntity(SceneManager::CreateWorldEnvironment(m_Scene, "World Environment"));
            }
            if (ImGui::MenuItem("Terrain"))
            {
                entity = SetSelectedEntity(SceneManager::CreateEmptyEntity(m_Scene, "Terrain"));
                if (entity.IsValid() && !entity.HasComponent<TerrainComponent>())
                {
                    entity.AddComponent<TerrainComponent>();
                }
            }
            ImGui::EndMenu();
        }

        return entity;
    }

    void ScenePanel::RenderEntityNode(Entity entity)
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

        auto &idComp = entity.GetComponent<IDComponent>();
        bool isDeleting = false;
        const bool isPrefab = idComp.IsInType(EntityType_Prefab);

        const bool isSelected = m_SelectedEntities.contains(entity.GetUUID());

        const std::function<bool(Entity)> hasSelectedDescendant = [&](Entity current) -> bool
        {
            if (!current.IsValid())
                return false;

            const IDComponent& currentID = current.GetComponent<IDComponent>();
            for (const UUID childUuid : currentID.children)
            {
                if (m_SelectedEntities.contains(childUuid))
                    return true;

                Entity child = SceneManager::GetEntity(m_Scene, childUuid);
                if (child.IsValid() && hasSelectedDescendant(child))
                    return true;
            }

            return false;
        };

        if (hasSelectedDescendant(entity))
        {
            ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        }

        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        const ImGuiTreeNodeFlags flags = (isSelected ? ImGuiTreeNodeFlags_Selected : 0) | (!idComp.HasChild() ? ImGuiTreeNodeFlags_Leaf : 0)
            | ImGuiTreeNodeFlags_OpenOnDoubleClick
            | ImGuiTreeNodeFlags_SpanAvailWidth
            | ImGuiTreeNodeFlags_OpenOnArrow
            | ImGuiTreeNodeFlags_LabelSpanAllColumns;

        const auto imguiPushId = static_cast<intptr_t>(static_cast<uint64_t>(static_cast<uint32_t>(entity)));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, { 0.435f, 0.287f, 0.000f, 1.000f });
        ImGui::PushStyleColor(ImGuiCol_Header, { 0.000f, 0.305f, 0.453f, 1.000f });
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, { 0.780f, 0.520f, 0.000f, 1.000f });
        
        const bool opened = ImGui::TreeNodeEx((void *)imguiPushId, flags, "%s", idComp.name.c_str());
        if (isSelected && entity.GetUUID() == m_TrackingSelectedEntity && s_LastAutoScrolledTarget != m_TrackingSelectedEntity)
        {
            if (!ImGui::IsItemVisible())
            {
                ImGui::SetScrollHereY(0.5f);
            }
            s_LastAutoScrolledTarget = m_TrackingSelectedEntity;
        }
        
        ImGui::PopStyleColor(3);

        if (!m_Scene->IsRunning() || true)
        {
            ImGui::PushID((int)imguiPushId);
            if (ImGui::BeginPopupContextItem(idComp.name.c_str()))
            {
                if (ImGui::BeginMenu("Create"))
                {
                    if (const Entity e = ShowEntityContextMenu())
                    {
                        if (SceneManager::AddChild(m_Scene, entity, e))
                            m_Scene->SetDirtyFlag(true);

                        SetSelectedEntity(e);
                    }

                    ImGui::EndMenu();
                }
                
                auto deletString = std::format("Delete ({})", m_SelectedEntities.size());
                if (ImGui::MenuItem(deletString.c_str()))
                {
                    DestroyEntity(entity);
                    for (Entity e : m_SelectedEntities | std::views::values)
                    {
                        if (e.IsValid())
                            DestroyEntity(e);
                    }
                    isDeleting = true;
                    m_SelectedEntities.clear();
                }

                ImGui::EndPopup();
            }
            ImGui::PopID();
        }

        if (!isDeleting)
        {
            // drag and drop
            if (isPrefab == false && ImGui::BeginDragDropSource())
            {
                // Collect UUIDs of selected entities
                std::vector<UUID> selectedUUIDs;
                selectedUUIDs.reserve(m_SelectedEntities.size() + 1); // Selected entities + the current entity being dragged
                selectedUUIDs.push_back(entity.GetUUID());

                if (m_SelectedEntities.size() > 1)
                {
                    for (auto &[uuid, selectedEntity] : m_SelectedEntities)
                    {
                        if (selectedEntity.IsValid())
                            selectedUUIDs.push_back(uuid);
                    }
                }

                ImGui::SetDragDropPayload(DND_PAYLOAD_ENTITY_SOURCE_ITEM, 
                    selectedUUIDs.data(),
                    selectedUUIDs.size() * sizeof(UUID));

                ImGui::BeginTooltip();
                ImGui::Text("%s %llu", idComp.name.c_str(), static_cast<u64>(idComp.uuid));
                ImGui::EndTooltip();

                ImGui::EndDragDropSource();
            }

            // target drop
            if (isPrefab == false && ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_ENTITY_SOURCE_ITEM))
                {
                    const size_t count = payload->DataSize / sizeof(UUID);
                    const auto droppedUUIDs = static_cast<const UUID *>(payload->Data);

                    for (size_t i = 0; i < count; ++i)
                    {
                        const UUID uuid = droppedUUIDs[i];
                        Entity droppedEntity = SceneManager::GetEntity(m_Scene, uuid);

                        if (droppedEntity)
                        {
                            // Capture old parent BEFORE re-parenting
                            UUID oldParent = droppedEntity.GetComponent<IDComponent>().parent;
                            UUID newParent = entity.GetComponent<IDComponent>().uuid;

                            // the current 'entity' is the target (new parent for src)
                            if (SceneManager::AddChild(m_Scene, entity, droppedEntity))
                            {
                                // Record the re-parent for undo
                                CommandManager::AddCommand(CreateScope<EntityReparentCommand>(
                                    m_Scene, droppedEntity.GetUUID(), oldParent, newParent));
                            }
                        }
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
                                    SetSelectedEntity(instantiated);
                                }
                            }
                        }
                    }
                }

                ImGui::EndDragDropTarget();
            }

            if (ImGui::IsItemHovered(ImGuiMouseButton_Left) && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            {
                SetSelectedEntity(entity);
            }
        }

        if (opened)
        {
            if (!isDeleting)
            {
                for (UUID uuid : entity.GetComponent<IDComponent>().children)
                {
                    Entity childEntity = SceneManager::GetEntity(m_Scene, uuid);
                    RenderEntityNode(childEntity);
                }
            }

            ImGui::TreePop();
        }
    }

    void ScenePanel::RenderInspector()
    {
        IGN_PROFILE_FUNCTION();
        ImGui::Begin("Inspector");

        Entity selectedEntity = GetSelectedEntity();
        if (selectedEntity.IsValid())
        {
            auto project = m_EditorLayer ? m_EditorLayer->GetActiveProject().get() : nullptr;
            auto assetManager = AssetManager::GetInstance();
            
            // Main Component
            // ID Component
            auto &idComp = selectedEntity.GetComponent<IDComponent>();
            char buffer[255] = {};
            strncpy(buffer, idComp.name.c_str(), sizeof(buffer) - 1);
            if (ImGui::InputText("##label", buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue))
            {
                std::string oldName = idComp.name;
                std::string newName(buffer);
                CommandManager::ExecuteCommand(CreateScope<EntityRenameCommand>(m_Scene, idComp.uuid, oldName, newName));
            }

            ImGui::SameLine();

            ImVec2 addCompBtSize = ImGui::CalcTextSize("Add");
            if (ImGui::Button("Add", { ImGui::GetContentRegionAvail().x, 25.0f * ImGui::GetWindowDpiScale() }))
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
                UI::State visibleState = UI::DrawCheckbox("Visible", &comp.visible);

            }, false); // false: not allowed to remove the component

            RenderComponent<PrefabComponent>("Prefab", selectedEntity, [&]()
            {
                auto &c = selectedEntity.GetComponent<PrefabComponent>();
                const bool isPrefabValid = c.prefabHandle != AssetHandle(0);
                std::string prefabName = isPrefabValid ? assetManager->GetAssetDisplayName(c.prefabHandle) : "Drag Prefab Asset Here";

                UI::DrawButtonWithColumn("Prefab Asset", prefabName.c_str(), nullptr, [&]()
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
                        ImGui::SameLine();
                        if (ImGui::Button("X##ClearPrefab"))
                        {
                            c.prefabHandle = AssetHandle(0);
                        }
                    }
                });

                ImGui::Spacing();
                if (UI::DrawButton("Apply to Prefab", { 120.0f, 28.0f }))
                {
                    SceneManager::ApplyPrefabChanges(selectedEntity, m_Scene, m_EditorLayer ? m_EditorLayer->GetActiveProject().get() : nullptr);
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Save local changes to .ixprefab file and sync all scene instances.");
                }

                ImGui::SameLine();
                if (UI::DrawButton("Revert", { 80.0f, 28.0f }))
                {
                    if (isPrefabValid)
                    {
                        SceneManager::SyncAllPrefabInstances(m_Scene, c.prefabHandle);
                    }
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Discard local changes and reload from .ixprefab asset.");
                }

                ImGui::SameLine();
                if (UI::DrawButton("Unpack", { 80.0f, 28.0f }))
                {
                    selectedEntity.RemoveComponent<PrefabComponent>();
                    SetSelectedEntity(selectedEntity);
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Break link to prefab asset and turn entity into normal scene object.");
                }

                ImGui::SameLine();
                if (UI::DrawButton("Open", { 60.0f, 28.0f }))
                {
                    if (isPrefabValid && m_EditorLayer)
                    {
                        m_EditorLayer->EnterPrefabIsolation(c.prefabHandle);
                    }
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Open prefab asset in Isolation Mode editor.");
                }
            });

            RenderComponent<WidgetComponent>("Widget", selectedEntity, [&]()
            {
                auto &c = selectedEntity.GetComponent<WidgetComponent>();

                const bool isWidgetLoaded = c.widgetHandle != AssetHandle(0);
                std::string label = isWidgetLoaded ? assetManager->GetAssetDisplayName(c.widgetHandle) : "Drag Here";
                UI::DrawButtonWithColumn("Widget", label.c_str(), nullptr, [&, this]()
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
                        ImGui::SameLine();
                        if (ImGui::Button("X##ClearWidget"))
                        {
                            c.widgetHandle = AssetHandle(0);
                            c.dirty = true;
                        }
                    }
                });
            });

            RenderComponent<WorldEnvironment>("World Environment", selectedEntity, [&]()
            {
                auto &c = selectedEntity.GetComponent<WorldEnvironment>();

                const char *skyTypes[] = { "HDRI", "Procedural Sky" };
                int currentType = static_cast<int>(c.skyType);
                if (ImGui::Combo("Sky Type", &currentType, skyTypes, 2))
                {
                    c.skyType = static_cast<SkyType>(currentType);
                    c.dirtyEnvironment = true;
                }

                UI::DrawFloatControl("Exposure", &c.exposure, 0.025f, 0.0f, FLT_MAX);
                UI::DrawFloatControl("Gamma", &c.gamma, 0.025f, 0.0f, FLT_MAX);
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
                    UI::DrawButtonWithColumn("HDR", buttonLabel.c_str(), nullptr, [&c, assetManager, this, &hasHDR]()
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
                                ImGui::SameLine();
                                if (ImGui::Button("X"))
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

                        if (ImGui::ColorEdit3("Ground Albedo", &atmo.groundAlbedo.r))
                        {
                            modified = true;
                        }

                        if (ImGui::Button("Reset to Earth Default"))
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

            RenderComponent<DirectionalLightComponent>("Directional Light", selectedEntity, [&]()
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
                UI::DrawCheckbox("Cascade Shadow", &c.cascadeShadow);

                if (c.cascadeShadow)
                {
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
                    //         ImTextureID texId = (ImTextureID)shadowMap->GetHandle().Get();
                    //         ImGui::Image(texId, { 256, 256 });
                    //     }
                    // }
                }
            });

            RenderComponent<PointLightComponent>("Point Light", selectedEntity, [&]()
            {
                auto &c = selectedEntity.GetComponent<PointLightComponent>();

                UI::DrawCheckbox("Enabled", &c.enabled);
                UI::DrawColorVec4("Color", c.color);
                UI::DrawFloatControl("Intensity", &c.intensity, 0.01f, 0.0f, 100.0f);
                UI::DrawFloatControl("Range", &c.range, 0.1f, 0.0f, 1000.0f);

                ImGui::SeparatorText("Attenuation");
                UI::DrawFloatControl("Constant", &c.constantAttenuation, 0.01f, 0.0f, 10.0f);
                UI::DrawFloatControl("Linear", &c.linearAttenuation, 0.001f, 0.0f, 10.0f);
                UI::DrawFloatControl("Quadratic", &c.quadraticAttenuation, 0.0001f, 0.0f, 10.0f);
            });

            RenderComponent<SpotLightComponent>("Spot Light", selectedEntity, [&]()
            {
                auto &c = selectedEntity.GetComponent<SpotLightComponent>();

                UI::DrawCheckbox("Enabled", &c.enabled);
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
            });

            RenderComponent<Sprite2DComponent>("Sprite 2D", selectedEntity, [&]()
            {
                auto &c = selectedEntity.GetComponent<Sprite2DComponent>();

                static Sprite2DComponent s_Sprite2DBefore;

                // Material 2D
                bool isMat2dLoaded = c.materialHandle != AssetHandle(0);
                std::string mat2dLabel = isMat2dLoaded ? assetManager->GetAssetDisplayName(c.materialHandle) : "Drag Here";
                UI::DrawButtonWithColumn("Material", mat2dLabel.c_str(), nullptr, [&c, &selectedEntity, &isMat2dLoaded, assetManager, this]()
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
                            ImGui::SameLine();
                            if (ImGui::Button("X"))
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
                    UI::DrawButtonWithColumn("Texture", textureLabel.c_str(), nullptr, [&c, &isTextureLoaded, this, assetManager, &selectedEntity]()
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
                                ImGui::SameLine();
                                if (ImGui::Button("X##ClearTexture"))
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

                UI::State flipXState = UI::DrawCheckbox("Flip X", &c.flipX);
                if (flipXState.isItemActivated)            s_Sprite2DBefore = c;
                if (flipXState.isItemDeactivatedAfterEdit)
                    CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<Sprite2DComponent>>(m_Scene,
                        selectedEntity.GetUUID(), s_Sprite2DBefore, c));

                UI::State flipYState = UI::DrawCheckbox("Flip Y", &c.flipY);
                if (flipYState.isItemActivated)            s_Sprite2DBefore = c;
                if (flipYState.isItemDeactivatedAfterEdit)
                    CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<Sprite2DComponent>>(m_Scene,
                        selectedEntity.GetUUID(), s_Sprite2DBefore, c));
            });

            RenderComponent<Animator2DComponent>("Animator 2D", selectedEntity, [&]()
            {
                auto &c = selectedEntity.GetComponent<Animator2DComponent>();

                bool isAnimatorLoaded = c.controllerHandle != AssetHandle(0);
                std::string animDropLabel = isAnimatorLoaded ? assetManager->GetAssetDisplayName(c.controllerHandle) : "Drop Here";
                UI::DrawButtonWithColumn("Controller", animDropLabel.c_str(), nullptr, [&c]()
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

            RenderComponent<PointLight2DComponent>("Point Light 2D", selectedEntity, [&]()
            {
                auto &c = selectedEntity.GetComponent<PointLight2DComponent>();
                UI::DrawCheckbox("Enabled", &c.enabled);
                UI::DrawColorVec4("Color", c.color);
                UI::DrawFloatControl("Radius", &c.radius, 0.025f, 0.0f, 10000.0f);
                UI::DrawFloatControl("Intensity", &c.intensity, 0.025f, 0.0f, 10000.0f);
            });

            RenderComponent<Circle2DComponent>("Circle 2D", selectedEntity, [&]()
            {
                auto &c = selectedEntity.GetComponent<Circle2DComponent>();

                static Circle2DComponent compBefore;

                UI::State colorState = UI::DrawColorVec4("Color", c.color);
                if (colorState.isItemActivated)
                    compBefore = c;

                if (colorState.isItemDeactivatedAfterEdit)
                    CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<Circle2DComponent>>(m_Scene, selectedEntity.GetUUID(), compBefore, c));
            });

            RenderComponent<StaticMeshComponent>("Static Mesh", selectedEntity, [&]()
            {
                auto &c = selectedEntity.GetComponent<StaticMeshComponent>();

                bool isMeshLoaded = c.handle != AssetHandle(0);

                std::string buttonLabel = isMeshLoaded ? assetManager->GetAssetDisplayName(c.handle) : "Drag Here";
                UI::DrawButtonWithColumn("Static Mesh Asset", buttonLabel.c_str(), nullptr, [&c, this, assetManager, &isMeshLoaded]()
                {
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                        {
                            LOG_ASSERT(payload->DataSize == sizeof(AssetHandle), "WRONG ITEM, that should be an asset handle");
                            AssetHandle handle = *static_cast<AssetHandle *>(payload->Data);
                            AssetMetaData metadata = assetManager->GetMetaData(handle);

                            if (metadata.type == AssetType::Mesh || metadata.type == AssetType::StaticMesh)
                            {
                                metadata.type = AssetType::StaticMesh;
                                assetManager->AssignMetaData(handle, metadata);
                                assetManager->UnloadAsset(handle);
                                c.handle = handle;
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }

                    if (isMeshLoaded)
                    {
                        ImGui::SameLine();
                        if (ImGui::Button("X"))
                        {
                            c.handle = AssetHandle(0); // reset the mesh
                        }
                    }
                });

                if (isMeshLoaded)
                {
                    Ref<StaticMesh> sm = assetManager->GetAsset<StaticMesh>(c.handle);
                    if (sm)
                    {
                        // Override Materials
                        if (ImGui::CollapsingHeader("Override Materials"))
                        {
                            const auto &instances = sm->GetMeshInstances();
                            for (size_t i = 0; i < instances.size(); ++i)
                            {
                                const auto &instance = instances[i];
                                std::string submeshName = instance->GetName();
                                if (submeshName.empty())
                                {
                                    submeshName = "Submesh " + std::to_string(i);
                                }
                                else
                                {
                                    submeshName = std::format("{} (Submesh {})", submeshName, i);
                                }

                                AssetHandle overrideMaterialHandle = AssetHandle(0);
                                auto it = c.overrideMaterials.find(static_cast<int>(i));
                                if (it != c.overrideMaterials.end())
                                {
                                    overrideMaterialHandle = it->second;
                                }

                                bool isOverrideLoaded = overrideMaterialHandle != AssetHandle(0);
                                std::string matLabel = isOverrideLoaded ? assetManager->GetAssetDisplayName(overrideMaterialHandle) : "Drag Material Here";

                                UI::DrawButtonWithColumn(submeshName.c_str(), matLabel.c_str(), nullptr, [this, &c, i, isOverrideLoaded, assetManager, &selectedEntity]()
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
                                                StaticMeshComponent before = c;
                                                c.overrideMaterials[static_cast<int>(i)] = handle;
                                                CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<StaticMeshComponent>>(m_Scene, selectedEntity.GetUUID(), before, c));
                                            }
                                        }
                                        ImGui::EndDragDropTarget();
                                    }

                                    if (isOverrideLoaded)
                                    {
                                        ImGui::SameLine();
                                        if (ImGui::Button((std::string("X##") + std::to_string(i)).c_str()))
                                        {
                                            StaticMeshComponent before = c;
                                            c.overrideMaterials.erase(static_cast<int>(i));
                                            CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<StaticMeshComponent>>(m_Scene, selectedEntity.GetUUID(), before, c));
                                        }
                                    }
                                });
                            }
                        }
                    }
                }
            });

            RenderComponent<SkeletalMeshComponent>("Skeletal Mesh", selectedEntity, [&]()
            {
                auto &c = selectedEntity.GetComponent<SkeletalMeshComponent>();

                bool isMeshLoaded = c.handle != AssetHandle(0);

                std::string buttonLabel = isMeshLoaded ? assetManager->GetAssetDisplayName(c.handle) : "Drag Here";
                UI::DrawButtonWithColumn("Skeletal Mesh Asset", buttonLabel.c_str(), nullptr, [&c, this, assetManager, &isMeshLoaded]()
                {
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                        {
                            LOG_ASSERT(payload->DataSize == sizeof(AssetHandle), "WRONG ITEM, that should be an asset handle");
                            AssetHandle handle = *static_cast<AssetHandle *>(payload->Data);
                            AssetMetaData metadata = assetManager->GetMetaData(handle);

                            if (metadata.type == AssetType::Mesh || metadata.type == AssetType::SkeletalMesh)
                            {
                                metadata.type = AssetType::SkeletalMesh;
                                assetManager->AssignMetaData(handle, metadata);
                                assetManager->UnloadAsset(handle);
                                c.handle = handle;
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }

                    if (isMeshLoaded)
                    {
                        ImGui::SameLine();
                        if (ImGui::Button("X"))
                        {
                            c.handle = AssetHandle(0); // reset the mesh
                        }
                    }
                });

                if (isMeshLoaded)
                {
                    Ref<SkeletalMesh> sm = assetManager->GetAsset<SkeletalMesh>(c.handle);
                    if (sm)
                    {
                        // Override Materials
                        if (ImGui::CollapsingHeader("Override Materials", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            const auto &instances = sm->GetMeshInstances();
                            for (size_t i = 0; i < instances.size(); ++i)
                            {
                                const auto &instance = instances[i];
                                std::string submeshName = instance->GetName();
                                if (submeshName.empty())
                                {
                                    submeshName = "Submesh " + std::to_string(i);
                                }
                                else
                                {
                                    submeshName = std::format("{} (Submesh {})", submeshName, i);
                                }

                                AssetHandle overrideMaterialHandle = AssetHandle(0);
                                auto it = c.overrideMaterials.find(static_cast<int>(i));
                                if (it != c.overrideMaterials.end())
                                {
                                    overrideMaterialHandle = it->second;
                                }

                                bool isOverrideLoaded = overrideMaterialHandle != AssetHandle(0);
                                std::string matLabel = isOverrideLoaded ? assetManager->GetAssetDisplayName(overrideMaterialHandle) : "Drag Material Here";

                                UI::DrawButtonWithColumn(submeshName.c_str(), matLabel.c_str(), nullptr, [this, &c, i, isOverrideLoaded, assetManager, &selectedEntity]()
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
                                                SkeletalMeshComponent before = c;
                                                c.overrideMaterials[static_cast<int>(i)] = handle;
                                                CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<SkeletalMeshComponent>>(m_Scene, selectedEntity.GetUUID(), before, c));
                                            }
                                        }
                                        ImGui::EndDragDropTarget();
                                    }

                                    if (isOverrideLoaded)
                                    {
                                        ImGui::SameLine();
                                        if (ImGui::Button((std::string("X##") + std::to_string(i)).c_str()))
                                        {
                                            SkeletalMeshComponent before = c;
                                            c.overrideMaterials.erase(static_cast<int>(i));
                                            CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<SkeletalMeshComponent>>(m_Scene, selectedEntity.GetUUID(), before, c));
                                        }
                                    }
                                });
                            }
                        }

                        // Animator
                        bool isAnimatorLoaded = c.runtimeAnimatorHandle != AssetHandle(0);
                        std::string buttonLabel = isAnimatorLoaded ? assetManager->GetAssetDisplayName(c.runtimeAnimatorHandle) : "Drag Here";
                        UI::DrawButtonWithColumn("Animator", buttonLabel.c_str(), nullptr, [&c, this, &sm, &isAnimatorLoaded, assetManager]()
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
                                ImGui::SameLine();
                                if (ImGui::Button("X"))
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
                            if (UI::DrawCheckbox("Unique", &c.uniqueAnimator))
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

                                        UI::DrawButtonWithColumn(socket.name.c_str(), socketMeshLabel.c_str(), nullptr, [this, &c, socketName = socket.name, assetManager, isAttached, &selectedEntity]()
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
                                                ImGui::SameLine();
                                                if (ImGui::Button((std::string("X##socket_") + socketName).c_str()))
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

            RenderComponent<CameraComponent>("Camera", selectedEntity, [&]()
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
                    if (UI::DrawCheckbox("Primary", &c.primary).isItemEdited)
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
                            c.dirty |= UI::DrawCheckbox("Enable", &pp.taaProperties.enable);
                            c.dirty |= UI::DrawFloatControl("Blend Factor", &pp.taaProperties.blendFactor, 0.025f, 0.01f, 1.0f, 1.0f);
                            ImGui::TreePop();
                        }

                        if (ImGui::TreeNodeEx("MSAA", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            c.dirty |= UI::DrawCheckbox("Enable", &pp.msaaProperties.enable);
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
                            const char *tonemapModes[] = { "Reinhard", "Uncharted 2", "Filmic" };
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
                        c.dirty |= UI::DrawCheckbox("Enable DOF", &c.camera.lens.enabledDOF).isItemEdited;
                        if (c.camera.lens.enabledDOF)
                        {
                            c.dirty |= UI::DrawFloatControl("Focal Length", &c.camera.lens.focalLength, 0.1f, 0.0f, FLT_MAX).isItemEdited;
                            c.dirty |= UI::DrawFloatControl("Focal Distance", &c.camera.lens.focalDistance, 0.05f, 0.0f, FLT_MAX).isItemEdited;
                            c.dirty |= UI::DrawFloatControl("fStop", &c.camera.lens.fStop, 0.05f, 0.0f, FLT_MAX).isItemEdited;
                            c.dirty |= UI::DrawFloatControl("Focus Range", &c.camera.lens.focusRange, 0.05f, 0.0f, FLT_MAX).isItemEdited;
                            c.dirty |= UI::DrawFloatControl("Blur Amount", &c.camera.lens.blurAmount, 0.05f, 0.0f, FLT_MAX).isItemEdited;
                        }

                        // Bloom
                        c.dirty |= UI::DrawCheckbox("Enable Bloom", &pp.enableBloom).isItemEdited;
                        if (pp.enableBloom)
                        {
                            c.dirty |= UI::DrawFloatControl("Bloom Intensity", &pp.bloomIntensity, 0.01f, 0.0f, 100.0f).isItemEdited;
                            c.dirty |= UI::DrawFloatControl("Bloom Threshold", &pp.bloomThreshold, 0.01f, 0.0f, 10.0f).isItemEdited;
                            c.dirty |= UI::DrawFloatControl("Bloom Knee", &pp.bloomKnee, 0.01f, 0.0f, 10.0f).isItemEdited;
                            c.dirty |= UI::DrawFloatControl("Bloom Radius", &pp.bloomRadius, 0.01f, 0.0f, 10.0f).isItemEdited;
                            c.dirty |= UI::DrawIntControl("Bloom Iterations", &pp.bloomIterations, 1.0f, 1, 16).isItemEdited;
                        }

                        // Vignette
                        c.dirty |= UI::DrawCheckbox("Enable Vignette", &pp.enableVignette).isItemEdited;
                        if (pp.enableVignette)
                        {
                            c.dirty |= UI::DrawColorVec3("Vignette Color", pp.vignetteColor).isItemEdited;
                            c.dirty |= UI::DrawFloatControl("Vignette Radius", &pp.vignetteRadius, 0.01f, 0.0f, 10.0f).isItemEdited;
                            c.dirty |= UI::DrawFloatControl("Vignette Softness", &pp.vignetteSoftness, 0.01f, 0.0f, 10.0f).isItemEdited;
                            c.dirty |= UI::DrawFloatControl("Vignette Intensity", &pp.vignetteIntensity, 0.01f, 0.0f, 10.0f).isItemEdited;
                        }

                        // Chromatic Aberration
                        c.dirty |= UI::DrawCheckbox("Enable Chromatic Aberration", &pp.enableChromAb).isItemEdited;
                        if (pp.enableChromAb)
                        {
                            c.dirty |= UI::DrawFloatControl("Chromatic Aberration Amount", &pp.chromAbAmount, 0.0001f, 0.0f, 0.1f).isItemEdited;
                            c.dirty |= UI::DrawFloatControl("Chromatic Aberration Radial", &pp.chromAbRadial, 0.01f, 0.0f, 10.0f).isItemEdited;
                        }

                        // SSAO
                        c.dirty |= UI::DrawCheckbox("Enable SSAO", &pp.enableSSAO).isItemEdited;
                        if (pp.enableSSAO)
                        {
                            c.dirty |= UI::DrawFloatControl("AO Radius", &pp.aoRadius, 0.01f, 0.0f, 5.0f).isItemEdited;
                            c.dirty |= UI::DrawFloatControl("AO Bias", &pp.aoBias, 0.001f, 0.0f, 1.0f).isItemEdited;
                            c.dirty |= UI::DrawFloatControl("AO Intensity", &pp.aoIntensity, 0.05f, 0.0f, 5.0f).isItemEdited;
                            c.dirty |= UI::DrawFloatControl("AO Power", &pp.aoPower, 0.05f, 0.0f, 5.0f).isItemEdited;
                        }

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

            RenderComponent<Rigidbody2DComponent>("Rigid Body 2D", selectedEntity, [&]()
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
                UI::DrawCheckbox("Awake", &c.isAwake);
                UI::DrawCheckbox("Enabled", &c.isEnabled);
                UI::DrawCheckbox("Sleep", &c.isEnableSleep);
                UI::DrawCheckbox("Fixed Rotation", &c.fixedRotation);

                if (!c.fixedRotation)
                {
                    UI::DrawCheckbox("Fast Rotation", &c.allowFastRotation);
                }
            });

            RenderComponent<BoxCollider2DComponent>("Box Collider 2D", selectedEntity, [&]()
            {
                auto &c = selectedEntity.GetComponent<BoxCollider2DComponent>();
                c.dirty = UI::DrawVec2Control("Size", c.size, 0.025f, 1.0f);
                c.dirty |= UI::DrawVec2Control("Offset", c.offset, 0.025f);
                c.dirty |= UI::DrawFloatControl("Restitution", &c.restitution, 0.025f, 0.0f, FLT_MAX);
                c.dirty |= UI::DrawFloatControl("Friction", &c.friction, 0.025f, 0.0f, FLT_MAX);
                c.dirty |= UI::DrawFloatControl("Density", &c.density, 0.025f);
                c.dirty |= UI::DrawCheckbox("Is Sensor", &c.isSensor);
            });

            RenderComponent<CircleCollider2DComponent>("Circle Collider 2D", selectedEntity, [&]()
            {
                auto &cc = selectedEntity.GetComponent<CircleCollider2DComponent>();
                cc.dirty = UI::DrawVec2Control("Center", cc.center, 0.025f);
                cc.dirty |= UI::DrawFloatControl("Radius", &cc.radius, 0.025f, 0.0f, FLT_MAX);
                cc.dirty |= UI::DrawFloatControl("Restitution", &cc.restitution, 0.025f, 0.0f, FLT_MAX);
                cc.dirty |= UI::DrawFloatControl("Friction", &cc.friction, 0.025f, 0.0f, FLT_MAX);
                cc.dirty |= UI::DrawFloatControl("Density", &cc.density, 0.025f);
                cc.dirty |= UI::DrawCheckbox("Is Sensor", &cc.isSensor);
            });

            RenderComponent<RigidbodyComponent>("Rigid Body", selectedEntity, [&]()
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

                c.dirty |= UI::DrawCheckbox("Is Sensor", &c.isSensor);
                c.dirty |= UI::DrawCheckbox("Use Gravity", &c.useGravity);
                c.dirty |= UI::DrawCheckbox("Allow sleeping", &c.allowSleeping);
                c.dirty |= UI::DrawCheckbox("Retain Acceleration", &c.retainAcceleration);
                c.dirty |= UI::DrawCheckbox("Apply Gyroscopic Force", &c.applyGyroscopicForce);
            });

            RenderComponent<BoxColliderComponent>("Box Collider", selectedEntity, [&]()
            {
                auto &c = selectedEntity.GetComponent<BoxColliderComponent>();
                c.dirty = UI::DrawVec3Control("Center", c.center, 0.025f, 0.0f);
                c.dirty |= UI::DrawVec3Control("Scale", c.scale, 0.025f, 1.0f);
                // TODO: Collider Edit from UI (TOGGLES)
            });

            RenderComponent<SphereColliderComponent>("Sphere Collider", selectedEntity, [&]()
            {
                auto &c = selectedEntity.GetComponent<SphereColliderComponent>();
                c.dirty = UI::DrawVec3Control("Center", c.center, 0.025f, 0.0f);
                c.dirty |= UI::DrawFloatControl("Radius", &c.radius, 0.025f, 0.01f, 10000.0f, 1.0f);

                // TODO: Collider Edit from UI (TOGGLES)
            });

            RenderComponent<CapsuleColliderComponent>("Capsule Collider", selectedEntity, [&]()
            {
                auto &c = selectedEntity.GetComponent<CapsuleColliderComponent>();
                c.dirty = UI::DrawVec3Control("Center", c.center, 0.025f, 0.0f);
                c.dirty |= UI::DrawFloatControl("Radius", &c.radius, 0.025f, 0.01f, 10000.0f, 1.0f);
                c.dirty |= UI::DrawFloatControl("Height", &c.height, 0.025f, 0.01f, 10000.0f, 1.0f);

                // TODO: Collider Edit from UI (TOGGLES)
            });

            RenderComponent<CharacterControllerComponent>("Character Controller", selectedEntity, [&]()
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

            RenderComponent<MeshColliderComponent>("Mesh Collider", selectedEntity, [&]()
            {
                auto &c = selectedEntity.GetComponent<MeshColliderComponent>();
                c.dirty = UI::DrawCheckbox("Convex", &c.convex);
                ImGui::Text("Vertices: %zu", c.vertices.size());
                ImGui::Text("Indices: %zu", c.indices.size());
                if (ImGui::Button("Clear Mesh Data"))
                {
                    c.dirty = false;
                    c.vertices.clear();
                    c.indices.clear();
                }
            });

            RenderComponent<HeightFieldColliderComponent>("HeightField Collider", selectedEntity, [&]()
            {
                auto &c = selectedEntity.GetComponent<HeightFieldColliderComponent>();
                c.dirty = UI::DrawVec3Control("Center", c.center, 0.025f, 0.0f);
                c.dirty |= UI::DrawVec3Control("Scale", c.scale, 0.025f, 1.0f);
                ImGui::Text("Sample Count: %u", c.sampleCount);
                ImGui::Text("Heights Size: %zu", c.heights.size());
            });

            RenderComponent<TextComponent>("Text", selectedEntity, [&]()
            {
                auto &c = selectedEntity.GetComponent<TextComponent>();

                const bool isFontLoaded = c.fontHandle != AssetHandle(0);
                std::string fontLabel = isFontLoaded ? "Font Loaded" : "Drag Here";
                UI::DrawButtonWithColumn("Font", fontLabel.c_str(), nullptr, [&, this]()
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
                            ImGui::SameLine();
                            if (ImGui::Button("X##ClearTextFont"))
                            {
                                c.fontHandle = AssetHandle(0);
                            }
                        }
                    });

                const bool isMaterialLoaded = c.material2dHandle != AssetHandle(0);
                std::string materialLabel = isMaterialLoaded ? "Material Loaded" : "Drag Here";
                UI::DrawButtonWithColumn("Material", materialLabel.c_str(), nullptr, [&c, this, assetManager, &isMaterialLoaded]()
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
                            ImGui::SameLine();
                            if (ImGui::Button("X##ClearTextMaterial"))
                            {
                                c.material2dHandle = AssetHandle(0);
                            }
                        }
                    });

                char textBuffer[2048] = {};
                strncpy(textBuffer, c.text.c_str(), sizeof(textBuffer) - 1);
                if (ImGui::InputTextMultiline("Text", textBuffer, sizeof(textBuffer), ImVec2{0.0f, 0.0f}))
                {
                    c.text = textBuffer;
                }

                UI::DrawColorVec4("Color", c.color);
                UI::DrawFloatControl("Kerning", &c.kerning, 0.001f, -10.0f, 10.0f);
                UI::DrawFloatControl("Line Spacing", &c.lineSpacing, 0.001f, -10.0f, 10.0f);
                UI::DrawCheckbox("Screen Space", &c.screenSpace);
            });

            RenderComponent<AudioSourceComponent>("Audio Source", selectedEntity, [&]()
            {
                auto &c = selectedEntity.GetComponent<AudioSourceComponent>();

                const bool isLoaded = c.handle != AssetHandle(0);
                std::string label = isLoaded ? assetManager->GetAssetDisplayName(c.handle) : "Drag Here";

                UI::DrawButtonWithColumn("Audio", label.c_str(), nullptr, [&c, this, &isLoaded, assetManager]()
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
                            ImGui::SameLine();
                            if (ImGui::Button("X"))
                            {
                                c.handle = AssetHandle(0);
                            }
                        }
                    });;
                
                if (isLoaded)
                {
                    if (Ref<FmodSound> sound = assetManager->GetAsset<FmodSound>(c.handle))
                    {
                        std::string soundId = std::format("##{}", (uint64_t)c.handle);

                        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                        ImGui::BeginGroup();
                        if (ImGui::Button("Play"))
                        {
                            sound->Stop();
                            sound->Play();
                            sound->SetVolume(c.volume);
                            sound->SetPitch(c.pitch);
                            sound->SetPan(c.pan);
                            sound->SetMode(c.loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);
                        }
                        
                        ImGui::SameLine();
                        if (ImGui::Button("Stop"))
                        {
                            sound->Stop();
                        }

                        const bool isPlaying = sound->IsPlaying();
                        const bool isPaused = sound->IsPaused();

                        ImGui::BeginDisabled(!isPlaying && !isPaused);
                        std::string pauseLabel = !isPaused ? "Pause" : "Resume";
                        ImGui::SameLine();
                        if (ImGui::Button(pauseLabel.c_str()))
                        {
                            if (sound->IsPaused())
                                sound->Resume();
                            else if (isPlaying)
                                sound->Pause();
                        }
                        ImGui::EndDisabled();

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

                        if (UI::DrawCheckbox("Play On Start", &c.playOnStart))
                        {
                        }

                        if (UI::DrawCheckbox("Loop", &c.loop))
                        {
                            if (c.loop)
                                sound->SetMode(FMOD_LOOP_NORMAL);
                            else
                                sound->SetMode(FMOD_LOOP_OFF);
                        }

                        bool dspChainChanged = false;

                        if (ImGui::Button("Add DSP"))
                        {
                            ImGui::OpenPopup("##add_audio_dsp_popup");
                        }

                        if (ImGui::BeginPopup("##add_audio_dsp_popup"))
                        {
                            if (ImGui::MenuItem("Reverb"))
                            {
                                AudioSourceComponent::DspSettings dsp;
                                dsp.type = AudioSourceComponent::DspType::Reverb;
                                c.dsps.push_back(dsp);
                                dspChainChanged = true;
                            }
                            if (ImGui::MenuItem("Distortion"))
                            {
                                AudioSourceComponent::DspSettings dsp;
                                dsp.type = AudioSourceComponent::DspType::Distortion;
                                c.dsps.push_back(dsp);
                                dspChainChanged = true;
                            }
                            if (ImGui::MenuItem("Chorus"))
                            {
                                AudioSourceComponent::DspSettings dsp;
                                dsp.type = AudioSourceComponent::DspType::Chorus;
                                c.dsps.push_back(dsp);
                                dspChainChanged = true;
                            }
                            if (ImGui::MenuItem("Compressor"))
                            {
                                AudioSourceComponent::DspSettings dsp;
                                dsp.type = AudioSourceComponent::DspType::Compressor;
                                c.dsps.push_back(dsp);
                                dspChainChanged = true;
                            }
                            if (ImGui::MenuItem("Delay"))
                            {
                                AudioSourceComponent::DspSettings dsp;
                                dsp.type = AudioSourceComponent::DspType::Delay;
                                c.dsps.push_back(dsp);
                                dspChainChanged = true;
                            }

                            ImGui::EndPopup();
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
                                        dspChainChanged |= UI::DrawCheckbox("Enabled", &dsp.enabled);
                                        ImGui::SameLine();
                                        if (ImGui::Button("Remove"))
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
                                                dspChainChanged |= UI::DrawCheckbox("Use Sidechain", &dsp.compressorUseSidechain);
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

            RenderComponent<ScriptComponent>("C# Script", selectedEntity, [&]()
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
                    if (ImGui::Button("Detach", { -1.0f, 0.0f }))
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
                                        if (UI::DrawCheckbox(name.c_str(), &data))
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

                                        std::vector<const char*> labels;
                                        for (const auto& n : enumNames)
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
                                        UI::DrawButtonWithColumn(name.c_str(), label.c_str(), nullptr, [this, &name, &dummy, &classRegisteredInstanceField, &c, handle, assetManager, &field]()
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
                                                            Entity src{ SceneManager::GetEntity(m_Scene, entityUUID) };
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
                                            ImGui::SameLine();
                                            if (ImGui::Button("X"))
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

            RenderComponent<TerrainComponent>("Terrain", selectedEntity, [&]()
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

                    UI::DrawButtonWithColumn("Heightmap Data / Texture", label.c_str(), nullptr, [&, this]()
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
                            ImGui::SameLine();
                            if (ImGui::Button("X##ClearHeightmap"))
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

                    if (ImGui::Button("Rebuild Flat Terrain", ImVec2(-1.0f, 0.0f)))
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

                    if (ImGui::Button("Generate Noise Terrain", ImVec2(-1.0f, 0.0f)))
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
                    UI::DrawButtonWithColumn("Material", label.c_str(), nullptr, [&, this]()
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
                            ImGui::SameLine();
                            if (ImGui::Button("X##ClearMaterial"))
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

            if (ImGui::BeginPopup("##add_component_context", ImGuiWindowFlags_NoDecoration))
            {
                static char buffer[256] = { 0 };
                static std::string compNameFilterResultStr;
                static std::set<std::pair<std::string, CompType>> filteredCompName;

                // ImGui::SetKeyboardFocusHere();
                ImGui::InputTextWithHint("##component_name", "Component", buffer, sizeof(buffer) + 1, ImGuiInputTextFlags_EscapeClearsAll | ImGuiInputTextFlags_NoHorizontalScroll);

                compNameFilterResultStr = std::string(buffer);

                filteredCompName.clear();

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

                static std::function<void(Entity, CompType)> addCompFunc = [=](Entity entity, CompType type)-> void
                {
                    switch (type)
                    {
                    case CompType_Camera:
                        entity.AddComponent<CameraComponent>();
                        break;
                    case CompType_Sprite2D:
                        entity.AddComponent<Sprite2DComponent>();
                        break;
                 case CompType_Animator2D:
                        entity.AddComponent<Animator2DComponent>();
                        break;
                    case CompType_Circle2D:
                        entity.AddComponent<Circle2DComponent>();
                        break;
                    case CompType_PointLight2D:
                        entity.AddComponent<PointLight2DComponent>();
                        break;
                    case CompType_DirectionalLight:
                        entity.AddComponent<DirectionalLightComponent>();
                        break;
                    case CompType_PointLight:
                        entity.AddComponent<PointLightComponent>();
                        break;
                    case CompType_SpotLight:
                        entity.AddComponent<SpotLightComponent>();
                        break;
                    case CompType_Text:
                        entity.AddComponent<TextComponent>();
                        break;
                    case CompType_Widget:
                        entity.AddComponent<WidgetComponent>();
                        break;
                    case CompType_Rigidbody2D:
                        entity.AddComponent<Rigidbody2DComponent>();
                        break;
                    case CompType_BoxCollider2D:
                        entity.AddComponent<BoxCollider2DComponent>();
                        break;
                    case CompType_CircleCollider2D:
                        entity.AddComponent<CircleCollider2DComponent>();
                        break;
                    case CompType_SkeletalMesh:
                        entity.AddComponent<SkeletalMeshComponent>();
                        break;
                    case CompType_StaticMesh:
                        entity.AddComponent<StaticMeshComponent>();
                        break;
                    case CompType_Rigidbody:
                        entity.AddComponent<RigidbodyComponent>();
                        break;
                    case CompType_BoxCollider:
                        entity.AddComponent<BoxColliderComponent>();
                        break;
                    case CompType_SphereCollider:
                        entity.AddComponent<SphereColliderComponent>();
                        break;
                    case CompType_CapsuleCollider:
                        entity.AddComponent<CapsuleColliderComponent>();
                        break;
                    case CompType_CharacterController:
                        entity.AddComponent<CharacterControllerComponent>();
                        break;
                    case CompType_MeshCollider:
                        entity.AddComponent<MeshColliderComponent>();
                        break;
                    case CompType_Terrain:
                        entity.AddComponent<TerrainComponent>();
                        break;
                    case CompType_HeightFieldCollider:
                        entity.AddComponent<HeightFieldColliderComponent>();
                        break;
                    case CompType_Prefab:
                        entity.AddComponent<PrefabComponent>();
                        break;
                    case CompType_AudioSource:
                        entity.AddComponent<AudioSourceComponent>();
                        break;
                    case CompType_WorldEnvironment:
                        entity.AddComponent<WorldEnvironment>();
                        break;
                    case CompType_Script:
                        entity.AddComponent<ScriptComponent>();
                        break;
                    default: break;
                    }
                };

                if (compNameFilterResultStr.empty())
                {
                    for (const auto &[strName, type] : s_ComponentsName)
                    {
                        if (ImGui::Selectable(strName.c_str()))
                        {
                            addCompFunc(Entity{ selectedEntity, m_Scene}, type);
                            ImGui::CloseCurrentPopup();
                        }
                    }
                }

                for (const auto &[strName, type] : filteredCompName)
                {
                    if (ImGui::Selectable(strName.c_str()))
                    {
                        addCompFunc(Entity{ selectedEntity, m_Scene }, type);
                        ImGui::CloseCurrentPopup();
                    }
                }
                ImGui::EndPopup();
            }
        }

        ImGui::End();
    }

    void ScenePanel::RenderSceneEditViewport()
    {
        IGN_PROFILE_FUNCTION();
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

        if (m_Scene && m_Scene->IsDirty())
        {
            windowFlags |= ImGuiWindowFlags_UnsavedDocument;
        }

        m_Data.sceneViewportEditorVisible = ImGui::Begin("Viewport", nullptr, windowFlags);
        if (m_Data.sceneViewportEditorVisible)
        {
            SceneRenderer *activeSceneRenderer = nullptr;
            if (m_Scene)
            {
                activeSceneRenderer = m_Scene->GetSceneRenderer();

                // During Play, render to the Editor Viewport using the EditorPlayCamera (mirror of game camera
                // with an editor-viewport-sized projection). During Stop/Simulate, use the editor camera as usual.
                ICamera *editorViewCamera = nullptr;
                if (m_Scene->IsPlaying())
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

                if (m_EditorLayer && m_EditorLayer->IsInPrefabIsolation())
                {
                    if (UI::DrawButton("Back to scene", { 96.0f, 28.0f }))
                        m_EditorLayer->ExitPrefabIsolation(true);
                }

                // Calculating Scene Viewport location
                const ImVec2 &canvasPos = ImGui::GetCursorScreenPos();
                const ImVec2 &canvasSize = ImGui::GetContentRegionAvail();

                globals::GEditor::EditorViewport.min = { canvasPos.x, canvasPos.y };
                globals::GEditor::EditorViewport.max = { canvasSize.x, canvasSize.y };

                // When not playing/simulating, make sure we clear focus state and restore cursor
                const bool isPlayOrSimulate = m_Scene->IsRunning();
                const bool mouseWantsFocus = (InputSystem::GetCursorMode() == CursorMode::Disabled || InputSystem::GetCursorMode() == CursorMode::Hidden);

                if (!isPlayOrSimulate)
                {
                    if (m_SceneFocused)
                    {
                        m_SceneFocused = false;
                        InputSystem::SetCursorMode(CursorMode::Normal);
                    }
                    m_SceneFocusCooldown = 0;
                }
                else if (m_SceneFocusCooldown > 0)
                {
                    m_SceneFocusCooldown--;
                }

                // Mouse position in screen space
                const ImVec2 &mousePos = ImGui::GetMousePos();
                m_ViewportData.mousePos = { mousePos.x - canvasPos.x, mousePos.y - canvasPos.y };

                if (target)
                {
                    // Render scene texture to imgui
                    ImTextureID editorViewImage = (ImTextureID)target->compositeRT->GetColorAttachment(0)->GetHandle().Get();
                    ImGui::Image(editorViewImage, canvasSize);
                }

                const bool imageHovered = ImGui::IsItemHovered();
                const bool mouseDown = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
                const bool mouseDoubleDown = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

                ImDrawList *drawList = ImGui::GetWindowDrawList();

                // if (m_SceneFocused && mouseWantsFocus)
                {
                    const char *bannerText = "SCENE FOCUSED - LeftShift + F1 to release";
                    const ImVec2 textSize = ImGui::CalcTextSize(bannerText);
                    const ImVec2 bannerSize = ImVec2{ textSize.x + 24.0f, textSize.y + 8.0f };
                    const ImVec2 bannerCursor = { canvasPos.x + (canvasSize.x - bannerSize.x) * 0.5f, canvasPos.y + 10.0f };
                    //ImGui::SetCursorScreenPos(bannerCursor);
                    UI::DrawBannerText(bannerText, { 124.0f, 200.0f });
                }

                // Click inside the viewport to (re-)focus in Play or Simulate modes
                // Only allow focusing if the scene is not already focused and the cooldown has expired
                // This prevents accidental focus when clicking on the viewport immediately after starting Play/Simulate
                if (!m_Data.sceneViewportGameplayVisible && isPlayOrSimulate && !m_SceneFocused && m_SceneFocusCooldown == 0)
                {
                    if (imageHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    {
                        m_SceneFocused = true;
                        m_SceneFocusCooldown = 10;
                        InputSystem::SetCursorMode(CursorMode::Disabled);
                        InputSystem::SetMouseToCenter();
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
                                drawList->AddImage(reinterpret_cast<ImTextureID>(texture->GetHandle().Get()), iconMin, iconMax);

                                if (imageHovered && (mouseDown || mouseDoubleDown) && !m_Gizmo.IsManipulating() && !m_Gizmo.IsHovered() && !m_Data.is2DBoundsHovered)
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
                                drawList->AddImage(reinterpret_cast<ImTextureID>(texture->GetHandle().Get()), iconMin, iconMax, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), col);

                                if (imageHovered && (mouseDown || mouseDoubleDown) && !m_Gizmo.IsManipulating() && !m_Gizmo.IsHovered() && !m_Data.is2DBoundsHovered)
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

                {
                    constexpr float padding = 18.0f;
                    float yPosition = 6.0f;

                    const float fps = ImGui::GetIO().Framerate;
                    std::string statusStr = std::format("FPS {:.5}", fps);
                    drawList->AddText(ImVec2(canvasPos.x + 6.0f, canvasPos.y + yPosition), 0xFFFFFFFF, statusStr.c_str());

                    yPosition += padding;
                    statusStr = std::format("Response Time {:.3} ms", 1000.0f / fps);
                    drawList->AddText(ImVec2(canvasPos.x + 6.0f, canvasPos.y + yPosition), 0xFFFFFFFF, statusStr.c_str());
                }

                // Mouse picking from viewport object-id attachment (on mouse down only)
                if (!m_Scene->IsPlaying())
                {
                    if (activeSceneRenderer)
                    {
                        const auto localMouseX = static_cast<uint32_t>(std::max(m_ViewportData.mousePos.x, 0.0f));
                        const auto localMouseY = static_cast<uint32_t>(std::max(m_ViewportData.mousePos.y, 0.0f));
                        activeSceneRenderer->SetEditorWidgetMousePosition(localMouseX, localMouseY, imageHovered);
                        activeSceneRenderer->SetCameraWidgetMousePosition(&m_EditorCamera, localMouseX, localMouseY, imageHovered);
                    }
                    
                    if (imageHovered && (mouseDown || mouseDoubleDown) && !m_Gizmo.IsManipulating() && !m_Gizmo.IsHovered() && !m_Data.is2DBoundsHovered)
                    {
                        if (clickedIconEntity.IsValid())
                        {
                            Entity targetSelection = clickedIconEntity;
                            if (!mouseDoubleDown && !ImGui::IsKeyDown(ImGuiKey_LeftShift))
                            {
                                const auto parent = clickedIconEntity.GetParentUUID();
                                if (parent != UUID(0))
                                {
                                    if (Entity parentEntity = SceneManager::GetEntity(m_Scene, parent); parentEntity.IsValid())
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
                            }
                        }
                        else // invalid entity
                        {
                            Ref<Texture> objectIdTexture = target->sceneRT->GetColorAttachment(1);
                            if (objectIdTexture && objectIdTexture->GetHandle())
                            {
                                const int texWidth = objectIdTexture->GetWidth();
                                const int texHeight = objectIdTexture->GetHeight();

                                if (canvasSize.x > 0.0f && canvasSize.y > 0.0f && texWidth > 0 && texHeight > 0)
                                {
                                    const int pixelX = std::clamp(static_cast<int>((m_ViewportData.mousePos.x / canvasSize.x) * static_cast<float>(texWidth)), 0, texWidth - 1);
                                    const int pixelY = std::clamp(static_cast<int>((m_ViewportData.mousePos.y / canvasSize.y) * static_cast<float>(texHeight)), 0, texHeight - 1);

                                    nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();
                                    nvrhi::TextureDesc stagingDesc = objectIdTexture->GetHandle()->getDesc();
                                    stagingDesc.initialState = nvrhi::ResourceStates::CopyDest;
                                    nvrhi::StagingTextureHandle stagingTexture = device->createStagingTexture(stagingDesc, nvrhi::CpuAccessMode::Read);

                                    nvrhi::CommandListHandle copyCmd = device->createCommandList();
                                    copyCmd->open();
                                    copyCmd->copyTexture(stagingTexture, nvrhi::TextureSlice(), objectIdTexture->GetHandle(), nvrhi::TextureSlice());
                                    copyCmd->close();
                                    device->executeCommandList(copyCmd);

                                    size_t rowPitch = 0;
                                    if (void *mapped = device->mapStagingTexture(stagingTexture, nvrhi::TextureSlice(), nvrhi::CpuAccessMode::Read, &rowPitch))
                                    {
                                        const auto pixelData = static_cast<const uint32_t *>(mapped);
                                        const uint32_t pickedObjectId = pixelData[pixelY * (rowPitch / sizeof(uint32_t)) + pixelX];
                                        device->unmapStagingTexture(stagingTexture);

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

                                            // Single click: prefer selecting the direct parent group first.
                                            // Double click: select the exact clicked entity.
                                            if (!mouseDoubleDown && !ImGui::IsKeyDown(ImGuiKey_LeftShift))
                                            {
                                                const auto parent = pickedEntity.GetParentUUID();
                                                if (parent != UUID(0))
                                                {
                                                    if (Entity parentEntity = SceneManager::GetEntity(m_Scene, parent); parentEntity.IsValid())
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
                                                m_SelectedEntities.clear();
                                                m_SelectedEntities[instantiated.GetUUID()] = instantiated;
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

                    // Draw orientation just for Perspective projection
                    if (m_EditorCamera.projectionType != ProjectionType::Orthographic)
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
                            glm::vec3 f = glm::vec3(view[0][2], view[1][2], view[2][2]);
                            m_EditorCamera.pitch = glm::clamp(std::asin(glm::clamp(-f.y, -1.0f, 1.0f)), m_EditorCamera.controls.minPitch, m_EditorCamera.controls.maxPitch);
                            m_EditorCamera.yaw = std::atan2(-f.z, -f.x);
                        }
                    }

                    // Setup gizmo
                    GizmoInfo gizmoInfo;
                    gizmoInfo.cameraView = view;
                    gizmoInfo.cameraProjection = projection;
                    gizmoInfo.cameraType = m_EditorCamera.projectionType;
                    gizmoInfo.viewRect = Rect(globals::GEditor::EditorViewport.min, globals::GEditor::EditorViewport.min + globals::GEditor::EditorViewport.max);
                    switch (m_Gizmo.GetOperation())
                    {
                        default:
                        case ImGuizmo::OPERATION::TRANSLATE: gizmoInfo.snapValue = m_ViewportData.snapValues[0]; break;
                        case ImGuizmo::OPERATION::ROTATE: gizmoInfo.snapValue = m_ViewportData.snapValues[1]; break;
                        case ImGuizmo::OPERATION::SCALE: gizmoInfo.snapValue = m_ViewportData.snapValues[2]; break;
                    }

                    m_Gizmo.SetInfo(gizmoInfo);

                    Render2DBoundsSizing();

                    // Start manipulation: Fired only on the first frame of interaction
                    const bool allowGizmoManipulation = !m_Data.is2DBoundsSizing;
                    bool isManipulatingNow = allowGizmoManipulation && m_Gizmo.IsManipulating();

                    static std::unordered_map<UUID, TransformComponent> initialTransforms;

                    if (isManipulatingNow && !m_Data.isGizmoManipulating)
                    {
                        initialTransforms.clear();
                        for (const auto &[uuid, entity] : m_SelectedEntities)
                        {
                            // Store the original transform of each selected entity
                            initialTransforms[uuid] = entity.GetTransform();
                        }
                    }

                    // Capture PREVIOUS frame value before overwriting — needed for the release-commit below
                    bool wasManipulating = m_Data.isGizmoManipulating;
                    m_Data.isGizmoManipulating = isManipulatingNow;
                    m_Data.isGizmoBeingUse = isManipulatingNow || m_Gizmo.IsHovered() || m_Data.is2DBoundsHovered || m_Data.is2DBoundsSizing;

                    if (allowGizmoManipulation && m_SelectedEntities.size() > 1)
                    {
                        // Step 1: Compute shared pivot (center of all selected entities)
                        glm::vec3 pivot(0.0f);
                        for (Entity entity : m_SelectedEntities | std::views::values)
                        {
                            pivot += entity.GetTransform().world.translation;
                        }
                        pivot /= static_cast<float>(m_SelectedEntities.size());

                        // Step 2: create a transform matrix for the gizmo at the pivot point
                        glm::mat4 gizmoTransform = glm::translate(glm::mat4(1.0f), pivot);
                        glm::mat4 manipulatedTransform = gizmoTransform; // This will be modified by the gizmo

                        // Step 3: Manipulate the matrix
                        m_Gizmo.Manipulate(manipulatedTransform);

                        if (m_Data.isGizmoManipulating)
                        {
                            // THis delta is now the TOTAL change from the moment of manipulation began
                            glm::mat4 gizmoDelta = glm::inverse(gizmoTransform) * manipulatedTransform;

                            // Decompose the total delta
                            glm::vec3 deltaTranslation, deltaScale, deltaRotation;
                            Math::DecomposeTransformEuler(gizmoDelta, deltaTranslation, deltaRotation, deltaScale);

                            for (auto &[uuid, entity] : m_SelectedEntities)
                            {
                                // Get the live transform component to apply changes to it
                                auto &tr = entity.GetTransform();

                                // Get the ORIGINAL transform we stored at the beginning of the manipulation
                                const auto &initialTransform = initialTransforms.at(uuid);
                                glm::mat4 initialWorldMatrix = initialTransform.world.GetMatrix();

                                // Apply Translation and Rotation around the shared pivot
                                glm::mat4 toPivot = glm::translate(glm::mat4(1.0f), -pivot);
                                glm::mat4 fromPivot = glm::translate(glm::mat4(1.0f), pivot);
                                glm::mat4 noScaleDelta = Math::RemoveScale(gizmoDelta);

                                // Apply the total delta to the ORIGINAL world matrix
                                glm::mat4 newWorldMatrix = fromPivot * noScaleDelta * toPivot * tr.world.GetMatrix();
                                glm::vec3 newTranslation, newRotationEuler, newScale;
                                Math::DecomposeTransformEuler(newWorldMatrix, newTranslation, newRotationEuler, newScale);

                                // ----- Apply Scale and Update Local Transform -----
                                if (entity.GetParentUUID() != UUID(0))
                                {
                                    Entity parent = SceneManager::GetEntity(m_Scene, entity.GetParentUUID());
                                    const auto &parentTr = parent.GetTransform();
                                    glm::mat4 parentWorld = parentTr.world.GetMatrix();
                                    glm::mat4 localMatrix = glm::inverse(parentWorld) * newWorldMatrix;

                                    glm::vec3 localTranslation, localEuler, localScale;
                                    Math::DecomposeTransformEuler(localMatrix, localTranslation, localEuler, localScale);
                                    tr.local.translation = localTranslation;
                                    tr.local.rotation = glm::quat(localEuler);

                                    // Apply the total scale delta to the ORIGINAL local scale
                                    tr.local.scale = initialTransform.local.scale * deltaScale;
                                }
                                else
                                {
                                    tr.local.translation = newTranslation;
                                    tr.local.rotation = glm::quat(newRotationEuler);

                                    // Apply the total scale delta to the ORIGINAL local scale
                                    tr.local.scale = initialTransform.local.scale * deltaScale;
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
                        if (allowGizmoManipulation)
                        {
                            auto &tr = entity.GetTransform();
                            glm::mat4 transformMatrix = tr.world.GetMatrix();

                            m_Gizmo.Manipulate(transformMatrix);

                            if (m_Gizmo.IsManipulating())
                            {
                                const glm::vec3 preservedLocalScale = tr.local.scale;
                                glm::vec3 translation, rotation, scale;
                                Math::DecomposeTransformEuler(transformMatrix, translation, rotation, scale);
                                const ImGuizmo::OPERATION op = m_Gizmo.GetOperation();

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

                                    if (op == ImGuizmo::SCALE)
                                    {
                                        tr.local.scale = localScale;
                                    }
                                    else
                                    {
                                        tr.local.scale = preservedLocalScale;
                                    }
                                }
                                else
                                {
                                    tr.local.translation = translation;
                                    tr.local.rotation = glm::quat(rotation);

                                    if (op == ImGuizmo::SCALE)
                                    {
                                        tr.local.scale = scale;
                                    }
                                    else
                                    {
                                        tr.local.scale = preservedLocalScale;
                                    }
                                }
                                tr.dirty = true;

                                // check if the entity has rigidbody and update its position and rotation in the physics engine
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
                    }
                }
            }
            else
            {
                UI::DrawCenteredText("No Scene");
            }
        }

        ImGui::End();

    }

    void ScenePanel::RenderSceneGameViewport()
    {
        IGN_PROFILE_FUNCTION();
        if (m_EditorLayer->GetState().gameplayViewportWindow)
        {
            m_Data.sceneViewportGameplayVisible = ImGui::Begin("Game", &m_EditorLayer->GetState().gameplayViewportWindow);
            if (m_Data.sceneViewportGameplayVisible)
            {
                // Preview camera
                if (m_Scene)
                {
                    auto activeSceneRenderer = m_Scene->GetSceneRenderer();
                    ImGui::TextUnformatted("Zoom");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(120.0f);
                    ImGui::SliderFloat("##GamePreviewZoom", &m_Data.gamePreviewZoom, 0.25f, 4.0f, "%.2fx");

                    ImGui::SameLine();
                    if (ImGui::Button("Reset##GamePreviewZoomPan"))
                    {
                        m_Data.gamePreviewZoom = 1.0f;
                        m_Data.gamePreviewPan = glm::vec2(0.0f);
                    }

                    // Calculating Scene Viewport location
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

                                // Click inside the viewport to (re-)focus in Play or Simulate modes
                                if (m_Scene->IsRunning() && !m_SceneFocused && m_SceneFocusCooldown == 0)
                                {
                                    if (imageHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                                    {
                                        m_SceneFocused = true;
                                        m_SceneFocusCooldown = 10;
                                        InputSystem::SetCursorMode(CursorMode::Disabled);
                                        InputSystem::SetMouseToCenter();
                                    }
                                }

                                uint32_t localMouseX = 0;
                                uint32_t localMouseY = 0;

                                // We need to store the Game-play Mouse Position if only the Image is hovered
                                if (imageHovered)
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

                            ImTextureID gameplayViewImaage = (ImTextureID)target->compositeRT->GetColorAttachment(0)->GetHandle().Get();
                            drawList->PushClipRect(baseImagePos, ImVec2(baseImagePos.x + baseImageSize.x, baseImagePos.y + baseImageSize.y), true);
                            drawList->AddImage(gameplayViewImaage, imagePos, ImVec2(imagePos.x + imageSize.x, imagePos.y + imageSize.y));
                            drawList->PopClipRect();
                        }
                        
                        {
                            constexpr float padding = 18.0f;
                            float yPosition = 6.0f;
                            const float fps = ImGui::GetIO().Framerate;
                            std::string statusStr = std::format("FPS {:.5}", fps);
                            drawList->AddText(ImVec2(canvasPos.x + 6, canvasPos.y + 6), 0xFFFFFFFF, statusStr.c_str());

                            yPosition += padding;
                            statusStr = std::format("Response Time {:.3} ms", 1000.0f / fps);
                            drawList->AddText(ImVec2(canvasPos.x + 6, canvasPos.y + yPosition), 0xFFFFFFFF, statusStr.c_str());

                            yPosition += padding;
                            statusStr = std::format("Viewport x: {} y: {} w: {} h: {}", baseImagePos.x, baseImagePos.y, baseImagePos.x + baseImageSize.x, baseImagePos.y + baseImageSize.y);
                            drawList->AddText(ImVec2(canvasPos.x + 6, canvasPos.y + yPosition), 0xFFFFFFFF, statusStr.c_str());
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
                    ImGui::Text("No Scene");
                }
            }
            ImGui::End();
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
        }

        ImGui::SameLine();

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));

        auto drawGizmoBtn = [&](const std::string &iconName, const ImVec2 &buttonSize, bool active)
        {
            const auto texID = (ImTextureID)m_Icons[iconName]->GetHandle().Get();
            return UI::DrawSelectImageButton(iconName.c_str(), texID, buttonSize, active);;
        };

        if (drawGizmoBtn("picking", buttonSize, m_Data.gizmoOp == GizmoOperation::NONE)) SetGizmoOperation(GizmoOperation::NONE);
        ImGui::SameLine();
        if (drawGizmoBtn("translate", buttonSize, m_Data.gizmoOp == GizmoOperation::TRANSLATE)) SetGizmoOperation(GizmoOperation::TRANSLATE);
        ImGui::SameLine();
        if (drawGizmoBtn("rotate", buttonSize, m_Data.gizmoOp == GizmoOperation::ROTATE)) SetGizmoOperation(GizmoOperation::ROTATE);
        ImGui::SameLine();
        if (drawGizmoBtn("scale", buttonSize, m_Data.gizmoOp == GizmoOperation::SCALE)) SetGizmoOperation(GizmoOperation::SCALE);

        if (m_EditorCamera.GetNavigationMode() == EditorCamera::NavigationMode::Mode2D)
        {
            ImGui::SameLine();
            bool isBoundSizing = m_Data.gizmoOp == GizmoOperation::BOUND_SIZING_2D;
            if (isBoundSizing) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
            else ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            if (ImGui::Button("2D Bounds", ImVec2(0, 24))) SetGizmoOperation(GizmoOperation::BOUND_SIZING_2D);
            ImGui::PopStyleColor();
        }

        ImGui::SameLine();
        ImGui::Spacing();
        ImGui::SameLine();

        const bool isLocal = m_Gizmo.GetMode() == ImGuizmo::LOCAL;
        if (drawGizmoBtn("transform_local", buttonSize, isLocal)) m_Gizmo.SetMode(ImGuizmo::LOCAL);
        ImGui::SameLine();
        if (drawGizmoBtn("transform_world", buttonSize, !isLocal)) m_Gizmo.SetMode(ImGuizmo::WORLD);

        ImGui::PopStyleVar(2);

        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        switch (m_Gizmo.GetOperation())
        {
        default:
        case ImGuizmo::OPERATION::TRANSLATE:
            ImGui::DragFloat("##GizmoSnapping", &m_ViewportData.snapValues[0], 0.05f, 0.0f, 100.0f, "Snap %.2f");
            break;
        case ImGuizmo::OPERATION::ROTATE:
            ImGui::DragFloat("##GizmoSnapping", &m_ViewportData.snapValues[1], 0.05f, 0.0f, 100.0f, "Snap %.2f");
            break;
        case ImGuizmo::OPERATION::SCALE:
            ImGui::DragFloat("##GizmoSnapping", &m_ViewportData.snapValues[2], 0.05f, 0.0f, 100.0f, "Snap %.2f");
            break;
        }
        ImGui::SameLine();

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));

        const bool isScenePlaying = m_Scene && m_Scene->IsPlaying();
        Ref<Texture> scenePlayStopTex = isScenePlaying ? m_Icons["stop"] : m_Icons["play"];
        const auto scenePlayStopID = (ImTextureID)scenePlayStopTex->GetHandle().Get();

        ImGui::SameLine();
        if (UI::DrawImageButton("##PlayButton", scenePlayStopID, buttonSize))
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

        const bool isSceneSimulate = m_Scene && m_Scene->IsSimulating();
        Ref<Texture> sceneSimulateTex = isSceneSimulate ? m_Icons["stop"] : m_Icons["simulate"];
        const auto sceneSimulateID = (ImTextureID)sceneSimulateTex->GetHandle().Get();

        ImGui::SameLine();
        if (UI::DrawImageButton("##SimulateButton", sceneSimulateID, buttonSize))
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

        ImGui::PopStyleVar(2);

        ImGui::EndDisabled();
    }

    bool ScenePanel::Is2DResizableEntity(Entity entity) const
    {
        return entity.IsValid() && (entity.HasComponent<Sprite2DComponent>() || entity.HasComponent<Circle2DComponent>() || entity.HasComponent<TextComponent>());
    }

    glm::vec3 ScenePanel::ScreenToWorldOnPlane(const glm::vec2 &screenPos, float planeZ, bool *isValid)
    {
        return Math::ScreenToWorldOnPlane(screenPos, planeZ,
            m_EditorCamera.GetProjection() * m_EditorCamera.GetView(),
            { globals::GEditor::EditorViewport.min, globals::GEditor::EditorViewport.min + globals::GEditor::EditorViewport.max },
            isValid
        );
    }

    void ScenePanel::Render2DBoundsSizing()
    {
        IGN_PROFILE_FUNCTION();
        m_Data.is2DBoundsHovered = false;

        auto clearResizeState = [this]()
        {
            m_Data.is2DBoundsSizing = false;
            m_Data.active2DCorner = -1;
            m_Data.active2DEntity = UUID(0);
        };

        auto releaseResizeCommand = [this, &clearResizeState]()
        {
            if (!m_Data.is2DBoundsSizing)
                return;

            if (!m_Scene)
            {
                clearResizeState();
                return;
            }

            Entity resizedEntity = SceneManager::GetEntity(m_Scene, m_Data.active2DEntity);
            if (resizedEntity.IsValid())
            {
                CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<TransformComponent>>(
                    m_Scene, resizedEntity.GetUUID(), m_Data.before2DResize, resizedEntity.GetTransform()));
            }

            clearResizeState();
        };

        if (!m_Scene || m_EditorCamera.GetNavigationMode() != EditorCamera::NavigationMode::Mode2D || m_SelectedEntities.size() != 1 || m_Data.gizmoOp != GizmoOperation::BOUND_SIZING_2D)
        {
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                releaseResizeCommand();
            }
            return;
        }

        Entity entity = GetSelectedEntity();
        if (!Is2DResizableEntity(entity))
        {
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
                releaseResizeCommand();
            return;
        }

        auto &tr = entity.GetTransform();
        const glm::mat4 worldMatrix = tr.world.GetMatrix();
        const glm::mat4 viewProjection = m_EditorCamera.GetProjection() * m_EditorCamera.GetView();

        std::array<glm::vec3, 4> worldCorners{};
        std::array<ImVec2, 4> screenCorners{};
        for (size_t i = 0; i < kBoundsCorners.size(); ++i)
        {
            const glm::vec4 world = worldMatrix * glm::vec4(kBoundsCorners[i].x, kBoundsCorners[i].y, 0.0f, 1.0f);
            worldCorners[i] = glm::vec3(world);
            if (!Math::ProjectWorldToScreen(worldCorners[i], viewProjection, {globals::GEditor::EditorViewport.min, globals::GEditor::EditorViewport.min + globals::GEditor::EditorViewport.max}, screenCorners[i]))
            {
                if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
                {
                    releaseResizeCommand();
                }
                return;
            }
        }

        ImDrawList *drawList = ImGui::GetWindowDrawList();
        constexpr ImU32 boundsColor = IM_COL32(247, 210, 60, 255);
        drawList->AddPolyline(screenCorners.data(), static_cast<int>(screenCorners.size()), boundsColor, ImDrawFlags_Closed, 2.0f);

        const ImVec2 mousePos = ImGui::GetMousePos();
        const bool mouseInViewport = 
            mousePos.x >= globals::GEditor::EditorViewport.min.x && mousePos.x <= (globals::GEditor::EditorViewport.min.x + globals::GEditor::EditorViewport.max.x) &&
            mousePos.y >= globals::GEditor::EditorViewport.min.y && mousePos.y <= (globals::GEditor::EditorViewport.min.y + globals::GEditor::EditorViewport.max.y);

        constexpr float handleRadius = 6.0f;
        const float handleRadiusSq = handleRadius * handleRadius;

        int hoveredCorner = -1;
        float bestDistanceSq = FLT_MAX;
        for (int i = 0; i < static_cast<int>(screenCorners.size()); ++i)
        {
            const float dx = mousePos.x - screenCorners[i].x;
            const float dy = mousePos.y - screenCorners[i].y;
            const float distanceSq = dx * dx + dy * dy;
            if (distanceSq <= handleRadiusSq && distanceSq < bestDistanceSq)
            {
                bestDistanceSq = distanceSq;
                hoveredCorner = i;
            }
        }

        m_Data.is2DBoundsHovered = mouseInViewport && hoveredCorner != -1;

        if (m_Data.gizmoOp == GizmoOperation::BOUND_SIZING_2D)
        {
            for (int i = 0; i < static_cast<int>(screenCorners.size()); ++i)
            {
                const bool isActive = m_Data.is2DBoundsSizing && m_Data.active2DCorner == i;
                const bool isHovered = hoveredCorner == i;
                const ImU32 fillColor = isActive ? IM_COL32(255, 185, 0, 255) : (isHovered ? IM_COL32(255, 220, 110, 255) : IM_COL32(240, 240, 240, 230));

                const ImVec2 min = { screenCorners[i].x - handleRadius, screenCorners[i].y - handleRadius };
                const ImVec2 max = { screenCorners[i].x + handleRadius, screenCorners[i].y + handleRadius };
                drawList->AddRectFilled(min, max, fillColor, 2.0f);
                drawList->AddRect(min, max, IM_COL32(30, 30, 30, 255), 2.0f, 0, 1.0f);
            }
        }

        if (!m_Data.is2DBoundsSizing && m_Data.gizmoOp == GizmoOperation::BOUND_SIZING_2D && mouseInViewport && hoveredCorner != -1 && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !m_Gizmo.IsManipulating())
        {
            m_Data.is2DBoundsSizing = true;
            m_Data.active2DCorner = hoveredCorner;
            m_Data.active2DEntity = entity.GetUUID();
            m_Data.active2DPlaneZ = worldMatrix[3].z;
            m_Data.active2DAxisX = glm::normalize(glm::vec3(worldMatrix[0]));
            m_Data.active2DAxisY = glm::normalize(glm::vec3(worldMatrix[1]));
            m_Data.before2DResize = tr;

            const int oppositeCorner = (hoveredCorner + 2) % 4;
            m_Data.active2DOppositeWorld = worldCorners[oppositeCorner];
        }

        if (m_Data.is2DBoundsSizing && m_Data.gizmoOp == GizmoOperation::BOUND_SIZING_2D)
        {
            if (m_Data.active2DEntity != entity.GetUUID())
            {
                releaseResizeCommand();
                return;
            }

            if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                bool validDragPoint = false;
                glm::vec3 dragWorld = ScreenToWorldOnPlane(glm::vec2(mousePos.x, mousePos.y), m_Data.active2DPlaneZ, &validDragPoint);
                if (validDragPoint)
                {
                    const glm::vec2 cornerSign = glm::sign(kBoundsCorners[m_Data.active2DCorner]);
                    const glm::vec3 delta = dragWorld - m_Data.active2DOppositeWorld;

                    float halfX = cornerSign.x * glm::dot(delta, m_Data.active2DAxisX) * 0.5f;
                    float halfY = cornerSign.y * glm::dot(delta, m_Data.active2DAxisY) * 0.5f;

                    halfX = glm::max(halfX, 0.01f);
                    halfY = glm::max(halfY, 0.01f);

                    const glm::vec3 centerWorld = m_Data.active2DOppositeWorld
                        + (m_Data.active2DAxisX * (cornerSign.x * halfX))
                        + (m_Data.active2DAxisY * (cornerSign.y * halfY));

                    const float targetWorldScaleX = halfX * 2.0f;
                    const float targetWorldScaleY = halfY * 2.0f;

                    const float currentWorldScaleZ = glm::max(glm::length(glm::vec3(worldMatrix[2])), 0.0001f);

                    glm::mat4 worldRotation = Math::RemoveScale(worldMatrix);
                    worldRotation[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

                    const glm::mat4 targetWorld =
                        glm::translate(glm::mat4(1.0f), centerWorld)
                        * worldRotation
                        * glm::scale(glm::mat4(1.0f), glm::vec3(targetWorldScaleX, targetWorldScaleY, currentWorldScaleZ));

                    if (entity.GetParentUUID() != UUID(0))
                    {
                        Entity parent = SceneManager::GetEntity(m_Scene, entity.GetParentUUID());
                        const glm::mat4 parentWorld = parent.GetTransform().world.GetMatrix();
                        const glm::mat4 localMatrix = glm::inverse(parentWorld) * targetWorld;

                        glm::vec3 localTranslation, localEuler, localScale;
                        Math::DecomposeTransformEuler(localMatrix, localTranslation, localEuler, localScale);
                        tr.local.translation = localTranslation;
                        tr.local.rotation = glm::quat(localEuler);
                        tr.local.scale = localScale;
                    }
                    else
                    {
                        tr.local.translation = centerWorld;
                        tr.local.scale.x = targetWorldScaleX;
                        tr.local.scale.y = targetWorldScaleY;
                    }

                    tr.dirty = true;
                }
            }
            else
            {
                releaseResizeCommand();
            }
        }
    }

    template<typename T, typename UIFunction>
    void ScenePanel::RenderComponent(const std::string &name, Entity entity, UIFunction uiFunction, bool allowedToRemove)
    {
        if (entity.HasComponent<T>())
        {
            constexpr ImGuiTreeNodeFlags treeNdeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed
                | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_FramePadding;

            T &comp = entity.GetComponent<T>();
            UUID compID = comp.GetCompID();

            ImGui::PushID(static_cast<int>(compID));

            // ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2 { 6.0f, 3.0f });
            ImGui::Separator();

            const bool open = ImGui::TreeNodeEx((const char *)(uint32_t *)(uint64_t *)&compID, treeNdeFlags, name.c_str());
            // ImGui::PopStyleVar();

            const float buttonSize = ImGui::GetFrameHeight();
            const float cursorX = ImGui::GetCursorPosX();
            const float available = ImGui::GetContentRegionAvail().x;
            ImGui::SameLine();
            ImGui::SetCursorPosX(cursorX + available - buttonSize);
            if (ImGui::Button("...", { buttonSize, buttonSize }))
                ImGui::OpenPopup("comp_settings");

            bool componentRemoved = false;

            if (ImGui::BeginPopup("comp_settings"))
            {
                if (allowedToRemove && ImGui::MenuItem("Remove"))
                    componentRemoved = true;
                ImGui::EndPopup();
            }

            if (open)
            {
                uiFunction();
                ImGui::TreePop();
            }

            if (componentRemoved)
            {
                entity.RemoveComponent<T>();
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
        if (m_SceneFocused)
        {
            if (m_Scene && m_Scene->IsRunning() && event.GetKeyCode() == Key::F1 && InputSystem::IsModifierPressed(KeyMod::LeftShift))
            {
                m_SceneFocused = false;
                m_SceneFocusCooldown = 10;
                InputSystem::SetCursorMode(CursorMode::Normal);
                LOG_INFO("Scene play/simulate focused state released");
                return true;
            }
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
        if (m_EditorCamera.GetNavigationMode() != EditorCamera::NavigationMode::Mode2D && op == GizmoOperation::BOUND_SIZING_2D)
            return;

        m_Data.gizmoOp = op;

        switch (op)
        {
        case GizmoOperation::NONE:
            m_Gizmo.SetOperation(ImGuizmo::NONE);
            break;
        case GizmoOperation::TRANSLATE:
            m_Gizmo.SetOperation(ImGuizmo::TRANSLATE);
            break;
        case GizmoOperation::ROTATE:
            m_Gizmo.SetOperation(ImGuizmo::ROTATE);
            break;
        case GizmoOperation::SCALE:
            m_Gizmo.SetOperation(ImGuizmo::SCALE);
            break;
        case GizmoOperation::BOUND_SIZING_2D:
            m_Gizmo.SetOperation(ImGuizmo::NONE);
            break;
        }
    }

    void ScenePanel::SetGizmoMode(ImGuizmo::MODE mode)
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

            m_EditorCamera.yaw += deltaTime * camViewAxis.x;
            m_EditorCamera.pitch += deltaTime * camViewAxis.y;

            // m_Camera.position += m_Camera.GetForwardDirection() * deltaTime * m_CameraData.moveSpeed * -camMoveAxis.y;
            // m_Camera.position += m_Camera.GetRightDirection() * deltaTime * m_CameraData.moveSpeed * camMoveAxis.x;

            LOG_INFO(j->ToString());
        }

        m_EditorCamera.UpdateMouseState();
        if (m_IsHovered && !m_Gizmo.IsManipulating() && !m_Data.is2DBoundsSizing)
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
    }

    void ScenePanel::ClearSelection()
    {
        m_SelectedEntities.clear();
    }

    Entity ScenePanel::SetSelectedEntity(Entity entity)
    {
        auto activeSceneRenderer = m_Scene->GetSceneRenderer();

        if (!entity.IsValid())
        {
            m_SelectedEntities.clear();
            m_TrackingSelectedEntity = UUID(0);

            activeSceneRenderer->ClearSelectedEntities();
            return {};
        }

        // multi select
        if (m_EditorLayer->GetState().multiSelect)
        {
            if (auto it = m_SelectedEntities.find(entity.GetUUID()); it != m_SelectedEntities.end())
            {
                // de-select
                activeSceneRenderer->UnselectEntity(it->second);
                it = m_SelectedEntities.erase(it);

                if (!m_SelectedEntities.empty())
                {
                    m_TrackingSelectedEntity = m_SelectedEntities.begin()->first;
                    activeSceneRenderer->SetSelectedEntity(m_SelectedEntities.begin()->second);

                    return m_SelectedEntities.begin()->second;
                }
            }
            else
            {
                m_SelectedEntities[entity.GetUUID()] = entity;
                activeSceneRenderer->SetSelectedEntity(entity);
            }
        }
        else // single select
        {
            m_SelectedEntities.clear();
            activeSceneRenderer->ClearSelectedEntities();

            m_SelectedEntities[entity.GetUUID()] = entity;
            activeSceneRenderer->SetSelectedEntity(entity);
        }

        if (m_SelectedEntities.empty())
        {
            SetGizmoOperation(GizmoOperation::NONE);
        }

        m_TrackingSelectedEntity = entity.GetUUID();
        return entity;
    }

    Entity ScenePanel::GetSelectedEntity()
    {
        return m_SelectedEntities.empty() ? Entity{} : m_SelectedEntities.begin()->second;
    }

    void ScenePanel::DuplicateSelectedEntity()
    {
        for (const Entity& entity : m_SelectedEntities | std::views::values)
        {
            if (entity.IsValid())
            {
                SceneManager::DuplicateEntity(m_Scene, entity);
            }
        }
    }
}
