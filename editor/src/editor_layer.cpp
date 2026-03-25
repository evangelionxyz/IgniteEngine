//Copyright (c) 2026 Evangelion Manuhutu | IGNITE STUDIO

#include "editor_layer.hpp"
#include "panels/scene_panel.hpp"
#include "panels/content_browser_panel.hpp"
#include "panels/material_panel.hpp"
#include "panels/asset_importer_panel.hpp"

#include "ignite/core/command.hpp"
#include "ignite/graphics/renderer_2d.hpp"
#include "ignite/asset/asset.hpp"
#include "ignite/asset/asset_importer.hpp"
#include "ignite/scripting/script_engine.hpp"
#include "ignite/graphics/objects/shadow_map.hpp"

#include "stb_image_write.h"

#include <cmath>
#include <SDL3/SDL_dialog.h>

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

        const SDL_DialogFileFilter kScreenshotFileFilters[] =
        {
            { "PNG Image (png)", "png" }
        };

        const SDL_DialogFileFilter kHDRFileFilters[] =
        {
            { "HDR Image (hdr)", "hdr" }
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
    }

    void EditorLayer::OnAttach()
    {
        Layer::OnAttach();

        m_Device = DeviceManager::GetInstance()->GetDevice();

        auto *app = Application::GetInstance();

        m_ScenePanel = new ScenePanel("Scene Panel", this);
        m_ContentBrowserPanel = new ContentBrowserPanel("Content Browser", this);
        m_MaterialsPanel = new MaterialPanel("Material Panel", this);
        m_AssetImporterPanel = new AssetImporterPanel("AssetImporter Panel", this);

        app->PushLayer(m_ScenePanel);
        app->PushLayer(m_ContentBrowserPanel);
        app->PushLayer(m_MaterialsPanel);
        app->PushLayer(m_AssetImporterPanel);

        // create render target framebuffer
        m_SceneRenderer = CreateRef<SceneRenderer>();

        m_Cmd = m_Device->createCommandList();

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

		s_EditorLayerInstance = nullptr;
    }

    void EditorLayer::OnUpdate(float deltaTime)
    {
        Layer::OnUpdate(deltaTime);

        ProcessPendingFileLoading();

        if (m_ContentBrowserPanel)
        {
            m_ContentBrowserPanel->OnUpdate(deltaTime);
        }

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

        // Perform Resize (use integer sizes to avoid continuous resizing from fractional values)
        const glm::uvec2 framebufferSize = m_ScenePanel->GetSceneViewportRT()->GetSize();
        const glm::vec2 currentViewportSize = m_ScenePanel->GetViewportSize();
        const glm::uvec2 desiredSize
        {
            static_cast<uint32_t>(std::round(std::max(0.0f, currentViewportSize.x))),
            static_cast<uint32_t>(std::round(std::max(0.0f, currentViewportSize.y)))
        };

        const glm::uvec2 sceneSize
        {
            m_ActiveScene->GetViewportWidth(),
            m_ActiveScene->GetViewportHeight()
        };

        const bool framebufferNeedsResize = framebufferSize.x != desiredSize.x || framebufferSize.y != desiredSize.y;
        const bool sceneNeedsResize = sceneSize.x != desiredSize.x || sceneSize.y != desiredSize.y;

        if (desiredSize.x > 0u && desiredSize.y > 0u)
        {
            if (framebufferNeedsResize)
            {
                m_ScenePanel->ResizeFramebuffer(desiredSize.x, desiredSize.y);
            }

            if (sceneNeedsResize)
            {
                m_ActiveScene->Resize(desiredSize.x, desiredSize.y);
            }
        }

        // Scene Render
        switch (m_Data.sceneState)
        {
        case State::SceneSimulate:
        case State::SceneEdit:
        {
            m_SceneRenderer->RenderTo(&m_ScenePanel->GetViewportCamera(),
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
			m_SceneRenderer->RenderTo(camera,
                m_ScenePanel->GetSceneViewportRT(),
                m_ScenePanel->GetUIViewportRT(),
                m_ScenePanel->GetCompositeViewportRT());
            break;
        }
        }
		
        m_Cmd->open();
        // Create staging texture for read-back
        if (m_Data.isPickingEntity && false) // FIXME: No mouse picking
        {
            nvrhi::TextureDesc stagingDesc = m_ScenePanel->GetCompositeViewportRT()->GetColorAttachment(1)->GetHandle()->getDesc();
            stagingDesc.initialState = nvrhi::ResourceStates::CopyDest;
            m_MousePickingStagingTexture = m_Device->createStagingTexture(stagingDesc, nvrhi::CpuAccessMode::Read);
            m_Cmd->copyTexture(m_MousePickingStagingTexture, nvrhi::TextureSlice(), m_ScenePanel->GetCompositeViewportRT()->GetColorAttachment(1)->GetHandle(), nvrhi::TextureSlice());
        }

        if (m_Data.takeScreenshot)
        {
            nvrhi::TextureDesc stagingDesc = m_ScenePanel->GetCompositeViewportRT()->GetColorAttachment(0)->GetHandle()->getDesc();
            stagingDesc.initialState = nvrhi::ResourceStates::CopyDest;
            m_ScreenshotStagingTexture = m_Device->createStagingTexture(stagingDesc, nvrhi::CpuAccessMode::Read);
            m_Cmd->copyTexture(m_ScreenshotStagingTexture, nvrhi::TextureSlice(), m_ScenePanel->GetCompositeViewportRT()->GetColorAttachment(0)->GetHandle(), nvrhi::TextureSlice());
        }

        m_Cmd->close();
        Application::SubmitWorkerCommandList(m_Cmd);

        if (m_Data.takeScreenshot)
        {
            // Map and read the pixel data
            size_t rowPitch = 0;
            if (void *mappedData = m_Device->mapStagingTexture(m_ScreenshotStagingTexture, nvrhi::TextureSlice(), nvrhi::CpuAccessMode::Read, &rowPitch))
            {
                m_ScreenshotWidth = static_cast<int>(m_ScreenshotStagingTexture->getDesc().width);
                m_ScreenshotHeight = static_cast<int>(m_ScreenshotStagingTexture->getDesc().height);

                size_t packedStride = m_ScreenshotWidth * 4;
                m_ScreenshotPixelData.resize(m_ScreenshotHeight * packedStride);

                const uint8_t* src = static_cast<const uint8_t*>(mappedData);
                uint8_t* dst = m_ScreenshotPixelData.data();

                for (int y = 0; y < m_ScreenshotHeight; ++y)
                {
                    memcpy(dst + y * packedStride, src + y * rowPitch, packedStride);
                }

                m_Device->unmapStagingTexture(m_ScreenshotStagingTexture);

                SDL_ShowSaveFileDialog(OnScreenshotSaveFileSelected, this,
                    Application::GetInstance()->GetWindow()->GetWindowHandle(),
                    kScreenshotFileFilters, IM_ARRAYSIZE(kScreenshotFileFilters),
                    "Screenshot.png");
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
            if (!m_Data.projectCreateInfo.filepath.empty())
            {
                std::string filepathCopy = m_Data.projectCreateInfo.filepath.generic_string();
                if (filepathCopy.length() < sizeof(filepathBuffer))
                    memcpy(filepathBuffer, filepathCopy.c_str(), filepathCopy.length() + 1);
            }

            if (ImGui::InputText("Location", filepathBuffer, sizeof(filepathBuffer), ImGuiInputTextFlags_ReadOnly))
            {
                m_Data.projectCreateInfo.filepath = std::string(filepathBuffer);
            }

            ImGui::SameLine();
            if (ImGui::Button("..."))
            {
                SDL_ShowOpenFolderDialog(OnProjectFolderSelected, this,
                    Application::GetInstance()->GetWindow()->GetWindowHandle(),
                    nullptr, false);
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
                            // Use GetAssetImmediate since we're in modal and need synchronous load
                            if (Ref<Scene> activeScene = m_ActiveProject->GetAssetImmediate<Scene>(m_ActiveProject->GetInfo().defaultSceneHandle))
                            {
                                m_EditorScene = SceneManager::Copy(activeScene);
                                m_EditorScene->SetDirtyFlag(false);

                                SetActiveScene(m_EditorScene);

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
            // // scene dock space
            // m_ScenePanel->OnGuiRender();
            // m_ContentBrowserPanel->OnGuiRender();
            // m_MaterialsPanel->OnImGuiRender();

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
        }

        ImGui::End(); // end dock space
    }

    void EditorLayer::SetActiveScene(const Ref<Scene> &scene)
    {
		if (m_ActiveScene == scene)
		{
			return;
		}

		// Clear references in all systems before changing active scene
		if (m_ScenePanel)
		{
			m_ScenePanel->SetActiveScene(nullptr);
		}
		m_SceneRenderer->SetActiveScene(nullptr);
		if (m_ActiveProject)
		{
			m_ActiveProject->SetActiveScene(nullptr);
		}

		// Set new scene
		m_ActiveScene = scene;

		// Update all systems with new scene
		if (m_ScenePanel)
		{
			m_ScenePanel->SetActiveScene(scene);
		}
		m_SceneRenderer->SetActiveScene(scene);
		if (m_ActiveProject)
		{
			m_ActiveProject->SetActiveScene(scene);
		}
    }

    void EditorLayer::NewScene()
    {
        if (m_EditorScene)
        {
            m_EditorScene->OnStop();
        }

        if (m_Data.sceneState == State::ScenePlay)
        {
            OnSceneStop();
        }

        m_CurrentSceneFilePath.clear();

        m_CurrentSceneHandle = AssetHandle(0);

        // Clear active scene first to release references in renderer and panels
        SetActiveScene(nullptr);
        
        // Reset scenes - this should trigger destructors
        m_EditorScene.reset();
        m_ActiveScene.reset();
        
        // Unload unused assets (assets not referenced by anything else)
        if (m_ActiveProject)
        {
            m_ActiveProject->GetAssetManager().UnloadUnusedAssets();
        }
        
        // Force a brief wait to allow cleanup
        m_Device->waitForIdle();
        
        // Create new editor scene
        m_EditorScene = CreateRef<Scene>(m_ActiveProject.get(), "New Scene");

        // Set as active scene
        SetActiveScene(m_EditorScene);
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
        AssetHandle openSceneHandle = m_ActiveProject->GetAssetManager().GetAssetHandle(filepath);

        if (m_CurrentSceneHandle == openSceneHandle)
            return;

        m_CurrentSceneHandle = openSceneHandle;

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
            // Clear active scene references before loading new one
            SetActiveScene(nullptr);
            
            // Wait for GPU
            if (m_Device)
            {
                m_Device->waitForIdle();
            }
            
            // Reset old scene
            m_EditorScene.reset();
            m_ActiveScene.reset();
            
            // Unload unused assets from previous scene
            if (m_ActiveProject)
            {
                m_ActiveProject->GetAssetManager().UnloadUnusedAssets();
            }
            
            m_EditorScene = SceneManager::Copy(openScene);
            m_EditorScene->SetDirtyFlag(false);

            SetActiveScene(m_EditorScene);

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

        // Clear old project's loaded assets before opening new project
        if (m_ActiveProject)
        {
            SetActiveScene(nullptr);

            m_EditorScene.reset();
            m_ActiveScene.reset();

            // Clear all assets from old project
            m_ActiveProject->GetAssetManager().ClearAllLoadedAssets();
        }

        if (const Ref<Project> openedProject = ProjectSerializer::Deserialize(filepath))
        {
            m_ActiveProject = openedProject;
            m_CurrentProjectFilepath = filepath;

            // Reload project files
            m_ContentBrowserPanel->LoadProjectFiles();

            // Get Project default scene (use immediate load for synchronous path)
            if (m_ActiveProject->GetInfo().defaultSceneHandle != AssetHandle(0))
            {
                // Use GetAssetImmediate since we're on main thread and need synchronous load
                if (Ref<Scene> activeScene = m_ActiveProject->GetAssetImmediate<Scene>(m_ActiveProject->GetInfo().defaultSceneHandle))
                {
                    m_EditorScene = SceneManager::Copy(activeScene);
                    m_EditorScene->SetDirtyFlag(false);

                    SetActiveScene(m_EditorScene);

                    const auto &[assetFilepath, assetType] = m_ActiveProject->GetAssetManager().GetMetaData(activeScene->handle);

                    m_CurrentSceneFilePath = m_ActiveProject->GetAssetFilepath(assetFilepath);
                    m_CurrentSceneHandle = activeScene->handle;
                }
                else
                {
                    // Create a default scene if load failed
                    NewScene();
                }
            }
            else
            {
                // Create a default scene
                NewScene();
            }

			// Register callback for when textures are loaded to invalidate material binding sets
			openedProject->GetAssetManager().RegisterAssetLoadedCallback(
				[this](AssetHandle handle, AssetType type)
				{
					if (type == AssetType::Texture)
					{
						// Find all materials that use this texture and mark them dirty
						const auto &assets = Project::GetInstance()->GetAssetManager().GetLoadedAssets();
						for (const auto &[matHandle, asset] : assets)
						{
							if (asset->GetAssetType() == AssetType::Material)
							{
								Ref<Material> material = std::static_pointer_cast<Material>(asset);
								if (material)
								{
									// Check if this material uses the loaded texture
									if (material->baseColorTextureHandle == handle ||
										material->emissiveTextureHandle == handle ||
										material->metallicRoughnessTextureHandle == handle ||
										material->normalTextureHandle == handle ||
										material->occlusionTextureHandle == handle)
									{
										material->InvalidateBindingSet();
									}
								}
							}
						}
					}
				}
			);
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
        SetActiveScene(SceneManager::Copy(m_EditorScene));
        m_ActiveScene->OnStart();
    }

    void EditorLayer::OnSceneStop()
    {
        m_Data.sceneState = State::SceneEdit;
        
        m_ActiveScene->OnStop();
        SetActiveScene(m_EditorScene);
    }

    void  EditorLayer::OnSceneSimulate()
    {
        if (m_Data.sceneState != State::SceneEdit)
            OnSceneStop();

        m_Data.sceneState = State::SceneSimulate;

        // copy initial components to new scene
        SetActiveScene(SceneManager::Copy(m_EditorScene));
        m_ActiveScene->OnStart();
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

    void EditorLayer::OnScreenshotSaveFileSelected(void* userData, const char* const* filelist, int filter)
    {
        if (filelist == nullptr || *filelist == nullptr) return;

        std::string filepath = filelist[0];
        if (!filepath.empty())
        {
            if (!filepath.ends_with(".png"))
            {
                filepath += ".png";
            }

            EditorLayer* editor = static_cast<EditorLayer*>(userData);
            if (!editor->m_ScreenshotPixelData.empty())
            {
                const int channels = 4;
                // stride is width * 4 because we packed it
                stbi_write_png(filepath.c_str(), editor->m_ScreenshotWidth, editor->m_ScreenshotHeight, channels, editor->m_ScreenshotPixelData.data(), editor->m_ScreenshotWidth * channels);
                
                editor->m_ScreenshotPixelData.clear();
                editor->m_ScreenshotPixelData.shrink_to_fit();
            }
        }
    }

    void EditorLayer::OnProjectFolderSelected(void* userData, const char* const* filelist, int filter)
    {
        if (filelist == nullptr || *filelist == nullptr) return;

        std::string filepath = filelist[0];
        if (!filepath.empty())
        {
            EditorLayer* editor = static_cast<EditorLayer*>(userData);
            editor->m_Data.projectCreateInfo.filepath = std::filesystem::path(filepath);
        }
    }

    void EditorLayer::OnLoadHDRTextureSelected(void* userData, const char* const* filelist, int filter)
    {
        if (filelist == nullptr || *filelist == nullptr) return;

        std::string filepath = filelist[0];
        if (!filepath.empty())
        {
            EditorLayer* editor = static_cast<EditorLayer*>(userData);
            Project::GetInstance()->GetAssetManager().SubmitJob([editor, f = filepath, sr = editor->m_SceneRenderer]() mutable
            {
                auto &env = sr->GetEnvironment();
                env->LoadTexture(f);
                
                // Signal the renderer on the main thread that the environment has changed
                Application::SubmitToMainThread([sr]()
                {
                    sr->OnEnvironmentTextureChanged();
                });
            });
        }
    }

    void EditorLayer::ProcessPendingFileLoading()
    {
        while (!m_PendingFileLoading.empty())
        {
            auto pf = m_PendingFileLoading.front();
            m_PendingFileLoading.pop();

            switch (pf.type)
            {
                case PendingFileLoading::Open:
                {
                    if (pf.metadata.type == AssetType::Scene)
                    {
                        // Submit scene loading to asset worker
                        std::filesystem::path filepath = pf.metadata.filepath;
                        AssetHandle sceneHandle = m_ActiveProject->GetAssetManager().GetAssetHandle(filepath);
                        
                        if (m_CurrentSceneHandle == sceneHandle)
                            break;
                        
                        m_CurrentSceneHandle = sceneHandle;
                        
                        // Submit heavy I/O work to asset worker
                        m_ActiveProject->GetAssetManager().SubmitJob([this, filepath, sceneHandle]()
                        {
                            // Load scene on worker thread (I/O happens here)
                            Ref<Scene> loadedScene = SceneSerializer::Deserialize(filepath, m_ActiveProject.get());
                            if (loadedScene)
                            {
                                // Submit UI update back to main thread
                                Application::SubmitToMainThread([this, loadedScene, filepath]() mutable
                                {
                                    if (m_EditorScene)
                                    {
                                        m_EditorScene->OnStop();
                                    }
                                    
                                    if (m_Data.sceneState == State::ScenePlay)
                                    {
                                        OnSceneStop();
                                    }
                                    
                                    // Clear active scene references
                                    SetActiveScene(nullptr);
                                    
                                    // Wait for GPU
                                    if (m_Device)
                                    {
                                        m_Device->waitForIdle();
                                    }
                                    
                                    // Reset old scene
                                    m_EditorScene.reset();
                                    m_ActiveScene.reset();
                                    
                                    // Unload unused assets
                                    if (m_ActiveProject)
                                    {
                                        m_ActiveProject->GetAssetManager().UnloadUnusedAssets();
                                    }
                                    
                                    // Copy and activate new scene
                                    m_EditorScene = SceneManager::Copy(loadedScene);
                                    m_EditorScene->SetDirtyFlag(false);
                                    SetActiveScene(m_EditorScene);
                                    m_CurrentSceneFilePath = filepath;
                                });
                            }
                            else
                            {
                                LOG_ERROR("[Editor] Failed to load scene: {}", filepath.generic_string());
                            }
                        });
                    }
                    else if (pf.metadata.type == AssetType::Project)
                    {
                        std::filesystem::path filepath = pf.metadata.filepath;

                        if (filepath == m_CurrentProjectFilepath)
                        {
                            LOG_TRACE("Dismiss opening current project {0}", filepath.generic_string());
                            break;
                        }

                        Ref<Project> loadedProject = ProjectSerializer::Deserialize(filepath);
                        if (loadedProject)
                        {
                            // Submit UI update back to main thread
                            Application::SubmitToMainThread([this, loadedProject, filepath]() mutable {
                                // Clear old project's assets
                                if (m_ActiveProject)
                                {
                                    SetActiveScene(nullptr);

                                    if (m_Device)
                                    {
                                        m_Device->waitForIdle();
                                    }

                                    m_EditorScene.reset();
                                    m_ActiveScene.reset();
                                    m_ActiveProject->GetAssetManager().ClearAllLoadedAssets();
                                }

                                m_ActiveProject = loadedProject;
                                m_CurrentProjectFilepath = filepath;

                                // Reload content browser
                                m_ContentBrowserPanel->LoadProjectFiles();

                                // Load default scene
                                if (m_ActiveProject->GetInfo().defaultSceneHandle != AssetHandle(0))
                                {
                                    if (Ref<Scene> activeScene = m_ActiveProject->GetAssetImmediate<Scene>(m_ActiveProject->GetInfo().defaultSceneHandle))
                                    {
                                        m_EditorScene = SceneManager::Copy(activeScene);
                                        m_EditorScene->SetDirtyFlag(false);
                                        SetActiveScene(m_EditorScene);

                                        const auto &[assetFilepath, assetType] = m_ActiveProject->GetAssetManager().GetMetaData(activeScene->handle);
                                        m_CurrentSceneFilePath = m_ActiveProject->GetAssetFilepath(assetFilepath);
                                        m_CurrentSceneHandle = activeScene->handle;
                                    }
                                    else
                                    {
                                        NewScene();
                                    }
                                }
                                else
                                {
                                    NewScene();
                                }
                                });
                        }
                        else
                        {
                            LOG_ERROR("[Editor] Failed to load project: {}", filepath.generic_string());
                        }
                    }
                    break;
                }
                case PendingFileLoading::Save:
                {
                    if (pf.metadata.type == AssetType::Scene)
                    {
                        // Submit scene save to asset worker
                        std::filesystem::path filepath = pf.metadata.filepath;
                        
                        m_ActiveProject->GetAssetManager().SubmitJob([this, filepath]() {
                            SceneSerializer serializer(m_ActiveScene, m_ActiveProject.get());
                            serializer.Serialize(filepath);
                            
                            LOG_INFO("[Editor] Scene saved: {}", filepath.generic_string());
                        });
                    }
                    else if (pf.metadata.type == AssetType::Project)
                    {
                        SaveProjectAs();
                    }
                    break;
                }
            }
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
                        m_SceneRenderer->SetFillMode(m_Data.rasterFillMode);
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
                    SDL_ShowOpenFileDialog(OnLoadHDRTextureSelected, this,
                        Application::GetInstance()->GetWindow()->GetWindowHandle(),
                        kHDRFileFilters, IM_ARRAYSIZE(kHDRFileFilters),
                        nullptr, false);
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
                    auto csm = m_SceneRenderer->GetCascadedShadowMap();
                    auto &data = csm->GetGPUData();
                    ImGui::SliderFloat("Strength", &data.shadowStrength, 0.0f, 1.0f);
                    ImGui::DragFloat("Min Bias", &data.minBias, 0.0001f, 0.0f, 0.1f, "%.6f");
                    ImGui::DragFloat("Max Bias", &data.maxBias, 0.0001f, 0.0f, 0.1f, "%.6f");
                    ImGui::SliderFloat("PCF Radius", &data.pcfRadius, 0.1f, 4.0f);

                    static const char *resolutionLabels[] = { "Low - 512px", "Medium - 1024px", "High - 2048px", "Ultra - 4096px" };
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
            const auto &loadedAssets = m_ActiveProject->GetAssetManager().GetLoadedAssets();

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
            static AssetType selectedTypeFilter = AssetType::Invalid; // All types
            static int sortColumn = 0; // 0=Handle, 1=Type, 2=Filepath, 3=Status
            static bool sortAscending = true;

			ImGui::SetNextWindowSize(ImVec2(1200, 700), ImGuiCond_FirstUseEver);
			ImGui::Begin("Asset Registry & Memory Monitor", &m_Data.assetRegistryWindow);
			ImGui::BeginChild("asset_registry_scroll", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar);

            // === STATISTICS PANEL ===
			ImGui::Text("Asset Statistics & Memory Usage");
			ImGui::Separator();

			// Calculate statistics
			std::unordered_map<AssetType, int> registeredCounts;
			std::unordered_map<AssetType, int> loadedCounts;
			std::unordered_map<AssetType, size_t> memoryUsage;

			for (const auto &[handle, metadata] : assetRegistry)
			{
				registeredCounts[metadata.type]++;
			}

			for (const auto &[handle, asset] : loadedAssets)
			{
				if (asset)
				{
					AssetType type = m_ActiveProject->GetAssetManager().GetAssetType(handle);
					loadedCounts[type]++;
					// Estimate memory usage (this is approximate)
					memoryUsage[type] += asset.use_count() * 8; // Basic pointer overhead
				}
			}

			// Display statistics in columns
			ImGui::Columns(2, "stats_columns", true);

			// Left column - Asset counts
			ImGui::Text("REGISTERED ASSETS");
			ImGui::Separator();
			int totalRegistered = 0;
			for (const auto &[type, count] : registeredCounts)
			{
				if (type != AssetType::Invalid)
				{
					ImGui::Text("%s: %d", AssetTypeToString(type).c_str(), count);
					totalRegistered += count;
				}
			}
			ImGui::Separator();
			ImGui::Text("Total Registered: %d", totalRegistered);

			ImGui::NextColumn();

			// Right column - Loaded assets & memory
			ImGui::Text("LOADED IN MEMORY");
			ImGui::Separator();
			int totalLoaded = 0;
			for (const auto &[type, count] : loadedCounts)
			{
				if (type != AssetType::Invalid)
				{
					float percentage = registeredCounts[type] > 0
						? (float)count / registeredCounts[type] * 100.0f
						: 0.0f;

					// Color code by type
					ImVec4 color = ImVec4(0.5f, 0.8f, 0.5f, 1.0f);
					if (type == AssetType::Texture) color = ImVec4(0.9f, 0.5f, 0.5f, 1.0f);
					else if (type == AssetType::StaticMesh) color = ImVec4(0.5f, 0.5f, 0.9f, 1.0f);
					else if (type == AssetType::Material) color = ImVec4(0.9f, 0.9f, 0.5f, 1.0f);

					ImGui::TextColored(color, "%s: %d (%.1f%%)",
						AssetTypeToString(type).c_str(), count, percentage);

					// Memory bar
					if (memoryUsage[type] > 0)
					{
						ImGui::SameLine();
						ImGui::Text("~%zu KB", memoryUsage[type] / 1024);
					}

					totalLoaded += count;
				}
			}
			ImGui::Separator();
			ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Total Loaded: %d", totalLoaded);

			ImGui::Columns(1);

			// Memory usage bar
			ImGui::Spacing();
			float loadRatio = totalRegistered > 0 ? (float)totalLoaded / totalRegistered : 0.0f;
			ImGui::ProgressBar(loadRatio, ImVec2(-1, 0),
				std::string("Memory Load: " + std::to_string((int)(loadRatio * 100)) + "%").c_str());

			// === FILTERS & CONTROLS ===
			if (ImGui::BeginTable("asset_registry_controls", 2, ImGuiTableFlags_SizingStretchProp))
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::AlignTextToFramePadding();
				ImGui::Text("Search:");
				ImGui::SameLine();

				static char buffer[256] = { 0 };
				ImGui::SetNextItemWidth(300);
				if (ImGui::InputTextWithHint("##asset_registry_filter", "Handle, Type, or Filepath...",
					buffer, sizeof(buffer), ImGuiInputTextFlags_EscapeClearsAll))
				{
					assetRegistryFilterResultStr = std::string(buffer);
				}

				ImGui::SameLine();
				ImGui::Text("Type Filter:");
				ImGui::SameLine();

				// Type filter dropdown
				const char *typeNames[] = { "All", "Scene", "Texture", "Material", "StaticMesh", "Audio", "Skeleton" };
				const AssetType typeValues[] = {
					AssetType::Invalid, AssetType::Scene, AssetType::Texture,
					AssetType::Material, AssetType::StaticMesh, AssetType::Audio, AssetType::Skeleton
				};

				int currentTypeIndex = 0;
				for (int i = 0; i < IM_ARRAYSIZE(typeValues); i++)
				{
					if (typeValues[i] == selectedTypeFilter)
					{
						currentTypeIndex = i;
						break;
					}
				}

				ImGui::SetNextItemWidth(150);
				if (ImGui::Combo("##type_filter", &currentTypeIndex, typeNames, IM_ARRAYSIZE(typeNames)))
				{
					selectedTypeFilter = typeValues[currentTypeIndex];
				}

				ImGui::SameLine();
				ImGui::Checkbox("Full Path", &showFullPath);

				ImGui::TableNextColumn();
				ImGui::BeginGroup();
				if (ImGui::SmallButton("Refresh Registry"))
				{
					m_ActiveProject->ValidateAssetRegistry();
				}
				if (ImGui::SmallButton("Unload Unused Assets"))
				{
					m_ActiveProject->GetAssetManager().UnloadUnusedAssets();
				}
				ImGui::EndGroup();
				ImGui::EndTable();
			}

            ImGui::Spacing();

            // === ASSET TABLE ===
            // Apply filters
            filteredAssets.clear();
            const std::string findKey = stringutils::ToLower(assetRegistryFilterResultStr);

            for (const auto &[handle, metadata] : assetRegistry)
            {
                // Type filter
                if (selectedTypeFilter != AssetType::Invalid && metadata.type != selectedTypeFilter)
                    continue;

                // Text search filter
                if (!assetRegistryFilterResultStr.empty())
                {
                    const std::string &handleStr = std::to_string(handle);
                    const std::string &typeStr = stringutils::ToLower(AssetTypeToString(metadata.type));
                    const std::string &filepathStr = stringutils::ToLower(
                        std::filesystem::absolute(m_ActiveProject->GetAssetFilepath(metadata.filepath)).generic_string());

                    if (handleStr.find(findKey) == std::string::npos &&
                        typeStr.find(findKey) == std::string::npos &&
                        filepathStr.find(findKey) == std::string::npos)
                    {
                        continue;
                    }
                }

                filteredAssets.insert({ handle, metadata });
            }

            // Display table
            ImGuiTableFlags tableFlags = ImGuiTableFlags_Sortable | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
                ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;

            if (ImGui::BeginTable("asset_registry_table", 5, tableFlags))
            {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("Handle", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed, 100.0f, 0);
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 100.0f, 1);
                ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 80.0f, 2);
                ImGui::TableSetupColumn("Refs", ImGuiTableColumnFlags_WidthFixed, 50.0f, 3);
                ImGui::TableSetupColumn("Filepath", ImGuiTableColumnFlags_WidthStretch, 0.0f, 4);
                ImGui::TableHeadersRow();

                // Handle sorting
                ImGuiTableSortSpecs *sortSpecs = ImGui::TableGetSortSpecs();
                if (sortSpecs && sortSpecs->SpecsDirty)
                {
                    sortColumn = sortSpecs->Specs[0].ColumnUserID;
                    sortAscending = sortSpecs->Specs[0].SortDirection == ImGuiSortDirection_Ascending;
                    sortSpecs->SpecsDirty = false;
                }

                // Determine which assets to display
                bool useFiltered = !filteredAssets.empty() || !assetRegistryFilterResultStr.empty() || selectedTypeFilter != AssetType::Invalid;
                
                // Iterate through appropriate asset collection
                if (useFiltered)
                {
                    for (const auto &[handle, metadata] : filteredAssets)
                    {
                        ImGui::TableNextRow();

                        // Column 0: Handle
                        ImGui::TableNextColumn();
                        ImGui::Text("%llu", static_cast<uint64_t>(handle));

                        // Column 1: Type with color coding
                        ImGui::TableNextColumn();
                        ImVec4 typeColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
                        if (metadata.type == AssetType::Texture) typeColor = ImVec4(0.9f, 0.5f, 0.5f, 1.0f);
                        else if (metadata.type == AssetType::StaticMesh) typeColor = ImVec4(0.5f, 0.5f, 0.9f, 1.0f);
                        else if (metadata.type == AssetType::Material) typeColor = ImVec4(0.9f, 0.9f, 0.5f, 1.0f);
                        else if (metadata.type == AssetType::Scene) typeColor = ImVec4(0.5f, 0.9f, 0.9f, 1.0f);

                        std::string assetTypeStr = AssetTypeToString(metadata.type);
                        ImGui::TextColored(typeColor, "%s", assetTypeStr.c_str());

                        // Column 2: Load Status
                        ImGui::TableNextColumn();
                        bool isLoaded = loadedAssets.find(handle) != loadedAssets.end();
                        if (isLoaded)
                        {
                            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "LOADED");
                        }
                        else
                        {
                            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Disk");
                        }

                        // Column 3: Reference count
                        ImGui::TableNextColumn();
                        if (isLoaded)
                        {
                            auto it = loadedAssets.find(handle);
                            if (it != loadedAssets.end() && it->second)
                            {
                                long refCount = it->second.use_count();
                                ImVec4 refColor = refCount == 1 ? ImVec4(1.0f, 0.5f, 0.3f, 1.0f) : ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
                                ImGui::TextColored(refColor, "%ld", refCount);
                            }
                        }
                        else
                        {
                            ImGui::Text("-");
                        }

                        // Column 4: Filepath
                        ImGui::TableNextColumn();
                        std::string displayPath;
                        if (showFullPath)
                        {
                            displayPath = std::filesystem::absolute(m_ActiveProject->GetAssetFilepath(metadata.filepath)).generic_string();
                        }
                        else
                        {
                            displayPath = metadata.filepath.generic_string();
                        }
                        ImGui::TextWrapped("%s", displayPath.c_str());

                        // Actions for Scene assets
                        if (metadata.type == AssetType::Scene)
                        {
                            ImGui::SameLine();
                            ImGui::PushID(static_cast<int>(handle));
                            if (ImGui::SmallButton("Default"))
                            {
                                m_ActiveProject->GetInfo().defaultSceneHandle = handle;
                                SaveProject();
                            }
                            ImGui::PopID();
                        }
                    }
                }
                else
                {
                    for (const auto &[handle, metadata] : assetRegistry)
                    {
                        ImGui::TableNextRow();

                        // Column 0: Handle
                        ImGui::TableNextColumn();
                        ImGui::Text("%llu", static_cast<uint64_t>(handle));

                        // Column 1: Type with color coding
                        ImGui::TableNextColumn();
                        ImVec4 typeColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
                        if (metadata.type == AssetType::Texture) typeColor = ImVec4(0.9f, 0.5f, 0.5f, 1.0f);
                        else if (metadata.type == AssetType::StaticMesh) typeColor = ImVec4(0.5f, 0.5f, 0.9f, 1.0f);
                        else if (metadata.type == AssetType::Material) typeColor = ImVec4(0.9f, 0.9f, 0.5f, 1.0f);
                        else if (metadata.type == AssetType::Scene) typeColor = ImVec4(0.5f, 0.9f, 0.9f, 1.0f);

                        std::string assetTypeStr = AssetTypeToString(metadata.type);
                        ImGui::TextColored(typeColor, "%s", assetTypeStr.c_str());

                        // Column 2: Load Status
                        ImGui::TableNextColumn();
                        bool isLoaded = loadedAssets.find(handle) != loadedAssets.end();
                        if (isLoaded)
                        {
                            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "LOADED");
                        }
                        else
                        {
                            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Disk");
                        }

                        // Column 3: Reference count
                        ImGui::TableNextColumn();
                        if (isLoaded)
                        {
                            auto it = loadedAssets.find(handle);
                            if (it != loadedAssets.end() && it->second)
                            {
                                long refCount = it->second.use_count();
                                ImVec4 refColor = refCount == 1 ? ImVec4(1.0f, 0.5f, 0.3f, 1.0f) : ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
                                ImGui::TextColored(refColor, "%ld", refCount);
                            }
                        }
                        else
                        {
                            ImGui::Text("-");
                        }

                        // Column 4: Filepath
                        ImGui::TableNextColumn();
                        std::string displayPath;
                        if (showFullPath)
                        {
                            displayPath = std::filesystem::absolute(m_ActiveProject->GetAssetFilepath(metadata.filepath)).generic_string();
                        }
                        else
                        {
                            displayPath = metadata.filepath.generic_string();
                        }
                        ImGui::TextWrapped("%s", displayPath.c_str());

                        // Actions for Scene assets
                        if (metadata.type == AssetType::Scene)
                        {
                            ImGui::SameLine();
                            ImGui::PushID(static_cast<int>(handle));
                            if (ImGui::SmallButton("Set as default"))
                            {
                                m_ActiveProject->GetInfo().defaultSceneHandle = handle;
                                SaveProject();
                            }
                            ImGui::PopID();
                        }
                    }
                }
                ImGui::EndTable();
            }
			ImGui::EndChild();
			ImGui::End();
        }
    }
}
