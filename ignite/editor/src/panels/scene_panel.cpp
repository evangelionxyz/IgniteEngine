// Copyright (c) 2026 Evangelion Manuhutu

#include "pch.hpp"
#include "scene_panel.hpp"
#include "editor_layer.hpp"
#include "ignite/audio/fmod_sound.hpp"
#include "ignite/audio/fmod_dsp.hpp"
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
#include "ignite/scripting/script_instances/script_instance.hpp"
#include "ignite/animation/animator/animator_controller.hpp"
#include "ignite/asset/asset_importer.hpp"
#include "ignite/core/profiler/profiler.hpp"
#include "ignite/scene/entity.hpp"
#include "ignite/scene/sprite_sheet.hpp"
#include "ignite/scene/entity_destroy_command.hpp"
#include "ignite/scene/entity_rename_command.hpp"
#include "ignite/scene/entity_reparent_command.hpp"
#include "ignite/scene/component_property_command.hpp"
#include "ignite/globals/globals.hpp"

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
        constexpr std::array<glm::vec2, 4> kBoundsCorners =
        {
            glm::vec2(-0.5f, -0.5f),
            glm::vec2( 0.5f, -0.5f),
            glm::vec2( 0.5f,  0.5f),
            glm::vec2(-0.5f,  0.5f)
        };
    }

    UUID ScenePanel::m_TrackingSelectedEntity = UUID(0);

    ScenePanel::ScenePanel(const char *windowTitle, EditorLayer *editor)
        : IPanel(windowTitle, editor), m_Gizmo()
    {
        Application* app = Application::GetInstance();

		const uint32_t width = app->GetCreateInfo().width;
        const uint32_t height = app->GetCreateInfo().height;

        m_EditorCamera = EditorCamera("ScenePanel-Editor Camera");

        m_EditorCamera.SetTarget(glm::vec3(0.0f));
        m_EditorCamera.SetDistance(12.0f);
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
    }

    void ScenePanel::SetActiveScene(const Ref<Scene> &scene)
    {
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
        if (m_Scene && m_EditorLayer->GetState().sceneState != ESceneState::Play)
        {
            UpdateCameraInput(deltaTime);
        }
    }

    void ScenePanel::RenderHierarchy()
    {
        IGN_PROFILE_FUNCTION();
        ImGui::Begin("Hierarchy");
        ImGui::Button(m_Scene->name.c_str(), { ImGui::GetContentRegionAvail().x, 0.0f });

        // target drop
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_ENTITY_SOURCE_ITEM))
            {
                LOG_ASSERT(payload->DataSize == sizeof(Entity), "WRONG ITEM, that should be an entity");
                Entity src{ *static_cast<entt::entity *>(payload->Data), m_Scene.get() };

                // check if src entity has parent
                auto &idComp = src.GetComponent<IDComponent>();
                if (idComp.parent != 0)
                {
                    UUID oldParent = idComp.parent;
                    // current parent should be removed
                    Entity parent = SceneManager::GetEntity(m_Scene.get(), idComp.parent);
                    parent.GetComponent<IDComponent>().RemoveChild(idComp.uuid);
                    idComp.parent = UUID(0);

                    // Record for undo — reparenting to root (UUID 0)
                    CommandManager::AddCommand(CreateScope<EntityReparentCommand>(m_Scene.get(), src.GetUUID(), oldParent, UUID(0)));
                }
            }

            ImGui::EndDragDropTarget();
        }

        ImGuiTableFlags tableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_NoClip | ImGuiTableFlags_PadOuterX
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

            std::vector<Entity> rootEntities;
            m_Scene->registry->view<IDComponent>().each([&](const entt::entity e, const auto &id)
            {
                if (id.parent == UUID(0))
                {
                    rootEntities.emplace_back(e, m_Scene.get());
                }
            });

            for (const Entity &entity : rootEntities)
            {
                RenderEntityNode(entity);
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
            entity = SetSelectedEntity(SceneManager::CreateEmptyEntity(m_Scene.get(), "Empty"));
        }
        if (ImGui::MenuItem("Camera"))
        {
            entity = SetSelectedEntity(SceneManager::CreateCamera(m_Scene.get(), "Camera"));
        }
        if (ImGui::MenuItem("Widget"))
        {
            entity = SetSelectedEntity(SceneManager::CreateEmptyEntity(m_Scene.get(), "Widget"));
            if (entity.IsValid() && !entity.HasComponent<WidgetComponent>())
            {
                entity.AddComponent<WidgetComponent>();
            }
        }

        if (ImGui::BeginMenu("2D"))
        {
            if (ImGui::MenuItem("Sprite"))
            {
                entity = SetSelectedEntity(SceneManager::CreateSprite(m_Scene.get(), "Sprite"));
            }
            if (ImGui::MenuItem("Circle"))
            {
                entity = SetSelectedEntity(SceneManager::CreateCircle(m_Scene.get(), "Circle"));
            }
            if (ImGui::MenuItem("Point Light"))
            {
                entity = SetSelectedEntity(SceneManager::CreatePointLight2D(m_Scene.get(), "Point Light 2D"));
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("3D"))
        {
            if (ImGui::MenuItem("Skeletal Mesh"))
            {
                entity = SetSelectedEntity(SceneManager::CreateEmptyEntity(m_Scene.get(), "Skeletal Mesh"));
                if (entity.IsValid() && !entity.HasComponent<SkeletalMeshComponent>())
                {
                    entity.AddComponent<SkeletalMeshComponent>();
                }
            }
			if (ImGui::MenuItem("Static Mesh"))
			{
				entity = SetSelectedEntity(SceneManager::CreateEmptyEntity(m_Scene.get(), "Static Mesh"));
				if (entity.IsValid() && !entity.HasComponent<StaticMeshComponent>())
				{
					entity.AddComponent<StaticMeshComponent>();
				}
			}
            if (ImGui::MenuItem("Directional Light"))
            {
                entity = SetSelectedEntity(SceneManager::CreateEmptyEntity(m_Scene.get(), "Directional Light"));
                if (entity.IsValid() && !entity.HasComponent<DirectionalLightComponent>())
                {
                    entity.AddComponent<DirectionalLightComponent>();
                }
            }
            if (ImGui::MenuItem("Point Light"))
            {
                entity = SetSelectedEntity(SceneManager::CreateEmptyEntity(m_Scene.get(), "Point Light"));
                if (entity.IsValid() && !entity.HasComponent<PointLightComponent>())
                {
                    entity.AddComponent<PointLightComponent>();
                }
            }
            if (ImGui::MenuItem("Spot Light"))
            {
                entity = SetSelectedEntity(SceneManager::CreateEmptyEntity(m_Scene.get(), "Spot Light"));
                if (entity.IsValid() && !entity.HasComponent<SpotLightComponent>())
                {
                    entity.AddComponent<SpotLightComponent>();
                }
            }
            if (ImGui::MenuItem("World Environment"))
            {
                entity = SetSelectedEntity(SceneManager::CreateWorldEnvironment(m_Scene.get(), "World Environment"));
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

                Entity child = SceneManager::GetEntity(m_Scene.get(), childUuid);
                if (child.IsValid() && hasSelectedDescendant(child))
                    return true;
            }

            return false;
        };

        if (hasSelectedDescendant(entity))
        {
            ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        }

        ImGuiTreeNodeFlags flags = (isSelected ? ImGuiTreeNodeFlags_Selected : 0) | (!idComp.HasChild() ? ImGuiTreeNodeFlags_Leaf : 0)
            | ImGuiTreeNodeFlags_OpenOnDoubleClick
            | ImGuiTreeNodeFlags_SpanAvailWidth
            | ImGuiTreeNodeFlags_OpenOnArrow
            | ImGuiTreeNodeFlags_LabelSpanAllColumns;

        const intptr_t imguiPushId = static_cast<intptr_t>(static_cast<uint64_t>(static_cast<uint32_t>(entity)));
        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, { 0.435f, 0.287f, 0.000f, 1.000f });
        ImGui::PushStyleColor(ImGuiCol_Header, { 0.000f, 0.305f, 0.453f, 1.000f });
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, { 0.780f, 0.520f, 0.000f, 1.000f });
        
        const bool opened = ImGui::TreeNodeEx(reinterpret_cast<void *>(imguiPushId), flags, "%s", idComp.name.c_str());

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
                        SceneManager::AddChild(m_Scene.get(), entity, e);
                        SetSelectedEntity(e);
                    }

                    ImGui::EndMenu();
                }
                
                if (ImGui::MenuItem("Delete"))
                {
                    DestroyEntity(entity);
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
                ImGui::SetDragDropPayload(DND_PAYLOAD_ENTITY_SOURCE_ITEM, &entity, sizeof(Entity));

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
                    LOG_ASSERT(payload->DataSize == sizeof(Entity), "WRONG ITEM, that should be an entity");
                    Entity src { *static_cast<entt::entity *>(payload->Data), m_Scene.get() };
                    
                    // Capture old parent BEFORE reparenting
                    UUID oldParent = src.GetComponent<IDComponent>().parent;
                    UUID newParent = entity.GetComponent<IDComponent>().uuid;

                    // the current 'entity' is the target (new parent for src)
                    SceneManager::AddChild(m_Scene.get(), entity, src);

                    // Record the reparent for undo
                    CommandManager::AddCommand(CreateScope<EntityReparentCommand>(m_Scene.get(), src.GetUUID(), oldParent, newParent));
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
                    Entity childEntity = SceneManager::GetEntity(m_Scene.get(), uuid);
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
            auto *project = m_EditorLayer ? m_EditorLayer->GetActiveProject().get() : nullptr;
            auto *assetManager = project ? project->GetAssetManager() : nullptr;

            // Main Component
            // ID Component
            auto &idComp = selectedEntity.GetComponent<IDComponent>();
            char buffer[255] = {};
            strncpy(buffer, idComp.name.c_str(), sizeof(buffer) - 1);
            if (ImGui::InputText("##label", buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue))
            {
                std::string oldName = idComp.name;
                std::string newName(buffer);
                CommandManager::ExecuteCommand(CreateScope<EntityRenameCommand>(m_Scene.get(), idComp.uuid, oldName, newName));
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
                if (translationState.isItemDeactivatedAfterEdit) CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<TransformComponent>>(m_Scene.get(), selectedEntity.GetUUID(), s_TransformBefore, comp));

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
                if (rotationState.isItemDeactivatedAfterEdit) { CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<TransformComponent>>(m_Scene.get(), selectedEntity.GetUUID(), s_TransformBefore, comp)); s_RotationEditing = false; }

                UI::State scaleState = UI::DrawVec3Control("Scale", comp.local.scale, 0.025f, 1.0f);
                if (scaleState.isItemActivated)            s_TransformBefore = comp;
                if (scaleState.isItemEdited)               comp.dirty = true;
                if (scaleState.isItemDeactivatedAfterEdit) CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<TransformComponent>>(m_Scene.get(), selectedEntity.GetUUID(), s_TransformBefore, comp));

                // Visibility checkbox — instant commit
                {
                    TransformComponent before = comp;
                    UI::State visibleState = UI::DrawCheckbox("Visible", &comp.visible);
                    if (visibleState.isItemEdited)
                        CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<TransformComponent>>(m_Scene.get(), selectedEntity.GetUUID(), before, comp));
                }

            }, false); // false: not allowed to remove the component

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

                UI::DrawFloatControl("Exposure", &c.exposure, 0.025f, 0.0f, FLT_MAX);
                UI::DrawFloatControl("Gamma", &c.gamma, 0.025f, 0.0f, FLT_MAX);
                UI::DrawFloatControl("Ambient", &c.ambient, 0.025f, 0.0f, FLT_MAX);

                {
                    static const char *tonemapLabels[] = { "Reinhard", "Uncharted2", "Filmic" };
                    int tonemapIndex = static_cast<int>(c.tonemapMode);
                    if (UI::DrawComboBox("Tonemap", tonemapLabels, IM_ARRAYSIZE(tonemapLabels), &tonemapIndex))
                    {
                        c.tonemapMode = static_cast<TonemapMode>(tonemapIndex);
                    }
                }



                // Fog
                UI::DrawFloatControl("Fog Density", &c.fogDensity, 0.001f, 0.0f, FLT_MAX);
                if (c.fogDensity > 0.0f)
                {
                    UI::DrawColorVec4("Fog Color", c.fogColor);
                    UI::DrawFloatControl("Fog Start", &c.fogStart, 0.1f, 0.0f, FLT_MAX);
                    UI::DrawFloatControl("Fog End", &c.fogEnd, 0.1f, 0.0f, FLT_MAX);
                }

                const bool hasHDR = c.hdrHandle != AssetHandle(0);
                std::string buttonLabel = hasHDR ? assetManager->GetAssetDisplayName(c.hdrHandle) : "Drag Here";
                UI::DrawButtonWithColumn("HDR", buttonLabel.c_str(), nullptr, [&c, this, &hasHDR]()
                    {
                        if (ImGui::BeginDragDropTarget())
                        {
                            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                            {
                                LOG_ASSERT(payload->DataSize == sizeof(AssetHandle), "WRONG ITEM, that should be an asset handle");
                                AssetHandle handle = *static_cast<AssetHandle *>(payload->Data);
                                AssetMetaData metadata = m_EditorLayer->GetActiveProject()->GetAssetManager()->GetMetaData(handle);
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

                    if (auto sceneRenderer = m_Scene->GetSceneRenderer())
                    {
                        auto shadowMap = sceneRenderer->GetCascadedShadowMapDepthTexture();
                        if (shadowMap)
                        {
                            ImTextureID texId = (ImTextureID)shadowMap->GetHandle().Get();
                            ImGui::Image(texId, { 256, 256 });
                        }
                    }
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
                UI::DrawButtonWithColumn("Material", mat2dLabel.c_str(), nullptr, [&c, &selectedEntity, &isMat2dLoaded, this]()
                    {
                        if (ImGui::BeginDragDropTarget())
                        {
                            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                            {
                                LOG_ASSERT(payload->DataSize == sizeof(AssetHandle), "WRONG ITEM, that should be an asset handle");
                                AssetHandle handle = *static_cast<AssetHandle *>(payload->Data);
                                AssetType type = m_EditorLayer->GetActiveProject()->GetAssetManager()->GetAssetType(handle);
                                if (type == AssetType::Material2D)
                                {
                                    Sprite2DComponent before = c;
                                    c.materialHandle = handle;
                                    CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<Sprite2DComponent>>(m_Scene.get(), selectedEntity.GetUUID(), before, c));
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
                                CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<Sprite2DComponent>>(m_Scene.get(), selectedEntity.GetUUID(), before, c));
                            }
                        }
                    });

                // Get material 2d
                Ref<Material2D> mat2d = nullptr;
                if (isMat2dLoaded)
                {
                    m_EditorLayer->GetActiveProject()->GetAsset<Material2D>(c.materialHandle);
                }

                if (!isMat2dLoaded)
                {
                    // Texture on sprite 2d
                    const bool isTextureLoaded = c.handle != AssetHandle(0);
                    const std::string textureLabel = isTextureLoaded ? assetManager->GetAssetDisplayName(c.handle) : "Drag Here";
                    UI::DrawButtonWithColumn("Texture", textureLabel.c_str(), nullptr, [&c, &isTextureLoaded, this, &selectedEntity]()
                        {
                            if (ImGui::BeginDragDropTarget())
                            {
                                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                                {
                                    LOG_ASSERT(payload->DataSize == sizeof(AssetHandle), "WRONG ITEM, that should be an asset handle");
                                    AssetHandle handle = *static_cast<AssetHandle *>(payload->Data);
                                    AssetType type = m_EditorLayer->GetActiveProject()->GetAssetManager()->GetAssetType(handle);
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
                                        CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<Sprite2DComponent>>(m_Scene.get(), selectedEntity.GetUUID(), before, c));
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
                        CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<Sprite2DComponent>>(m_Scene.get(),
                            selectedEntity.GetUUID(), s_Sprite2DBefore, c));

                    auto colorState = UI::DrawColorVec4("Color", c.color);
                    if (colorState.isItemActivated) s_Sprite2DBefore = c;
                    if (colorState.isItemDeactivatedAfterEdit)
                        CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<Sprite2DComponent>>(m_Scene.get(),
                            selectedEntity.GetUUID(), s_Sprite2DBefore, c));
                }

                UI::State uv0State = UI::DrawVec2Control("UV0", c.uv0, 0.001f);
                if (uv0State.isItemActivated)            s_Sprite2DBefore = c;
                if (uv0State.isItemDeactivatedAfterEdit)
                    CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<Sprite2DComponent>>(m_Scene.get(),
                        selectedEntity.GetUUID(), s_Sprite2DBefore, c));

                UI::State uv1State = UI::DrawVec2Control("UV1", c.uv1, 0.001f);
                if (uv1State.isItemActivated)            s_Sprite2DBefore = c;
                if (uv1State.isItemDeactivatedAfterEdit)
                    CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<Sprite2DComponent>>(m_Scene.get(),
                        selectedEntity.GetUUID(), s_Sprite2DBefore, c));

                UI::State flipXState = UI::DrawCheckbox("Flip X", &c.flipX);
                if (flipXState.isItemActivated)            s_Sprite2DBefore = c;
                if (flipXState.isItemDeactivatedAfterEdit)
                    CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<Sprite2DComponent>>(m_Scene.get(),
                        selectedEntity.GetUUID(), s_Sprite2DBefore, c));

                UI::State flipYState = UI::DrawCheckbox("Flip Y", &c.flipY);
                if (flipYState.isItemActivated)            s_Sprite2DBefore = c;
                if (flipYState.isItemDeactivatedAfterEdit)
                    CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<Sprite2DComponent>>(m_Scene.get(),
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
                    Ref<AnimatorController2D> animCtrl = m_EditorLayer->GetActiveProject()->GetAsset<AnimatorController2D>(c.controllerHandle);
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
                    CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<Circle2DComponent>>(m_Scene.get(), selectedEntity.GetUUID(), compBefore, c));
            });

			RenderComponent<StaticMeshComponent>("Static Mesh", selectedEntity, [&]()
			{
				auto &c = selectedEntity.GetComponent<StaticMeshComponent>();

				bool isMeshLoaded = c.handle != AssetHandle(0);

				std::string buttonLabel = isMeshLoaded ? assetManager->GetAssetDisplayName(c.handle) : "Drag Here";
				UI::DrawButtonWithColumn("Static Mesh Asset", buttonLabel.c_str(), nullptr, [&c, this, &isMeshLoaded]()
				{
					if (ImGui::BeginDragDropTarget())
					{
						if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
						{
							LOG_ASSERT(payload->DataSize == sizeof(AssetHandle), "WRONG ITEM, that should be an asset handle");
							AssetHandle handle = *static_cast<AssetHandle *>(payload->Data);
							auto assetManager = m_EditorLayer->GetActiveProject()->GetAssetManager();
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
					Ref<StaticMesh> sm = m_EditorLayer->GetActiveProject()->GetAsset<StaticMesh>(c.handle);
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

								UI::DrawButtonWithColumn(submeshName.c_str(), matLabel.c_str(), nullptr, [this, &c, i, isOverrideLoaded, &selectedEntity]()
								{
									if (ImGui::BeginDragDropTarget())
									{
										if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
										{
											LOG_ASSERT(payload->DataSize == sizeof(AssetHandle), "WRONG ITEM, that should be an asset handle");
											AssetHandle handle = *static_cast<AssetHandle *>(payload->Data);
											auto assetManager = m_EditorLayer->GetActiveProject()->GetAssetManager();
											AssetMetaData metadata = assetManager->GetMetaData(handle);

											if (metadata.type == AssetType::Material)
											{
												StaticMeshComponent before = c;
												c.overrideMaterials[static_cast<int>(i)] = handle;
												CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<StaticMeshComponent>>(m_Scene.get(), selectedEntity.GetUUID(), before, c));
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
											CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<StaticMeshComponent>>(m_Scene.get(), selectedEntity.GetUUID(), before, c));
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
                UI::DrawButtonWithColumn("Skeletal Mesh Asset", buttonLabel.c_str(), nullptr, [&c, this, &isMeshLoaded]()
                {
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                        {
                            LOG_ASSERT(payload->DataSize == sizeof(AssetHandle), "WRONG ITEM, that should be an asset handle");
                            AssetHandle handle = *static_cast<AssetHandle *>(payload->Data);
                            auto assetManager = m_EditorLayer->GetActiveProject()->GetAssetManager();
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
                    Ref<SkeletalMesh> sm = m_EditorLayer->GetActiveProject()->GetAsset<SkeletalMesh>(c.handle);
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

                                UI::DrawButtonWithColumn(submeshName.c_str(), matLabel.c_str(), nullptr, [this, &c, i, isOverrideLoaded, &selectedEntity]()
                                {
                                    if (ImGui::BeginDragDropTarget())
                                    {
                                        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                                        {
                                            LOG_ASSERT(payload->DataSize == sizeof(AssetHandle), "WRONG ITEM, that should be an asset handle");
                                            AssetHandle handle = *static_cast<AssetHandle *>(payload->Data);
                                            auto assetManager = m_EditorLayer->GetActiveProject()->GetAssetManager();
                                            AssetMetaData metadata = assetManager->GetMetaData(handle);

                                            if (metadata.type == AssetType::Material)
                                            {
                                                SkeletalMeshComponent before = c;
                                                c.overrideMaterials[static_cast<int>(i)] = handle;
                                                CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<SkeletalMeshComponent>>(m_Scene.get(), selectedEntity.GetUUID(), before, c));
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
                                            CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<SkeletalMeshComponent>>(m_Scene.get(), selectedEntity.GetUUID(), before, c));
                                        }
                                    }
                                });
                            }
                        }

                        // Animator
                        bool isAnimatorLoaded = c.runtimeAnimatorHandle != AssetHandle(0);
                        std::string buttonLabel = isAnimatorLoaded ? assetManager->GetAssetDisplayName(c.runtimeAnimatorHandle) : "Drag Here";
                        UI::DrawButtonWithColumn("Animator", buttonLabel.c_str(), nullptr, [&c, this, &sm, &isAnimatorLoaded]()
                        {
                            if (ImGui::BeginDragDropTarget())
                            {
                                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                                {
                                    LOG_ASSERT(payload->DataSize == sizeof(AssetHandle), "WRONG ITEM, that should be an asset handle");
                                    AssetHandle handle = *static_cast<AssetHandle *>(payload->Data);
                                    auto assetManager = m_EditorLayer->GetActiveProject()->GetAssetManager();
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

                            Ref<AnimatorController> animCtrl = m_EditorLayer->GetActiveProject()->GetAsset<AnimatorController>(c.runtimeAnimatorHandle);
                            if (animCtrl)
                            {
                                std::erase_if(c.runtimeParams, [&animCtrl](const AnimParam &param)
                                {
                                    return animCtrl->GetParam(param.name) == nullptr;
                                });

                                for (const AnimParam &param : animCtrl->params)
                                {
                                    auto it = std::find_if(c.runtimeParams.begin(), c.runtimeParams.end(), [&param](const AnimParam &runtimeParam)
                                    {
                                        return runtimeParam.name == param.name;
                                    });

                                    if (it == c.runtimeParams.end())
                                    {
                                        c.runtimeParams.push_back(param);
                                    }
                                    else if (it->type != param.type)
                                    {
                                        *it = param;
                                    }
                                }

                                if (ImGui::CollapsingHeader("Animator Preview", ImGuiTreeNodeFlags_DefaultOpen))
                                {
                                    ImGui::TextDisabled("Default State: %s", animCtrl->defaultState.empty() ? "(None)" : animCtrl->defaultState.c_str());

                                    if (!animCtrl->states.empty())
                                    {
                                        std::vector<const char *> stateLabels;
                                        stateLabels.reserve(animCtrl->states.size());

                                        std::string activeState = c.currentStateName;
                                        if (activeState.empty())
                                        {
                                            activeState = !animCtrl->defaultState.empty() ? animCtrl->defaultState : animCtrl->states.front().name;
                                        }

                                        int currentStateIndex = 0;
                                        for (size_t i = 0; i < animCtrl->states.size(); ++i)
                                        {
                                            stateLabels.push_back(animCtrl->states[i].name.c_str());
                                            if (animCtrl->states[i].name == activeState)
                                            {
                                                currentStateIndex = static_cast<int>(i);
                                            }
                                        }

                                        ImGui::BeginDisabled(true);
                                        if (UI::DrawComboBox("Preview State", stateLabels.data(), static_cast<int>(stateLabels.size()), &currentStateIndex))
                                        {
                                            c.currentStateName = animCtrl->states[static_cast<size_t>(currentStateIndex)].name;
                                            c.stateElapsed = 0.0f;
                                            c.stateNormalized = 0.0f;
                                        }
                                        ImGui::EndDisabled();

                                        ImGui::TextDisabled("State Time: %.3fs", c.stateElapsed);
                                        ImGui::TextDisabled("State Normalized: %.3f", c.stateNormalized);
                                    }

                                    if (!c.runtimeParams.empty())
                                    {
                                        ImGui::SeparatorText("Parameters");
                                        for (AnimParam &param : c.runtimeParams)
                                        {
                                            switch (param.type)
                                            {
                                                case AnimParam::Type::Float:
                                                    UI::DrawFloatControl(param.name.c_str(), &param.floatVal, 0.05f, -FLT_MAX, FLT_MAX);
                                                    break;
                                                case AnimParam::Type::Int:
                                                    UI::DrawIntControl(param.name.c_str(), &param.intVal, 1.0f, INT_MIN, INT_MAX);
                                                    break;
                                                case AnimParam::Type::Bool:
                                                    UI::DrawCheckbox(param.name.c_str(), &param.boolVal);
                                                    break;
                                                case AnimParam::Type::String:
                                                {
                                                    char buffer[256] = {};
                                                    strncpy(buffer, param.strVal.c_str(), sizeof(buffer) - 1);
                                                    if (ImGui::InputText(param.name.c_str(), buffer, sizeof(buffer)))
                                                    {
                                                        param.strVal = buffer;
                                                    }
                                                    break;
                                                }
                                            }
                                        }
                                    }

                                    if (!animCtrl->transitions.empty())
                                    {
                                        ImGui::SeparatorText("Transitions");

                                        std::string activeState = c.currentStateName;
                                        if (activeState.empty())
                                        {
                                            activeState = !animCtrl->defaultState.empty() ? animCtrl->defaultState : (animCtrl->states.empty() ? std::string {} : animCtrl->states.front().name);
                                        }

                                        auto findRuntimeParam = [&c](const std::string &name) -> const AnimParam *
                                        {
                                            auto it = std::find_if(c.runtimeParams.begin(), c.runtimeParams.end(), [&name](const AnimParam &param)
                                            {
                                                return param.name == name;
                                            });
                                            return it != c.runtimeParams.end() ? &(*it) : nullptr;
                                        };

                                        for (const AnimTransition &transition : animCtrl->transitions)
                                        {
                                            if (!transition.fromState.empty() && transition.fromState != activeState)
                                            {
                                                continue;
                                            }

                                            const std::string fromName = transition.fromState.empty() ? "Any State" : transition.fromState;
                                            bool allPass = !transition.hasExitTime || c.stateNormalized >= transition.exitTime;

                                            if (ImGui::TreeNode((std::format("{} -> {}", fromName, transition.toState) + "###transition_" + fromName + "_" + transition.toState).c_str()))
                                            {
                                                if (transition.hasExitTime)
                                                {
                                                    const bool exitPass = c.stateNormalized >= transition.exitTime;
                                                    ImGui::TextColored(exitPass ? ImVec4(0.25f, 0.9f, 0.35f, 1.0f) : ImVec4(0.95f, 0.35f, 0.35f, 1.0f),
                                                        "Exit Time %.3f (%s)", transition.exitTime, exitPass ? "PASS" : "WAIT");
                                                }
                                                else
                                                {
                                                    ImGui::TextDisabled("Exit Time: Disabled");
                                                }

                                                for (const AnimCondition &condition : transition.conditions)
                                                {
                                                    const AnimParam *runtimeParam = findRuntimeParam(condition.paramName);
                                                    const bool condPass = anim_utils::EvalCondition(condition, runtimeParam);
                                                    allPass &= condPass;

                                                    std::string threshold = "?";
                                                    if (runtimeParam)
                                                    {
                                                        switch (runtimeParam->type)
                                                        {
                                                            case AnimParam::Type::Float: threshold = std::format("{}", condition.floatThreshold); break;
                                                            case AnimParam::Type::Int: threshold = std::format("{}", condition.intThreshold); break;
                                                            case AnimParam::Type::Bool: threshold = condition.boolThreshold ? "true" : "false"; break;
                                                            case AnimParam::Type::String: threshold = condition.strThreshold; break;
                                                        }
                                                    }

                                                    ImGui::BulletText("%s %s %s [%s]",
                                                        condition.paramName.c_str(),
                                                        anim_utils::OpToStr(condition.op),
                                                        threshold.c_str(),
                                                        condPass ? "PASS" : "FAIL");
                                                }

                                                ImGui::TextColored(allPass ? ImVec4(0.25f, 0.9f, 0.35f, 1.0f) : ImVec4(0.95f, 0.35f, 0.35f, 1.0f),
                                                    "Transition %s", allPass ? "READY" : "BLOCKED");
                                                ImGui::TreePop();
                                            }
                                        }
                                    }
                                }
                            }
                        }

						// --- SOCKET SYSTEM: Render Socket Attachments UI ---
						if (sm->GetSkeletonHandle() != AssetHandle(0))
						{
							Ref<Skeleton> skeleton = m_EditorLayer->GetActiveProject()->GetAsset<Skeleton>(sm->GetSkeletonHandle());
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
										std::string socketMeshLabel = isAttached
											? assetManager->GetAssetDisplayName(attachedMeshHandle)
											: "Drag Mesh Here";

										UI::DrawButtonWithColumn(socket.name.c_str(), socketMeshLabel.c_str(), nullptr, [this, &c, socketName = socket.name, isAttached, &selectedEntity]()
											{
												if (ImGui::BeginDragDropTarget())
												{
													if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
													{
														LOG_ASSERT(payload->DataSize == sizeof(AssetHandle), "WRONG ITEM, that should be an asset handle");
														AssetHandle handle = *static_cast<AssetHandle *>(payload->Data);
														auto assetManager = m_EditorLayer->GetActiveProject()->GetAssetManager();
														AssetMetaData metadata = assetManager->GetMetaData(handle);

														if (metadata.type == AssetType::Mesh)
														{
															SkeletalMeshComponent before = c;
															c.socketAttachments[socketName] = handle;
															CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<SkeletalMeshComponent>>(m_Scene.get(), selectedEntity.GetUUID(), before, c));
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
														CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<SkeletalMeshComponent>>(m_Scene.get(), selectedEntity.GetUUID(), before, c));
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
                    int projectionIdx = static_cast<int>(c.camera.projectionType);
                    if (UI::DrawComboBox("Projection", projectionTypeStr, IM_ARRAYSIZE(projectionTypeStr), &projectionIdx))
                    {
                        c.camera.projectionType = static_cast<ProjectionType>(projectionIdx);
                        c.dirty = true;
                    }
                }

                // Aspect Ratio
                {
                    static const char *aspectRatioLabels[] = { "Free", "16:9", "16:10", "4:3", "21:9", "1:1" };
                    int aspectRatioIndex = static_cast<int>(c.camera.GetAspectRatioPreset());
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
                        CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<CameraComponent>>(m_Scene.get(), selectedEntity.GetUUID(), s_CameraBefore, c));
                }
                else
                {
                    UI::State orthoState = UI::DrawFloatControl("Ortho Size", &c.camera.orthoSize, 0.025f, 0.0f, FLT_MAX);
                    if (orthoState.isItemActivated)
                        s_CameraBefore = c;
                    if (orthoState.isItemEdited)
                        c.dirty = true;
                    if (orthoState.isItemDeactivatedAfterEdit)
                        CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<CameraComponent>>(m_Scene.get(), selectedEntity.GetUUID(), s_CameraBefore, c));
                }

                UI::State nearState = UI::DrawFloatControl("Near", &c.camera.nearPlane, 0.025f, 0.0f, FLT_MAX);
                if (nearState.isItemActivated)
                    s_CameraBefore = c;
                if (nearState.isItemEdited)
                    c.dirty = true;
                if (nearState.isItemDeactivatedAfterEdit)
                    CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<CameraComponent>>(m_Scene.get(), selectedEntity.GetUUID(), s_CameraBefore, c));

                UI::State farState = UI::DrawFloatControl("Far", &c.camera.farPlane, 0.025f, 0.0f, FLT_MAX);
                if (farState.isItemActivated)
                    s_CameraBefore = c;
                if (farState.isItemEdited)
                    c.dirty = true;
                if (farState.isItemDeactivatedAfterEdit)
                    CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<CameraComponent>>(m_Scene.get(), selectedEntity.GetUUID(), s_CameraBefore, c));

                {
                    CameraComponent before = c;
                    if (UI::DrawCheckbox("Primary", &c.primary).isItemEdited)
                    {
                        c.dirty = true;
                        CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<CameraComponent>>(m_Scene.get(), selectedEntity.GetUUID(), before, c));
                    }
                }

                ImGui::SeparatorText("Post Processing");
                auto &pp = c.camera.postProcessing;

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
                    c.dirty |= UI::DrawFloatControl("AO Bias", &pp.aoBias, 0.001f, 0.0f, 0.5f).isItemEdited;
                    c.dirty |= UI::DrawFloatControl("AO Intensity", &pp.aoIntensity, 0.05f, 0.0f, 5.0f).isItemEdited;
                    c.dirty |= UI::DrawFloatControl("AO Power", &pp.aoPower, 0.05f, 0.0f, 5.0f).isItemEdited;
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
                    c.bodyType = static_cast<Rigidbody2DComponent::EBodyType>(bodyTypeIndex);
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

                std::array<const char *, 3> bodyTypeLabels = { "Static", "Kinematic", "Dynamic" };
                int bodyTypeIndex = static_cast<int>(c.bodyType);

                if (UI::DrawComboBox("Body Type", bodyTypeLabels.data(), static_cast<int>(bodyTypeLabels.size()), &bodyTypeIndex))
                {
                    c.bodyType = static_cast<RigidbodyComponent::EBodyType>(bodyTypeIndex);
                }
            });

            RenderComponent<BoxColliderComponent>("Box Collider", selectedEntity, [&]()
            {
                auto &c = selectedEntity.GetComponent<BoxColliderComponent>();
                c.dirty = UI::DrawVec3Control("Center", c.center, 0.025f, 0.0f);
                c.dirty |= UI::DrawVec3Control("Scale", c.scale, 0.025f, 1.0f);
                c.dirty |= UI::DrawFloatControl("Friction", &c.friction, 0.025f);
                c.dirty |= UI::DrawFloatControl("Static Friction", &c.staticFriction, 0.025f);
                c.dirty |= UI::DrawFloatControl("Restitution", &c.restitution, 0.025f);
                c.dirty |= UI::DrawFloatControl("Density", &c.density, 0.025f);

                // TODO: Collider Edit from UI (TOGGLES)
            });

            RenderComponent<SphereColliderComponent>("Sphere Collider", selectedEntity, [&]()
            {
                auto &c = selectedEntity.GetComponent<SphereColliderComponent>();
                c.dirty = UI::DrawVec3Control("Center", c.center, 0.025f, 0.0f);
                c.dirty |= UI::DrawFloatControl("Radius", &c.radius, 0.025f, 0.01f, 10000.0f, 1.0f);
                c.dirty |= UI::DrawFloatControl("Friction", &c.friction, 0.025f);
                c.dirty |= UI::DrawFloatControl("Static Friction", &c.staticFriction, 0.025f);
                c.dirty |= UI::DrawFloatControl("Restitution", &c.restitution, 0.025f);
                c.dirty |= UI::DrawFloatControl("Density", &c.density, 0.025f);

                // TODO: Collider Edit from UI (TOGGLES)
            });

            RenderComponent<CapsuleColliderComponent>("Capsule Collider", selectedEntity, [&]()
            {
                auto &c = selectedEntity.GetComponent<CapsuleColliderComponent>();
                c.dirty = UI::DrawVec3Control("Center", c.center, 0.025f, 0.0f);
                c.dirty |= UI::DrawFloatControl("Radius", &c.radius, 0.025f, 0.01f, 10000.0f, 1.0f);
                c.dirty |= UI::DrawFloatControl("Height", &c.height, 0.025f, 0.01f, 10000.0f, 1.0f);
                c.dirty |= UI::DrawFloatControl("Friction", &c.friction, 0.025f);
                c.dirty |= UI::DrawFloatControl("Static Friction", &c.staticFriction, 0.025f);
                c.dirty |= UI::DrawFloatControl("Restitution", &c.restitution, 0.025f);
                c.dirty |= UI::DrawFloatControl("Density", &c.density, 0.025f);

                // TODO: Collider Edit from UI (TOGGLES)
            });

            RenderComponent<MeshColliderComponent>("Mesh Collider", selectedEntity, [&]()
            {
                auto &c = selectedEntity.GetComponent<MeshColliderComponent>();
                c.dirty = UI::DrawCheckbox("Convex", &c.convex);
                ImGui::Text("Vertices: %zu", c.vertices.size());
                ImGui::Text("Indices: %zu", c.indices.size());
                c.dirty |= UI::DrawFloatControl("Friction", &c.friction, 0.025f);
                c.dirty |= UI::DrawFloatControl("Static Friction", &c.staticFriction, 0.025f);
                c.dirty |= UI::DrawFloatControl("Restitution", &c.restitution, 0.025f);
                c.dirty |= UI::DrawFloatControl("Density", &c.density, 0.025f);
                if (ImGui::Button("Clear Mesh Data"))
                {
                    c.dirty = false;
                    c.vertices.clear();
                    c.indices.clear();
                }
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
                                    if (m_EditorLayer->GetActiveProject()->GetAssetManager()->GetAssetType(handle) == AssetType::Font)
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
                    UI::DrawButtonWithColumn("Material", materialLabel.c_str(), nullptr, [&c, this, &isMaterialLoaded]()
                        {
                            if (ImGui::BeginDragDropTarget())
                            {
                                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                                {
                                    LOG_ASSERT(payload->DataSize == sizeof(AssetHandle), "WRONG ITEM, that should be an asset handle");
                                    AssetHandle handle = *static_cast<AssetHandle *>(payload->Data);
                                    if (m_EditorLayer->GetActiveProject()->GetAssetManager()->GetAssetType(handle) == AssetType::Material2D)
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

                UI::DrawButtonWithColumn("Audio", label.c_str(), nullptr, [&c, this, &isLoaded]()
                    {
                        if (ImGui::BeginDragDropTarget())
                        {
                            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                            {
                                if (payload->DataSize == sizeof(AssetHandle))
                                {
                                    AssetHandle *handle = static_cast<AssetHandle *>(payload->Data);
                                    if (handle && *handle != AssetHandle(0))
                                    {
                                        AssetMetaData metadata = m_EditorLayer->GetActiveProject()->GetAssetManager()->GetMetaData(*handle);
                                        if (metadata.type == AssetType::Audio)
                                        {
                                            c.handle = *handle;
                                            Ref<FmodSound> sound = m_EditorLayer->GetActiveProject()->GetAsset<FmodSound>(*handle);
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
                    if (Ref<FmodSound> sound = m_EditorLayer->GetActiveProject()->GetAsset<FmodSound>(c.handle))
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
                        auto assetManager = m_EditorLayer->GetActiveProject()->GetAssetManager();
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
                                        if (UI::DrawFloatControl(name.c_str(), &data, 0.1f, -FLT_MAX, FLT_MAX))
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
                                        if (UI::DrawIntControl(name.c_str(), &data, 1.0f, INT_MIN, INT_MAX))
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
                                        if (UI::DrawIntControl(name.c_str(), &data, 1.0f, 0, INT_MAX))
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
                                        if (UI::DrawIntControl(name.c_str(), &data, 1.0f, 0, 255))
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
                                        if (UI::DrawIntControl(name.c_str(), &data, 1.0f, -128, 127))
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
                                        if (UI::DrawIntControl(name.c_str(), &data, 1.0f, SHRT_MIN, SHRT_MAX))
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
                                        if (UI::DrawIntControl(name.c_str(), &data, 1.0f, 0, USHRT_MAX))
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
                                        if (UI::DrawIntControl(name.c_str(), &data, 1.0f, INT_MIN, INT_MAX))
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
                                        if (UI::DrawIntControl(name.c_str(), &data, 1.0f, 0, INT_MAX))
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
                                        if (UI::DrawFloatControl(name.c_str(), &data, 0.1f, -FLT_MAX, FLT_MAX))
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
                                        if (UI::DrawVec2Control(name.c_str(), data, 0.1f))
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
                                        if (UI::DrawVec3Control(name.c_str(), data, 0.1f))
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
                                        if (UI::DrawVec4Control(name.c_str(), data, 0.1f))
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
                                        if (UI::DrawVec4Control(name.c_str(), vec, 0.1f))
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
                                                Entity e = SceneManager::GetEntity(m_Scene.get(), UUID(handle));
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
                                                        LOG_ASSERT(payload->DataSize == sizeof(Entity), "WRONG ENTITY ITEM");
                                                        if (payload->DataSize == sizeof(Entity))
                                                        {
                                                            Entity src{ *static_cast<entt::entity *>(payload->Data), m_Scene.get() };
                                                            uint64_t id = (uint64_t)src.GetUUID();
                                                            dummy.SetValue<uint64_t>(id);
                                                            (*classRegisteredInstanceField)[name] = dummy;
                                                            if (c.runtimeScriptInstance)
                                                                c.runtimeScriptInstance->SetFieldValue<uint64_t>(name, id);
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
                                                        Entity e = SceneManager::GetEntity(m_Scene.get(), UUID(handle));
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

            if (ImGui::BeginPopup("##add_component_context", ImGuiWindowFlags_NoDecoration))
            {
                static char buffer[256] = { 0 };
                static std::string compNameFilterResultStr;
                static std::set<std::pair<std::string, CompType>> filteredCompName;

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
                    case CompType_MeshCollider:
                        entity.AddComponent<MeshColliderComponent>();
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
                            addCompFunc(Entity{ selectedEntity, m_Scene.get()}, type);
                            ImGui::CloseCurrentPopup();
                        }
                    }
                }

                for (const auto &[strName, type] : filteredCompName)
                {
                    if (ImGui::Selectable(strName.c_str()))
                    {
                        addCompFunc(Entity{ selectedEntity, m_Scene.get() }, type);
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
				const ESceneState sceneState = m_EditorLayer->GetState().sceneState;
				if (sceneState == ESceneState::Play)
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

				// Calculating Scene Viewport location
				const ImVec2 &canvasPos = ImGui::GetCursorScreenPos();
				const ImVec2 &canvasSize = ImGui::GetContentRegionAvail();

				globals::GEditor::EditorViewport.min = { canvasPos.x, canvasPos.y };
				globals::GEditor::EditorViewport.max = { canvasSize.x, canvasSize.y };

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

				Entity clickedIconEntity = {};

				// Draw editor icons (cameras, directional lights, etc.)
				if (m_EditorLayer->GetState().sceneState != ESceneState::Play)
                {
					glm::mat4 viewProjection = editorViewCamera->GetProjection() * editorViewCamera->GetView();
					Rect viewportRect = { globals::GEditor::EditorViewport.min, globals::GEditor::EditorViewport.min + globals::GEditor::EditorViewport.max };

					// Camera icons & frustum outlines
					auto cameraViewReg = m_Scene->registry->view<TransformComponent, CameraComponent>();
					for (entt::entity e : cameraViewReg)
					{
						auto &tr = m_Scene->registry->get<TransformComponent>(e);
						if (!tr.visible)
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
										clickedIconEntity = Entity{ e, m_Scene.get() };
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
					auto dirLightReg = m_Scene->registry->view<TransformComponent, DirectionalLightComponent>();
					for (entt::entity e : dirLightReg)
					{
						auto &tr = m_Scene->registry->get<TransformComponent>(e);
						if (!tr.visible)
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
										clickedIconEntity = Entity{ e, m_Scene.get() };
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
					const float padding = 18.0f;
					float yPosition = 6.0f;
					const float fps = ImGui::GetIO().Framerate;
					std::string statusStr = std::format("FPS {:.5} {:.3}ms", fps, 1000.0f / fps);
					drawList->AddText(ImVec2(canvasPos.x + 6, canvasPos.y + 6), 0xFFFFFFFF, statusStr.c_str());

					yPosition += padding;
					statusStr = std::format("Response Time {:.3} ms", 1000.0f / fps);
					drawList->AddText(ImVec2(canvasPos.x + 6, canvasPos.y + yPosition), 0xFFFFFFFF, statusStr.c_str());
				}

				// Mouse picking from viewport object-id attachment (on mouse down only)
				if (m_EditorLayer->GetState().sceneState != ESceneState::Play)
				{
					if (activeSceneRenderer)
					{
						const uint32_t localMouseX = static_cast<uint32_t>(std::max(m_ViewportData.mousePos.x, 0.0f));
						const uint32_t localMouseY = static_cast<uint32_t>(std::max(m_ViewportData.mousePos.y, 0.0f));
						activeSceneRenderer->SetEditorWidgetMousePosition(localMouseX, localMouseY, imageHovered);
					}					if (imageHovered && (mouseDown || mouseDoubleDown) && !m_Gizmo.IsManipulating() && !m_Gizmo.IsHovered() && !m_Data.is2DBoundsHovered)
					{
						if (clickedIconEntity.IsValid())
						{
							Entity targetSelection = clickedIconEntity;
							if (!mouseDoubleDown && !ImGui::IsKeyDown(ImGuiKey_LeftShift))
							{
								const auto parent = clickedIconEntity.GetParentUUID();
								if (parent != UUID(0))
								{
									if (Entity parentEntity = SceneManager::GetEntity(m_Scene.get(), parent); parentEntity.IsValid())
									{
										targetSelection = parentEntity;
									}
								}
							}
							SetSelectedEntity(targetSelection);
						}
						else
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
													pickedEntity = Entity{ e, m_Scene.get() };
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
													if (Entity parentEntity = SceneManager::GetEntity(m_Scene.get(), parent); parentEntity.IsValid())
													{
														targetSelection = parentEntity;
													}
												}
											}

											SetSelectedEntity(targetSelection);
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

					if (ImGui::BeginDragDropTarget())
					{
						if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
						{
							if (payload->DataSize == sizeof(AssetHandle))
							{
								auto *handle = static_cast<AssetHandle *>(payload->Data);
								if (handle && *handle != AssetHandle(0))
								{
									AssetMetaData metadata = m_EditorLayer->GetActiveProject()->GetAssetManager()->GetMetaData(*handle);
									if (metadata.type == AssetType::Scene)
									{
										const auto &filepath = m_EditorLayer->GetActiveProject()->GetProjectFilepath(metadata.filepath);
										m_EditorLayer->OpenScene(filepath);
									}
								}
							}
						}

						ImGui::EndDragDropTarget();
					}

                    auto view = m_EditorCamera.GetView();
                    auto &projection = m_EditorCamera.GetProjection();

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
                                    Entity parent = SceneManager::GetEntity(m_Scene.get(), entity.GetParentUUID());
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
                            }
                        }

                        // Commit commands when the multi-entity gizmo is released
                        if (!isManipulatingNow && wasManipulating)
                        {
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
                                CommandManager::AddCommand(CreateScope<ComponentPropertyBatchCommand<TransformComponent>>(m_Scene.get(), std::move(entries)));
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
                                    Entity parent = SceneManager::GetEntity(m_Scene.get(), entity.GetParentUUID());
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
                            }

                            // Commit a single command when the gizmo is released (single entity)
                            if (!isManipulatingNow && wasManipulating)
                            {
                                if (auto it = initialTransforms.find(entity.GetUUID()); it != initialTransforms.end())
                                {
                                    CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<TransformComponent>>(m_Scene.get(), entity.GetUUID(), it->second, entity.GetTransform()));
                                }
                            }
                        }
                    }
                }
            }
            else
            {
				ImGui::Text("No Scene");
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
                            const float padding = 18.0f;
                            float yPosition = 6.0f;
                            const float fps = ImGui::GetIO().Framerate;
                            std::string statusStr = std::format("FPS {:.5} {:.3}ms", fps, 1000.0f / fps);
                            drawList->AddText(ImVec2(canvasPos.x + 6, canvasPos.y + 6), 0xFFFFFFFF, statusStr.c_str());

                            yPosition += padding;
                            statusStr = std::format("Viewport x: {} y: {} w: {} h: {}", baseImagePos.x, baseImagePos.y,
                                baseImagePos.x + baseImageSize.x, baseImagePos.y + baseImageSize.y);
                            
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
            const auto mode = cameraModeIndex == 0 ? EditorCamera::NavigationMode::Orbit : (cameraModeIndex == 1 ? EditorCamera::NavigationMode::Fly : EditorCamera::NavigationMode::Mode2D);
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

        auto drawGizmoBtn = [&](const std::string &iconName, bool active)
        {
            ImTextureID texID = (ImTextureID)m_Icons[iconName]->GetHandle().Get();
            ImVec4 tint = active ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f) : ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
            ImVec4 bg = active ? ImVec4(1.0f, 0.78f, 0.0f, 1.0f) : ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
            return ImGui::ImageButton(iconName.c_str(), texID, buttonSize, ImVec2(0, 0), ImVec2(1, 1), bg, tint);
        };

        if (drawGizmoBtn("picking", m_Data.gizmoOp == GizmoOperation::NONE)) SetGizmoOperation(GizmoOperation::NONE);
        ImGui::SameLine();
        if (drawGizmoBtn("translate", m_Data.gizmoOp == GizmoOperation::TRANSLATE)) SetGizmoOperation(GizmoOperation::TRANSLATE);
        ImGui::SameLine();
        if (drawGizmoBtn("rotate", m_Data.gizmoOp == GizmoOperation::ROTATE)) SetGizmoOperation(GizmoOperation::ROTATE);
        ImGui::SameLine();
        if (drawGizmoBtn("scale", m_Data.gizmoOp == GizmoOperation::SCALE)) SetGizmoOperation(GizmoOperation::SCALE);

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

        bool isLocal = m_Gizmo.GetMode() == ImGuizmo::LOCAL;
        if (drawGizmoBtn("transform_local", isLocal)) m_Gizmo.SetMode(ImGuizmo::LOCAL);
        ImGui::SameLine();
        if (drawGizmoBtn("transform_world", !isLocal)) m_Gizmo.SetMode(ImGuizmo::WORLD);

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

        ESceneState sceneState = m_EditorLayer->GetState().sceneState;
        const bool isScenePlaying = sceneState == ESceneState::Play;
        Ref<Texture> scenePlayStopTex = isScenePlaying ? m_Icons["stop"] : m_Icons["play"];
        ImTextureID scenePlayStopID = (ImTextureID)scenePlayStopTex->GetHandle().Get();

        ImGui::SameLine();
        ImVec4 bgColPlay = isScenePlaying ? ImVec4(0.3f, 0.3f, 0.3f, 1.0f) : ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        if (ImGui::ImageButton("##PlayButton", scenePlayStopID, buttonSize, ImVec2(0, 0), ImVec2(1, 1), bgColPlay))
        {
            if (isScenePlaying)
            {
                Application::SubmitToMainThread([this]() { m_EditorLayer->OnSceneStop(); });
#if _WIN32
                HWND hwnd = Application::GetInstance()->GetWindow()->GetNativeWindow();
                COLORREF rgbRed = 0x00E86071;
                DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &rgbRed, sizeof(rgbRed));
#endif
            }
            else
            {
				Application::SubmitToMainThread([this]() { m_EditorLayer->OnScenePlay(); });
#if _WIN32
                HWND hwnd = Application::GetInstance()->GetWindow()->GetNativeWindow();
                COLORREF rgbRed = 0x000000AB;
                DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &rgbRed, sizeof(rgbRed));
#endif
            }
        }

        const bool isSceneSimulate = sceneState == ESceneState::Simulate;
        Ref<Texture> sceneSimulateTex = isSceneSimulate ? m_Icons["stop"] : m_Icons["simulate"];
        ImTextureID sceneSimulateID = (ImTextureID)sceneSimulateTex->GetHandle().Get();

        ImGui::SameLine();
        ImVec4 bgColSim = isSceneSimulate ? ImVec4(0.3f, 0.3f, 0.3f, 1.0f) : ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        if (ImGui::ImageButton("##SimulateButton", sceneSimulateID, buttonSize, ImVec2(0, 0), ImVec2(1, 1), bgColSim))
        {
            if (isSceneSimulate)
            {
				Application::SubmitToMainThread([this]() { m_EditorLayer->OnSceneStop(); });
#if _WIN32
                HWND hwnd = Application::GetInstance()->GetWindow()->GetNativeWindow();
                COLORREF rgbRed = 0x00E86071;
                DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &rgbRed, sizeof(rgbRed));
#endif
            }
            else
            {
				Application::SubmitToMainThread([this]() { m_EditorLayer->OnSceneSimulate(); });
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

            Entity resizedEntity = SceneManager::GetEntity(m_Scene.get(), m_Data.active2DEntity);
            if (resizedEntity.IsValid())
            {
                CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<TransformComponent>>(
                    m_Scene.get(), resizedEntity.GetUUID(), m_Data.before2DResize, resizedEntity.GetTransform()));
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
                        Entity parent = SceneManager::GetEntity(m_Scene.get(), entity.GetParentUUID());
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

        dispatcher.Dispatch<MouseScrolledEvent>(BIND_CLASS_EVENT_FN(ScenePanel::OnMouseScrolledEvent));
        dispatcher.Dispatch<MouseMovedEvent>(BIND_CLASS_EVENT_FN(ScenePanel::OnMouseMovedEvent));
        dispatcher.Dispatch<JoystickConnectionEvent>(BIND_CLASS_EVENT_FN(ScenePanel::OnJoystickConnectionEvent));
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
        CommandManager::ExecuteCommand(CreateScope<EntityDestroyCommand>(m_Scene.get(), entity));
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
                SceneManager::DuplicateEntity(m_Scene.get(), entity);
            }
        }
    }
}
