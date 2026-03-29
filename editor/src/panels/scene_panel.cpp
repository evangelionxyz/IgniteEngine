//Copyright (c) 2026 Evangelion Manuhutu

#include "scene_panel.hpp"

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
#include "editor_layer.hpp"
#include "ignite/graphics/objects/mesh.hpp"
#include "ignite/graphics/objects/material_2d.hpp"
#include "ignite/graphics/font.hpp"

#include "ignite/scripting/script_engine.hpp"
#include "ignite/scripting/script_field.hpp"
#include "ignite/scripting/script_instance.hpp"
#include "ignite/asset/asset_importer.hpp"

#include "ignite/scene/entity.hpp"
#include "../states.hpp"

#include "ignite/scene/entity_destroy_command.hpp"
#include "ignite/scene/entity_rename_command.hpp"
#include "ignite/scene/entity_reparent_command.hpp"
#include "ignite/scene/component_property_command.hpp"

#include <set>
#include <unordered_map>
#include <string>
#include <algorithm>
#include <ranges>

#ifdef _WIN32
    #include <dwmapi.h>
#endif

namespace ignite
{
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
        m_Icons["checker128"] = Texture::Create("resources/ui/checker-128px.jpg", createInfo, cmd);

        cmd->close();
        device->executeCommandList(cmd);

        // Create scene render target
        RenderTargetCreateInfo rtCreateInfo = {};
        rtCreateInfo.attachments =
        {
            FramebufferAttachments{ "[Scene DepthAttachment]", nvrhi::Format::D32S8, nvrhi::ResourceStates::DepthWrite}, // Depth
            FramebufferAttachments{ "[Scene ColorAttachment]", nvrhi::Format::RGBA8_UNORM, nvrhi::ResourceStates::RenderTarget} // Main Color
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
        ImGui::ShowDemoWindow();

        if (m_Scene)
        {
            RenderHierarchy();
            RenderInspector();
        }
        
        RenderSceneEditViewport();
        RenderSceneGameViewport();
    }

    void ScenePanel::OnUpdate(float deltaTime)
    {
        UpdateCameraInput(deltaTime);
    }

    void ScenePanel::RenderHierarchy()
    {
        ImGui::Begin("Hierarchy");
        ImGui::Button(m_Scene->name.c_str(), { ImGui::GetContentRegionAvail().x, 0.0f });

        // target drop
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ENTITY_SOURCE_ITEM"))
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

        ImGuiTableFlags tableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable;

        if (ImGui::BeginTable("entity_hierarchy_table", 3, tableFlags))
        {
            // setup table 3 columns
            // Name, Type, Active (check box)
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("Active");
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
        if (!entity.IsValid())
            return;

        IDComponent &idComp = entity.GetComponent<IDComponent>();
        bool isDeleting = false;
        const bool isPrefab = idComp.IsInType(EntityType_Prefab);

        ImGuiTreeNodeFlags flags = (GetSelectedEntity() == entity ? ImGuiTreeNodeFlags_Selected : 0) | (!idComp.HasChild() ? ImGuiTreeNodeFlags_Leaf : 0)
            | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_OpenOnArrow
            | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_SpanFullWidth;

        const intptr_t imguiPushId = static_cast<intptr_t>(static_cast<uint64_t>(static_cast<uint32_t>(entity)));
        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, { 0.435f, 0.287f, 0.000f, 1.000f });
        ImGui::PushStyleColor(ImGuiCol_Header, { 0.000f, 0.305f, 0.453f, 1.000f });
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, { 0.780f, 0.520f, 0.000f, 1.000f });
        
        const bool opened = ImGui::TreeNodeEx(reinterpret_cast<void *>(imguiPushId), flags, "%s", idComp.name.c_str());
        
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
                ImGui::SetDragDropPayload("ENTITY_SOURCE_ITEM", &entity, sizeof(Entity));

                ImGui::BeginTooltip();
                ImGui::Text("%s %llu", idComp.name.c_str(), static_cast<u64>(idComp.uuid));
                ImGui::EndTooltip();

                ImGui::EndDragDropSource();
            }

            // target drop
            if (isPrefab == false && ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ENTITY_SOURCE_ITEM"))
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

            ImGui::PushID((int)imguiPushId);

            // second column
            ImGui::TableNextColumn();
            ImGui::TextWrapped(EntityTypeFlagsToString(idComp.type).c_str());

            // third column (last)
            ImGui::TableNextColumn();
			auto& tc = entity.GetComponent<TransformComponent>();
            ImGui::Checkbox("##Active", &tc.visible);
            ImGui::PopID();
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
                TransformComponent &comp = selectedEntity.GetComponent<TransformComponent>();

                // Snapshot before the user starts dragging.
                // IsItemActivated() fires on the FIRST click frame before any value changes,
                // so it must be checked AFTER the widget call, unconditionally.
                static TransformComponent s_TransformBefore;

                bool changed = false;

                ImGui::DragFloat3("Translation", &comp.localTranslation.x, 0.025f);
                if (ImGui::IsItemActivated())            s_TransformBefore = comp;
                if (ImGui::IsItemEdited())               { comp.dirty = true; changed = true; }
                if (ImGui::IsItemDeactivatedAfterEdit()) CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<TransformComponent>>(m_Scene.get(), selectedEntity.GetUUID(), s_TransformBefore, comp));

                glm::vec3 euler = eulerAngles(comp.localRotation);
                ImGui::DragFloat3("Rotation", &euler.x, 0.025f);
                if (ImGui::IsItemActivated())            s_TransformBefore = comp;
                if (ImGui::IsItemEdited())               { comp.localRotation = glm::quat(euler); comp.dirty = true; changed = true; }
                if (ImGui::IsItemDeactivatedAfterEdit()) CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<TransformComponent>>(m_Scene.get(), selectedEntity.GetUUID(), s_TransformBefore, comp));

                ImGui::DragFloat3("Scale", &comp.localScale.x, 0.025f);
                if (ImGui::IsItemActivated())            s_TransformBefore = comp;
                if (ImGui::IsItemEdited())               { comp.dirty = true; changed = true; }
                if (ImGui::IsItemDeactivatedAfterEdit()) CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<TransformComponent>>(m_Scene.get(), selectedEntity.GetUUID(), s_TransformBefore, comp));

                // Visibility checkbox — instant commit
                {
                    TransformComponent before = comp;
                    if (ImGui::Checkbox("Visible", &comp.visible))
                        CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<TransformComponent>>(m_Scene.get(), selectedEntity.GetUUID(), before, comp));
                }

            }, false); // false: not allowed to remove the component

            RenderComponent<WorldEnvironment>("World Environment", selectedEntity, [&]()
            {
                WorldEnvironment &c = selectedEntity.GetComponent<WorldEnvironment>();

                ImGui::Checkbox("Primary", &c.primary);
                ImGui::Checkbox("Enabled", &c.enabled);

                const bool hasHDR = c.hdrHandle != AssetHandle(0);
                std::string buttonLabel = hasHDR ? "HDR Loaded" : "Drag .hdr Here";
                ImGui::Button(buttonLabel.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - 30.0f, 0.0f));

                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("content_browser_item"))
                    {
                        LOG_ASSERT(payload->DataSize == sizeof(AssetHandle), "WRONG ITEM, that should be an asset handle");
                        AssetHandle handle = *static_cast<AssetHandle *>(payload->Data);
                        AssetMetaData metadata = Project::GetInstance()->GetAssetManager().GetMetaData(handle);
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

                    ImGui::TextDisabled("HDR Handle: %llu", static_cast<u64>(c.hdrHandle));
                }

                ImGui::Separator();
                ImGui::ColorEdit4("Sun Color", &c.sceneGPUData.sunColor.x);
                ImGui::DragFloat2("Sun Angles", &c.sceneGPUData.sungAngles.x, 0.01f);
                ImGui::DragFloat("Sun Angular Radius", &c.sceneGPUData.sunAngularRadius, 0.01f, 0.0f, 10.0f);
                ImGui::DragInt("Render Mode", &c.sceneGPUData.renderMode, 1.0f, 0, 16);
                bool debugShadow = c.sceneGPUData.debugShadow != 0;
                if (ImGui::Checkbox("Debug Shadow", &debugShadow))
                {
                    c.sceneGPUData.debugShadow = debugShadow ? 1 : 0;
                }
                ImGui::DragFloat("Exposure", &c.sceneGPUData.exposure, 0.01f, 0.0f, 32.0f);
                ImGui::DragFloat("Gamma", &c.sceneGPUData.gamma, 0.01f, 0.1f, 8.0f);
                ImGui::DragFloat("Ambient", &c.sceneGPUData.ambient, 0.01f, 0.0f, 4.0f);
            });

            RenderComponent<Sprite2DComponent>("Sprite 2D", selectedEntity, [&]()
            {
                Sprite2DComponent &c = selectedEntity.GetComponent<Sprite2DComponent>();

                bool isMaterialLoaded = c.materialHandle != AssetHandle(0);

                std::string btLabel = isMaterialLoaded ? "Material Loaded" : "Drag Material2D Here";
                ImGui::Button(btLabel.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - 30.0f, 0.0f));

                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("content_browser_item"))
                    {
                        LOG_ASSERT(payload->DataSize == sizeof(AssetHandle), "WRONG ITEM, that should be an asset handle");
                        AssetHandle handle = *static_cast<AssetHandle *>(payload->Data);
                        AssetType type = Project::GetInstance()->GetAssetManager().GetAssetType(handle);
                        if (type == AssetType::Material2D)
                        {
                            Sprite2DComponent before = c;
                            c.materialHandle = handle;
                            CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<Sprite2DComponent>>(m_Scene.get(), selectedEntity.GetUUID(), before, c));
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                if (isMaterialLoaded)
                {
                    ImGui::SameLine();
                    if (ImGui::Button("X"))
                    {
                        Sprite2DComponent before = c;
                        c.materialHandle = AssetHandle(0);
                        CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<Sprite2DComponent>>(m_Scene.get(), selectedEntity.GetUUID(), before, c));
                    }

                    ImGui::SameLine();
                    ImGui::Text("Material: %llu", static_cast<u64>(c.materialHandle));
                }

                Ref<Material2D> material2D = nullptr;
                if (c.materialHandle != AssetHandle(0))
                {
                    material2D = Project::GetInstance()->GetAsset<Material2D>(c.materialHandle, AssetType::Material2D);
                }

                if (material2D)
                {
                    if (ImGui::BeginCombo("Material Type", material2D->data.type == MATERIAL_2D_TYPE_LIT ? "Lit" : "Unlit"))
                    {
                        if (ImGui::Selectable("Unlit", material2D->data.type == MATERIAL_2D_TYPE_UNLIT))
                        {
                            material2D->data.type = MATERIAL_2D_TYPE_UNLIT;
                            material2D->SetDirtyFlag(true);
                        }

                        if (ImGui::Selectable("Lit", material2D->data.type == MATERIAL_2D_TYPE_LIT))
                        {
                            material2D->data.type = MATERIAL_2D_TYPE_LIT;
                            material2D->SetDirtyFlag(true);
                        }
                        ImGui::EndCombo();
                    }
                    if (ImGui::ColorEdit4("Base Color", &material2D->data.baseColor.x))
                    {
                        material2D->SetDirtyFlag(true);
                    }

                    if (ImGui::ColorEdit4("Additive Color", &material2D->data.additiveColor.x))
                    {
                        material2D->SetDirtyFlag(true);
                    }

                    if (ImGui::DragFloat2("Tiling", &material2D->data.tilingFactor.x, 0.025f))
                    {
                        material2D->SetDirtyFlag(true);
                    }

                    ImGui::Button("Drag Texture Here", { 130.0f, 25.0f * ImGui::GetWindowDpiScale() });
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("content_browser_item"))
                        {
                            LOG_ASSERT(payload->DataSize == sizeof(AssetHandle), "WRONG ITEM, that should be an asset handle");
                            AssetHandle handle = *static_cast<AssetHandle *>(payload->Data);
                            AssetType type = Project::GetInstance()->GetAssetManager().GetAssetType(handle);
                            if (type == AssetType::Texture)
                            {
                                material2D->textureHandle = handle;
                                material2D->SetDirtyFlag(true);
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }

                    ImGui::SameLine();
                    ImGui::Text("Texture: %llu", static_cast<u64>(material2D->textureHandle));
                }
                else
                {
                    static Sprite2DComponent s_Sprite2DBefore;

                    ImGui::DragFloat2("Tiling", &c.tilingFactor.x, 0.025f);
                    if (ImGui::IsItemActivated())            s_Sprite2DBefore = c;
                    if (ImGui::IsItemDeactivatedAfterEdit()) CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<Sprite2DComponent>>(m_Scene.get(), selectedEntity.GetUUID(), s_Sprite2DBefore, c));

                    ImGui::ColorEdit4("Color", &c.color.x);
                    if (ImGui::IsItemActivated())            s_Sprite2DBefore = c;
                    if (ImGui::IsItemDeactivatedAfterEdit()) CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<Sprite2DComponent>>(m_Scene.get(), selectedEntity.GetUUID(), s_Sprite2DBefore, c));
                }
            });

            RenderComponent<PointLight2DComponent>("Point Light 2D", selectedEntity, [&]()
            {
                PointLight2DComponent &c = selectedEntity.GetComponent<PointLight2DComponent>();
                ImGui::Checkbox("Enabled", &c.enabled);
                ImGui::ColorEdit4("Color", &c.color.x);
                ImGui::DragFloat("Radius", &c.radius, 0.025f, 0.0f, 10000.0f);
                ImGui::DragFloat("Intensity", &c.intensity, 0.025f, 0.0f, 10000.0f);
            });

			RenderComponent<Circle2DComponent>("Circle 2D", selectedEntity, [&]()
				{
					Circle2DComponent &c = selectedEntity.GetComponent<Circle2DComponent>();

					static Circle2DComponent compBefore;

					ImGui::ColorEdit4("Color", &c.color.x);
					if (ImGui::IsItemActivated())
						compBefore = c;

					if (ImGui::IsItemDeactivatedAfterEdit())
						CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<Circle2DComponent>>(m_Scene.get(), selectedEntity.GetUUID(), compBefore, c));
				});

			RenderComponent<StaticMeshComponent>("Static Mesh", selectedEntity, [&]()
			{
				StaticMeshComponent &c = selectedEntity.GetComponent<StaticMeshComponent>();

				bool isMeshLoaded = c.handle != AssetHandle(0);

				// Mesh loading section with better styling
				ImGui::AlignTextToFramePadding();
				ImGui::Text("Mesh Asset:");
				ImGui::SameLine();

				std::string buttonLabel = isMeshLoaded ? "Loaded" : "Drag Mesh Here";
				ImVec4 buttonColor = isMeshLoaded ? ImVec4(0.2f, 0.7f, 0.3f, 1.0f) : ImVec4(0.3f, 0.3f, 0.3f, 1.0f);

				ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
				ImGui::Button(buttonLabel.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - 30.0f, 0.0f));
				ImGui::PopStyleColor();

				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("content_browser_item"))
					{
						LOG_ASSERT(payload->DataSize == sizeof(AssetHandle), "WRONG ITEM, that should be an asset handle");
						AssetHandle handle = *static_cast<AssetHandle *>(payload->Data);
                        auto &assetManager = Project::GetInstance()->GetAssetManager();
                        AssetMetaData metadata = assetManager.GetMetaData(handle);

                        if (metadata.type == AssetType::GLTF)
                        {
                            metadata.type = AssetType::StaticMesh;
                            assetManager.AssignMetaData(handle, metadata);
                            assetManager.UnloadAsset(handle);
                        }

                        if (assetManager.GetAssetType(handle) == AssetType::StaticMesh)
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

                    ImGui::Indent(8.0f);
                    ImGui::TextDisabled("Handle: %llu", static_cast<u64>(c.handle));
                    ImGui::Unindent(8.0f);

                    Ref<StaticMesh> sm = Project::GetInstance()->GetAsset<StaticMesh>(c.handle);
                    if (sm)
                    {
                        ImGui::Spacing();
                        ImGui::Separator();
                        ImGui::Spacing();


                        // Display mesh instances with materials
                        int meshIndex = 0;
                        for (auto &m : sm->GetMeshInstances())
                        {
                            ImGui::PushID(meshIndex++);

                            // Mesh instance header
                            std::string meshLabel = "Mesh: " + m->GetName();

#if 0
                            bool meshTreeOpen = ImGui::TreeNodeEx(meshLabel.c_str(),
                                ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
                                ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding);

                            if (meshTreeOpen)
                            {
                                ImGui::Indent(8.0f);

                                // Material section
                                Ref<Material> mat = Project::GetInstance()->GetAsset<Material>(m->GetMaterialHandle());
                                if (mat)
                                {
                                    ImGui::Spacing();
                                    ImGui::Text("Material: %s", mat->name.c_str());
                                    ImGui::Spacing();

                                    // Material properties in columns
                                    ImGui::Columns(2, "material_props", false);
                                    ImGui::SetColumnWidth(0, 120.0f);

                                    // Base Color
                                    ImGui::Text("Base Color:");
                                    ImGui::NextColumn();
                                    ImGui::ColorEdit3("##BaseColor", &mat->gpuData.baseColorFactor.x,
                                        ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
                                    ImGui::NextColumn();

                                    // Metallic
                                    ImGui::Text("Metallic:");
                                    ImGui::NextColumn();
                                    ImGui::SliderFloat("##Metallic", &mat->gpuData.metallicFactor, 0.0f, 1.0f);
                                    ImGui::NextColumn();

                                    // Roughness
                                    ImGui::Text("Roughness:");
                                    ImGui::NextColumn();
                                    ImGui::SliderFloat("##Roughness", &mat->gpuData.roughnessFactor, 0.0f, 1.0f);
                                    ImGui::NextColumn();

                                    // Emissive
                                    ImGui::Text("Emissive:");
                                    ImGui::NextColumn();
                                    ImGui::ColorEdit4("##Emissive", &mat->gpuData.emissiveFactor.x,
                                        ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
                                    ImGui::NextColumn();

                                    // Occlusion Strength
                                    ImGui::Text("Occlusion:");
                                    ImGui::NextColumn();
                                    ImGui::SliderFloat("##Occlusion", &mat->gpuData.occlusionStrength, 0.0f, 1.0f);
                                    ImGui::NextColumn();

                                    ImGui::Columns(1);

                                    ImGui::Spacing();
                                    ImGui::Separator();
                                    ImGui::Spacing();

                                    // Texture previews
                                    ImGui::Text("Textures:");
                                    ImGui::Spacing();

                                    constexpr float thumbnailSize = 64.0f;
                                    constexpr float spacing = 8.0f;

                                    auto renderTexturePreview = [](const char *label, AssetHandle handle)
                                        {
                                            ImGui::PushID(label);
                                            ImGui::PushID(static_cast<int>(static_cast<uint64_t>(handle)));
                                            Ref<Texture> texture = Project::GetInstance()->GetAsset<Texture>(handle);

                                            if (texture && texture->GetHandle())
                                            {
                                                ImGui::BeginGroup();
                                                ImTextureID texID = (ImTextureID)texture->GetHandle().Get();
                                                ImGui::Image(texID, ImVec2(thumbnailSize, thumbnailSize));
                                                if (ImGui::IsItemHovered())
                                                {
                                                    ImGui::BeginTooltip();
                                                    ImGui::Image(texID, ImVec2(256.0f, 256.0f));
                                                    ImGui::EndTooltip();
                                                }
                                                ImGui::TextWrapped("%s", label);
                                                ImGui::EndGroup();
                                            }
                                            else
                                            {
                                                ImGui::BeginGroup();
                                                ImGui::Button("None", ImVec2(thumbnailSize, thumbnailSize));
                                                ImGui::TextWrapped("%s", label);
                                                ImGui::EndGroup();
                                            }
                                            ImGui::PopID();
                                            ImGui::PopID();
                                        };

                                    // First row of textures
                                    renderTexturePreview("Base Color", mat->baseColorTextureHandle);
                                    ImGui::SameLine(0.0f, spacing);
                                    renderTexturePreview("Normal", mat->normalTextureHandle);
                                    ImGui::SameLine(0.0f, spacing);
                                    renderTexturePreview("Metallic/Rough", mat->metallicRoughnessTextureHandle);

                                    // Second row of textures
                                    renderTexturePreview("Emissive", mat->emissiveTextureHandle);
                                    ImGui::SameLine(0.0f, spacing);
                                    renderTexturePreview("Occlusion", mat->occlusionTextureHandle);
                                }
                                else
                                {
                                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No Material Assigned");
                                }

                                ImGui::Unindent(8.0f);
                                ImGui::TreePop();
                            }
#endif
                            ImGui::PopID();
                            ImGui::Spacing();
                        }
                    }
                }
            });

         RenderComponent<SkeletalMeshComponent>("Skeletal Mesh", selectedEntity, [&]()
			{
				SkeletalMeshComponent &c = selectedEntity.GetComponent<SkeletalMeshComponent>();

				bool isMeshLoaded = c.handle != AssetHandle(0);

				// Mesh loading section with better styling
				ImGui::AlignTextToFramePadding();
				ImGui::Text("Mesh Asset:");
				ImGui::SameLine();

				std::string buttonLabel = isMeshLoaded ? "Loaded" : "Drag Mesh Here";
				ImVec4 buttonColor = isMeshLoaded ? ImVec4(0.2f, 0.7f, 0.3f, 1.0f) : ImVec4(0.3f, 0.3f, 0.3f, 1.0f);

				ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
				ImGui::Button(buttonLabel.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - 30.0f, 0.0f));
				ImGui::PopStyleColor();

				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("content_browser_item"))
					{
						LOG_ASSERT(payload->DataSize == sizeof(AssetHandle), "WRONG ITEM, that should be an asset handle");
						AssetHandle handle = *static_cast<AssetHandle *>(payload->Data);
                        auto &assetManager = Project::GetInstance()->GetAssetManager();
                        AssetMetaData metadata = assetManager.GetMetaData(handle);

                        if (metadata.type == AssetType::FBX)
                        {
                            metadata.type = AssetType::SkeletalMesh;
                            assetManager.AssignMetaData(handle, metadata);
                            assetManager.UnloadAsset(handle);
                        }

                        if (assetManager.GetAssetType(handle) == AssetType::SkeletalMesh)
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

					ImGui::Indent(8.0f);
					ImGui::TextDisabled("Handle: %llu", static_cast<u64>(c.handle));
					ImGui::Unindent(8.0f);

                    Ref<SkeletalMesh> sm = Project::GetInstance()->GetAsset<SkeletalMesh>(c.handle);
					if (sm)
					{
						ImGui::Spacing();
						ImGui::Separator();
						ImGui::Spacing();

						// Display mesh instances with materials
						int meshIndex = 0;
						for (auto &m : sm->GetMeshInstances())
						{
							ImGui::PushID(meshIndex++);

							// Mesh instance header

#if 0
							std::string meshLabel = "Mesh: " + m->GetName();
							bool meshTreeOpen = ImGui::TreeNodeEx(meshLabel.c_str(),
								ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
								ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding);

							if (meshTreeOpen)
							{
								ImGui::Indent(8.0f);

								// Material section
								Ref<Material> mat = Project::GetInstance()->GetAsset<Material>(m->GetMaterialHandle());
								if (mat)
								{
									ImGui::Spacing();
									ImGui::Text("Material: %s", mat->name.c_str());
									ImGui::Spacing();

									// Material properties in columns
									ImGui::Columns(2, "material_props", false);
									ImGui::SetColumnWidth(0, 120.0f);

									// Base Color
									ImGui::Text("Base Color:");
									ImGui::NextColumn();
									ImGui::ColorEdit3("##BaseColor", &mat->gpuData.baseColorFactor.x,
										ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
									ImGui::NextColumn();

									// Metallic
									ImGui::Text("Metallic:");
									ImGui::NextColumn();
									ImGui::SliderFloat("##Metallic", &mat->gpuData.metallicFactor, 0.0f, 1.0f);
									ImGui::NextColumn();

									// Roughness
									ImGui::Text("Roughness:");
									ImGui::NextColumn();
									ImGui::SliderFloat("##Roughness", &mat->gpuData.roughnessFactor, 0.0f, 1.0f);
									ImGui::NextColumn();

									// Emissive
									ImGui::Text("Emissive:");
									ImGui::NextColumn();
									ImGui::ColorEdit4("##Emissive", &mat->gpuData.emissiveFactor.x,
										ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
									ImGui::NextColumn();

									// Occlusion Strength
									ImGui::Text("Occlusion:");
									ImGui::NextColumn();
									ImGui::SliderFloat("##Occlusion", &mat->gpuData.occlusionStrength, 0.0f, 1.0f);
									ImGui::NextColumn();

									ImGui::Columns(1);

									ImGui::Spacing();
									ImGui::Separator();
									ImGui::Spacing();

									// Texture previews
									ImGui::Text("Textures:");
									ImGui::Spacing();

									constexpr float thumbnailSize = 64.0f;
									constexpr float spacing = 8.0f;

									auto renderTexturePreview = [](const char *label, AssetHandle handle)
										{
											ImGui::PushID(label);
											ImGui::PushID(static_cast<int>(static_cast<uint64_t>(handle)));
											Ref<Texture> texture = Project::GetInstance()->GetAsset<Texture>(handle);

											if (texture && texture->GetHandle())
											{
												ImGui::BeginGroup();
												ImTextureID texID = (ImTextureID)texture->GetHandle().Get();
												ImGui::Image(texID, ImVec2(thumbnailSize, thumbnailSize));
												if (ImGui::IsItemHovered())
												{
													ImGui::BeginTooltip();
													ImGui::Image(texID, ImVec2(256.0f, 256.0f));
													ImGui::EndTooltip();
												}
												ImGui::TextWrapped("%s", label);
												ImGui::EndGroup();
											}
											else
											{
												ImGui::BeginGroup();
												ImGui::Button("None", ImVec2(thumbnailSize, thumbnailSize));
												ImGui::TextWrapped("%s", label);
												ImGui::EndGroup();
											}
											ImGui::PopID();
											ImGui::PopID();
										};

									// First row of textures
									renderTexturePreview("Base Color", mat->baseColorTextureHandle);
									ImGui::SameLine(0.0f, spacing);
									renderTexturePreview("Normal", mat->normalTextureHandle);
									ImGui::SameLine(0.0f, spacing);
									renderTexturePreview("Metallic/Rough", mat->metallicRoughnessTextureHandle);

									// Second row of textures
									renderTexturePreview("Emissive", mat->emissiveTextureHandle);
									ImGui::SameLine(0.0f, spacing);
									renderTexturePreview("Occlusion", mat->occlusionTextureHandle);
								}
								else
								{
									ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No Material Assigned");
								}

								ImGui::Unindent(8.0f);
								ImGui::TreePop();
							}
#endif

							ImGui::PopID();
							ImGui::Spacing();
						}
					}
				}
			});

			RenderComponent<Rigidbody2DComponent>("Rigid Body 2D", selectedEntity, [&]()
            {
                Rigidbody2DComponent &c = selectedEntity.GetComponent<Rigidbody2DComponent>();

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

				ImGui::DragFloat2("Linear Vel", &c.linearVelocity.x, 0.025f, FLT_MIN, FLT_MAX);
				ImGui::DragFloat("Angular Vel", &c.angularVelocity, 0.025f, FLT_MIN, FLT_MAX);
				ImGui::DragFloat("Gravity", &c.gravityScale, 0.025f, FLT_MIN, FLT_MAX);
				ImGui::DragFloat("Linear Damping", &c.linearDamping, 0.0f, FLT_MAX);
				ImGui::DragFloat("Angular Damping", &c.angularDamping, 0.025f, 0.0f, FLT_MAX);
				ImGui::Checkbox("Awake", &c.isAwake);
				ImGui::Checkbox("Enabled", &c.isEnabled);
				ImGui::Checkbox("Sleep", &c.isEnableSleep);
				ImGui::Checkbox("Fixed Rotation", &c.fixedRotation);

				if (!c.fixedRotation)
				{
					ImGui::Checkbox("Fast Rotation", &c.allowFastRotation);
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
                    ImGui::DragFloat("Fov", &c.camera.fov, 0.025f, 0.0f, FLT_MAX);
                    if (ImGui::IsItemActivated())
                        s_CameraBefore = c;
                    if (ImGui::IsItemEdited())
                        c.dirty = true;
                    if (ImGui::IsItemDeactivatedAfterEdit()) 
                        CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<CameraComponent>>(m_Scene.get(), selectedEntity.GetUUID(), s_CameraBefore, c));
                }
                else
                {
                    ImGui::DragFloat("Ortho Size", &c.camera.orthoSize, 0.025f, 0.0f, FLT_MAX);
                    if (ImGui::IsItemActivated())
                        s_CameraBefore = c;
                    if (ImGui::IsItemEdited())
                        c.dirty = true;
                    if (ImGui::IsItemDeactivatedAfterEdit()) 
                        CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<CameraComponent>>(m_Scene.get(), selectedEntity.GetUUID(), s_CameraBefore, c));
                }

                ImGui::DragFloat("Near", &c.camera.nearPlane, 0.025f, 0.0f, FLT_MAX);
                if (ImGui::IsItemActivated())
                    s_CameraBefore = c;
                if (ImGui::IsItemEdited())
                    c.dirty = true;
                if (ImGui::IsItemDeactivatedAfterEdit())
                    CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<CameraComponent>>(m_Scene.get(), selectedEntity.GetUUID(), s_CameraBefore, c));

                ImGui::DragFloat("Far", &c.camera.farPlane, 0.025f, 0.0f, FLT_MAX);
                if (ImGui::IsItemActivated())
                    s_CameraBefore = c;
                if (ImGui::IsItemEdited())
                    c.dirty = true;
                if (ImGui::IsItemDeactivatedAfterEdit())
                    CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<CameraComponent>>(m_Scene.get(), selectedEntity.GetUUID(), s_CameraBefore, c));

                {
                    CameraComponent before = c;
                    if (ImGui::Checkbox("Primary", &c.primary))
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
                c.dirty = ImGui::DragFloat2("Size", &c.size.x, 0.025f, 0.0f, FLT_MAX);
                c.dirty |= ImGui::DragFloat2("Offset", &c.offset.x, 0.025f, 0.0f, FLT_MAX);
                c.dirty |= ImGui::DragFloat("Restitution", &c.restitution, 0.025f, 0.0f, FLT_MAX);
                c.dirty |= ImGui::DragFloat("Friction", &c.friction, 0.025f, 0.0f, FLT_MAX);
                c.dirty |= ImGui::DragFloat("Density", &c.density, 0.025f);
                c.dirty |= ImGui::Checkbox("Is Sensor", &c.isSensor);
            });

			RenderComponent<CircleCollider2DComponent>("Circle Collider 2D", selectedEntity, [&]()
				{
					CircleCollider2DComponent &cc = selectedEntity.GetComponent<CircleCollider2DComponent>();
					cc.dirty = ImGui::DragFloat("Radius", &cc.radius, 0.025f, 0.0f, FLT_MAX);
					cc.dirty |= ImGui::DragFloat2("Center", &cc.center.x, 0.025f, 0.0f, FLT_MAX);
					cc.dirty |= ImGui::DragFloat("Restitution", &cc.restitution, 0.025f, 0.0f, FLT_MAX);
					cc.dirty |= ImGui::DragFloat("Friction", &cc.friction, 0.025f, 0.0f, FLT_MAX);
					cc.dirty |= ImGui::DragFloat("Density", &cc.density, 0.025f);
					cc.dirty |= ImGui::Checkbox("Is Sensor", &cc.isSensor);
				});

            RenderComponent<RigibodyComponent>("Rigid Body", selectedEntity, [&]()
            {
                RigibodyComponent &c = selectedEntity.GetComponent<RigibodyComponent>();
                ImGui::Checkbox("Static", &c.isStatic);
            });

            RenderComponent<BoxColliderComponent>("Box Collider", selectedEntity, [&]()
            {
                BoxColliderComponent &c = selectedEntity.GetComponent<BoxColliderComponent>();
                c.dirty = ImGui::DragFloat3("Scale", &c.scale.x, 0.025f, 0.0f, 10000.0f);
                c.dirty |= ImGui::DragFloat("Friction", &c.friction, 0.025f);
                c.dirty |= ImGui::DragFloat("Static Friction", &c.staticFriction, 0.025f);
                c.dirty |= ImGui::DragFloat("Restitution", &c.restitution, 0.025f);
                c.dirty |= ImGui::DragFloat("Density", &c.density, 0.025f);
            });

            RenderComponent<SphereColliderComponent>("Sphere Collider", selectedEntity, [&]()
            {
                SphereColliderComponent &c = selectedEntity.GetComponent<SphereColliderComponent>();
                c.dirty = ImGui::DragFloat("Radius", &c.radius, 0.025f, 0.01f, 10000.0f);
                c.dirty |= ImGui::DragFloat("Friction", &c.friction, 0.025f);
                c.dirty |= ImGui::DragFloat("Static Friction", &c.staticFriction, 0.025f);
                c.dirty |= ImGui::DragFloat("Restitution", &c.restitution, 0.025f);
                c.dirty |= ImGui::DragFloat("Density", &c.density, 0.025f);
            });

            RenderComponent<CapsuleColliderComponent>("Capsule Collider", selectedEntity, [&]()
            {
                CapsuleColliderComponent &c = selectedEntity.GetComponent<CapsuleColliderComponent>();
                c.dirty = ImGui::DragFloat("Radius", &c.radius, 0.025f, 0.01f, 10000.0f);
                c.dirty |= ImGui::DragFloat("Height", &c.height, 0.025f, 0.01f, 10000.0f);
                c.dirty |= ImGui::DragFloat("Friction", &c.friction, 0.025f);
                c.dirty |= ImGui::DragFloat("Static Friction", &c.staticFriction, 0.025f);
                c.dirty |= ImGui::DragFloat("Restitution", &c.restitution, 0.025f);
                c.dirty |= ImGui::DragFloat("Density", &c.density, 0.025f);
            });

            RenderComponent<MeshColliderComponent>("Mesh Collider", selectedEntity, [&]()
            {
                MeshColliderComponent &c = selectedEntity.GetComponent<MeshColliderComponent>();
                c.dirty = ImGui::Checkbox("Convex", &c.convex);
                ImGui::Text("Vertices: %zu", c.vertices.size());
                ImGui::Text("Indices: %zu", c.indices.size());
                c.dirty |= ImGui::DragFloat("Friction", &c.friction, 0.025f);
                c.dirty |= ImGui::DragFloat("Static Friction", &c.staticFriction, 0.025f);
                c.dirty |= ImGui::DragFloat("Restitution", &c.restitution, 0.025f);
                c.dirty |= ImGui::DragFloat("Density", &c.density, 0.025f);
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
					std::string fontLabel = isFontLoaded ? "Font Loaded" : "Drag Font Here";
					ImGui::Button(fontLabel.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - 30.0f, 0.0f));

					if (ImGui::BeginDragDropTarget())
					{
						if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("content_browser_item"))
						{
							LOG_ASSERT(payload->DataSize == sizeof(AssetHandle), "WRONG ITEM, that should be an asset handle");
							AssetHandle handle = *static_cast<AssetHandle *>(payload->Data);
							if (Project::GetInstance()->GetAssetManager().GetAssetType(handle) == AssetType::Font)
							{
								c.fontHandle = handle;
							}
						}
						ImGui::EndDragDropTarget();
					}

					const bool isMaterialLoaded = c.material2dHandle != AssetHandle(0);
					std::string materialLabel = isMaterialLoaded ? "Material Loaded" : "Drag Material2D Here";
					ImGui::Button(materialLabel.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - 30.0f, 0.0f));

					if (ImGui::BeginDragDropTarget())
					{
						if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("content_browser_item"))
						{
							LOG_ASSERT(payload->DataSize == sizeof(AssetHandle), "WRONG ITEM, that should be an asset handle");
							AssetHandle handle = *static_cast<AssetHandle *>(payload->Data);
							if (Project::GetInstance()->GetAssetManager().GetAssetType(handle) == AssetType::Material2D)
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

						ImGui::SameLine();
						ImGui::Text("Material: %llu", static_cast<u64>(c.material2dHandle));
					}

					if (isFontLoaded)
					{
						ImGui::SameLine();
						if (ImGui::Button("X##ClearTextFont"))
						{
							c.fontHandle = AssetHandle(0);
						}

						ImGui::SameLine();
						ImGui::Text("Font: %llu", static_cast<u64>(c.fontHandle));
					}

					char textBuffer[2048] = {};
					strncpy(textBuffer, c.text.c_str(), sizeof(textBuffer) - 1);
					if (ImGui::InputTextMultiline("Text", textBuffer, sizeof(textBuffer), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 4.0f)))
					{
						c.text = textBuffer;
					}

					ImGui::ColorEdit4("Color", &c.color.x);
					ImGui::DragFloat("Kerning", &c.kerning, 0.001f, -10.0f, 10.0f);
					ImGui::DragFloat("Line Spacing", &c.lineSpacing, 0.001f, -10.0f, 10.0f);
					ImGui::Checkbox("Screen Space", &c.screenSpace);
				});

            RenderComponent<AudioSourceComponent>("Audio Source", selectedEntity, [&]()
            {
                AudioSourceComponent &c = selectedEntity.GetComponent<AudioSourceComponent>();

                ImGui::Button("Drag Here", { 65, 30.0f });

                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("content_browser_item"))
                    {
                        if (payload->DataSize == sizeof(AssetHandle))
                        {
                            AssetHandle *handle = static_cast<AssetHandle *>(payload->Data);
                            if (handle && *handle != AssetHandle(0))
                            {
                                AssetMetaData metadata = Project::GetInstance()->GetAssetManager().GetMetaData(*handle);
                                if (metadata.type == AssetType::Audio)
                                {
                                    c.handle = *handle;
                                    Ref<FmodSound> sound = Project::GetInstance()->GetAsset<FmodSound>(*handle);
                                }
                            }
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                ImGui::SameLine();
                ImGui::Text("%llu", c.handle);

                if (c.handle != AssetHandle(0))
                {
                    if (Ref<FmodSound> sound = Project::GetInstance()->GetAsset<FmodSound>(c.handle))
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

                        ImGui::DragFloat("Volume", &c.volume, 0.001f, 0.0f, 1.0f);
                        ImGui::DragFloat("Pitch", &c.pitch, 0.001f);
                        ImGui::DragFloat("Pan", &c.pan, 0.001f);
                        ImGui::Checkbox("Play On Start", &c.playOnStart);
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

								ImGui::Text(name.c_str());

                                ImGui::SameLine();

                                auto it = classRegisteredInstanceField->find(name);
								if (it != classRegisteredInstanceField->end())
								{
									ScriptInstanceField &instanceField = it->second;

									switch (instanceField.field.Type)
									{
									case ScriptFieldType::Float:
									{
										auto data = instanceField.GetValue<float>();
										if (ImGui::DragFloat("##_field_value", &data, 0.1f))
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
										if (ImGui::DragInt("##_field_value", &data))
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
										if (ImGui::DragFloat("##_field_value", &data, 0.1f))
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
										if (ImGui::DragFloat2("##_field_value", &data.x, 0.1f))
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
										if (ImGui::DragFloat3("##_field_value", &data.x, 0.1f))
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
										if (ImGui::DragFloat4("##_field_value", &data.x, 0.1f))
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

										ImGui::Button(label.c_str());

										if (ImGui::BeginDragDropTarget())
										{
											if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ENTITY_SOURCE_ITEM"))
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
												ImGui::Text("%llu", uuid);
											else
												ImGui::Text("Null Entity!");

											ImGui::EndTooltip();
										}

										ImGui::SameLine();
										if (ImGui::Button("X"))
										{
                                           if (scriptInstance)
                                                scriptInstance->SetFieldValue<uint64_t>(name, 0);
                                            else
                                                instanceField.SetValue<uint64_t>(0);
										}
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
                        /*bool found = false;
                        for (IComponent *comp : comps)
                        {
                            if (comp->GetType() == type)
                            {
                                found = true;
                                break;
                            }
                        }
                        if (found)
                        {
                            continue;
                        }*/

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
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

        if (m_Scene && m_Scene->IsDirty())
        {
            windowFlags |= ImGuiWindowFlags_UnsavedDocument;
        }

        ImGui::Begin("Viewport", nullptr, windowFlags);

        const ImGuiWindow* window = ImGui::GetCurrentWindow();

        m_IsFocused = ImGui::IsWindowFocused();
        m_IsHovered = ImGui::IsWindowHovered();

		static std::array<const char *, 3> kGizmoOperationLabels = { "Translate", "Rotate", "Scale" };
		int operationIndex = 0;
		switch (m_Gizmo.GetOperation())
		{
		case ImGuizmo::ROTATE: operationIndex = 1; break;
		case ImGuizmo::SCALE: operationIndex = 2; break;
		default: operationIndex = 0; break;
		}
		ImGui::SetNextItemWidth(90.0f);
		if (ImGui::Combo("##GizmoOperation", &operationIndex, kGizmoOperationLabels.data(), static_cast<int>(kGizmoOperationLabels.size())))
		{
			auto op = operationIndex == 0 ? ImGuizmo::TRANSLATE : operationIndex == 1 ? ImGuizmo::ROTATE : ImGuizmo::SCALE;
			m_Gizmo.SetOperation(op);
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

        State sceneState = EditorLayer::GetInstance()->GetState().sceneState;
        const bool isScenePlaying = sceneState == ignite::State::ScenePlay;
        Ref<Texture> scenePlayStopTex = isScenePlaying ? m_Icons["stop"] : m_Icons["play"];
        ImTextureID scenePlayStopID = (ImTextureID)scenePlayStopTex->GetHandle().Get();

        ImGui::SameLine();
        ImGui::Image(scenePlayStopID, buttonSize);
        if (ImGui::IsItemClicked())
        {  
            if (isScenePlaying)
            {
                EditorLayer::GetInstance()->OnSceneStop();
#if _WIN32
				HWND hwnd = Application::GetInstance()->GetWindow()->GetNativeWindow();
                COLORREF rgbRed = 0x00E86071;
                DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &rgbRed, sizeof(rgbRed));
#endif
            }
            else
            {
                EditorLayer::GetInstance()->OnScenePlay();
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
                EditorLayer::GetInstance()->OnSceneStop();
#if _WIN32
                HWND hwnd = Application::GetInstance()->GetWindow()->GetNativeWindow();
                COLORREF rgbRed = 0x00E86071;
                DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &rgbRed, sizeof(rgbRed));
#endif
            }
            else
            {
                EditorLayer::GetInstance()->OnSceneSimulate();
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
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("content_browser_item"))
            {
                if (payload->DataSize == sizeof(AssetHandle))
                {
                    auto* handle = static_cast<AssetHandle*>(payload->Data);
                    if (handle && *handle != AssetHandle(0))
                    {
                        AssetMetaData metadata = Project::GetInstance()->GetAssetManager().GetMetaData(*handle);
                        if (metadata.type == AssetType::Scene)
                        {
                            std::filesystem::path filepath = Project::GetInstance()->GetAssetFilepath(metadata.filepath);
                            EditorLayer::GetInstance()->OpenScene(filepath);
                        }
                    }
                }
            }

            ImGui::EndDragDropTarget();
        }

        GizmoInfo gizmoInfo;
        gizmoInfo.cameraView = m_EditorCamera.GetView();
        gizmoInfo.cameraProjection = m_EditorCamera.GetProjection();
        gizmoInfo.cameraType = m_EditorCamera.projectionType;
        gizmoInfo.snapValue = m_ViewportData.snapValue;
        gizmoInfo.viewRect = m_ViewportEditRT.rect;

        m_Gizmo.SetInfo(gizmoInfo);

        // Start manipulation: Fired only on the first frame of interaction
        bool isManipulatingNow = m_Gizmo.IsManipulating();

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
        m_Data.isGizmoBeingUse = isManipulatingNow || m_Gizmo.IsHovered();

        if (m_SelectedEntities.size() > 1)
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
            TransformComponent &tr = entity.GetTransform();
            glm::mat4 transformMatrix = tr.GetWorldMatrix();

            m_Gizmo.Manipulate(transformMatrix);

            if (m_Gizmo.IsManipulating())
            {
                glm::vec3 translation, rotation, scale;
                Math::DecomposeTransformEuler(transformMatrix, translation, rotation, scale);

                if (entity.GetParentUUID() != UUID(0))
                {
                    Entity parent = SceneManager::GetEntity(m_Scene.get(), entity.GetParentUUID());
                    const TransformComponent &parentTr = parent.GetTransform();
                    glm::vec4 localTranslation = glm::inverse(parentTr.GetWorldMatrix()) * glm::vec4(translation, 1.0f);
                    tr.localTranslation = localTranslation;
                    tr.localRotation = glm::inverse(parentTr.rotation) * glm::quat(rotation);
                    tr.localScale = scale / parentTr.scale;
                }
                else
                {
                    tr.localTranslation = translation;
                    tr.localRotation = glm::quat(rotation);
                    tr.localScale = scale;
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

        ImGui::End();
    }

	void ScenePanel::RenderSceneGameViewport()
	{
        if (ImGui::Begin("Game"))
        {
			// Calculating Scene Viewport location
			const ImVec2 &canvasPos = ImGui::GetCursorScreenPos();
			const ImVec2 &canvasSize = ImGui::GetContentRegionAvail();

			// Preview camera
            if (Entity cameraEntity = m_Scene->GetPrimaryCamera())
            {
                CameraComponent &cameraComp = cameraEntity.GetComponent<CameraComponent>();

                ImVec2 imagePos = canvasPos;
                ImVec2 imageSize = canvasSize;

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
                    imageSize.x = safeCanvasH * targetAspect;
                    imagePos.x += (safeCanvasW - imageSize.x) * 0.5f;
                }
                else
                {
                    imageSize.y = safeCanvasW / targetAspect;
                    imagePos.y += (safeCanvasH - imageSize.y) * 0.5f;
                }

                m_ViewportGameRT.rect.min = { imagePos.x, imagePos.y };
                m_ViewportGameRT.rect.max = { imagePos.x + imageSize.x, imagePos.y + imageSize.y };

                ImTextureID previewImage = (ImTextureID)m_ViewportGameRT.composite->GetColorAttachment(0)->GetHandle().Get();
                ImGui::SetCursorScreenPos(imagePos);
                ImGui::Image(previewImage, imageSize);
            }
            else
            {
                m_ViewportGameRT.rect.min = { canvasPos.x, canvasPos.y };
                m_ViewportGameRT.rect.max = { canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y };
                ImGui::Text("No Camera");
            }

        }
        ImGui::End();
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

    void ScenePanel::SetGizmoOperation(ImGuizmo::OPERATION op)
    {
        m_Gizmo.SetOperation(op);
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
		if (m_IsHovered && !m_Gizmo.IsManipulating() && !m_Gizmo.IsHovered())
		{
			m_EditorCamera.HandleOrbit(deltaTime);
			m_EditorCamera.HandlePan(deltaTime);
			m_EditorCamera.HandleZoom(deltaTime);
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
        if (EditorLayer::GetInstance()->GetState().multiSelect)
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
            m_Gizmo.SetOperation(ImGuizmo::OPERATION::NONE);
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
