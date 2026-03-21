/* MIT License
* 
* Copyright (c) 2025 Evangelion Manuhutu | IGNITE STUDIO
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

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

#include "ignite/scripting/script_engine.hpp"
#include "ignite/scripting/script_field.hpp"
#include "ignite/scripting/script_instance.hpp"
#include "ignite/asset/asset_importer.hpp"

#include "ignite/scene/entity.hpp"
#include "../states.hpp"

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

    ScenePanel::ScenePanel(const char *windowTitle)
        : IPanel(windowTitle), m_Gizmo()
    {
        Application* app = Application::GetInstance();

        auto width = static_cast<float>(app->GetCreateInfo().width);
        auto height = static_cast<float>(app->GetCreateInfo().height);

        m_Camera = EditorCamera("ScenePanel-Editor Camera");

		m_Camera.target = glm::vec3(0.0f);
		m_Camera.distance = 5.5f;
		m_Camera.yaw = glm::radians(90.0f);
		m_Camera.pitch = 0.0f;

		m_Camera.UpdateSphericalPosition();
		m_Camera.UpdateMatrices(width, height);

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
        m_SceneViewportRT = RenderTarget::Create(rtCreateInfo, "[SceneViewportRT]");
        m_SceneCameraRT = RenderTarget::Create(rtCreateInfo, "[SceneCameraRT]");
        m_UIViewportRT = RenderTarget::Create(rtCreateInfo, "[UIViewportRT]");
        m_UICameraRT = RenderTarget::Create(rtCreateInfo, "[UICameraRT]");

        // Composite render target
        {
            RenderTargetCreateInfo rtCreateInfo = {};
            rtCreateInfo.attachments =
            {
                //FramebufferAttachments{ "[Composite Depth Attachment]", nvrhi::Format::D32S8, nvrhi::ResourceStates::DepthWrite }, // Depth
                FramebufferAttachments{ "[Composite Color Attachment]", nvrhi::Format::RGBA8_UNORM, nvrhi::ResourceStates::RenderTarget} // Main Color
            };

            m_CompositeViewportRT = RenderTarget::Create(rtCreateInfo, "[CompositeViewportRT]");
            m_CompositeCameraRT = RenderTarget::Create(rtCreateInfo, "[CompositeCameraRT]");
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
        
        RenderViewport();
    }

    void ScenePanel::OnUpdate(f32 deltaTime)
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
                if (IDComponent &idComp = src.GetComponent<IDComponent>(); idComp.parent != 0)
                {
                    // current parent should be removed it
                    Entity parent = SceneManager::GetEntity(m_Scene.get(), idComp.parent);
                    parent.GetComponent<IDComponent>().RemoveChild(idComp.uuid);

                    // reset the parent to 0
                    idComp.parent = UUID(0);
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
            entity = SetSelectedEntity(SceneManager::CreateEntity(m_Scene.get(), "Empty", EntityType_Node));
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
                    
                    // the current 'entity' is the target (new parent for src)
                    SceneManager::AddChild(m_Scene.get(), entity, src);
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
                SceneManager::RenameEntity(m_Scene.get(), selectedEntity, std::string(buffer));
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
                if (ImGui::DragFloat3("Translation", &comp.localTranslation.x, 0.025f))
                {
                    comp.dirty = true;
                }

                glm::vec3 euler = eulerAngles(comp.localRotation);
                if (ImGui::DragFloat3("Rotation", &euler.x, 0.025f))
                {
                    comp.localRotation = glm::quat(euler);
                    comp.dirty = true;
                }
                if (ImGui::DragFloat3("Scale", &comp.localScale.x, 0.025f))
                {
                    comp.dirty = true;
                }

            }, false); // false: not allowed to remove the component

            RenderComponent<Sprite2DComponent>("Sprite 2D", selectedEntity, [&]()
            {
                Sprite2DComponent &c = selectedEntity.GetComponent<Sprite2DComponent>();

                bool isTextureLoaded = c.handle != AssetHandle(0);

                std::string btLabel = "Load Texture";
                if (isTextureLoaded)
                {
                    btLabel = "Loaded";
                }

                if (ImGui::Button(btLabel.c_str()))
                {
                }

                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("content_browser_item"))
                    {
                        LOG_ASSERT(payload->DataSize == sizeof(AssetHandle), "WRONG ITEM, that should be an asset handle");
                        AssetHandle handle = *static_cast<AssetHandle *>(payload->Data);
                        AssetType type = Project::GetInstance()->GetAssetManager().GetAssetType(handle);
                        if (type == AssetType::Texture)
                        {
                            c.handle = handle;
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                if (isTextureLoaded)
                {
                    ImGui::SameLine();
                    if (ImGui::Button("X"))
                    {
                        c.handle = AssetHandle(0); // reset the texture handle
                    }

                    ImGui::SameLine();
                    ImGui::Text("Handle: %llu", static_cast<u64>(c.handle));
                }

                ImGui::DragFloat2("Tiling", &c.tilingFactor.x, 0.025f);
                ImGui::ColorEdit4("Color", &c.color.x);
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
						AssetType type = Project::GetInstance()->GetAssetManager().GetAssetType(handle);
						if (type == AssetType::StaticMesh)
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

                if (m_Scene->IsRunning())
                {
                    if (ImGui::DragFloat2("Linear Vel", &c.linearVelocity.x, 0.025f))
                        b2Body_SetLinearVelocity(c.bodyId, { c.linearVelocity.x, c.linearVelocity.y });

                    if (ImGui::DragFloat("Angular Vel", &c.angularVelocity, 0.025f))
                        b2Body_SetAngularVelocity(c.bodyId, c.angularVelocity);

                    if (ImGui::DragFloat("Gravity", &c.gravityScale, 0.025f))
                        b2Body_SetGravityScale(c.bodyId, c.gravityScale);


                    if (ImGui::DragFloat("Linear Damping", &c.linearDamping, 0.025f))
                        b2Body_SetLinearDamping(c.bodyId, c.linearDamping);

                    if (ImGui::DragFloat("Angular Damping", &c.angularDamping, 0.025f))
                        b2Body_SetAngularDamping(c.bodyId, c.angularDamping);

                    if (ImGui::Checkbox("Awake", &c.isAwake))
                        b2Body_SetAwake(c.bodyId, c.isAwake);

                    if (ImGui::Checkbox("Enabled", &c.isEnabled))
                    {
                        c.isEnabled ? b2Body_Enable(c.bodyId) : b2Body_Disable(c.bodyId);
                    }

                    if (ImGui::Checkbox("Sleep", &c.isEnableSleep))
                        b2Body_EnableSleep(c.bodyId, c.isEnableSleep);
                }
                else
                {
                    ImGui::DragFloat2("Linear Vel", &c.linearVelocity.x, 0.025f, FLT_MIN, FLT_MAX);
                    ImGui::DragFloat("Angular Vel", &c.angularVelocity, 0.025f, FLT_MIN, FLT_MAX);
                    ImGui::DragFloat("Gravity", &c.gravityScale, 0.025f, FLT_MIN, FLT_MAX);
                    ImGui::DragFloat("Linear Damping", &c.linearDamping, 0.0f, FLT_MAX);
                    ImGui::DragFloat("Angular Damping", &c.angularDamping, 0.025f, 0.0f, FLT_MAX);
                    ImGui::Checkbox("Awake", &c.isAwake);
                    ImGui::Checkbox("Enabled", &c.isEnabled);
                    ImGui::Checkbox("Sleep", &c.isEnableSleep);
                }
            });
            RenderComponent<CameraComponent>("Camera", selectedEntity, [&]()
            {
                CameraComponent &c = selectedEntity.GetComponent<CameraComponent>();

                static const char *projectionTypeStr[] = { "Orthographic", "Perspective" };
                const char *currentProjectionTypeStr = projectionTypeStr[static_cast<int>(c.camera.projectionType)];

                if (ImGui::BeginCombo("Projection", currentProjectionTypeStr))
                {
                    for (size_t i = 0; i < std::size(projectionTypeStr); ++i)
                    {
                        bool isSelected = false;
                        if (ImGui::Selectable(projectionTypeStr[i], &isSelected))
                        {
                            c.camera.projectionType = static_cast<ProjectionType>(i);
                            c.camera.UpdateMatrices(static_cast<float>(m_Scene->GetViewportWidth()), static_cast<float>(m_Scene->GetViewportHeight()));
                        }

                        if (isSelected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }

                bool modified = false;

                if (c.camera.projectionType == ProjectionType::Perspective)
                {
                    modified |= ImGui::DragFloat("Fov", &c.camera.fov, 0.025f, 0.0f, FLT_MAX);
                }
                else
                {
                    modified |= ImGui::DragFloat("Zoom", &c.camera.distance, 0.025f, 0.0f, FLT_MAX);
                }

                modified |= ImGui::DragFloat("Near", &c.camera.nearPlane, 0.025f, 0.0f, FLT_MAX);
                modified |= ImGui::DragFloat("Far", &c.camera.farPlane, 0.025f, 0.0f, FLT_MAX);
                modified |= ImGui::Checkbox("Primary", &c.primary);

                if (modified)
                {
                    c.camera.UpdateMatrices(static_cast<float>(m_Scene->GetViewportWidth()), static_cast<float>(m_Scene->GetViewportHeight()));
                }
            });

            RenderComponent<BoxCollider2DComponent>("Box Collider 2D", selectedEntity, [&]()
            {
                BoxCollider2DComponent &c = selectedEntity.GetComponent<BoxCollider2DComponent>();
                ImGui::DragFloat2("Size", &c.size.x, 0.025f, 0.0f, FLT_MAX);
                ImGui::DragFloat2("Offset", &c.offset.x, 0.025f, 0.0f, FLT_MAX);
                ImGui::DragFloat("Restitution", &c.restitution, 0.025f, 0.0f, FLT_MAX);
                ImGui::DragFloat("Friction", &c.friction, 0.025f, 0.0f, FLT_MAX);
                ImGui::DragFloat("Density", &c.density, 0.025f);
                ImGui::Checkbox("Is Sensor", &c.isSensor);
            });

            RenderComponent<RigibodyComponent>("Rigid Body", selectedEntity, [&]()
            {
                RigibodyComponent &c = selectedEntity.GetComponent<RigibodyComponent>();
                ImGui::Checkbox("Static", &c.isStatic);
            });

            RenderComponent<BoxColliderComponent>("Box Collider", selectedEntity, [&]()
            {
                BoxColliderComponent &c = selectedEntity.GetComponent<BoxColliderComponent>();
                ImGui::DragFloat3("Scale", &c.scale.x, 0.025f, 0.0f, 10000.0f);
                ImGui::DragFloat("Friction", &c.friction, 0.025f);
                ImGui::DragFloat("Static Friction", &c.staticFriction, 0.025f);
                ImGui::DragFloat("Restitution", &c.restitution, 0.025f);
                ImGui::DragFloat("Density", &c.density, 0.025f);
            });

            RenderComponent<SphereColliderComponent>("Sphere Collider", selectedEntity, [&]()
            {
                SphereColliderComponent &c = selectedEntity.GetComponent<SphereColliderComponent>();
                ImGui::DragFloat("Radius", &c.radius, 0.025f, 0.01f, 10000.0f);
                ImGui::DragFloat("Friction", &c.friction, 0.025f);
                ImGui::DragFloat("Static Friction", &c.staticFriction, 0.025f);
                ImGui::DragFloat("Restitution", &c.restitution, 0.025f);
                ImGui::DragFloat("Density", &c.density, 0.025f);
            });

            RenderComponent<CapsuleColliderComponent>("Capsule Collider", selectedEntity, [&]()
            {
                CapsuleColliderComponent &c = selectedEntity.GetComponent<CapsuleColliderComponent>();
                ImGui::DragFloat("Radius", &c.radius, 0.025f, 0.01f, 10000.0f);
                ImGui::DragFloat("Height", &c.height, 0.025f, 0.01f, 10000.0f);
                ImGui::DragFloat("Friction", &c.friction, 0.025f);
                ImGui::DragFloat("Static Friction", &c.staticFriction, 0.025f);
                ImGui::DragFloat("Restitution", &c.restitution, 0.025f);
                ImGui::DragFloat("Density", &c.density, 0.025f);
            });

            RenderComponent<MeshColliderComponent>("Mesh Collider", selectedEntity, [&]()
            {
                MeshColliderComponent &c = selectedEntity.GetComponent<MeshColliderComponent>();
                ImGui::Checkbox("Convex", &c.convex);
                ImGui::Text("Vertices: %zu", c.vertices.size());
                ImGui::Text("Indices: %zu", c.indices.size());
                ImGui::DragFloat("Friction", &c.friction, 0.025f);
                ImGui::DragFloat("Static Friction", &c.staticFriction, 0.025f);
                ImGui::DragFloat("Restitution", &c.restitution, 0.025f);
                ImGui::DragFloat("Density", &c.density, 0.025f);
                if (ImGui::Button("Clear Mesh Data"))
                {
                    c.vertices.clear();
                    c.indices.clear();
                }
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

                bool detached = c.className == "Detached";

                // classFields
                bool isRunning = m_Scene->IsRunning();

                // Editable classFields (edit mode)
                if (isRunning && !detached)
                {
                    if (Ref<ScriptInstance> scriptInstance = ScriptEngine::GetInstance()->GetEntityScriptInstance(selectedEntity.GetUUID()))
                    {
                        auto classFields = scriptInstance->GetScriptClass()->GetFields();

                        for (const auto &[fieldName, field] : classFields)
                        {
                            switch (field.Type)
                            {
                            case ScriptFieldType::Float:
                            {
                                float data = scriptInstance->GetFieldValue<float>(fieldName);
                                if (ImGui::DragFloat(fieldName.c_str(), &data, 0.1f))
                                {
                                    scriptInstance->SetFieldValue<float>(fieldName, data);
                                }
                                break;
                            }
                            case ScriptFieldType::Int:
                            {
                                int data = scriptInstance->GetFieldValue<int>(fieldName);
                                if (ImGui::DragInt(fieldName.c_str(), &data, 1))
                                {
                                    scriptInstance->SetFieldValue<int>(fieldName, data);
                                }
                                break;
                            }
                            case ScriptFieldType::Vector2:
                            {
                                auto data = scriptInstance->GetFieldValue<glm::vec2>(fieldName);
                                if (ImGui::DragFloat2(fieldName.c_str(), &data.x, 0.1f))
                                {
                                    scriptInstance->SetFieldValue<glm::vec2>(fieldName, data);
                                }
                                break;
                            }
                            case ScriptFieldType::Vector3:
                            {
                                auto data = scriptInstance->GetFieldValue<glm::vec3>(fieldName);
                                if (ImGui::DragFloat3(fieldName.c_str(), &data.x, 0.1f))
                                {
                                    scriptInstance->SetFieldValue<glm::vec3>(fieldName, data);
                                }
                                break;
                            }
                            case ScriptFieldType::Vector4:
                            {
                                auto data = scriptInstance->GetFieldValue<glm::vec4>(fieldName);
                                if (ImGui::DragFloat4(fieldName.c_str(), &data.x, 0.1f))
                                {
                                    scriptInstance->SetFieldValue<glm::vec4>(fieldName, data);
                                }
                                break;
                            }
                            case ScriptFieldType::Entity:
                            {
                                auto uuid = scriptInstance->GetFieldValue<uint64_t>(fieldName);
                                if (Entity entity = SceneManager::GetEntity(m_Scene.get(), UUID(uuid)))
                                {
                                    ImGui::Button(fieldName.c_str());
                                    if (ImGui::IsItemHovered())
                                    {
                                        ImGui::BeginTooltip();
                                        ImGui::Text("%llu", uuid);
                                        ImGui::EndTooltip();
                                    }
                                }
                                break;
                            }
                            }
                        }
                    }
                }

                // Scene is running, we don't have to set anything
                else if (!isRunning && scriptClassExist && !detached)
                {
                    // !IsRunning
                    Ref<ScriptClass> entityClass = ScriptEngine::GetInstance()->GetEntityClassesByName(c.className);
                    if (entityClass)
                    {
                        const auto &classFields = entityClass->GetFields();
                        auto &entityFields = ScriptEngine::GetInstance()->GetScriptFieldMap(selectedEntity);

                        for (const auto &[name, field] : classFields)
                        {
                            if (entityFields.find(name) != entityFields.end())
                            {
                                ScriptFieldInstance &scriptField = entityFields.at(name);

                                switch (field.Type)
                                {
                                case ScriptFieldType::Float:
                                {
                                    auto data = scriptField.GetValue<float>();
                                    if (ImGui::DragFloat(name.c_str(), &data, 0.1f))
                                    {
                                        scriptField.SetValue<float>(data);
                                    }
                                    break;
                                }
                                case ScriptFieldType::Int:
                                {
                                    auto data = scriptField.GetValue<int>();
                                    if (ImGui::DragInt(name.c_str(), &data))
                                    {
                                        scriptField.SetValue<int>(data);
                                    }
                                    break;
                                }
                                case ScriptFieldType::Vector2:
                                {
                                    auto data = scriptField.GetValue<glm::vec2>();
                                    if (ImGui::DragFloat2(name.c_str(), &data.x, 0.1f))
                                    {
                                        scriptField.SetValue<glm::vec2>(data);
                                    }
                                    break;
                                }
                                case ScriptFieldType::Vector3:
                                {
                                    auto data = scriptField.GetValue<glm::vec3>();
                                    if (ImGui::DragFloat3(name.c_str(), &data.x, 0.1f))
                                    {
                                        scriptField.SetValue<glm::vec3>(data);
                                    }
                                    break;
                                }
                                case ScriptFieldType::Vector4:
                                {
                                    auto data = scriptField.GetValue<glm::vec4>();
                                    if (ImGui::DragFloat4(name.c_str(), &data.x, 0.1f))
                                    {
                                        scriptField.SetValue<glm::vec4>(data);
                                    }
                                    break;
                                }
                                case ScriptFieldType::Entity:
                                {
                                    auto uuid = scriptField.GetValue<uint64_t>();
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
                                                Entity src{ *static_cast<entt::entity *>(payload->Data), m_Scene.get()};
                                                uint64_t id = (uint64_t)src.GetUUID();
                                                scriptField.Field.Type = ScriptFieldType::Entity;
                                                scriptField.SetValue<uint64_t>(id);
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
                                        scriptField.SetValue<uint64_t>(0);
                                    }
                                    break;
                                }
                                }
                            }
                            else
                            {
                                ScriptFieldInstance &fieldInstance = entityFields[name];
                                switch (field.Type)
                                {
                                case ScriptFieldType::Float:
                                {
                                    float data = 0.0f;
                                    if (ImGui::DragFloat(name.c_str(), &data, 0.1f))
                                    {
                                        fieldInstance.Field = field;
                                        fieldInstance.SetValue<float>(data);
                                    }
                                    break;
                                }
                                case ScriptFieldType::Int:
                                {
                                    int data = 0;
                                    if (ImGui::DragInt(name.c_str(), &data))
                                    {
                                        fieldInstance.Field = field;
                                        fieldInstance.SetValue<int>(data);
                                    }
                                    break;
                                }
                                case ScriptFieldType::Vector2:
                                {
                                    glm::vec2 data(0.0f);
                                    if (ImGui::DragFloat2(name.c_str(), &data.x, 0.1f))
                                    {
                                        fieldInstance.Field = field;
                                        fieldInstance.SetValue<glm::vec2>(data);
                                    }
                                    break;
                                }
                                case ScriptFieldType::Vector3:
                                {
                                    glm::vec3 data(0.0f);
                                    if (ImGui::DragFloat3(name.c_str(), &data.x, 0.1f))
                                    {
                                        fieldInstance.Field = field;
                                        fieldInstance.SetValue<glm::vec3>(data);
                                    }
                                    break;
                                }
                                case ScriptFieldType::Vector4:
                                {
                                    glm::vec4 data(0.0f);
                                    if (ImGui::DragFloat4(name.c_str(), &data.x, 0.1f))
                                    {
                                        fieldInstance.Field = field;
                                        fieldInstance.SetValue<glm::vec4>(data);
                                    }
                                    break;
                                }
                                case ScriptFieldType::Entity:
                                {
                                    ImGui::Button("Drag Here");

                                    if (ImGui::BeginDragDropTarget())
                                    {
                                        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ENTITY_SOURCE_ITEM"))
                                        {
                                            LOG_ASSERT(payload->DataSize == sizeof(Entity), "WRONG ENTITY ITEM");
                                            if (payload->DataSize == sizeof(Entity))
                                            {
                                                Entity src{ *static_cast<entt::entity *>(payload->Data), m_Scene.get()};
                                                fieldInstance.Field = field;
                                                fieldInstance.SetValue<uint64_t>(src.GetUUID());
                                            }
                                        }
                                        ImGui::EndDragDropTarget();
                                    }

                                    if (ImGui::IsItemHovered())
                                    {
                                        ImGui::BeginTooltip();
                                        ImGui::Text("Null Entity!");
                                        ImGui::EndTooltip();
                                    }

                                    ImGui::SameLine();
                                    if (ImGui::Button("X"))
                                    {
                                        fieldInstance.Field = field;
                                        fieldInstance.SetValue<uint64_t>(0);
                                    }

                                    break;
                                }
                                default: break;
                                }
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
                    case CompType_Rigidbody2D:
                        entity.AddComponent<Rigidbody2DComponent>();
                        break;
                    case CompType_BoxCollider2D:
                        entity.AddComponent<BoxCollider2DComponent>();
                        break;
                    case CompType_StaticMesh:
                        entity.AddComponent<StaticMeshComponent>();
                        break;
                        // case CompType_SkeletalMesh:
                        //     entity.AddComponent<SkeletalMeshComponent>();
                        //     break;
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

    void ScenePanel::RenderViewport()
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

		ImGui::TextUnformatted("Operation");
		ImGui::SameLine();
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

		ImGui::TextUnformatted("Mode");
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
        ImGui::TextUnformatted("Snapping");
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

        m_ViewportData.rect.min = { canvasPos.x, canvasPos.y };
        m_ViewportData.rect.max = { canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y };

        // Mouse position in screen space
        const ImVec2 &mousePos = ImGui::GetMousePos();
        m_ViewportData.mousePos = { mousePos.x - canvasPos.x, mousePos.y - canvasPos.y };

        // Update UI input handling
        auto sceneRenderer = m_Scene->GetSceneRenderer();
        if (sceneRenderer)
        {
            glm::vec2 viewportPos = { canvasPos.x, canvasPos.y };
            glm::vec2 viewportSize = { canvasSize.x, canvasSize.y };
            glm::vec2 screenMousePos = { mousePos.x, mousePos.y };
            bool mousePressed = ImGui::IsMouseDown(ImGuiMouseButton_Left);
            
            sceneRenderer->UpdateUIInput(screenMousePos, viewportPos, viewportSize, mousePressed);
        }

        ImTextureID sceneImage = (ImTextureID)m_CompositeViewportRT->GetColorAttachment(0)->GetHandle().Get(); // Current composite RT
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
        gizmoInfo.cameraView = m_Camera.view;
        gizmoInfo.cameraProjection = m_Camera.projection;
        gizmoInfo.cameraType = m_Camera.projectionType;
        gizmoInfo.snapValue = m_ViewportData.snapValue;
        gizmoInfo.viewRect = m_ViewportData.rect;

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
        // Set the master flag for the current frame
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
        }

        // Preview camera
        if (Entity cameraEntity = GetSelectedEntity())
        {
            if (cameraEntity.HasComponent<CameraComponent>())
            {
                const glm::uvec2 previewRtSize = m_CompositeCameraRT->GetSize();
                ICamera previewCamera = cameraEntity.GetComponent<CameraComponent>().camera;
                if (auto sceneRenderer = m_Scene->GetSceneRenderer(); sceneRenderer && previewRtSize.x > 0u && previewRtSize.y > 0u)
                {
                    const float aspect = static_cast<float>(previewRtSize.x) / static_cast<float>(previewRtSize.y);
                    const float padding = 8.0f;
                    const float width = 256.0f;
                    const float height = width / aspect;

                    sceneRenderer->RenderTo(&previewCamera,
                        GetSceneCameraRT(),
                        GetUICameratRT(),
                        GetCompositeCameraRT());

                    ImGui::SetCursorPos({ canvasSize.x - width - padding, canvasSize.y - height + padding });
                    ImTextureID previewImage = (ImTextureID)m_CompositeCameraRT->GetColorAttachment(0)->GetHandle().Get();
                    ImGui::Image(previewImage, {width, height});
                }
            }
        }

        ImGui::End();
    }

    void ScenePanel::ResizeFramebuffer(uint32_t width, uint32_t height)
    {
        m_SceneViewportRT->Resize(width, height);
        m_CompositeViewportRT->Resize(width, height);
        m_UIViewportRT->Resize(width, height);

        m_UICameraRT->Resize(width, height);
        m_SceneCameraRT->Resize(width, height);
        m_CompositeCameraRT->Resize(width, height);

        if (m_Scene)
        {
            m_Scene->Resize(width, height);
            m_Camera.UpdateMatrices(static_cast<float>(width), static_cast<float>(height));
        }
    }

    void ScenePanel::UISettings()
    {
        // ImGui::Text("Mouse Pos: %.2f, %.2f Entity: (%d)", m_ViewportData.mousePos.x, m_ViewportData.mousePos.y, static_cast<entt::entity>(GetSelectedEntity()));

        if (ImGui::TreeNodeEx("Camera", ImGuiTreeNodeFlags_DefaultOpen))
        {
            static std::array<const char *, 2> cameraModeStr = { "Orthographic", "Perspective" };
            const char *currentCameraModeStr = cameraModeStr[static_cast<i32>(m_Camera.projectionType)];
            if (ImGui::BeginCombo("Mode", currentCameraModeStr))
            {
                for (size_t i = 0; i < std::size(cameraModeStr); ++i)
                {
                    bool isSelected = strcmp(currentCameraModeStr, cameraModeStr[i]) == 0;
                    if (ImGui::Selectable(cameraModeStr[i], isSelected))
                    {
                        // Set projection type
                        m_Camera.projectionType = static_cast<ProjectionType>(i);

                        // Recalculate matrices
                        const glm::vec2 &size = m_ViewportData.rect.GetSize();
                        m_Camera.UpdateMatrices(size.x, size.y);
                    }

                    if (isSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::DragFloat3("Position", &m_Camera.position[0], 0.025f);

            glm::vec2 yawPitch = { m_Camera.yaw, m_Camera.pitch };
            if (ImGui::DragFloat2("Yaw/Pitch", &yawPitch.x, 0.025f, -89.0f, 89.0f))
            {
                m_Camera.yaw = yawPitch.x;
                m_Camera.pitch = yawPitch.y;
            }

            if (m_Camera.projectionType == ProjectionType::Perspective)
            {
                ImGui::DragFloat("FOV", &m_Camera.fov, 0.25f, 20.0f, 120.0f);
                ImGui::DragFloat("Distance", &m_Camera.distance, 0.025f);

                const glm::vec2 &size = m_ViewportData.rect.GetSize();
                m_Camera.UpdateMatrices(size.x, size.y);
            }
            else if (m_Camera.projectionType == ProjectionType::Orthographic)
            {
                if (ImGui::DragFloat("Ortho size", &m_Camera.orthoSize, 0.025f))
                {
                    const glm::vec2 &size = m_ViewportData.rect.GetSize();
                    m_Camera.UpdateMatrices(size.x, size.y);
                }
            }

            ImGui::TreePop();
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
		dispatcher.Dispatch<MouseMovedEvent>(BIND_CLASS_EVENT_FN(ScenePanel::OnMouseMovedEvent));
    }

    bool ScenePanel::OnMouseScrolledEvent(MouseScrolledEvent &event)
    {
        if (m_IsHovered)
        {
			m_Camera.mouse.scroll = { event.GetXOffset(), event.GetYOffset() };
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

    void ScenePanel::UpdateCameraInput(f32 deltaTime)
    {
        for (const Ref<Joystick> &j : JoystickManager::GetConnectedJoystick())
        {
            const glm::vec2 &camViewAxis = j->GetRightAxis();
            const glm::vec2 &camMoveAxis = j->GetLeftAxis();
            const glm::vec2 &l2r2 = j->GetTriggerAxis();

            m_Camera.yaw += deltaTime * camViewAxis.x;
            m_Camera.pitch += deltaTime * camViewAxis.y;

            // m_Camera.position += m_Camera.GetForwardDirection() * deltaTime * m_CameraData.moveSpeed * -camMoveAxis.y;
            // m_Camera.position += m_Camera.GetRightDirection() * deltaTime * m_CameraData.moveSpeed * camMoveAxis.x;

            LOG_INFO(j->ToString());
        }

		m_Camera.UpdateMouseState();
		if (m_IsHovered && !m_Gizmo.IsManipulating() && !m_Gizmo.IsHovered())
		{
			m_Camera.HandleOrbit(deltaTime);
			m_Camera.HandlePan(deltaTime);
			m_Camera.HandleZoom(deltaTime);
		}
		m_Camera.ApplyInertia(deltaTime);
		m_Camera.UpdateCameraPosition();
    }

    void ScenePanel::DestroyEntity(Entity entity)
    {
        SceneManager::DestroyEntity(m_Scene.get(), entity);
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
