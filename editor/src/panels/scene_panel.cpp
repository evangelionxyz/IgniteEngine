//Copyright (c) 2026 Evangelion Manuhutu

#include "scene_panel.hpp"
#include "editor_layer.hpp"
#include "ignite/audio/fmod_sound.hpp"
#include "ignite/core/application.hpp"
#include "ignite/core/input/event.hpp"
#include "ignite/core/input/key_event.hpp"
#include "ignite/core/input/mouse_event.hpp"
#include "ignite/core/input/joystick_event.hpp"
#include "ignite/graphics/texture.hpp"
#include "ignite/scene/icomponent.hpp"
#include "ignite/core/platform_utils.hpp"
#include "ignite/graphics/ui_renderer.hpp"
#include "ignite/graphics/objects/mesh.hpp"
#include "ignite/graphics/objects/material_2d.hpp"
#include "ignite/graphics/font.hpp"
#include "ignite/math/math.hpp"
#include "ignite/scripting/script_engine.hpp"
#include "ignite/scripting/script_field.hpp"
#include "ignite/scripting/script_instance.hpp"
#include "ignite/asset/asset_importer.hpp"
#include "ignite/core/profiler/profiler.hpp"
#include "ignite/scene/entity.hpp"
#include "ignite/scene/sprite_sheet.hpp"
#include "ignite/scene/entity_destroy_command.hpp"
#include "ignite/scene/entity_rename_command.hpp"
#include "ignite/scene/entity_reparent_command.hpp"
#include "ignite/scene/component_property_command.hpp"

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

#ifdef _WIN32
    #include <dwmapi.h>
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

        auto width = static_cast<float>(app->GetCreateInfo().width);
        auto height = static_cast<float>(app->GetCreateInfo().height);

        m_EditorCamera = EditorCamera("ScenePanel-Editor Camera");

		m_EditorCamera.SetTarget(glm::vec3(0.0f));
		m_EditorCamera.SetDistance(5.5f);
		m_EditorCamera.yaw = glm::radians(90.0f);
		m_EditorCamera.pitch = 0.0f;

		m_EditorCamera.UpdateSphericalPosition();
        m_EditorCamera.UpdateView();
		m_EditorCamera.UpdateProjection(width, height);
        
        m_EditorCamera2D = m_EditorCamera;
        m_EditorCamera3D = m_EditorCamera;
        m_EditorCamera2D->SetNavigationMode(EditorCamera::NavigationMode::Mode2D);
        m_EditorCamera3D->SetNavigationMode(EditorCamera::NavigationMode::Orbit);

        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();
        nvrhi::CommandListHandle cmd = device->createCommandList();
        cmd->open();

        // Load icons
        TextureCreateInfo createInfo;
        createInfo.format = nvrhi::Format::RGBA8_UNORM;
    	
        createInfo.initialState = nvrhi::ResourceStates::ShaderResource;
    	createInfo.keepInitialState = true;

        m_Icons["simulate"] = Texture::Create("resources/ui/ic_simulate.png", createInfo, cmd);
        m_Icons["play"] = Texture::Create("resources/ui/ic_play.png", createInfo, cmd);
        m_Icons["stop"] = Texture::Create("resources/ui/ic_stop.png", createInfo, cmd);
        m_Icons["pause"] = Texture::Create("resources/ui/ic_pause.png", createInfo, cmd);
        m_Icons["stepping"] = Texture::Create("resources/ui/ic_stepping.png", createInfo, cmd);
        m_Icons["checker128"] = Texture::Create("resources/ui/checker-128px.jpg", createInfo, cmd);

        cmd->close();
        device->executeCommandList(cmd);

        // Create scene render target
        RenderTargetCreateInfo rtCreateInfo = {};
        rtCreateInfo.attachments =
        {
            FramebufferAttachments{ "[Scene DepthAttachment]", nvrhi::Format::D32S8, nvrhi::ResourceStates::DepthWrite}, // Depth
            FramebufferAttachments{ "[Scene ColorAttachment]", nvrhi::Format::RGBA8_UNORM, nvrhi::ResourceStates::RenderTarget}, // Main Color
            FramebufferAttachments{ "[Scene ObjectIDAttachment]", nvrhi::Format::R32_UINT, nvrhi::ResourceStates::RenderTarget} // Object ID
        };

        // Edit RT
        m_ViewportEditRT.scene = RenderTarget::Create(rtCreateInfo, "[Edit Viewport Scene RT]");
        m_ViewportEditRT.ui = RenderTarget::Create(rtCreateInfo, "[Edit Viewport UI RT]");
        
        // Game RT
        m_ViewportGameRT.scene = RenderTarget::Create(rtCreateInfo, "[Game Viewport Scene RT]");
        m_ViewportGameRT.ui = RenderTarget::Create(rtCreateInfo, "[Game Viewport UI RT]");

        // Composite render target
        {
            RenderTargetCreateInfo rtCreateInfo = {};
            rtCreateInfo.attachments =
            {
                //FramebufferAttachments{ "[Composite Depth Attachment]", nvrhi::Format::D32S8, nvrhi::ResourceStates::DepthWrite }, // Depth
                FramebufferAttachments{ "[Composite Color Attachment]", nvrhi::Format::RGBA8_UNORM, nvrhi::ResourceStates::RenderTarget} // Main Color
            };

            // Edit RT
            m_ViewportEditRT.composite = RenderTarget::Create(rtCreateInfo, "[Edit Viewport Composite RT]");
            
            // Game RT
            m_ViewportGameRT.composite = RenderTarget::Create(rtCreateInfo, "[Game Viewport Composite RT]");
        }
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
        ImGui::ShowDemoWindow();

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
        if (m_Scene && m_EditorLayer->GetData().sceneState != State::ScenePlay)
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
                IDComponent &idComp = src.GetComponent<IDComponent>();
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

        ImGuiTableFlags tableFlags = 
            ImGuiTableFlags_RowBg 
            | ImGuiTableFlags_NoClip 
            | ImGuiTableFlags_PadOuterX | ImGuiTableFlags_NoPadInnerX | ImGuiTableFlags_NoPadOuterX 
            | ImGuiTableFlags_NoBordersInBodyUntilResize 
            | ImGuiTableFlags_Resizable;

        if (ImGui::BeginTable("entity_hierarchy_table", 1, tableFlags))
        {
            // setup table 3 columns
            // Name, Type, Active (check box)
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            // ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_NoResize);
            // ImGui::TableSetupColumn("Active", ImGuiTableColumnFlags_WidthFixed, 24.0f);
            ImGui::TableHeadersRow();

            ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, { 0.000f, 0.245f, 0.409f, 1.000f });
            ImGui::PushStyleColor(ImGuiCol_FrameBg, { 0.000f, 0.000f, 0.000f, 0.620f });
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, { 0.000f, 0.243f, 0.408f, 1.000f });
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 2.0f, 0.0f });

            std::vector<Entity> rootEntities;
            m_Scene->registry->view<IDComponent>().each([&](const entt::entity e, const IDComponent &id)
            {
                if (id.parent == UUID(0))
                {
                    rootEntities.emplace_back(e, m_Scene.get());
                }
            });

            std::ranges::sort(rootEntities, [](const Entity &a, const Entity &b)
            {
                if (a.GetName() == b.GetName())
                {
                    return static_cast<u64>(a) < static_cast<u64>(b);
                }
                return a.GetName() < b.GetName();
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
            return;

        static UUID s_LastAutoScrolledTarget = UUID(0);
        if (m_TrackingSelectedEntity == UUID(0))
        {
            s_LastAutoScrolledTarget = UUID(0);
        }

        IDComponent &idComp = entity.GetComponent<IDComponent>();
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
            // Main Component
            // ID Component
            IDComponent &idComp = selectedEntity.GetComponent<IDComponent>();
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

                UI::State translationState = UI::DrawVec3Control("Translation", comp.localTranslation, 0.025f);
                if (translationState.isItemActivated)            s_TransformBefore = comp;
                if (translationState.isItemEdited)               comp.dirty = true;
                if (translationState.isItemDeactivatedAfterEdit) CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<TransformComponent>>(m_Scene.get(), selectedEntity.GetUUID(), s_TransformBefore, comp));

                glm::vec3 euler = eulerAngles(comp.localRotation);
                UI::State rotationState = UI::DrawVec3Control("Rotation", euler, 0.025f);
                if (rotationState.isItemActivated)            s_TransformBefore = comp;
                if (rotationState.isItemEdited)               { comp.localRotation = glm::quat(euler); comp.dirty = true; }
                if (rotationState.isItemDeactivatedAfterEdit) CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<TransformComponent>>(m_Scene.get(), selectedEntity.GetUUID(), s_TransformBefore, comp));

                UI::State scaleState = UI::DrawVec3Control("Scale", comp.localScale, 0.025f, 1.0f);
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

            RenderComponent<WorldEnvironment>("World Environment", selectedEntity, [&]()
            {
                WorldEnvironment &c = selectedEntity.GetComponent<WorldEnvironment>();

                UI::DrawCheckbox("Primary", &c.primary);
                UI::DrawCheckbox("Enabled", &c.enabled);

                const bool hasHDR = c.hdrHandle != AssetHandle(0);
                std::string buttonLabel = hasHDR ? "HDR Loaded" : "Drag Here";
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
									c.loadedHDRHandle = AssetHandle(0);
									c.dirtyEnvironment = true;
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
								c.loadedHDRHandle = AssetHandle(0);
								c.dirtyEnvironment = true;
							}
						}
                    });
                

                ImGui::Separator();
                ImGui::ColorEdit4("Sun Color", &c.sceneGPUData.sunColor.x);
                UI::DrawVec2Control("Sun Angles", c.sceneGPUData.sungAngles, 0.01f);
                UI::DrawFloatControl("Sun Angular Radius", &c.sceneGPUData.sunAngularRadius, 0.01f, 0.0f, 10.0f);
                UI::DrawIntControl("Render Mode", &c.sceneGPUData.renderMode, 1.0f, 0, 16);
                bool debugShadow = c.sceneGPUData.debugShadow != 0;
                if (UI::DrawCheckbox("Debug Shadow", &debugShadow).isItemEdited)
                {
                    c.sceneGPUData.debugShadow = debugShadow ? 1 : 0;
                }
                UI::DrawFloatControl("Exposure", &c.sceneGPUData.exposure, 0.01f, 0.0f, 32.0f);
                UI::DrawFloatControl("Gamma", &c.sceneGPUData.gamma, 0.01f, 0.1f, 8.0f);
                UI::DrawFloatControl("Ambient", &c.sceneGPUData.ambient, 0.01f, 0.0f, 4.0f);
            });

            RenderComponent<Sprite2DComponent>("Sprite 2D", selectedEntity, [&]()
            {
                auto &c = selectedEntity.GetComponent<Sprite2DComponent>();

				static Sprite2DComponent s_Sprite2DBefore;

                // Material 2D
                bool isMat2dLoaded = c.materialHandle != AssetHandle(0);
                std::string mat2dLabel = isMat2dLoaded ? std::to_string(c.materialHandle) : "Drag Here";
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
                    m_EditorLayer->GetActiveProject()->GetAsset<Material2D>(c.materialHandle, AssetType::Material2D);
                }

                if (!isMat2dLoaded)
                {
					// Texture on sprite 2d
					const bool isTextureLoaded = c.handle != AssetHandle(0);
					const std::string textureLabel = isTextureLoaded ? std::to_string(c.handle) : "Drag Here";
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
										CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<Sprite2DComponent>>(m_Scene.get(),
                                            selectedEntity.GetUUID(), before, c));
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
                    if (tilingState.isItemDeactivatedAfterEdit) CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<Sprite2DComponent>>(m_Scene.get(), selectedEntity.GetUUID(), s_Sprite2DBefore, c));

                    auto colorState = UI::DrawColorVec4("Color", c.color);
                    if (colorState.isItemActivated) s_Sprite2DBefore = c;
                    if (colorState.isItemDeactivatedAfterEdit) CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<Sprite2DComponent>>(m_Scene.get(), selectedEntity.GetUUID(), s_Sprite2DBefore, c));
                }

                UI::State flipXState = UI::DrawCheckbox("Flip X", &c.flipX);
				if (flipXState.isItemActivated)            s_Sprite2DBefore = c;
				if (flipXState.isItemDeactivatedAfterEdit) CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<Sprite2DComponent>>(m_Scene.get(), selectedEntity.GetUUID(), s_Sprite2DBefore, c));

                UI::State flipYState = UI::DrawCheckbox("Flip Y", &c.flipY);
				if (flipYState.isItemActivated)            s_Sprite2DBefore = c;
				if (flipYState.isItemDeactivatedAfterEdit) CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<Sprite2DComponent>>(m_Scene.get(), selectedEntity.GetUUID(), s_Sprite2DBefore, c));

                UI::State uv0State = UI::DrawVec2Control("UV0", c.uv0, 0.001f);
                if (uv0State.isItemActivated)            s_Sprite2DBefore = c;
                if (uv0State.isItemDeactivatedAfterEdit) CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<Sprite2DComponent>>(m_Scene.get(), selectedEntity.GetUUID(), s_Sprite2DBefore, c));

                UI::State uv1State = UI::DrawVec2Control("UV1", c.uv1, 0.001f);
                if (uv1State.isItemActivated)            s_Sprite2DBefore = c;
                if (uv1State.isItemDeactivatedAfterEdit) CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<Sprite2DComponent>>(m_Scene.get(), selectedEntity.GetUUID(), s_Sprite2DBefore, c));
            });

            RenderComponent<Animator2DComponent>("Animator 2D", selectedEntity, [&]()
            {
                auto &c = selectedEntity.GetComponent<Animator2DComponent>();

                bool isAnimatorLoaded = c.controllerHandle != AssetHandle(0);
                std::string animDropLabel = !isAnimatorLoaded ? "Drop Here" : std::to_string(c.controllerHandle);
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
                    if (animCtrl)
                    {
                        if (ImGui::BeginCombo("Current State", c.currentStateName.c_str()))
                        {
                            for (size_t i = 0; i < animCtrl->states.size(); ++i)
                            {
                                bool isSelected = strcmp(animCtrl->states[i].name.c_str(), c.currentStateName.c_str()) == 0;
                                if (ImGui::Selectable(animCtrl->states[i].name.c_str(), isSelected))
                                {
                                    c.currentStateName = animCtrl->states[i].name;
                                }

                                if (isSelected)
                                {
                                    ImGui::SetItemDefaultFocus();
                                }
                            }
                            ImGui::EndCombo();
                        }

                    }

                }
            });

            RenderComponent<PointLight2DComponent>("Point Light 2D", selectedEntity, [&]()
            {
                PointLight2DComponent &c = selectedEntity.GetComponent<PointLight2DComponent>();
                UI::DrawCheckbox("Enabled", &c.enabled);
                UI::DrawVec4Control("Color", c.color, 0.025f, 1.0f);
                UI::DrawFloatControl("Radius", &c.radius, 0.025f, 0.0f, 10000.0f);
                UI::DrawFloatControl("Intensity", &c.intensity, 0.025f, 0.0f, 10000.0f);
            });

			RenderComponent<Circle2DComponent>("Circle 2D", selectedEntity, [&]()
				{
					Circle2DComponent &c = selectedEntity.GetComponent<Circle2DComponent>();

					static Circle2DComponent compBefore;

                    UI::State colorState = UI::DrawVec4Control("Color", c.color, 0.025f, 1.0f);
                    if (colorState.isItemActivated)
						compBefore = c;

                    if (colorState.isItemDeactivatedAfterEdit)
						CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<Circle2DComponent>>(m_Scene.get(), selectedEntity.GetUUID(), compBefore, c));
				});

			RenderComponent<StaticMeshComponent>("Static Mesh", selectedEntity, [&]()
			{
				StaticMeshComponent &c = selectedEntity.GetComponent<StaticMeshComponent>();

				bool isMeshLoaded = c.handle != AssetHandle(0);

				std::string buttonLabel = isMeshLoaded ? "Loaded" : "Drag Mesh Here";
                UI::DrawButtonWithColumn("Mesh Asset", buttonLabel.c_str(), nullptr, [&c, this, &isMeshLoaded]()
                    {
                        if (ImGui::BeginDragDropTarget())
                        {
                            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                            {
                                LOG_ASSERT(payload->DataSize == sizeof(AssetHandle), "WRONG ITEM, that should be an asset handle");
                                AssetHandle handle = *static_cast<AssetHandle *>(payload->Data);
                                auto assetManager = m_EditorLayer->GetActiveProject()->GetAssetManager();
                                AssetMetaData metadata = assetManager->GetMetaData(handle);

                                if (metadata.type == AssetType::GLTF)
                                {
                                    metadata.type = AssetType::StaticMesh;
                                    assetManager->AssignMetaData(handle, metadata);
                                    assetManager->UnloadAsset(handle);
                                }

                                if (assetManager->GetAssetType(handle) == AssetType::StaticMesh)
                                {
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
                    ImGui::Indent(8.0f);
                    ImGui::TextDisabled("Handle: %llu", static_cast<u64>(c.handle));
                    ImGui::Unindent(8.0f);

                    Ref<StaticMesh> sm = m_EditorLayer->GetActiveProject()->GetAsset<StaticMesh>(c.handle);
                }
            });

         RenderComponent<SkeletalMeshComponent>("Skeletal Mesh", selectedEntity, [&]()
			{
				SkeletalMeshComponent &c = selectedEntity.GetComponent<SkeletalMeshComponent>();

				bool isMeshLoaded = c.handle != AssetHandle(0);

				std::string buttonLabel = isMeshLoaded ? "Loaded" : "Drag Mesh Here";
                UI::DrawButtonWithColumn("Mesh Asset", buttonLabel.c_str(), nullptr, [&c, this, &isMeshLoaded]()
                    {
						if (ImGui::BeginDragDropTarget())
						{
							if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
							{
								LOG_ASSERT(payload->DataSize == sizeof(AssetHandle), "WRONG ITEM, that should be an asset handle");
								AssetHandle handle = *static_cast<AssetHandle *>(payload->Data);
								auto assetManager = m_EditorLayer->GetActiveProject()->GetAssetManager();
								AssetMetaData metadata = assetManager->GetMetaData(handle);

								if (metadata.type == AssetType::FBX)
								{
									metadata.type = AssetType::SkeletalMesh;
									assetManager->AssignMetaData(handle, metadata);
									assetManager->UnloadAsset(handle);
								}

								if (assetManager->GetAssetType(handle) == AssetType::SkeletalMesh)
								{
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
					ImGui::Indent(8.0f);
					ImGui::TextDisabled("Handle: %llu", static_cast<u64>(c.handle));
					ImGui::Unindent(8.0f);

                    Ref<SkeletalMesh> sm = m_EditorLayer->GetActiveProject()->GetAsset<SkeletalMesh>(c.handle);
					if (sm)
					{
                        UI::DrawCheckbox("Play Anim", &sm->isPlaying);
					}
				}
			});

			RenderComponent<Rigidbody2DComponent>("Rigid Body 2D", selectedEntity, [&]()
            {
                auto &c = selectedEntity.GetComponent<Rigidbody2DComponent>();

                std::array<const char *, 3> bodyTypeStr = { "Static", "Dynamic", "Kinematic" };
                const char *currentBodyType = bodyTypeStr[static_cast<i32>(c.type)];

                if (ImGui::BeginCombo("Body Type", currentBodyType))
                {
                    for (size_t i = 0; i < bodyTypeStr.size(); ++i)
                    {
                        bool isSelected = strcmp(bodyTypeStr[i], currentBodyType) == 0;
                        if (ImGui::Selectable(bodyTypeStr[i], isSelected))
                        {
                            c.type = static_cast<Body2DType>(i);
                        }

                        if (isSelected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
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
            RenderComponent<CameraComponent>("Camera", selectedEntity, [&]()
            {
                CameraComponent &c = selectedEntity.GetComponent<CameraComponent>();
                static CameraComponent s_CameraBefore;

                static const char *projectionTypeStr[] = { "Orthographic", "Perspective" };
                const char *currentProjectionTypeStr = projectionTypeStr[static_cast<int>(c.camera.projectionType)];

                if (ImGui::BeginCombo("Projection", currentProjectionTypeStr))
                {
                    for (size_t i = 0; i < std::size(projectionTypeStr); ++i)
                    {
                        bool isSelected = false;
                        CameraComponent before = c;
                        if (ImGui::Selectable(projectionTypeStr[i], &isSelected))
                        {
                            const auto w = static_cast<float>(m_Scene->GetViewportWidth());
                            const auto h = static_cast<float>(m_Scene->GetViewportHeight());
                            c.camera.projectionType = static_cast<ProjectionType>(i);
                            c.camera.UpdateView();
                            c.camera.UpdateProjection(w, h);
                            CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<CameraComponent>>(m_Scene.get(), selectedEntity.GetUUID(), before, c));
                        }

                        if (isSelected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }

                static const char *aspectRatioLabels[] = { "Free", "16:9", "16:10", "4:3", "21:9", "1:1" };
                int aspectRatioIndex = static_cast<int>(c.camera.GetAspectRatioPreset());
                if (ImGui::BeginCombo("Aspect Ratio", aspectRatioLabels[aspectRatioIndex]))
                {
                    for (int i = 0; i < IM_ARRAYSIZE(aspectRatioLabels); ++i)
                    {
                        const bool isSelected = (aspectRatioIndex == i);
                        CameraComponent before = c;
                        if (ImGui::Selectable(aspectRatioLabels[i], isSelected))
                        {
                            c.camera.SetAspectRatioPreset(static_cast<SceneCamera::AspectRatioPreset>(i));
                            c.dirty = true;
                            CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<CameraComponent>>(m_Scene.get(), selectedEntity.GetUUID(), before, c));
                        }

                        if (isSelected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
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

                if (c.dirty)
                {
                    c.camera.UpdateView();
                    c.camera.UpdateProjection(static_cast<float>(m_Scene->GetViewportWidth()), static_cast<float>(m_Scene->GetViewportHeight()));
                    c.dirty = false;
                }
            });

            RenderComponent<BoxCollider2DComponent>("Box Collider 2D", selectedEntity, [&]()
            {
                BoxCollider2DComponent &c = selectedEntity.GetComponent<BoxCollider2DComponent>();
                c.dirty = UI::DrawVec2Control("Size", c.size, 0.025f, 1.0f);
                c.dirty |= UI::DrawVec2Control("Offset", c.offset, 0.025f);
                c.dirty |= UI::DrawFloatControl("Restitution", &c.restitution, 0.025f, 0.0f, FLT_MAX);
                c.dirty |= UI::DrawFloatControl("Friction", &c.friction, 0.025f, 0.0f, FLT_MAX);
                c.dirty |= UI::DrawFloatControl("Density", &c.density, 0.025f);
                c.dirty |= UI::DrawCheckbox("Is Sensor", &c.isSensor);
            });

			RenderComponent<CircleCollider2DComponent>("Circle Collider 2D", selectedEntity, [&]()
				{
					CircleCollider2DComponent &cc = selectedEntity.GetComponent<CircleCollider2DComponent>();
                   cc.dirty = UI::DrawFloatControl("Radius", &cc.radius, 0.025f, 0.0f, FLT_MAX);
                    cc.dirty |= UI::DrawVec2Control("Center", cc.center, 0.025f);
                    cc.dirty |= UI::DrawFloatControl("Restitution", &cc.restitution, 0.025f, 0.0f, FLT_MAX);
                    cc.dirty |= UI::DrawFloatControl("Friction", &cc.friction, 0.025f, 0.0f, FLT_MAX);
                    cc.dirty |= UI::DrawFloatControl("Density", &cc.density, 0.025f);
                    cc.dirty |= UI::DrawCheckbox("Is Sensor", &cc.isSensor);
				});

            RenderComponent<RigibodyComponent>("Rigid Body", selectedEntity, [&]()
            {
                RigibodyComponent &c = selectedEntity.GetComponent<RigibodyComponent>();
                UI::DrawCheckbox("Static", &c.isStatic);
            });

            RenderComponent<BoxColliderComponent>("Box Collider", selectedEntity, [&]()
            {
                BoxColliderComponent &c = selectedEntity.GetComponent<BoxColliderComponent>();
                c.dirty = UI::DrawVec3Control("Scale", c.scale, 0.025f, 1.0f);
                c.dirty |= UI::DrawFloatControl("Friction", &c.friction, 0.025f);
                c.dirty |= UI::DrawFloatControl("Static Friction", &c.staticFriction, 0.025f);
                c.dirty |= UI::DrawFloatControl("Restitution", &c.restitution, 0.025f);
                c.dirty |= UI::DrawFloatControl("Density", &c.density, 0.025f);
            });

            RenderComponent<SphereColliderComponent>("Sphere Collider", selectedEntity, [&]()
            {
                SphereColliderComponent &c = selectedEntity.GetComponent<SphereColliderComponent>();
                c.dirty = UI::DrawFloatControl("Radius", &c.radius, 0.025f, 0.01f, 10000.0f, 1.0f);
                c.dirty |= UI::DrawFloatControl("Friction", &c.friction, 0.025f);
                c.dirty |= UI::DrawFloatControl("Static Friction", &c.staticFriction, 0.025f);
                c.dirty |= UI::DrawFloatControl("Restitution", &c.restitution, 0.025f);
                c.dirty |= UI::DrawFloatControl("Density", &c.density, 0.025f);
            });

            RenderComponent<CapsuleColliderComponent>("Capsule Collider", selectedEntity, [&]()
            {
                CapsuleColliderComponent &c = selectedEntity.GetComponent<CapsuleColliderComponent>();
                c.dirty = UI::DrawFloatControl("Radius", &c.radius, 0.025f, 0.01f, 10000.0f, 1.0f);
                c.dirty |= UI::DrawFloatControl("Height", &c.height, 0.025f, 0.01f, 10000.0f, 1.0f);
                c.dirty |= UI::DrawFloatControl("Friction", &c.friction, 0.025f);
                c.dirty |= UI::DrawFloatControl("Static Friction", &c.staticFriction, 0.025f);
                c.dirty |= UI::DrawFloatControl("Restitution", &c.restitution, 0.025f);
                c.dirty |= UI::DrawFloatControl("Density", &c.density, 0.025f);
            });

            RenderComponent<MeshColliderComponent>("Mesh Collider", selectedEntity, [&]()
            {
                MeshColliderComponent &c = selectedEntity.GetComponent<MeshColliderComponent>();
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
                    TextComponent &c = selectedEntity.GetComponent<TextComponent>();

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

                    UI::DrawVec4Control("Color", c.color, 0.001f, 1.0f);
                    UI::DrawFloatControl("Kerning", &c.kerning, 0.001f, -10.0f, 10.0f);
                    UI::DrawFloatControl("Line Spacing", &c.lineSpacing, 0.001f, -10.0f, 10.0f);
                    UI::DrawCheckbox("Screen Space", &c.screenSpace);
				});

            RenderComponent<AudioSourceComponent>("Audio Source", selectedEntity, [&]()
            {
                AudioSourceComponent &c = selectedEntity.GetComponent<AudioSourceComponent>();

                bool isLoaded = c.handle != AssetHandle(0);
                std::string label = isLoaded ? std::to_string((uint64_t)c.handle) : "Drag Here";

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
                        if (ImGui::Button("Play", { 55.0f, 30.0f }))
                        {
                            sound->Stop();

                            sound->Play();
                            sound->SetVolume(c.volume);
                            sound->SetPitch(c.pitch);
                            sound->SetPan(c.pan);
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Stop", { 55.0f, 30.0f }))
                        {
                            sound->Stop();
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Pause", { 55.0f, 30.0f }))
                        {
                            sound->Pause();
                        }
                        if (sound->IsPaused())
                        {
                            ImGui::SameLine();
                            if (ImGui::Button("Resume", { 55.0f, 30.0f }))
                            {
                                sound->Resume();
                            }
                        }

                        UI::DrawFloatControl("Volume", &c.volume, 0.001f, 0.0f, 1.0f);
                        UI::DrawFloatControl("Pitch", &c.pitch, 0.001f);
                        UI::DrawFloatControl("Pan", &c.pan, 0.001f);
                        UI::DrawCheckbox("Play On Start", &c.playOnStart);
                    }
                }
            });
            RenderComponent<ScriptComponent>("C# Script", selectedEntity, [&]()
            {
                ScriptComponent &c = selectedEntity.GetComponent<ScriptComponent>();

                bool scriptClassExist = ScriptEngine::GetInstance()->EntityClassExists(c.className);
                bool isSelected = false;

                if (!scriptClassExist)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
                }

                auto scriptStorage = ScriptEngine::GetInstance()->GetScriptClassStorage();
                std::string currentScriptClasses = c.className;

                // drop-down
                if (ImGui::BeginCombo("Script Class", currentScriptClasses.c_str()))
                {
                    for (size_t i = 0; i < scriptStorage.size(); i++)
                    {
                        isSelected = currentScriptClasses == scriptStorage[i];
                        if (ImGui::Selectable(scriptStorage[i].c_str(), isSelected))
                        {
                            currentScriptClasses = scriptStorage[i];
                            c.className = scriptStorage[i];
                        }
                        if (isSelected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }

                if (ImGui::Button("Detach"))
                {
                    c.className = "Detached";
                    isSelected = false;
                }

                const bool detached = c.className == "Detached";

                if (scriptClassExist && !detached)
                {
                    const bool isRunning = m_Scene && m_Scene->IsRunning();
                    Ref<ScriptClass> scriptClass = ScriptEngine::GetInstance()->GetEntityClassesByName(c.className);
                    if (scriptClass)
                    {
                        const uint64_t instanceId = selectedEntity.GetUUID();

                        auto classRegisteredInstanceField = scriptClass->GetInstanceFieldsById(instanceId);
                        if (!classRegisteredInstanceField)
                        {
                            std::unordered_map<std::string, ScriptInstanceField> defaultInstanceFields;
                            for (auto &[name, field] : scriptClass->GetFields())
                            {
                                ScriptInstanceField instanceField;
                                instanceField.field = field;
                                defaultInstanceFields[name] = instanceField;
                            }

                            scriptClass->InsertInstanceFields(instanceId, defaultInstanceFields);
                            classRegisteredInstanceField = scriptClass->GetInstanceFieldsById(instanceId);
                        }

                        Ref<ScriptInstance> scriptInstance = nullptr;
                        if (isRunning)
                        {
                            scriptInstance = ScriptEngine::GetInstance()->GetEntityScriptInstance(selectedEntity.GetUUID());
                        }

                        if (classRegisteredInstanceField)
                        {
							for (const auto &[name, field] : scriptClass->GetFields())
							{
								ImGui::PushID(name.c_str());

                                auto it = classRegisteredInstanceField->find(name);
								if (it != classRegisteredInstanceField->end())
								{
									ScriptInstanceField &instanceField = it->second;

                                    switch (instanceField.field.Type)
                                    {
                                    case ScriptFieldType::Float:
                                    {
                                        auto data = instanceField.GetValue<float>();
                                        if (UI::DrawFloatControl(name.c_str(), &data, 0.1f, -FLT_MAX, FLT_MAX))
                                        {
                                            if (scriptInstance)
                                                scriptInstance->SetFieldValue<float>(name, data);
                                            else
                                                instanceField.SetValue<float>(data);
                                        }
                                        break;
                                    }
                                    case ScriptFieldType::Int:
                                    {
                                        auto data = instanceField.GetValue<int>();
                                        if (UI::DrawIntControl(name.c_str(), &data, 1.0f, INT_MIN, INT_MAX))
                                        {
                                            if (scriptInstance)
                                                scriptInstance->SetFieldValue<int>(name, data);
                                            else
                                                instanceField.SetValue<int>(data);
                                        }
                                        break;
                                    }
                                    case ScriptFieldType::Double:
                                    {
                                        auto dData = instanceField.GetValue<double>();
                                        float data = static_cast<float>(dData);
                                        if (UI::DrawFloatControl(name.c_str(), &data, 0.1f, -FLT_MAX, FLT_MAX))
                                        {
                                            dData = static_cast<double>(data);
                                            if (scriptInstance)
                                                scriptInstance->SetFieldValue<double>(name, dData);
                                            else
                                                instanceField.SetValue<double>(dData);
                                        }
                                        break;
                                    }
                                    case ScriptFieldType::Vector2:
                                    {
                                        auto data = instanceField.GetValue<glm::vec2>();
                                        if (UI::DrawVec2Control(name.c_str(), data, 0.1f))
                                        {
                                            if (scriptInstance)
                                                scriptInstance->SetFieldValue<glm::vec2>(name, data);
                                            else
                                                instanceField.SetValue<glm::vec2>(data);
                                        }
                                        break;
                                    }
                                    case ScriptFieldType::Vector3:
                                    {
                                        auto data = instanceField.GetValue<glm::vec3>();
                                        if (UI::DrawVec3Control(name.c_str(), data, 0.1f))
                                        {
                                            if (scriptInstance)
                                                scriptInstance->SetFieldValue<glm::vec3>(name, data);
                                            else
                                                instanceField.SetValue<glm::vec3>(data);
                                        }
                                        break;
                                    }
                                    case ScriptFieldType::Vector4:
                                    {
                                        auto data = instanceField.GetValue<glm::vec4>();
                                        if (UI::DrawVec4Control(name.c_str(), data, 0.1f))
                                        {
                                            if (scriptInstance)
                                                scriptInstance->SetFieldValue<glm::vec4>(name, data);
                                            else
                                                instanceField.SetValue<glm::vec4>(data);
                                        }
                                        break;
                                    }
                                    case ScriptFieldType::Entity:
                                    {
                                        auto uuid = instanceField.GetValue<uint64_t>();
                                        std::string label = "Drag Here";
                                        if (uuid)
                                        {
                                            Entity e = SceneManager::GetEntity(m_Scene.get(), UUID(uuid));
                                            if (e)
                                            {
                                                label = e.GetName();
                                            }
                                        }

                                        UI::DrawButtonWithColumn(name.c_str(), label.c_str(), nullptr, [this, &name, &instanceField, &scriptInstance, &uuid]()
                                            {
												if (ImGui::BeginDragDropTarget())
												{
													if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_ENTITY_SOURCE_ITEM))
													{
														LOG_ASSERT(payload->DataSize == sizeof(Entity), "WRONG ENTITY ITEM");
														if (payload->DataSize == sizeof(Entity))
														{
															Entity src{ *static_cast<entt::entity *>(payload->Data), m_Scene.get() };
															uint64_t id = (uint64_t)src.GetUUID();
															instanceField.field.Type = ScriptFieldType::Entity;
															if (scriptInstance)
																scriptInstance->SetFieldValue<uint64_t>(name, id);
															else
																instanceField.SetValue<uint64_t>(id);
														}
													}
													ImGui::EndDragDropTarget();
												}

												if (ImGui::IsItemHovered())
												{
													ImGui::BeginTooltip();

                                                    if (uuid)
                                                    {
														ImGui::Text("%llu", uuid);
                                                    }
													else
                                                    {
														ImGui::Text("Null Entity!");
                                                    }

													ImGui::EndTooltip();
												}

                                                if (uuid != UUID(0))
                                                {
													ImGui::SameLine();
													if (ImGui::Button("X"))
													{
														if (scriptInstance)
														{
															scriptInstance->SetFieldValue<uint64_t>(name, 0);
														}
														else
														{
															instanceField.SetValue<uint64_t>(0);
														}
													}
                                                }
                                            });
                                        break;
                                    }
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
                        /*bool found = false;
                        for (int i = 0; i < CompType_LAST; ++i)
                        {
                            m_Scene->registry->
                            if (entity->GetType() == type)
                            {
                                found = true;
                                break;
                            }
                        }
                        if (found)
                            continue;*/

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
                    case CompType_Font:
                        entity.AddComponent<TextComponent>();
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
                    case CompType_StaticMesh:
                        entity.AddComponent<StaticMeshComponent>();
                        break;
                    case CompType_SkeletalMesh:
                        entity.AddComponent<SkeletalMeshComponent>();
                        break;
                    case CompType_Rigidbody:
                        entity.AddComponent<RigibodyComponent>();
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
            const ImGuiWindow *window = ImGui::GetCurrentWindow();

            m_IsFocused = ImGui::IsWindowFocused();
            m_IsHovered = ImGui::IsWindowHovered();

            static std::array<const char *, 3> kCameraModeLabels = { "Orbit", "Fly", "2D" };
            int cameraModeIndex = 0;
            switch (m_EditorCamera.GetNavigationMode())
            {
                case EditorCamera::NavigationMode::Fly: cameraModeIndex = 1; break;
                case EditorCamera::NavigationMode::Mode2D: cameraModeIndex = 2; break;
                default: cameraModeIndex = 0; break;
            }

            ImGui::SetNextItemWidth(80.0f);
            if (ImGui::Combo("##CameraMode", &cameraModeIndex, kCameraModeLabels.data(), static_cast<int>(kCameraModeLabels.size())))
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

            static std::array<const char *, 4> kGizmoOperationLabels = { "Translate", "Rotate", "Scale", "Bound Sizing 2D" };
            int operationIndex = 0;
            switch (m_Gizmo.GetOperation())
            {
                case ImGuizmo::ROTATE: operationIndex = 1; break;
                case ImGuizmo::SCALE: operationIndex = 2; break;
                default: operationIndex = 0; break;
            }
            ImGui::SetNextItemWidth(90.0f);

            int gizmoOpCount = static_cast<int>(kGizmoOperationLabels.size()) - 1;
            if (m_EditorCamera.GetNavigationMode() == EditorCamera::NavigationMode::Mode2D)
                gizmoOpCount = static_cast<int>(kGizmoOperationLabels.size());

            if (ImGui::Combo("##GizmoOperation", &operationIndex, kGizmoOperationLabels.data(), gizmoOpCount))
            {
                SetGizmoOperation((GizmoOperation)operationIndex);
            }
            ImGui::SameLine();

            static std::array<const char *, 2> kGizmoModeLabels = { "Local", "World" };
            int modeIndex = m_Gizmo.GetMode() == ImGuizmo::LOCAL ? 0 : 1;
            ImGui::SetNextItemWidth(90.0f);
            if (ImGui::Combo("##GizmoMode", &modeIndex, kGizmoModeLabels.data(), static_cast<int>(kGizmoModeLabels.size())))
            {
                auto mode = modeIndex == 0 ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
                m_Gizmo.SetMode(mode);
            }

            ImGui::SameLine();
            ImGui::TextUnformatted("Snap");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(90.0f);
            ImGui::DragFloat("##GizmoSnapping", &m_ViewportData.snapValue, 0.05f, 0.0f, 100.0f);

            ImGui::SameLine();

            // TOOLBAR: 
            constexpr ImVec2 buttonSize = { 24.0f, 24.0f };

            State sceneState = m_EditorLayer->GetData().sceneState;
            const bool isScenePlaying = sceneState == ignite::State::ScenePlay;
            Ref<Texture> scenePlayStopTex = isScenePlaying ? m_Icons["stop"] : m_Icons["play"];
            ImTextureID scenePlayStopID = (ImTextureID)scenePlayStopTex->GetHandle().Get();

            ImGui::SameLine();
            ImGui::Image(scenePlayStopID, buttonSize);
            if (ImGui::IsItemClicked())
            {
                if (isScenePlaying)
                {
                    m_EditorLayer->OnSceneStop();
#if _WIN32
                    HWND hwnd = Application::GetInstance()->GetWindow()->GetNativeWindow();
                    COLORREF rgbRed = 0x00E86071;
                    DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &rgbRed, sizeof(rgbRed));
#endif
                }
                else
                {
                    m_EditorLayer->OnScenePlay();
#if _WIN32
                    HWND hwnd = Application::GetInstance()->GetWindow()->GetNativeWindow();
                    COLORREF rgbRed = 0x000000AB;
                    DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &rgbRed, sizeof(rgbRed));
#endif
                }
            }

            const bool isSceneSimulate = sceneState == ignite::State::SceneSimulate;
            Ref<Texture> sceneSimulateTex = isSceneSimulate ? m_Icons["stop"] : m_Icons["simulate"];
            ImTextureID sceneSimulateID = (ImTextureID)sceneSimulateTex->GetHandle().Get();

            ImGui::SameLine();
            ImGui::Image(sceneSimulateID, buttonSize);
            if (ImGui::IsItemClicked())
            {
                if (isSceneSimulate)
                {
                    m_EditorLayer->OnSceneStop();
#if _WIN32
                    HWND hwnd = Application::GetInstance()->GetWindow()->GetNativeWindow();
                    COLORREF rgbRed = 0x00E86071;
                    DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &rgbRed, sizeof(rgbRed));
#endif
                }
                else
                {
                    m_EditorLayer->OnSceneSimulate();
#if _WIN32
                    HWND hwnd = Application::GetInstance()->GetWindow()->GetNativeWindow();
                    COLORREF rgbRed = 0x000000AB;
                    DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &rgbRed, sizeof(rgbRed));
#endif
                }
            }

            // Calculating Scene Viewport location
            const ImVec2 &canvasPos = ImGui::GetCursorScreenPos();
            const ImVec2 &canvasSize = ImGui::GetContentRegionAvail();

            m_ViewportEditRT.rect.min = { canvasPos.x, canvasPos.y };
            m_ViewportEditRT.rect.max = { canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y };

            // Mouse position in screen space
            const ImVec2 &mousePos = ImGui::GetMousePos();
            m_ViewportData.mousePos = { mousePos.x - canvasPos.x, mousePos.y - canvasPos.y };

            // Update UI input handling
            if (m_Scene)
            {
                auto sceneRenderer = m_Scene->GetSceneRenderer();
                if (sceneRenderer)
                {
                    glm::vec2 viewportPos = { canvasPos.x, canvasPos.y };
                    glm::vec2 viewportSize = { canvasSize.x, canvasSize.y };
                    glm::vec2 screenMousePos = { mousePos.x, mousePos.y };
                    bool mousePressed = ImGui::IsMouseDown(ImGuiMouseButton_Left);

                    sceneRenderer->UpdateUIInput(screenMousePos, viewportPos, viewportSize, mousePressed);
                }
            }

            // Render scene texture to imgui
            ImTextureID sceneImage = (ImTextureID)m_ViewportEditRT.composite->GetColorAttachment(0)->GetHandle().Get(); // Current composite RT
            ImGui::Image(sceneImage, canvasSize);

            // Mouse picking from viewport object-id attachment (on mouse down only)
            {
                const bool imageHovered = ImGui::IsItemHovered();
                const bool mouseDown = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
                const bool mouseDoubleDown = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

                if (m_Scene && imageHovered && (mouseDown || mouseDoubleDown) && !m_Gizmo.IsManipulating() && !m_Gizmo.IsHovered() && !m_Data.is2DBoundsHovered)
                {
                    Ref<Texture> objectIdTexture = m_ViewportEditRT.scene->GetColorAttachment(1);
                    if (objectIdTexture && objectIdTexture->GetHandle())
                    {
                        const glm::vec2 viewSize = m_ViewportEditRT.rect.GetSize();
                        const int texWidth = objectIdTexture->GetWidth();
                        const int texHeight = objectIdTexture->GetHeight();

                        if (viewSize.x > 0.0f && viewSize.y > 0.0f && texWidth > 0 && texHeight > 0)
                        {
                            const int pixelX = std::clamp(static_cast<int>((m_ViewportData.mousePos.x / viewSize.x) * static_cast<float>(texWidth)), 0, texWidth - 1);
                            const int pixelY = std::clamp(static_cast<int>((m_ViewportData.mousePos.y / viewSize.y) * static_cast<float>(texHeight)), 0, texHeight - 1);

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
                                const uint32_t *pixelData = static_cast<const uint32_t *>(mapped);
                                const uint32_t pickedObjectId = pixelData[pixelY * (rowPitch / sizeof(uint32_t)) + pixelX];
                                device->unmapStagingTexture(stagingTexture);

                                Entity pickedEntity = {};
                                if (pickedObjectId != 0xFFFFFFFFu)
                                {
                                    m_Scene->registry->view<IDComponent>().each([&](const entt::entity e, const IDComponent &id)
                                    {
                                        if (pickedEntity.IsValid())
                                            return;

                                        const uint32_t objectId = static_cast<uint32_t>(static_cast<uint64_t>(id.uuid));
                                        if (objectId == pickedObjectId)
                                        {
                                            pickedEntity = Entity { e, m_Scene.get() };
                                        }
                                    });
                                }

                                if (pickedEntity.IsValid())
                                {
                                    Entity targetSelection = pickedEntity;

                                    // Single click: prefer selecting the direct parent group first.
                                    // Double click: select the exact clicked entity.
                                    if (!mouseDoubleDown)
                                    {
                                        const UUID parent = pickedEntity.GetParentUUID();
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
                                else if (!m_EditorLayer->GetData().multiSelect)
                                {
                                    SetSelectedEntity(Entity {});
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
                                std::filesystem::path filepath = m_EditorLayer->GetActiveProject()->GetAssetFilepath(metadata.filepath);
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
                const float orientationSize = ImGuiOrientation::internal::config.mSize = 80.0f;
                const float orientationPadding = 25.0f;
                ImGuiOrientation::config.axisLengthScale = 0.25f;
                ImGuiOrientation::SetRect
                (
                    m_ViewportEditRT.rect.max.x - orientationSize - orientationPadding,
                    m_ViewportEditRT.rect.min.y + orientationPadding
                );

                if (ImGuiOrientation::DrawGizmo(ImGui::GetWindowDrawList(), (float *const)glm::value_ptr(view), glm::value_ptr(projection), 100.0f))
                {
                    glm::vec3 f = glm::vec3(view[0][2], view[1][2], view[2][2]);
                    m_EditorCamera.pitch = glm::clamp(std::asin(glm::clamp(-f.y, -1.0f, 1.0f)), m_EditorCamera.controls.minPitch, m_EditorCamera.controls.maxPitch);
                    m_EditorCamera.yaw = std::atan2(-f.z, -f.x);
                    m_EditorCamera.UpdateCameraPosition();
                    m_EditorCamera.UpdateView();
                }
            }

            GizmoInfo gizmoInfo;
            gizmoInfo.cameraView = view;
            gizmoInfo.cameraProjection = projection;
            gizmoInfo.cameraType = m_EditorCamera.projectionType;
            gizmoInfo.snapValue = m_ViewportData.snapValue;
            gizmoInfo.viewRect = m_ViewportEditRT.rect;

            m_Gizmo.SetInfo(gizmoInfo);

            Render2DBoundsSizing();

            // Start manipulation: Fired only on the first frame of interaction
            const bool allowGizmoManipulation = !m_Data.is2DBoundsSizing;
            bool isManipulatingNow = allowGizmoManipulation && m_Gizmo.IsManipulating();

            static std::unordered_map<UUID, TransformComponent> initialTransforms;

            if (isManipulatingNow && !m_Data.isGizmoManipulating)
            {
                initialTransforms.clear();
                for (auto [uuid, entity] : m_SelectedEntities)
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
                    pivot += entity.GetTransform().translation;
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
                        TransformComponent &tr = entity.GetTransform();

                        // Get the ORIGINAL transform we stored at the beginning of the manipulation
                        const TransformComponent &initialTransform = initialTransforms.at(uuid);
                        glm::mat4 initialWorldMatrix = initialTransform.GetWorldMatrix();

                        // Apply Translation and Rotation around the shared pivot
                        glm::mat4 toPivot = glm::translate(glm::mat4(1.0f), -pivot);
                        glm::mat4 fromPivot = glm::translate(glm::mat4(1.0f), pivot);
                        glm::mat4 noScaleDelta = Math::RemoveScale(gizmoDelta);

                        // Apply the total delta to the ORIGINAL world matrix
                        glm::mat4 newWorldMatrix = fromPivot * noScaleDelta * toPivot * tr.GetWorldMatrix();
                        glm::vec3 newTranslation, newRotationEuler, newScale;
                        Math::DecomposeTransformEuler(newWorldMatrix, newTranslation, newRotationEuler, newScale);

                        // ----- Apply Scale and Update Local Transform -----
                        if (entity.GetParentUUID() != UUID(0))
                        {
                            Entity parent = SceneManager::GetEntity(m_Scene.get(), entity.GetParentUUID());
                            const TransformComponent &parentTr = parent.GetTransform();
                            glm::mat4 parentWorld = parentTr.GetWorldMatrix();
                            glm::mat4 localMatrix = glm::inverse(parentWorld) * newWorldMatrix;

                            glm::vec3 localTranslation, localEuler, localScale;
                            Math::DecomposeTransformEuler(localMatrix, localTranslation, localEuler, localScale);
                            tr.localTranslation = localTranslation;
                            tr.localRotation = glm::quat(localEuler);

                            // Apply the total scale delta to the ORIGINAL local scale
                            tr.localScale = initialTransform.localScale * deltaScale;
                        }
                        else
                        {
                            tr.localTranslation = newTranslation;
                            tr.localRotation = glm::quat(newRotationEuler);

                            // Apply the total scale delta to the ORIGINAL local scale
                            tr.localScale = initialTransform.localScale * deltaScale;
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
                    TransformComponent &tr = entity.GetTransform();
                    glm::mat4 transformMatrix = tr.GetWorldMatrix();

                    m_Gizmo.Manipulate(transformMatrix);

                    if (m_Gizmo.IsManipulating())
                    {
                        const glm::vec3 preservedLocalScale = tr.localScale;
                        glm::vec3 translation, rotation, scale;
                        Math::DecomposeTransformEuler(transformMatrix, translation, rotation, scale);
                        const ImGuizmo::OPERATION op = m_Gizmo.GetOperation();

                        if (entity.GetParentUUID() != UUID(0))
                        {
                            Entity parent = SceneManager::GetEntity(m_Scene.get(), entity.GetParentUUID());
                            const TransformComponent &parentTr = parent.GetTransform();
                            const glm::mat4 parentWorld = parentTr.GetWorldMatrix();
                            const glm::mat4 localMatrix = glm::inverse(parentWorld) * transformMatrix;

                            glm::vec3 localTranslation, localEuler, localScale;
                            Math::DecomposeTransformEuler(localMatrix, localTranslation, localEuler, localScale);
                            tr.localTranslation = localTranslation;
                            tr.localRotation = glm::quat(localEuler);

                            if (op == ImGuizmo::SCALE)
                            {
                                tr.localScale = localScale;
                            }
                            else
                            {
                                tr.localScale = preservedLocalScale;
                            }
                        }
                        else
                        {
                            tr.localTranslation = translation;
                            tr.localRotation = glm::quat(rotation);

                            if (op == ImGuizmo::SCALE)
                            {
                                tr.localScale = scale;
                            }
                            else
                            {
                                tr.localScale = preservedLocalScale;
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

        ImGui::End();

    }

    void ScenePanel::RenderSceneGameViewport()
    {
        IGN_PROFILE_FUNCTION();
        m_Data.sceneViewportGameplayVisible = ImGui::Begin("Game");
        if (m_Data.sceneViewportGameplayVisible)
        {
            // Preview camera
            if (m_Scene)
            {
                // TOOLBAR: 
                constexpr ImVec2 buttonSize = { 24.0f, 24.0f };

                State sceneState = m_EditorLayer->GetData().sceneState;
                const bool isScenePlaying = sceneState == ignite::State::ScenePlay;
                Ref<Texture> scenePlayStopTex = isScenePlaying ? m_Icons["stop"] : m_Icons["play"];
                ImTextureID scenePlayStopID = (ImTextureID)scenePlayStopTex->GetHandle().Get();

                ImGui::SameLine();
                ImGui::Image(scenePlayStopID, buttonSize);
                if (ImGui::IsItemClicked())
                {
                    if (isScenePlaying)
                    {
                        m_EditorLayer->OnSceneStop();
#if _WIN32
                        HWND hwnd = Application::GetInstance()->GetWindow()->GetNativeWindow();
                        COLORREF rgbRed = 0x00E86071;
                        DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &rgbRed, sizeof(rgbRed));
#endif
                    }
                    else
                    {
                        m_EditorLayer->OnScenePlay();
#if _WIN32
                        HWND hwnd = Application::GetInstance()->GetWindow()->GetNativeWindow();
                        COLORREF rgbRed = 0x000000AB;
                        DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &rgbRed, sizeof(rgbRed));
#endif
                    }
                }

                const bool isSceneSimulate = sceneState == ignite::State::SceneSimulate;
                Ref<Texture> sceneSimulateTex = isSceneSimulate ? m_Icons["stop"] : m_Icons["simulate"];
                ImTextureID sceneSimulateID = (ImTextureID)sceneSimulateTex->GetHandle().Get();

                ImGui::SameLine();
                ImGui::Image(sceneSimulateID, buttonSize);
                if (ImGui::IsItemClicked())
                {
                    if (isSceneSimulate)
                    {
                        m_EditorLayer->OnSceneStop();
#if _WIN32
                        HWND hwnd = Application::GetInstance()->GetWindow()->GetNativeWindow();
                        COLORREF rgbRed = 0x00E86071;
                        DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &rgbRed, sizeof(rgbRed));
#endif
                    }
                    else
                    {
                        m_EditorLayer->OnSceneSimulate();
#if _WIN32
                        HWND hwnd = Application::GetInstance()->GetWindow()->GetNativeWindow();
                        COLORREF rgbRed = 0x000000AB;
                        DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &rgbRed, sizeof(rgbRed));
#endif
                    }
                }

                ImGui::SameLine();
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
                    CameraComponent &cameraComp = cameraEntity.GetComponent<CameraComponent>();

                    ImVec2 baseImagePos = canvasPos;
                    ImVec2 baseImageSize = canvasSize;

                    const float safeCanvasW = glm::max(canvasSize.x, 1.0f);
                    const float safeCanvasH = glm::max(canvasSize.y, 1.0f);
                    const float canvasAspect = safeCanvasW / safeCanvasH;

                    float targetAspect = canvasAspect;
                    if (!cameraComp.camera.IsFreeAspect())
                    {
                        targetAspect = glm::max(cameraComp.camera.GetAspectRatioValue(), 0.0001f);
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

                    m_Data.gamePreviewZoom = glm::clamp(m_Data.gamePreviewZoom, 0.25f, 4.0f);

                    ImVec2 imageSize =
                    {
                        baseImageSize.x * m_Data.gamePreviewZoom,
                        baseImageSize.y * m_Data.gamePreviewZoom
                    };

                    const float maxPanX = glm::max((imageSize.x - baseImageSize.x) * 0.5f, 0.0f);
                    const float maxPanY = glm::max((imageSize.y - baseImageSize.y) * 0.5f, 0.0f);
                    m_Data.gamePreviewPan.x = glm::clamp(m_Data.gamePreviewPan.x, -maxPanX, maxPanX);
                    m_Data.gamePreviewPan.y = glm::clamp(m_Data.gamePreviewPan.y, -maxPanY, maxPanY);

                    ImVec2 imagePos =
                    {
                        baseImagePos.x + (baseImageSize.x - imageSize.x) * 0.5f + m_Data.gamePreviewPan.x,
                        baseImagePos.y + (baseImageSize.y - imageSize.y) * 0.5f + m_Data.gamePreviewPan.y
                    };

                    const bool imageHovered = ImGui::GetMousePos().x >= imagePos.x && ImGui::GetMousePos().x <= imagePos.x + imageSize.x &&
                        ImGui::GetMousePos().y >= imagePos.y && ImGui::GetMousePos().y <= imagePos.y + imageSize.y;

                    if (ImGui::IsWindowFocused() && ImGui::IsWindowHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
                    {
                        const ImVec2 delta = ImGui::GetIO().MouseDelta;
                        m_Data.gamePreviewPan += glm::vec2(delta.x, delta.y);
                    }

                    m_ViewportGameRT.rect.min = { baseImagePos.x, baseImagePos.y };
                    m_ViewportGameRT.rect.max = { baseImagePos.x + baseImageSize.x, baseImagePos.y + baseImageSize.y };

                    ImTextureID previewImage = (ImTextureID)m_ViewportGameRT.composite->GetColorAttachment(0)->GetHandle().Get();
                    ImDrawList *drawList = ImGui::GetWindowDrawList();
                    drawList->PushClipRect(baseImagePos, ImVec2(baseImagePos.x + baseImageSize.x, baseImagePos.y + baseImageSize.y), true);
                    drawList->AddImage(previewImage, imagePos, ImVec2(imagePos.x + imageSize.x, imagePos.y + imageSize.y));
                    drawList->PopClipRect();

                    ImGui::SetCursorScreenPos(baseImagePos);
                    ImGui::InvisibleButton("##GamePreviewCanvas", baseImageSize);
                }
                else
                {
                    m_ViewportGameRT.rect.min = { canvasPos.x, canvasPos.y };
                    m_ViewportGameRT.rect.max = { canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y };
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

    bool ScenePanel::Is2DResizableEntity(Entity entity) const
    {
        return entity.IsValid() &&
            (entity.HasComponent<Sprite2DComponent>() || entity.HasComponent<Circle2DComponent>() || entity.HasComponent<TextComponent>());
    }

    glm::vec3 ScenePanel::ScreenToWorldOnPlane(const glm::vec2 &screenPos, float planeZ, bool *isValid)
    {
        return Math::ScreenToWorldOnPlane(screenPos, planeZ, m_EditorCamera.GetProjection() * m_EditorCamera.GetView(), m_ViewportEditRT.rect, isValid);
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
                releaseResizeCommand();
            return;
        }

        Entity entity = GetSelectedEntity();
        if (!Is2DResizableEntity(entity))
        {
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
                releaseResizeCommand();
            return;
        }

        TransformComponent &tr = entity.GetTransform();
        const glm::mat4 worldMatrix = tr.GetWorldMatrix();
        const glm::mat4 viewProjection = m_EditorCamera.GetProjection() * m_EditorCamera.GetView();

        std::array<glm::vec3, 4> worldCorners{};
        std::array<ImVec2, 4> screenCorners{};
        for (size_t i = 0; i < kBoundsCorners.size(); ++i)
        {
            const glm::vec4 world = worldMatrix * glm::vec4(kBoundsCorners[i].x, kBoundsCorners[i].y, 0.0f, 1.0f);
            worldCorners[i] = glm::vec3(world);
            if (!Math::ProjectWorldToScreen(worldCorners[i], viewProjection, m_ViewportEditRT.rect, screenCorners[i]))
            {
                if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
                    releaseResizeCommand();
                return;
            }
        }

        ImDrawList *drawList = ImGui::GetWindowDrawList();
        constexpr ImU32 boundsColor = IM_COL32(247, 210, 60, 255);
        drawList->AddPolyline(screenCorners.data(), static_cast<int>(screenCorners.size()), boundsColor, ImDrawFlags_Closed, 2.0f);

        const ImVec2 mousePos = ImGui::GetMousePos();
        const bool mouseInViewport =
            mousePos.x >= m_ViewportEditRT.rect.min.x && mousePos.x <= m_ViewportEditRT.rect.max.x &&
            mousePos.y >= m_ViewportEditRT.rect.min.y && mousePos.y <= m_ViewportEditRT.rect.max.y;

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
                        const glm::mat4 parentWorld = parent.GetTransform().GetWorldMatrix();
                        const glm::mat4 localMatrix = glm::inverse(parentWorld) * targetWorld;

                        glm::vec3 localTranslation, localEuler, localScale;
                        Math::DecomposeTransformEuler(localMatrix, localTranslation, localEuler, localScale);
                        tr.localTranslation = localTranslation;
                        tr.localRotation = glm::quat(localEuler);
                        tr.localScale = localScale;
                    }
                    else
                    {
                        tr.localTranslation = centerWorld;
                        tr.localScale.x = targetWorldScaleX;
                        tr.localScale.y = targetWorldScaleY;
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

	void ScenePanel::ViewportEditResize(uint32_t width, uint32_t height)
    {
        // Resize framebuffers
        m_ViewportEditRT.scene->Resize(width, height);
        m_ViewportEditRT.ui->Resize(width, height);
        m_ViewportEditRT.composite->Resize(width, height);

        // Update camera view
        m_EditorCamera.UpdateProjection(static_cast<float>(width), static_cast<float>(height));
    }

	void ScenePanel::ViewportGameResize(uint32_t width, uint32_t height)
	{
        // Resize framebuffers
		m_ViewportGameRT.scene->Resize(width, height);
		m_ViewportGameRT.ui->Resize(width, height);
		m_ViewportGameRT.composite->Resize(width, height);
        
        // Update camera view
        if (m_Scene)
        {
            if (Entity cameraEntity = m_Scene->GetPrimaryCamera())
            {
                CameraComponent &cameraComp = cameraEntity.GetComponent<CameraComponent>();
                cameraComp.camera.UpdateProjection(static_cast<float>(width), static_cast<float>(height));
            }
        }
	}

    template<typename T, typename UIFunction>
    void ScenePanel::RenderComponent(const std::string &name, Entity entity, UIFunction uiFunction, bool allowedToRemove)
    {
        if (entity.HasComponent<T>())
        {
            constexpr ImGuiTreeNodeFlags treeNdeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed
                | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding;

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
        // glm::vec2 mouse = { event.GetX(), event.GetY() };
		// LOG_INFO("Mouse Moved: {0}, {1}", mouse.x, mouse.y);
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
        m_EditorCamera.UpdateCameraPosition();
        m_EditorCamera.UpdateView();
    }

    void ScenePanel::DestroyEntity(Entity entity)
    {
        // Snapshot the entire subtree BEFORE destroying, so undo can recreate it
        CommandManager::ExecuteCommand(CreateScope<EntityDestroyCommand>(m_Scene.get(), entity));
    }

    void ScenePanel::ClearSelection()
    {
        m_SelectedEntities.clear();
    }

    Entity ScenePanel::SetSelectedEntity(Entity entity)
    {
		auto sceneRenderer = m_Scene->GetSceneRenderer();

        if (!entity.IsValid())
        {
            m_SelectedEntities.clear();
            m_TrackingSelectedEntity = UUID(0);

            sceneRenderer->ClearSelectedEntities();
            return {};
        }

        // multi select
        if (m_EditorLayer->GetData().multiSelect)
        {
            if (auto it = m_SelectedEntities.find(entity.GetUUID()); it != m_SelectedEntities.end())
            {
                // de-select
                sceneRenderer->UnselectEntity(it->second);
                it = m_SelectedEntities.erase(it);

                if (!m_SelectedEntities.empty())
                {
                    m_TrackingSelectedEntity = m_SelectedEntities.begin()->first;
                    sceneRenderer->SetSelectedEntity(m_SelectedEntities.begin()->second);

                    return m_SelectedEntities.begin()->second;
                }
            }
            else
            {
                m_SelectedEntities[entity.GetUUID()] = entity;
                sceneRenderer->SetSelectedEntity(entity);
            }
        }
        else // single select
        {
            m_SelectedEntities.clear();
            sceneRenderer->ClearSelectedEntities();

            m_SelectedEntities[entity.GetUUID()] = entity;
            sceneRenderer->SetSelectedEntity(entity);
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
