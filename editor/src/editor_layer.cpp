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

#include "editor_layer.hpp"
#include "panels/scene_panel.hpp"
#include "panels/content_browser_panel.hpp"
#include "panels/materials_panel.hpp"

#include "ignite/core/platform_utils.hpp"
#include "ignite/core/command.hpp"
#include "ignite/graphics/renderer_2d.hpp"
#include "ignite/imgui/gui_function.hpp"
#include "ignite/asset/asset.hpp"
#include "ignite/asset/asset_importer.hpp"
#include "ignite/scripting/script_engine.hpp"
#include "stb_image_write.h"

#include <cinttypes>
#include <SDL3/SDL_dialog.h>

#include "ignite/graphics/objects/shadow_map.hpp"

namespace ignite
{
    namespace
    {
        const SDL_DialogFileFilter kSceneFileFilters[] =
        {
            { "Ignite Scene (ixscene)", "ixscene" },
        };

        const SDL_DialogFileFilter kProjectFileFilters[] =
        {
            {"Ignite Project (ixproj)", "ixproj"}
        };
    }

    EditorLayer *s_EditorLayerInstance = nullptr;

    EditorLayer *EditorLayer::GetInstance()
    {
        return s_EditorLayerInstance;
    }

    EditorLayer::EditorLayer(const std::string &name)
        : Layer(name)
    {
        s_EditorLayerInstance = this;
    }

    EditorLayer::~EditorLayer()
    {
        s_EditorLayerInstance = nullptr;
    }

    void EditorLayer::OnAttach()
    {
        Layer::OnAttach();

        m_Device = Application::GetGraphicsDevice();

        m_ScenePanel = CreateRef<ScenePanel>("Scene Panel");
        m_ContentBrowserPanel = CreateRef<ContentBrowserPanel>("Content Browser");
        m_MaterialsPanel = CreateRef<MaterialsPanel>();

        // Set up material panel callbacks
        m_MaterialsPanel->SetMaterialSelectionCallback([this](Ref<Material> material) {
            // Optional: Handle material selection in main editor
        });
        
        m_MaterialsPanel->SetMaterialEditCallback([this](Ref<Material> material) {

        });

        // create render target framebuffer
        m_SceneRenderer.Create();
        m_CommandList = CommandList::Create();

        const auto &cmdArgs = Application::GetInstance()->GetCreateInfo().cmdLineArgs;
        for (int i = 0; i < cmdArgs.count; ++i)
        {
            std::string args = cmdArgs[i];

            char projectArgs[] = "--project=";
            if (args.find(projectArgs) != std::string::npos)
            {
                std::string projectFilepath = args.substr(std::size(projectArgs) - 1, args.size() - std::size(projectArgs) + 1);
                OpenProject(projectFilepath);
            }
        }

        Application::GetInstance()->GetWindow()->Show();
    }

    void EditorLayer::OnDetach()
    {
        Layer::OnDetach();
    }

    void EditorLayer::OnUpdate(float deltaTime)
    {
        Layer::OnUpdate(deltaTime);

        ProcessPendingFileLoading();

        if (m_ContentBrowserPanel)
        {
            m_ContentBrowserPanel->OnUpdate(deltaTime);
        }

        Renderer::OnUpdate();

        // update panels
        if (m_ActiveScene)
        {
            // multi select entity
            m_Data.multiSelect = Input::IsModifierPressed(KeyMod::LeftShift);

            switch (m_Data.sceneState)
            {
            case State::SceneSimulate:
            case State::ScenePlay:
            {
                m_ActiveScene->OnUpdateRuntimeSimulate(deltaTime);
                break;
            }
            case State::SceneEdit:
            {
                m_ActiveScene->OnUpdateEdit(deltaTime);
                break;
            }
            }

            m_ScenePanel->OnUpdate(deltaTime);
        }
    }

    void EditorLayer::OnEvent(Event &e)
    {
        Layer::OnEvent(e);
        m_ScenePanel->OnEvent(e);

        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<KeyPressedEvent>(BIND_CLASS_EVENT_FN(EditorLayer::OnKeyPressedEvent));
        dispatcher.Dispatch<MouseButtonPressedEvent>(BIND_CLASS_EVENT_FN(EditorLayer::OnMouseButtonPressed));
    }

    bool EditorLayer::OnKeyPressedEvent(KeyPressedEvent &event)
    {
        bool control = Input::IsModifierPressed(KeyMod::Control);
        bool shift = Input::IsModifierPressed(KeyMod::LeftShift);

        if (ImGui::GetIO().WantTextInput)
            return false;

        switch (event.GetKeyCode())
        {
            case Key::F:
            {
                Entity entity = m_ScenePanel->GetSelectedEntity();
                if (entity.IsValid())
                {
                    m_ScenePanel->GetViewportCamera().target = entity.GetComponent<TransformComponent>().translation;
                }
                break;
            }
            case Key::Escape:
            {
                if (m_ScenePanel->IsFocused())
                {
                    m_ScenePanel->SetGizmoOperation(ImGuizmo::OPERATION::NONE);
                }
                break;
            }
            case Key::S:
            {
                if (control)
                {
                    if (shift)
                        SaveProjectAs();
                    else
                        SaveProject();
                }
                break;
            }
            case Key::T:
            {
                if (!Input::IsMouseButtonPressed(Mouse::ButtonRight))
                    m_ScenePanel->SetGizmoOperation(ImGuizmo::OPERATION::TRANSLATE);
                break;
            }
            case Key::R:
            {
                if (!Input::IsMouseButtonPressed(Mouse::ButtonRight))
                    m_ScenePanel->SetGizmoOperation(ImGuizmo::OPERATION::ROTATE);
                break;
            }
            case Key::E:
            {
                if (!Input::IsMouseButtonPressed(Mouse::ButtonRight))
                    m_ScenePanel->SetGizmoOperation(ImGuizmo::OPERATION::SCALE);
                break;
            }
            case Key::F5:
            {
                (m_Data.sceneState == State::SceneEdit || m_Data.sceneState == State::SceneSimulate)
                    ? OnScenePlay()
                    : OnSceneStop();

                break;
            }
            case Key::F6:
            {
                (m_Data.sceneState == State::SceneEdit || m_Data.sceneState == State::ScenePlay)
                    ? OnScenePlay()
                    : OnSceneStop();
                break;
            }
            case Key::D:
            {
                if (control)
                    m_ScenePanel->DuplicateSelectedEntity();
                break;
            }
            case Key::Z:
            {
                if (control)
                {
                    if (shift)
                        Application::GetCommandManager()->Redo();
                    else
                        Application::GetCommandManager()->Undo();
                }
                break;
            }
        default: break;
        }
        return false;
    }

    bool EditorLayer::OnMouseButtonPressed(MouseButtonPressedEvent &event)
    {
        if (event.Is(Mouse::ButtonLeft) && !m_ScenePanel->IsGizmoBeingUse() && m_ScenePanel->IsHovered())
        {
            m_Data.isPickingEntity = true;
        }

        return false;
    }

    void EditorLayer::OnRender(nvrhi::IFramebuffer *mainFramebuffer)
    {
        Layer::OnRender(mainFramebuffer);

        if (!m_ActiveScene)
            return;

        // Perform Resize
        auto framebufferSize = m_ScenePanel->GetSceneViewportRT()->GetSize();
        auto currentViewportSize = m_ScenePanel->GetViewportSize();
        if (currentViewportSize.x > 0.0f && currentViewportSize.y > 0
            && (framebufferSize.x != currentViewportSize.x || framebufferSize.y != currentViewportSize.y))
        {
            m_ScenePanel->ResizeFramebuffer(currentViewportSize.x, currentViewportSize.y);
        }

        // Scene Render
        switch (m_Data.sceneState)
        {
        case State::SceneSimulate:
        case State::SceneEdit:
        {
            m_SceneRenderer.RenderTo(&m_ScenePanel->GetViewportCamera(),
                m_ScenePanel->GetSceneViewportRT(),
                m_ScenePanel->GetUIViewportRT(),
                m_ScenePanel->GetCompositeViewportRT());
            break;
        }
        case State::ScenePlay:
        {
            ICamera *camera = &m_ScenePanel->GetViewportCamera();
            if (Entity primaryCam = m_ActiveScene->GetPrimaryCamera())
            {
				camera = &primaryCam.GetComponent<CameraComponent>().camera;
            }
            m_SceneRenderer.RenderTo(camera,
                m_ScenePanel->GetSceneViewportRT(),
                m_ScenePanel->GetUIViewportRT(),
                m_ScenePanel->GetCompositeViewportRT());
            break;
        }
        }

        if (Entity selectedEntity = m_ScenePanel->GetSelectedEntity())
        {
            if (selectedEntity.HasComponent<CameraComponent>())
            {
                ICamera *camera = &selectedEntity.GetComponent<CameraComponent>().camera;
                m_SceneRenderer.RenderTo(camera,
                    m_ScenePanel->GetSceneCameraRT(),
                    m_ScenePanel->GetUICameratRT(),
                    m_ScenePanel->GetCompositeCameraRT());
            }
        }

        m_CommandList->Begin();

        auto cmd = m_CommandList->GetActiveHandle();

        // Create staging texture for read-back
        if (m_Data.isPickingEntity && false) // FIXME: No mouse picking
        {
            nvrhi::TextureDesc stagingDesc = m_ScenePanel->GetCompositeViewportRT()->GetColorAttachment(1)->GetHandle()->getDesc();
            stagingDesc.initialState = nvrhi::ResourceStates::CopyDest;
            m_MousePickingStagingTexture = m_Device->createStagingTexture(stagingDesc, nvrhi::CpuAccessMode::Read);
            cmd->copyTexture(m_MousePickingStagingTexture, nvrhi::TextureSlice(), m_ScenePanel->GetCompositeViewportRT()->GetColorAttachment(1)->GetHandle(), nvrhi::TextureSlice());
        }

        if (m_Data.takeScreenshot)
        {
            nvrhi::TextureDesc stagingDesc = m_ScenePanel->GetCompositeViewportRT()->GetColorAttachment(0)->GetHandle()->getDesc();
            stagingDesc.initialState = nvrhi::ResourceStates::CopyDest;
            m_ScreenshotStagingTexture = m_Device->createStagingTexture(stagingDesc, nvrhi::CpuAccessMode::Read);
            cmd->copyTexture(m_ScreenshotStagingTexture, nvrhi::TextureSlice(), m_ScenePanel->GetCompositeViewportRT()->GetColorAttachment(0)->GetHandle(), nvrhi::TextureSlice());
        }

        m_CommandList->Submit();

        if (m_Data.takeScreenshot)
        {
            // Map and read the pixel data
            size_t rowPitch = 0;
            if (void *mappedData = m_Device->mapStagingTexture(m_ScreenshotStagingTexture, nvrhi::TextureSlice(), nvrhi::CpuAccessMode::Read, &rowPitch))
            {
                const void *pixelData = mappedData;
                std::string filepath = FileDialogs::SaveFile("PNG (*.png)\0*.png");
                if (!filepath.empty())
                {
                    const int channels = 4;
                    const int width = static_cast<int>(m_ScreenshotStagingTexture->getDesc().width);
                    const int height = static_cast<int>(m_ScreenshotStagingTexture->getDesc().height);
                    stbi_write_png(filepath.c_str(), width, height, channels, pixelData, static_cast<int>(rowPitch));
                }
            }
            m_Data.takeScreenshot = false;
        }

        if (m_Data.isPickingEntity && false) // FIXME: No mouse picking
        {
            // Map and read the pixel data
            size_t rowPitch = 0;
            if (void *mappedData = m_Device->mapStagingTexture(m_MousePickingStagingTexture, nvrhi::TextureSlice(), nvrhi::CpuAccessMode::Read, &rowPitch)) {
                uint32_t *pixelData = static_cast<uint32_t *>(mappedData);

                glm::vec2 mousePos = m_ScenePanel->GetViewportMousePos();
                const int pixelX = static_cast<i32>(mousePos.x);
                const int pixelY = static_cast<i32>(mousePos.y);

                // Get row pitch from texture mapping
                m_Data.hoveredEntity = pixelData[pixelY * (rowPitch / sizeof(uint32_t)) + pixelX];

                bool found = false;
                auto view = m_ActiveScene->registry->view<TransformComponent>();
                for (entt::entity e : view)
                {
                    if (uint32_t eId = static_cast<uint32_t>(e); eId == m_Data.hoveredEntity)
                    {
                        Entity entity { e, m_ActiveScene.get() };
                        m_ScenePanel->SetSelectedEntity(entity);
                        found = true;
                        break;
                    }
                }

                if (!found && !m_Data.multiSelect)
                {
                    m_ScenePanel->SetSelectedEntity(Entity{});
                    m_ScenePanel->SetGizmoOperation(ImGuizmo::OPERATION::NONE);
                }

                m_Device->unmapStagingTexture(m_MousePickingStagingTexture);
            }

            m_Data.isPickingEntity = false;
        }
    }

    void EditorLayer::OnGuiRender()
    {
        constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar
            | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        const ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGui::Begin("##main_dockspace", nullptr, windowFlags);
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        window->DC.LayoutType = ImGuiLayoutType_Horizontal;
        window->DC.NavLayerCurrent = ImGuiNavLayer_Menu;

        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("New Scene", nullptr, false, m_ActiveProject != nullptr))
                {
                    NewScene();
                }
                else if (ImGui::MenuItem("Open Scene", nullptr, false, m_ActiveProject != nullptr))
                {
                    OpenScene();
                }
                if (ImGui::MenuItem("Save Scene", nullptr, false, m_ActiveProject != nullptr))
                {
                    SaveScene();
                }
                else if (ImGui::MenuItem("Save Scene As", nullptr, false, m_ActiveProject != nullptr))
                {
                    SaveSceneAs();
                }

                ImGui::Separator();

                if (ImGui::MenuItem("New Project"))
                {
                    m_Data.popupNewProjectModal = true;
                }

                else if (ImGui::MenuItem("Save Project", nullptr, false, m_ActiveProject != nullptr))
                {
                    SaveProject();
                }

                else if (ImGui::MenuItem("Open Project"))
                {
                    OpenProject();
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Edit"))
            {
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View"))
            {
                if (ImGui::MenuItem("Asset Registry", nullptr, false, m_ActiveProject != nullptr))
                {
                    m_Data.assetRegistryWindow = true;
                }

                if (ImGui::MenuItem("Screenshot", nullptr, false, m_ActiveProject != nullptr))
                {
                    m_Data.takeScreenshot = true;
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Window"))
            {
                if (ImGui::MenuItem("Materials", nullptr, m_MaterialsPanel->IsOpen(), m_ActiveProject != nullptr))
                {
                    m_MaterialsPanel->SetOpen(!m_MaterialsPanel->IsOpen());
                }

                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        if (m_Data.popupNewProjectModal)
        {
            ImGui::OpenPopup("New Project");
            m_Data.popupNewProjectModal = false;
        }

        if (ImGui::BeginPopupModal("New Project", nullptr, 0))
        {
            ImGui::Text("Create a brand new project here...");

            static char nameBuffer[128] = {};
            if (ImGui::InputText("Project Name", nameBuffer, 128))
            {
                m_Data.projectCreateInfo.name = std::string(nameBuffer);
            }

            static char filepathBuffer[256] = {};
            if (ImGui::InputText("Location", filepathBuffer, sizeof(filepathBuffer), ImGuiInputTextFlags_ReadOnly))
            {
                m_Data.projectCreateInfo.filepath = std::string(filepathBuffer);
            }

            ImGui::SameLine();
            if (ImGui::Button("..."))
            {
                m_Data.projectCreateInfo.filepath = FileDialogs::SelectFolder();

                if (!m_Data.projectCreateInfo.filepath.empty())
                {
                    std::string filepathCopy = m_Data.projectCreateInfo.filepath.generic_string();
                    memcpy(filepathBuffer, filepathCopy.c_str(), sizeof(filepathBuffer));
                }
            }

            if (ImGui::Button("Create"))
            {
                if (!m_Data.projectCreateInfo.name.empty() && !m_Data.projectCreateInfo.filepath.empty())
                {
                    while (m_Data.projectCreateInfo.name.find(' ') != std::string::npos)
                    {
                        const size_t spacePos = m_Data.projectCreateInfo.name.find(' ');
                        m_Data.projectCreateInfo.name.replace(spacePos, 1, "");
                    }

                    m_Data.projectCreateInfo.filepath /= (m_Data.projectCreateInfo.name + ".ixproj");

                    if (Ref<Project> newProject = Project::Create(m_Data.projectCreateInfo))
                    {
                        m_ActiveProject = newProject;

                        ProjectSerializer serializer(m_ActiveProject.get());
                        serializer.Serialize(m_Data.projectCreateInfo.filepath);

                        // Reload content browser
                        m_ContentBrowserPanel->LoadProjectFiles();

                        if (m_ActiveProject->GetInfo().defaultSceneHandle != AssetHandle(0))
                        {
                            if (Ref<Scene> activeScene = m_ActiveProject->GetAsset<Scene>(m_ActiveProject->GetInfo().defaultSceneHandle))
                            {
                                m_EditorScene = SceneManager::Copy(activeScene);
                                m_EditorScene->SetDirtyFlag(false);

                                m_ActiveScene = m_EditorScene;
                                SetActiveScene(m_ActiveScene);

                                AssetMetaData metadata = m_ActiveProject->GetAssetManager().GetMetaData(activeScene->handle);
                                auto scenePath = m_ActiveProject->GetAssetFilepath(metadata.filepath);
                                m_CurrentSceneFilePath = scenePath;
                            }
                        }
                        else
                        {
                            // Create default scene
                            NewScene();
                        }
                    }

                    m_Data.projectCreateInfo.filepath.clear();
                    m_Data.projectCreateInfo.name.clear();

                    memset(nameBuffer, 0, sizeof(nameBuffer));
                    memset(filepathBuffer, 0, sizeof(filepathBuffer));

                    ImGui::CloseCurrentPopup();
                }
                else
                {
                    // TODO: Show error message text
                }
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel"))
            {
                m_Data.projectCreateInfo.filepath.clear();
                m_Data.projectCreateInfo.name.clear();

                memset(nameBuffer, 0, sizeof(nameBuffer));
                memset(filepathBuffer, 0, sizeof(filepathBuffer));

                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }


        // dock space
        ImGui::DockSpace(ImGui::GetID("main_dockspace"), ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
        {
            // scene dock space
            m_ScenePanel->OnGuiRender();
            m_ContentBrowserPanel->OnGuiRender();
            m_MaterialsPanel->OnImGuiRender();

            ImGui::Begin("Project");

            if (m_ActiveProject)
            {
                const auto &info = m_ActiveProject->GetInfo();
                std::string projectName = info.name;
                if (m_ActiveProject->IsDirty())
                    projectName += "*";
                ImGui::Text("Name: %s", projectName.c_str());
                ImGui::Text("Filepath: %s", info.filepath.generic_string().c_str());
            }

            ImGui::End();
            
            // Render GUI
            UISettings();
            UIImportMeshes();
        }

        ImGui::End(); // end dock space
    }

    void EditorLayer::SetActiveScene(const Ref<Scene> &scene)
    {
        m_ScenePanel->SetActiveScene(scene);
        m_SceneRenderer.SetActiveScene(scene);
        m_ActiveProject->SetActiveScene(scene);

        m_ActiveScene = scene;
    }

    void EditorLayer::NewScene()
    {
        if (m_EditorScene)
        {
            m_EditorScene->OnStop();
        }

        m_CurrentSceneFilePath.clear();

        // create editor scene
        m_EditorScene = CreateRef<Scene>(m_ActiveProject.get(), "New Scene");

        // pass to active scene
        m_ActiveScene = m_EditorScene;
        SetActiveScene(m_ActiveScene);
    }

    void EditorLayer::SaveScene()
    {
        if (m_CurrentSceneFilePath.empty())
        {
            SaveSceneAs();
        }
        else
        {
            SaveScene(m_CurrentSceneFilePath);
        }
    }

    void EditorLayer::SaveScene(const std::filesystem::path &filepath) const
    {
        SceneSerializer serializer(m_ActiveScene, m_ActiveProject.get());
        serializer.Serialize(filepath);
    }

    void EditorLayer::SaveSceneAs()
    {
        SDL_ShowSaveFileDialog(OnSceneSaveFileSelected, this,
            Application::GetInstance()->GetWindow()->GetWindowHandle(),
            kSceneFileFilters, IM_ARRAYSIZE(kSceneFileFilters),
            nullptr);
    }

    void EditorLayer::OpenScene()
    {
        SDL_ShowOpenFileDialog(OnSceneOpenFileSelected, this,
            Application::GetInstance()->GetWindow()->GetWindowHandle(),
            kSceneFileFilters, IM_ARRAYSIZE(kSceneFileFilters),
            nullptr, false);
    }

    void EditorLayer::OpenScene(const std::filesystem::path &filepath)
    {
        if (m_EditorScene)
        {
            m_EditorScene->OnStop();
        }

        if (m_Data.sceneState == State::ScenePlay)
        {
            OnSceneStop();
        }

        if (Ref<Scene> openScene = SceneSerializer::Deserialize(filepath, m_ActiveProject.get()))
        {
            m_EditorScene = SceneManager::Copy(openScene);
            m_EditorScene->SetDirtyFlag(false);

            m_ActiveScene = m_EditorScene;
            SetActiveScene(m_ActiveScene);

            m_CurrentSceneFilePath = filepath;
        }
    }

    void EditorLayer::SaveProject()
    {
        if (m_ActiveProject)
        {
            SaveScene();

            const auto &info = m_ActiveProject->GetInfo();
            ProjectSerializer sr(m_ActiveProject.get());
            if (!info.filepath.empty())
            {
                sr.Serialize(info.filepath);
            }
        }
    }

    void EditorLayer::SaveProjectAs()
    {
    }

    void EditorLayer::OpenProject()
    {
        SDL_ShowOpenFileDialog(OnProjectOpenFileSelected, this,
            Application::GetInstance()->GetWindow()->GetWindowHandle(),
            kProjectFileFilters, IM_ARRAYSIZE(kProjectFileFilters),
            nullptr, false);
    }

    void EditorLayer::OpenProject(const std::filesystem::path &filepath)
    {
        if (filepath == m_CurrentProjectFilepath)
        {
            LOG_TRACE("Dismiss opening current project {0}", filepath.generic_string());
    		return;
        }

        if (const Ref<Project> openedProject = ProjectSerializer::Deserialize(filepath))
        {
            m_ActiveProject = openedProject;
        	m_CurrentProjectFilepath = filepath;

            // Reload project files
            m_ContentBrowserPanel->LoadProjectFiles();

            // Get Project default scene
            if (m_ActiveProject->GetInfo().defaultSceneHandle != AssetHandle(0))
            {
                if (Ref<Scene> activeScene = m_ActiveProject->GetAsset<Scene>(m_ActiveProject->GetInfo().defaultSceneHandle))
                {
                    m_EditorScene = SceneManager::Copy(activeScene);
                    m_EditorScene->SetDirtyFlag(false);

                    m_ActiveScene = m_EditorScene;
                    SetActiveScene(m_ActiveScene);

                    const auto &[assetFilepath, assetType] = m_ActiveProject->GetAssetManager().GetMetaData(activeScene->handle);
                    m_CurrentSceneFilePath = m_ActiveProject->GetAssetFilepath(assetFilepath);
                }
            }
            else
            {
                // Create a default scene
                NewScene();
            }
        }
    }

    bool EditorLayer::OnMouseMovedEvent(MouseMovedEvent& event)
    {
	    return false;
    }

    void EditorLayer::OnScenePlay()
    {
        if (m_EditorScene)
        {
            m_EditorScene->OnStop();
        }

        m_ScenePanel->SetGizmoOperation(ImGuizmo::OPERATION::NONE);

        if (m_Data.sceneState != State::SceneEdit)
            OnSceneStop();

        m_Data.sceneState = State::ScenePlay;

        // copy initial components to new scene
        m_ActiveScene = SceneManager::Copy(m_EditorScene);
        m_ActiveScene->OnStart();

        SetActiveScene(m_ActiveScene);
    }

    void EditorLayer::OnSceneStop()
    {
        m_Data.sceneState = State::SceneEdit;
        
        m_ActiveScene->OnStop();
        m_ActiveScene = m_EditorScene;

        SetActiveScene(m_EditorScene);
    }

    void  EditorLayer::OnSceneSimulate()
    {
        if (m_Data.sceneState != State::SceneEdit)
            OnSceneStop();

        m_Data.sceneState = State::SceneSimulate;

        // copy initial components to new scene
        m_ActiveScene = SceneManager::Copy(m_EditorScene);
        m_ActiveScene->OnStart();

        SetActiveScene(m_ActiveScene);
    }

    void EditorLayer::OnSceneSaveFileSelected(void* userData, const char* const* filelist, int filter)
    {
        // Check for errors
        if (filelist == nullptr)
        {
            const char* error = SDL_GetError();
            LOG_ERROR("SDL File Dialog Error: {0}", error ? error : "Unknown error");
            return;
        }

        // Check if user canceled
        if (*filelist == nullptr)
        {
            return;
        }

        // Get the selected file path
        std::string filepath = filelist[0];
        if (!filepath.empty())
        {
            // Ensure the file has the correct extension
            if (!filepath.ends_with(".ixscene"))
            {
                filepath += ".ixscene";
            }

            s_EditorLayerInstance->m_CurrentSceneFilePath = filepath;

            PendingFileLoading pf = { PendingFileLoading::Save, AssetMetaData(filepath, AssetType::Scene), userData };
            s_EditorLayerInstance->m_PendingFileLoading.push(pf);
        }
    }

    void EditorLayer::OnSceneOpenFileSelected(void* userData, const char* const* filelist, int filter)
    {
        // Check for errors
        if (filelist == nullptr)
        {
            const char* error = SDL_GetError();
            LOG_ERROR("SDL File Dialog Error: {0}", error ? error : "Unknown error");
            return;
        }

        // Check if user canceled
        if (*filelist == nullptr)
        {
            return;
        }

        // Get the selected file path
        std::string filepath = filelist[0];
        if (!filepath.empty())
        {
            PendingFileLoading pf = { PendingFileLoading::Open, AssetMetaData(filepath, AssetType::Scene), userData };
            s_EditorLayerInstance->m_PendingFileLoading.push(pf);
        }
    }

    void EditorLayer::OnProjectSaveFileSelected(void* userData, const char* const* filelist, int filter)
    {
        // Check for errors
        if (filelist == nullptr)
        {
            const char* error = SDL_GetError();
            LOG_ERROR("SDL File Dialog Error: {0}", error ? error : "Unknown error");
            return;
        }

        // Check if user canceled
        if (*filelist == nullptr)
        {
            return;
        }

        // Get the selected file path
        std::string filepath = filelist[0];
        if (!filepath.empty())
        {
            // Ensure the file has the correct extension
            if (!filepath.ends_with(".ixproj"))
            {
                filepath += ".ixproj";
            }

            PendingFileLoading pf = { PendingFileLoading::Open, AssetMetaData(filepath, AssetType::Project), userData };
            s_EditorLayerInstance->m_PendingFileLoading.push(pf);
        }
    }

    void EditorLayer::OnProjectOpenFileSelected(void *userData, const char *const *filelist, int filter)
    {
        // Check for errors
        if (filelist == nullptr)
        {
            const char* error = SDL_GetError();
            LOG_ERROR("SDL File Dialog Error: {0}", error ? error : "Unknown error");
            return;
        }

        // Check if user canceled
        if (*filelist == nullptr)
        {
            return;
        }

        std::string filepath = filelist[0];
        if (!filepath.empty())
        {
            PendingFileLoading pf = { PendingFileLoading::Open, AssetMetaData(filepath, AssetType::Project), userData };
            s_EditorLayerInstance->m_PendingFileLoading.push(pf);
        }
    }

    void EditorLayer::ProcessPendingFileLoading()
    {
        while (!m_PendingFileLoading.empty())
        {
            auto& pf = m_PendingFileLoading.front();

            switch (pf.type)
            {
                case PendingFileLoading::Open:
                {
                    if (pf.metadata.type == AssetType::Scene)
                        OpenScene(pf.metadata.filepath);
                    else if (pf.metadata.type == AssetType::Project)
                        OpenProject(pf.metadata.filepath);

                    break;
                }
                case PendingFileLoading::Save:
                {
                    if (pf.metadata.type == AssetType::Scene)
                        SaveScene(pf.metadata.filepath);
                    else if (pf.metadata.type == AssetType::Project)
                        SaveProjectAs();
                    
                    break;
                }
            }

            m_PendingFileLoading.pop();
        }
    }

    void EditorLayer::UISettings()
    {
        ImGui::Begin("Settings", &m_Data.settingsWindow);

        constexpr ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_DefaultOpen;
        
        m_ScenePanel->UISettings();

        if (ImGui::TreeNodeEx("Pipeline", treeFlags))
        {
            // Raster settings
            static std::array<const char *, 2>rasterFillStr = { "Solid", "Wireframe" };
            const char *currentFillMode = rasterFillStr[static_cast<i32>(m_Data.rasterFillMode)];
            if (ImGui::BeginCombo("Fill", currentFillMode))
            {
                for (size_t i = 0; i < std::size(rasterFillStr); ++i)
                {
                    bool isSelected = strcmp(currentFillMode, rasterFillStr[i]) == 0;
                    if (ImGui::Selectable(rasterFillStr[i], isSelected))
                    {
                        m_Data.rasterFillMode = static_cast<nvrhi::RasterFillMode>(i);
                        m_SceneRenderer.SetFillMode(m_Data.rasterFillMode);
                    }

                    if (isSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::TreePop();
        }

        if (m_ActiveScene)
        {
            // Scene
            if (ImGui::TreeNodeEx("Scene Data", treeFlags))
            {
                if (ImGui::Button("Load HDR Texture"))
                {
                    std::string filepath = FileDialogs::OpenFile("HDR Files (*.hdr)\0*.hdr\0");
                    if (!filepath.empty())
                    {
                        Renderer::Submit([&](nvrhi::ICommandList *cmd)
                        {
                            auto env = m_SceneRenderer.GetEnvironment();
                            env->LoadTexture(filepath, cmd);
                            env->UpdateBindingSet();
                        });
                    }
                }

                auto &sceneData = m_ActiveScene->gpuData;
            
                ImGui::ColorEdit3("Color", &sceneData.sunColor.x);
                ImGui::DragFloat("Intensity", &sceneData.sunColor.w, 0.025f, 0.0f, 10.0f);
                ImGui::SliderFloat("Azimuth", &sceneData.sungAngles.x, -glm::radians(90.0f), glm::radians(90.0f));
                ImGui::SliderFloat("Elevation", &sceneData.sungAngles.y, 0.0f, glm::radians(90.0f));
                float angularRadius = glm::degrees(sceneData.sunAngularRadius);
                if (ImGui::SliderFloat("Angular Size", &angularRadius, 0.0f, 45.0f))
                {
                    sceneData.sunAngularRadius = glm::radians(angularRadius);
                }
                ImGui::DragFloat("Exposure", &sceneData.exposure, 0.005f, 0.1f, 10.0f);
                ImGui::DragFloat("Gamma", &sceneData.gamma, 0.005f, 0.1f, 10.0f);
                ImGui::DragFloat("Ambient", &sceneData.ambient, 0.005f, 0.01f, 100.0f);

                ImGui::SeparatorText("Shadows");
                {
                    auto csm = m_SceneRenderer.GetCascadedShadowMap();
                    auto &data = csm->GetGPUData();
                    ImGui::SliderFloat("Strength", &data.shadowStrength, 0.0f, 1.0f);
                    ImGui::DragFloat("Min Bias", &data.minBias, 0.0001f, 0.0f, 0.1f, "%.6f");
                    ImGui::DragFloat("Max Bias", &data.maxBias, 0.0001f, 0.0f, 0.1f, "%.6f");
                    ImGui::SliderFloat("PCF Radius", &data.pcfRadius, 0.1f, 4.0f);

                    static const char* resolutionLabels[] = {"Low - 512px", "Medium - 1024px", "High - 2048px", "Ultra - 4096px"};
                    int cascadeQualityIndex = static_cast<int>(csm->GetQuality());

                    if (ImGui::Combo("Resolution", &cascadeQualityIndex, resolutionLabels, IM_ARRAYSIZE(resolutionLabels)))
                    {
                        auto quality = static_cast<ShadowMapQuality>(cascadeQualityIndex);
                        csm->Resize(quality);
                    }

                    ImGui::Separator();
                    ImGui::Text("Shadow Debug");
                    ImGui::RadioButton("Off##ShadowDbg", &sceneData.debugShadow, 0); ImGui::SameLine();
                    ImGui::SameLine();
                    ImGui::RadioButton("Cascades", &sceneData.debugShadow, 1); ImGui::SameLine();
                    ImGui::SameLine();
                    ImGui::RadioButton("Visibility", &sceneData.debugShadow, 2);
                }

                if (ImGui::CollapsingHeader("Render Mode", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    if (ImGui::RadioButton("Color", sceneData.renderMode == RENDER_MODE_COLOR)) sceneData.renderMode = RENDER_MODE_COLOR;
                    ImGui::SameLine();
                    if (ImGui::RadioButton("Diffuse", sceneData.renderMode == RENDER_MODE_DIFFUSE)) sceneData.renderMode = RENDER_MODE_DIFFUSE;
                    ImGui::SameLine();
                    if (ImGui::RadioButton("Normals", sceneData.renderMode == RENDER_MODE_NORMALS)) sceneData.renderMode = RENDER_MODE_NORMALS;
                    ImGui::SameLine();
                    if (ImGui::RadioButton("Metallic", sceneData.renderMode == RENDER_MODE_METALLIC)) sceneData.renderMode = RENDER_MODE_METALLIC;
                    ImGui::SameLine();
                    if (ImGui::RadioButton("Roughness", sceneData.renderMode == RENDER_MODE_ROUGHNESS)) sceneData.renderMode = RENDER_MODE_ROUGHNESS;
                }

                ImGui::TreePop();
            }
        }

        ImGui::End();
        
        if (m_Data.assetRegistryWindow)
        {
            AssetRegistry assetRegistry = m_ActiveProject->GetAssetManager().GetAssetAssetRegistry();

            struct AssetPairCompare
            {
                bool operator()(const std::pair<AssetHandle, AssetMetaData> &lhs, const std::pair<AssetHandle, AssetMetaData> &rhs) const
                {
                    return lhs.first < rhs.first;
                }
            };
            
            static std::string assetRegistryFilterResultStr;
            static std::set<std::pair<AssetHandle, AssetMetaData>, AssetPairCompare> filteredAssets;
            
            static bool showFullPath = false;
            ImGui::Begin("Asset Registry", &m_Data.assetRegistryWindow);

            static char buffer[256] = { 0 };
            ImGui::Text("Filter");
            ImGui::SameLine();
            
            ImGui::InputTextWithHint("##asset_registry_filter", "AssetHandle, Type, Filepath", buffer, sizeof(buffer) + 1, ImGuiInputTextFlags_EscapeClearsAll | ImGuiInputTextFlags_NoHorizontalScroll);
            assetRegistryFilterResultStr = std::string(buffer);
            filteredAssets.clear();

            if (!assetRegistryFilterResultStr.empty())
            {
                const std::string findKey = stringutils::ToLower(assetRegistryFilterResultStr);
                for (const auto &[handle, metadata] : assetRegistry)
                {
                    const std::string &handleStr = std::to_string(handle);
                    const std::string &typeStr = stringutils::ToLower(AssetTypeToString(metadata.type));
                    const std::string &filepathStr = stringutils::ToLower(std::filesystem::absolute(m_ActiveProject->GetAssetFilepath(metadata.filepath)).generic_string());

                    if (handleStr.find(findKey) != std::string::npos || typeStr.find(findKey) != std::string::npos ||
                        filepathStr.find(findKey) != std::string::npos)
                    {
                        filteredAssets.insert({handle, metadata});             
                    }
                }
            }
            
            ImGui::SameLine();
            ImGui::Text("Full path");
            ImGui::SameLine();
            ImGui::Checkbox("##fullpath", &showFullPath);

            ImGui::SameLine();
            if (ImGui::Button("Refresh"))
            {
                m_ActiveProject->ValidateAssetRegistry();
            }

            ImGuiTableFlags tableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX;
            if (ImGui::BeginTable("asset_registry_table", 3, tableFlags))
            {
                // setup table 3 columns
                // AssetHandle, Type, Filepath
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("AssetHandle", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 0.5f);
                ImGui::TableSetupColumn("Filepath", ImGuiTableColumnFlags_WidthStretch, 1.5f);
                ImGui::TableHeadersRow();

                if (filteredAssets.empty())
                {
                    for (auto &[handle, metadata] : assetRegistry)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::Text("%llu", static_cast<uint64_t>(handle));

                        ImGui::TableNextColumn();

                        std::string assetTypeStr = AssetTypeToString(metadata.type);
                        ImGui::TextWrapped("%s", assetTypeStr.c_str());

                        ImGui::TableNextColumn();
                        if (showFullPath)
                        {
                            assetTypeStr = std::filesystem::absolute(m_ActiveProject->GetAssetFilepath(metadata.filepath)).generic_string();
                            ImGui::TextWrapped("%s", assetTypeStr.c_str());
                        }
                        else
                        {
                            assetTypeStr = metadata.filepath.generic_string();
                            ImGui::TextWrapped("%s", assetTypeStr.c_str());
                        }

                        if (metadata.type == AssetType::Scene)
                        {
                            ImGui::SameLine();
                            if (ImGui::Button("Set as default"))
                            {
                                m_ActiveProject->GetInfo().defaultSceneHandle = handle;
                                SaveProject();
                            }
                        }
                    }
                }
                else
                {
                    for (auto &[handle, metadata] : filteredAssets)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::Text("%llu", static_cast<uint64_t>(handle));

                        ImGui::TableNextColumn();

                        std::string assetTypeStr = AssetTypeToString(metadata.type);
                        ImGui::TextWrapped("%s", assetTypeStr.c_str());

                        ImGui::TableNextColumn();
                        if (showFullPath)
                        {
                            assetTypeStr = std::filesystem::absolute(m_ActiveProject->GetAssetFilepath(metadata.filepath)).generic_string();
                            ImGui::TextWrapped("%s", assetTypeStr.c_str());
                        }
                        else
                        {
                            assetTypeStr = metadata.filepath.generic_string();
                            ImGui::TextWrapped("%s", assetTypeStr.c_str());
                        }

                        if (metadata.type == AssetType::Scene)
                        {
                            ImGui::SameLine();
                            if (ImGui::Button("Set as default"))
                            {
                                m_ActiveProject->GetInfo().defaultSceneHandle = handle;
                                SaveProject();
                            }
                        }
                    }
                }
                ImGui::EndTable();
            }
            ImGui::End();
        }
    }

    void EditorLayer::UIImportMeshes()
    {
        if (m_LoadedMeshScene.has_value())
        {
            ImGui::Begin("Import Mesh");

            std::vector<const char *> meshNames;
            meshNames.reserve(m_LoadedMeshScene->flatMeshes.size());
            for (size_t i = 0; i < m_LoadedMeshScene->flatMeshes.size(); ++i)
            {
                meshNames.push_back(m_LoadedMeshScene->flatMeshes[i]->GetName().c_str());
            }

            const char *currentMeshName = meshNames[m_SelectedMesh];

            if (ImGui::BeginCombo("Mesh", currentMeshName))
            {
                for (size_t i = 0; i < meshNames.size(); ++i)
                {
                    bool isSelected = std::strcmp(currentMeshName, meshNames[i]) == 0;
                    if (ImGui::Selectable(meshNames[i], isSelected))
                    {
                        m_SelectedMesh = i;
                    }

                    if (isSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }

                ImGui::EndCombo();
            }

            if (ImGui::Button("Cancel"))
            {
                m_SelectedMesh = 0;
                m_LoadedMeshScene.reset();
                m_MeshInstanceData = nullptr;
            }

            ImGui::SameLine();
            if (ImGui::Button("Import"))
            {
                Ref<MeshInstance> &mInstance = *static_cast<Ref<MeshInstance> *>(m_MeshInstanceData);
                mInstance = m_LoadedMeshScene->flatMeshes[m_SelectedMesh];
                mInstance->SetMeshIndex(m_SelectedMesh);

                Application::SubmitToMainThread([mesh = mInstance, scene = m_ActiveScene]()
                {
                    mesh->UpdateBindingSet(scene.get());
                    return true;
                });

                m_SelectedMesh = 0;
                m_LoadedMeshScene.reset();
                m_MeshInstanceData = nullptr;
            }

            ImGui::End();
        }
    }
}
