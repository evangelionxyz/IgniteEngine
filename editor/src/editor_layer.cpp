#include "editor_layer.hpp"
#include "panels/scene_panel.hpp"
#include "panels/content_browser_panel.hpp"

#include "ignite/core/platform_utils.hpp"
#include "ignite/core/command.hpp"
#include "ignite/graphics/renderer_2d.hpp"
#include "ignite/imgui/gui_function.hpp"
#include "ignite/asset/asset.hpp"
#include "ignite/asset/asset_importer.hpp"
#include "ignite/scripting/script_engine.hpp"
#include "stb_image_write.h"

#include <cinttypes>

namespace ignite
{
    EditorLayer::EditorLayer(const std::string &name)
        : Layer(name)
    {
    }

    void EditorLayer::OnAttach()
    {
        Layer::OnAttach();

        m_Device = Application::GetDeviceManager()->GetDevice();
        m_CommandList = m_Device->createCommandList();

        // write buffer with command list
        Renderer2D::InitQuadData();
        Renderer2D::InitLineData();

        m_ScenePanel = CreateRef<ScenePanel>("Scene Panel", this);

        m_ContentBrowserPanel = CreateRef<ContentBrowserPanel>("Content Browser");

        const auto &cmdArgs = Application::GetInstance()->GetCreateInfo().cmdLineArgs;
        for (int i = 0; i < cmdArgs.count; ++i)
        {
            std::string args = cmdArgs[i];

            char projectArgs[] = "-project=";
            if (args.find(projectArgs) != std::string::npos)
            {
                std::string projectFilepath = args.substr(std::size(projectArgs) - 1, args.size() - std::size(projectArgs) + 1);
                OpenProject(projectFilepath);
            }
        }

        // create render target framebuffer
        m_SceneRenderer.Create();
    }

    void EditorLayer::OnDetach()
    {
        Layer::OnDetach();
    }

    void EditorLayer::OnUpdate(f32 deltaTime)
    {
        Layer::OnUpdate(deltaTime);
        
        AssetImporter::SyncMainThread();

        if (!m_ActiveScene)
            return;

        // multi select entity
        m_Data.multiSelect = Input::IsKeyPressed(Key::LeftShift);

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

        // update panels
        m_ScenePanel->OnUpdate(deltaTime);
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
        bool control = Input::IsKeyPressed(KEY_LEFT_CONTROL);
        bool shift = Input::IsKeyPressed(KEY_LEFT_SHIFT);

        if (ImGui::GetIO().WantTextInput)
            return false;

        switch (event.GetKeyCode())
        {
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
                        SaveSceneAs();
                    else
                        SaveScene();
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

        switch (m_Data.sceneState)
        {
        case State::SceneSimulate:
        case State::SceneEdit:
        {
            m_SceneRenderer.Render(&m_ScenePanel->GetViewportCamera());
            break;
        }
        case State::ScenePlay:
        {
            ICamera *camera = &m_ScenePanel->GetViewportCamera();
            if (Entity primaryCam = m_ActiveScene->GetPrimaryCamera())
            {
                camera = &primaryCam.GetComponent<Camera>().camera;
            }
            m_SceneRenderer.Render(camera, camera->projectionType == ICamera::Type::Perspective);
            break;
        }
        }

        m_CommandList->open();
        // Create staging texture for read-back
        if (m_Data.isPickingEntity)
        {
            nvrhi::TextureDesc stagingDesc = m_SceneRenderer.GetRenderTarget()->GetColorAttachment(1)->getDesc();
            stagingDesc.initialState = nvrhi::ResourceStates::CopyDest;
            m_MousePickingStagingTexture = m_Device->createStagingTexture(stagingDesc, nvrhi::CpuAccessMode::Read);
            m_CommandList->copyTexture(m_MousePickingStagingTexture, nvrhi::TextureSlice(), m_SceneRenderer.GetRenderTarget()->GetColorAttachment(1), nvrhi::TextureSlice());
        }

        if (m_Data.takeScreenshot)
        {
            nvrhi::TextureDesc stagingDesc = m_SceneRenderer.GetRenderTarget()->GetColorAttachment(0)->getDesc();
            stagingDesc.initialState = nvrhi::ResourceStates::CopyDest;
            m_ScreenshotStagingTexture = m_Device->createStagingTexture(stagingDesc, nvrhi::CpuAccessMode::Read);
            m_CommandList->copyTexture(m_ScreenshotStagingTexture, nvrhi::TextureSlice(), m_SceneRenderer.GetRenderTarget()->GetColorAttachment(0), nvrhi::TextureSlice());
        }

        m_CommandList->close();
        m_Device->executeCommandList(m_CommandList);

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

        if (m_Data.isPickingEntity)
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
                auto view = m_ActiveScene->registry->view<Transform>();
                for (entt::entity e : view)
                {
                    if (uint32_t eId = static_cast<uint32_t>(e); eId == m_Data.hoveredEntity)
                    {
                        m_ScenePanel->SetSelectedEntity(Entity{ e, m_ActiveScene.get() });
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

                        ProjectSerializer serializer(m_ActiveProject);
                        serializer.Serialize(m_Data.projectCreateInfo.filepath);

                        m_ContentBrowserPanel->SetActiveProject(m_ActiveProject);

                        ScriptEngine::Init();

                        if (m_ActiveProject->GetInfo().defaultSceneHandle != AssetHandle(0))
                        {
                            if (Ref<Scene> activeScene = Project::GetAsset<Scene>(m_ActiveProject->GetInfo().defaultSceneHandle))
                            {
                                m_EditorScene = SceneManager::Copy(activeScene);
                                m_EditorScene->SetDirtyFlag(false);

                                m_ActiveScene = m_EditorScene;
                                SetActiveScene(m_ActiveScene.get());

                                AssetMetaData metadata = Project::GetActive()->GetAssetManager().GetMetaData(activeScene->handle);
                                auto scenePath = Project::GetActive()->GetAssetFilepath(metadata.filepath);
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


        // dockspace
        ImGui::DockSpace(ImGui::GetID("main_dockspace"), ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
        {
            // scene dockspace
            m_ScenePanel->OnGuiRender();
            m_ContentBrowserPanel->OnGuiRender();

            ImGui::Begin("Project");

            if (Project *activeProject = Project::GetActive())
            {
                const auto &info = activeProject->GetInfo();
                std::string projectName = info.name;
                if (activeProject->IsDirty())
                    projectName += "*";
                ImGui::Text("Name: %s", projectName.c_str());
                ImGui::Text("Filepath: %s", info.filepath.generic_string().c_str());
            }

            ImGui::End();
            
            // Render GUI
            SettingsUI();

            m_SceneRenderer.OnGuiRender();
        }

        ImGui::End(); // end dockspace
    }

    void EditorLayer::SetActiveScene(Scene* scene)
    {
        m_ScenePanel->SetActiveScene(scene);
        m_SceneRenderer.SetActiveScene(scene);
        m_ActiveProject->SetActiveScene(scene);
    }

    void EditorLayer::NewScene()
    {
        if (m_EditorScene)
        {
            m_EditorScene->OnStop();
        }

        m_CurrentSceneFilePath.clear();

        // create editor scene
        m_EditorScene = CreateRef<Scene>("New Scene");

        // pass to active scene
        m_ActiveScene = m_EditorScene;
        SetActiveScene(m_ActiveScene.get());
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
        SceneSerializer serializer(m_ActiveScene);
        serializer.Serialize(filepath);
    }

    void EditorLayer::SaveSceneAs()
    {
        std::string filepath = FileDialogs::SaveFile("Ignite Scene (*.ixasset)\0*.ixasset\0");
        if (!filepath.empty())
        {
            m_CurrentSceneFilePath = filepath;
            SaveScene(filepath);
        }
    }

    void EditorLayer::OpenScene()
    {
        std::string filepath = FileDialogs::OpenFile("Ignite Scene (*.ixasset)\0*.ixasset\0");
        if (!filepath.empty())
        {
            OpenScene(filepath);
        }
    }

    void EditorLayer::OpenScene(const std::filesystem::path &filepath)
    {
        if (m_EditorScene)
        {
            m_EditorScene->OnStop();
        }

        if (m_Data.sceneState == State::ScenePlay)
            OnSceneStop();

        if (Ref<Scene> openScene = SceneSerializer::Deserialize(filepath))
        {
            m_EditorScene = SceneManager::Copy(openScene);
            m_EditorScene->SetDirtyFlag(false);

            m_ActiveScene = m_EditorScene;
            SetActiveScene(m_ActiveScene.get());

            m_CurrentSceneFilePath = filepath;
        }
    }

    void EditorLayer::SaveProject() const
    {
        if (m_ActiveProject)
        {
            const auto &info = m_ActiveProject->GetInfo();
            ProjectSerializer sr(m_ActiveProject);
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
        std::filesystem::path filepath = FileDialogs::OpenFile("Ignite Project (*.ixproj)\0*.ixproj\0");
        if (!filepath.empty())
        {
            OpenProject(filepath);
        }
    }

    void EditorLayer::OpenProject(const std::filesystem::path &filepath)
    {
        Ref<Project> openedProject = ProjectSerializer::Deserialize(filepath);
        if (!openedProject)
        {
            return;
        }

        m_ActiveProject = openedProject;
        m_ContentBrowserPanel->SetActiveProject(m_ActiveProject);

        ScriptEngine::Init();

        // Get Project default scene
        if (m_ActiveProject->GetInfo().defaultSceneHandle != AssetHandle(0))
        {
            if (Ref<Scene> activeScene = Project::GetAsset<Scene>(m_ActiveProject->GetInfo().defaultSceneHandle))
            {
                m_EditorScene = SceneManager::Copy(activeScene);
                m_EditorScene->SetDirtyFlag(false);

                m_ActiveScene = m_EditorScene;
                SetActiveScene(m_ActiveScene.get());

                AssetMetaData metadata = Project::GetActive()->GetAssetManager().GetMetaData(activeScene->handle);
                auto scenePath = Project::GetActive()->GetAssetFilepath(metadata.filepath);
                m_CurrentSceneFilePath = scenePath;
            }
        }
        else
        {
            // Create default scene
            NewScene();
        }
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

        SetActiveScene(m_ActiveScene.get());
    }

    void EditorLayer::OnSceneStop()
    {
        m_Data.sceneState = State::SceneEdit;
        
        m_ActiveScene->OnStop();
        m_ActiveScene = m_EditorScene;

        SetActiveScene(m_EditorScene.get());
    }

    void  EditorLayer::OnSceneSimulate()
    {
        if (m_Data.sceneState != State::SceneEdit)
            OnSceneStop();

        m_Data.sceneState = State::SceneSimulate;

        // copy initial components to new scene
        m_ActiveScene = SceneManager::Copy(m_EditorScene);
        m_ActiveScene->OnStart();

        SetActiveScene(m_ActiveScene.get());
    }

    void EditorLayer::SettingsUI()
    {
        ImGui::Begin("Settings", &m_Data.settingsWindow);

        if (ImGui::TreeNodeEx("Pipeline"))
        {
            m_ScenePanel->CameraSettingsUI();

            ImGui::SeparatorText("Pipeline");

            // Raster settings
            static const char *rasterFillStr[2] = { "Solid", "Wireframe" };
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
            // Environment
            if (ImGui::TreeNodeEx("Environment"))
            {
                static glm::vec2 sunAngles = { 50, -27.0f }; // pitch (elevation), yaw (azimuth)

                if (ImGui::Button("Load HDR Texture"))
                {
                    std::string filepath = FileDialogs::OpenFile("HDR Files (*.hdr)\0*.hdr\0");
                    if (!filepath.empty())
                    {
                        EnvironmentImporter::UpdateTexture(&m_SceneRenderer.GetEnvironment(), filepath);
                    }
                }

                ImGui::Separator();
            
                ImGui::Text("Sun Angles");

                if (ImGui::SliderFloat("Elevation", &sunAngles.x, 0.0f, 180.0f))
                    m_SceneRenderer.GetEnvironment()->SetSunDirection(sunAngles.x, sunAngles.y);
                if (ImGui::SliderFloat("Azimuth", &sunAngles.y, -180.0f, 180.0f))
                    m_SceneRenderer.GetEnvironment()->SetSunDirection(sunAngles.x, sunAngles.y);

                ImGui::Separator();
                ImGui::ColorEdit4("Color", &m_SceneRenderer.GetEnvironment()->dirLight.color.x);
                ImGui::SliderFloat("Intensity", &m_SceneRenderer.GetEnvironment()->dirLight.intensity, 0.01f, 1.0f);

                float angularSize = glm::degrees(m_SceneRenderer.GetEnvironment()->dirLight.angularSize);
                if (ImGui::SliderFloat("Angular Size", &angularSize, 0.1f, 90.0f))
                {
                    m_SceneRenderer.GetEnvironment()->dirLight.angularSize = glm::radians(angularSize);
                }

                ImGui::DragFloat("Ambient", &m_SceneRenderer.GetEnvironment()->dirLight.ambientIntensity, 0.005f, 0.01f, 100.0f);
                ImGui::DragFloat("Exposure", &m_SceneRenderer.GetEnvironment()->params.exposure, 0.005f, 0.1f, 10.0f);
                ImGui::DragFloat("Gamma", &m_SceneRenderer.GetEnvironment()->params.gamma, 0.005f, 0.1f, 10.0f);
        
                ImGui::TreePop();
            }
        }

        ImGui::End();
        
        if (m_Data.assetRegistryWindow)
        {
            AssetRegistry assetRegistry = m_ActiveProject->GetAssetManager().GetAssetAssetRegistry();

            struct AssetPairCompare {
                bool operator()(const std::pair<AssetHandle, AssetMetaData>& lhs, const std::pair<AssetHandle, AssetMetaData>& rhs) const
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

                    if (handleStr.find(findKey) != std::string::npos ||
                        typeStr.find(findKey) != std::string::npos ||
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
                Project::GetActive()->ValidateAssetRegistry();
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
}
