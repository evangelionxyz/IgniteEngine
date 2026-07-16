// Copyright (c) 2026 Evangelion Manuhutu | IGNITE STUDIO

#include "pch.hpp"
#include "editor_layer.hpp"
#include "panels/scene_panel.hpp"
#include "panels/content_browser_panel.hpp"
#include "panels/asset_importer_panel.hpp"
#include "panels/asset_editor_panel.hpp"
#include "ext/editor_ui.hpp"
#include "ignite/core/command.hpp"
#include "ignite/graphics/renderer/renderer_2d.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/asset/asset_worker.hpp"
#include "ignite/asset/asset.hpp"
#include "ignite/asset/asset_importer.hpp"
#include "ignite/scripting/script_engine.hpp"
#include "ignite/graphics/objects/shadow_map.hpp"
#include "ignite/core/platform_utils.hpp"
#include "ignite/core/profiler/profiler.hpp"
#include "ignite/imgui/imgui_nvrhi.hpp"
#include "ignite/imgui/imgui_layer.hpp"
#include "ignite/graphics/shader.hpp"
#include "ignite/graphics/ui/game_ui_system.hpp"
#include "ignite/globals/globals.hpp"
#include "stb_image_write.h"

#include <algorithm>
#include <mutex>
#include <spdlog/spdlog.h>
#include <cmath>
#include <format>
#include <limits>
#include <string_view>
#include <unordered_set>
#include <SDL3/SDL_dialog.h>

namespace ignite
{
    namespace
    {
        void BindSharedImGuiContext()
        {
            ImGuiContext *sharedContext = Application::GetImGuiContext();
            LOG_ASSERT(sharedContext, "Engine ImGui context has not been created");

            if (ImGui::GetCurrentContext() != sharedContext)
            {
                ImGui::SetCurrentContext(sharedContext);
            }
        }

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

    EditorLayer::EditorLayer(const std::string &name)
        : Layer(name), m_StatusText("Ready")
    {
    }

    EditorLayer::~EditorLayer()
    {
    }

    void EditorLayer::OnAttach()
    {
        Layer::OnAttach();
        BindSharedImGuiContext();

        m_Device = DeviceManager::GetInstance()->GetDevice();

        auto *app = Application::GetInstance();

        m_FileImportSignalToken = SignalBus::Subscribe<FileImportPayload>([this](const FileImportPayload & payload)
            { OnFileImport(payload); });

        m_ScenePanel = new ScenePanel("Scene Panel", this);
        m_AssetImporterPanel = new AssetImporterPanel("AssetImporter Panel", this);
        m_AssetEditorPanel = new AssetEditorPanel("Animation Panel", this);

        app->PushLayer(m_ScenePanel);
        app->PushLayer(m_AssetImporterPanel);
        app->PushLayer(m_AssetEditorPanel);

        AddContentBrowserPanel();
        
        AssetWorker::SetStatusCallback([this](std::string_view status, float progress)
        {
            Application::SubmitToMainThread([this, s = std::string(status), progress]()
            {
                SetStatusText(s);
                if (progress >= 0.0f)
                    SetLoadingProgress(progress);
            });
        });

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

        // Close project
        if (m_ActiveProject)
        {
            SaveProject();
            OnSceneStop();

            m_EditorScene.reset();
            m_ActiveScene.reset();

            m_ActiveProject->GetAssetManager()->ClearAllLoadedAssets();

            // Reset everything
            m_CurrentProjectFilepath.clear();
            m_CurrentSceneFilePath.clear();
            m_CurrentSceneHandle = AssetHandle(0);
        }

        // Unsubscribe signals
        SignalBus::Unsubscribe<SuccessResultSignal>(m_ProjectReadySignalToken);
        SignalBus::Unsubscribe<SuccessResultSignal>(m_FileImportSignalToken);
        m_ProjectReadySignalToken = kInvalidSignalToken;
        m_FileImportSignalToken = kInvalidSignalToken;

        m_ContentBrowserPanels.clear();
        m_ContentBrowserPanelsPendingRemoval.clear();
        m_ContentBrowserPanel = nullptr;
        ContentBrowserPanel::ReleaseSharedResources();

        m_SceneRenderer = nullptr;
        m_ActiveProject = nullptr;
    }

    void EditorLayer::AddContentBrowserPanel()
    {
        if (GetOpenContentBrowserCount() >= 4)
        {
            return;
        }

        const uint32_t panelId = m_NextContentBrowserPanelId++;
        const uint32_t panelNumber = GetOpenContentBrowserCount() + 1;
        const std::string panelTitle = panelNumber == 1
            ? std::format("Content Browser###ContentBrowser_{}", panelId)
            : std::format("Content Browser {}###ContentBrowser_{}", panelNumber, panelId);

        auto panel = new ContentBrowserPanel(panelTitle.c_str(), this);
        Application::GetInstance()->PushLayer(panel);

        m_ContentBrowserPanels.push_back(panel);
        if (!m_ContentBrowserPanel)
        {
            m_ContentBrowserPanel = panel;
        }

        if (m_ActiveProject)
        {
            panel->LoadProjectFiles(m_ActiveProject->GetAssetManager());
        }
    }

    void EditorLayer::ReloadContentBrowserPanels()
    {
        if (!m_ActiveProject)
        {
            return;
        }

        for (ContentBrowserPanel *panel : m_ContentBrowserPanels)
        {
            if (panel)
            {
                panel->LoadProjectFiles(m_ActiveProject->GetAssetManager());
            }
        }
    }

    uint32_t EditorLayer::GetOpenContentBrowserCount() const
    {
        uint32_t openCount = 0;
        for (ContentBrowserPanel *panel : m_ContentBrowserPanels)
        {
            if (panel && panel->IsOpen())
            {
                ++openCount;
            }
        }

        return openCount;
    }

    void EditorLayer::OnUpdate(float deltaTime)
    {
        Layer::OnUpdate(deltaTime);

        for (int i = static_cast<int>(m_ContentBrowserPanels.size()) - 1; i >= 0; --i)
        {
            ContentBrowserPanel *panel = m_ContentBrowserPanels[static_cast<size_t>(i)];
            if (panel == nullptr)
            {
                m_ContentBrowserPanels.erase(m_ContentBrowserPanels.begin() + i);
                continue;
            }

            if (!panel->IsOpen())
            {
                if (m_ContentBrowserPanelsPendingRemoval.insert(panel).second)
                {
                    Application::SubmitToMainThread([this, panel]()
                    {
                        auto it = std::find(m_ContentBrowserPanels.begin(), m_ContentBrowserPanels.end(), panel);
                        if (it != m_ContentBrowserPanels.end())
                        {
                            Application::GetInstance()->PopLayer(panel);
                            m_ContentBrowserPanels.erase(it);
                        }

                        m_ContentBrowserPanelsPendingRemoval.erase(panel);
                    });
                }
            }
        }

        if (m_ContentBrowserPanels.empty() && m_PendingContentBrowserPanelsToAdd == 0 && m_ContentBrowserPanelsPendingRemoval.empty())
        {
            m_PendingContentBrowserPanelsToAdd = 1;
        }

        m_ContentBrowserPanel = nullptr;
        for (ContentBrowserPanel *panel : m_ContentBrowserPanels)
        {
            if (panel && panel->IsOpen())
            {
                m_ContentBrowserPanel = panel;
                break;
            }
        }

        while (m_PendingContentBrowserPanelsToAdd > 0 && GetOpenContentBrowserCount() < 4)
        {
            Application::SubmitToMainThread([this]()
            {
                AddContentBrowserPanel();
            });
            --m_PendingContentBrowserPanelsToAdd;
        }

        for (ContentBrowserPanel *contentBrowserPanel : m_ContentBrowserPanels)
        {
            if (contentBrowserPanel && contentBrowserPanel->IsOpen())
            {
                contentBrowserPanel->OnUpdate(deltaTime);
            }
        }

        if (m_ActiveProject)
        {
            m_ActiveProject->GetAssetManager()->OnUpdate(deltaTime);
        }

        if (m_ActiveProject && m_ActiveProject->GetActiveScene() != m_ActiveScene)
        {
            SetActiveScene(m_ActiveProject->GetActiveScene());
        }

        // update panels
        if (m_ActiveScene)
        {
            // multi select entity
            m_State.multiSelect = InputSystem::IsModifierPressed(KeyMod::LeftShift);

            switch (m_ActiveScene->GetState())
            {
            case ESceneState::Simulate:
            case ESceneState::Play:
            {
                m_ActiveScene->OnUpdateRuntimeSimulate(deltaTime);
                break;
            }
            case ESceneState::Stop:
            {
                m_ActiveScene->OnUpdateEdit(deltaTime);
                break;
            }
            }

            m_ScenePanel->OnUpdate(deltaTime);

            // Block ImGui mouse/keyboard input while the scene viewport is focused
            // so users can't accidentally drag editor panels during gameplay.
            if (auto *imguiLayer = Application::GetInstance()->GetImGuiLayer())
            {
                const bool isSceneFocused = m_ScenePanel->IsFocused() && m_ScenePanel->m_SceneFocused && m_ActiveScene->IsRunning()
                    && (InputSystem::GetCursorMode() == CursorMode::Disabled || InputSystem::GetCursorMode() == CursorMode::Hidden);
                imguiLayer->SetBlock(isSceneFocused);
            }
        }
    }

    void EditorLayer::OnEvent(Event &e)
    {
        BindSharedImGuiContext();
        Layer::OnEvent(e);

        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<KeyPressedEvent>(BIND_CLASS_EVENT_FN(EditorLayer::OnKeyPressedEvent));
        dispatcher.Dispatch<MouseButtonPressedEvent>(BIND_CLASS_EVENT_FN(EditorLayer::OnMouseButtonPressed));
    }

    void EditorLayer::OnSDLEvent(SDL_Event *evt)
    {
        if (!m_SceneRenderer)
            return;

        SDL_Event modifiedEvent = *evt;
        if (m_ScenePanel && (evt->type == SDL_EVENT_MOUSE_MOTION || evt->type == SDL_EVENT_MOUSE_BUTTON_DOWN || evt->type == SDL_EVENT_MOUSE_BUTTON_UP))
        {
            const glm::vec2& mousePos = m_ScenePanel->GetViewportMousePos();
            if (evt->type == SDL_EVENT_MOUSE_MOTION)
            {
                modifiedEvent.motion.x = mousePos.x;
                modifiedEvent.motion.y = mousePos.y;
            }
            else
            {
                modifiedEvent.button.x = mousePos.x;
                modifiedEvent.button.y = mousePos.y;
            }
        }
    }

    bool EditorLayer::OnKeyPressedEvent(KeyPressedEvent &event)
    {
        bool control = InputSystem::IsModifierPressed(KeyMod::Control);
        bool shift = InputSystem::IsModifierPressed(KeyMod::LeftShift);

        if (ImGui::GetIO().WantTextInput)
            return false;

        switch (event.GetKeyCode())
        {
            case Key::F:
            {
                Entity entity = m_ScenePanel->GetSelectedEntity();
                if (entity.IsValid())
                {
                    auto &cam = m_ScenePanel->GetViewportCamera();
                    auto &tr = entity.GetComponent<TransformComponent>();

                    glm::vec3 focusCenter = tr.world.translation;
                    glm::vec3 halfExtents = glm::abs(tr.world.scale) * 0.5f;

                    if (entity.HasComponent<StaticMeshComponent>())
                    {
                        const auto &smc = entity.GetComponent<StaticMeshComponent>();
                        if (smc.handle != AssetHandle(0))
                        {
                            if (auto mesh = m_ActiveProject->GetAsset<StaticMesh>(smc.handle))
                            {
                                const auto &aabb = smc.worldAABB;
                                focusCenter = (aabb.min + aabb.max) * 0.5f;
                                halfExtents = glm::abs(aabb.max - aabb.min);
                            }
                        }
                    }
                    else if (entity.HasComponent<SkeletalMeshComponent>())
                    {
                        const auto &smc = entity.GetComponent<SkeletalMeshComponent>();
                        if (smc.handle != AssetHandle(0))
                        {
                            if (auto mesh = m_ActiveProject->GetAsset<SkeletalMesh>(smc.handle))
                            {
                                const auto &aabb = smc.worldAABB;
                                focusCenter = (aabb.min + aabb.max) * 0.5f;
                                halfExtents = glm::abs(aabb.max - aabb.min);
                            }
                        }
                    }

                    const float radius = glm::max(halfExtents.x, glm::max(halfExtents.y, halfExtents.z));
                    const float fov = glm::radians(cam.fov);
                    float distance = radius / std::tan(fov * 0.5f);

                    cam.FocusTarget(focusCenter, distance);
                }
                break;
            }
            case Key::Escape:
            {
                if (m_ScenePanel->IsFocused())
                {
                    m_ScenePanel->SetGizmoOperation(GizmoOperation::NONE);
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
            case Key::Q:
            {
                if (!InputSystem::IsMouseButtonPressed(Mouse::ButtonRight))
                    m_ScenePanel->SetGizmoOperation(GizmoOperation::BOUND_SIZING_2D);
                break;
            }
            case Key::T:
            {
                if (!InputSystem::IsMouseButtonPressed(Mouse::ButtonRight))
                    m_ScenePanel->SetGizmoOperation(GizmoOperation::TRANSLATE);
                break;
            }
            case Key::R:
            {
                if (!InputSystem::IsMouseButtonPressed(Mouse::ButtonRight))
                    m_ScenePanel->SetGizmoOperation(GizmoOperation::ROTATE);
                break;
            }
            case Key::E:
            {
                if (!InputSystem::IsMouseButtonPressed(Mouse::ButtonRight))
                    m_ScenePanel->SetGizmoOperation(GizmoOperation::SCALE);
                break;
            }
            case Key::F5:
            {
                if (m_ActiveScene)
                {
                    (m_ActiveScene->IsStopped() || m_ActiveScene->IsRunning())
                        ? OnScenePlay()
                        : OnSceneStop();
                }
                break;
            }
            case Key::F6:
            {
                if (m_ActiveScene)
                {
                    (m_ActiveScene->IsStopped() || m_ActiveScene->IsRunning())
                        ? OnSceneSimulate()
                        : OnSceneStop();
                }
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
        return false;
    }

    void EditorLayer::OnRender(nvrhi::IFramebuffer *mainFramebuffer)
    {
        IGN_PROFILE_FUNCTION();
        Layer::OnRender(mainFramebuffer);

        if (!m_ActiveScene)
            return;

        if (m_SceneRenderer)
        {
            m_SceneRenderer->BeginFrame();
        }

        // Resizing editor camera
        ICamera *editCamera = &m_ScenePanel->GetViewportCamera();
        if (editCamera)
        {
            // Resize Edit Viewport Framebuffer
            auto target = m_SceneRenderer->GetRenderTarget(editCamera);
            if (target)
            {
                const glm::uvec2 framebufferSize = target->compositeRT->GetSize();
                const glm::uvec2 desiredSize = glm::max(glm::uvec2(0), glm::uvec2(globals::GEditor::EditorViewport.max));
                const bool framebufferNeedsResize = (framebufferSize.x != desiredSize.x || framebufferSize.y != desiredSize.y);
                const bool isFramebufferSizeValid = desiredSize.x > 0 && desiredSize.y > 0;

                if (isFramebufferSizeValid)
                {
                    // Resize camera
                    if (framebufferNeedsResize)
                    {
                        m_ScenePanel->GetViewportCamera().UpdateProjection(desiredSize.x, desiredSize.y);
                        m_State.editorResizing = true;
                    }

                    // Resize framebuffer when in stable frame
                    if (m_State.editorResizing)
                    {
                        if (m_State.editorResizingFrame++ >= m_State.STABLE_RESIZE_FRAME)
                        {
                            m_SceneRenderer->ResizeFramebuffer(editCamera, desiredSize.x, desiredSize.y);
                            m_State.editorResizing = false;
                            m_State.editorResizingFrame = 0;
                        }
                    }
                }
            }
            
        }

        // Resizing game-play camera (Game Viewport)
        if (Entity primaryCam = m_ActiveScene->GetPrimaryCamera())
        {
            auto &cc = primaryCam.GetComponent<CameraComponent>();
            ICamera *gameCamera = &cc.camera;
            {
                auto target = m_SceneRenderer->GetRenderTarget(gameCamera);
                
                if (target)
                {
                    // Resize Game Viewport Framebuffer
                    const glm::uvec2 framebufferSize = target->compositeRT->GetSize();
                    const glm::uvec2 desiredSize = glm::max(glm::uvec2(0), glm::uvec2(globals::GEditor::GameViewport.max));
                    const bool framebufferNeedsResize = framebufferSize.x != desiredSize.x || framebufferSize.y != desiredSize.y;
                    const bool isFramebufferSizeValid = desiredSize.x > 0 && desiredSize.y > 0;

                    if (isFramebufferSizeValid)
                    {
                        if (framebufferNeedsResize)
                        {
                            gameCamera->UpdateProjection(desiredSize.x, desiredSize.y);
                            m_State.gameplayResizing = true;
                        }

                        if (m_State.gameplayResizing)
                        {
                            if (m_State.gameplayResizingFrame++ >= m_State.STABLE_RESIZE_FRAME)
                            {
                                m_SceneRenderer->ResizeFramebuffer(gameCamera, desiredSize.x, desiredSize.y);
                                m_State.gameplayResizing = false;
                                m_State.gameplayResizingFrame = 0;
                            }
                        }
                    }
                }
            }
        }

        // Resize the EditorPlayCamera (mirror camera for Play mode → Editor Viewport).
        // It gets its own independent render target sized to the Editor Viewport,
        // so resizing the editor panel never affects the Game Viewport's projection.
        if (m_ActiveScene->IsRunning())
        {
            if (Entity primaryCam = m_ActiveScene->GetPrimaryCamera())
            {
                auto &cc = primaryCam.GetComponent<CameraComponent>();

                // Sync camera properties (fov, near/far, projection type) from the game camera
                m_EditorPlayCamera.fov           = cc.camera.fov;
                m_EditorPlayCamera.nearPlane     = cc.camera.nearPlane;
                m_EditorPlayCamera.farPlane      = cc.camera.farPlane;
                m_EditorPlayCamera.orthoSize     = cc.camera.orthoSize;
                m_EditorPlayCamera.projectionType = cc.camera.projectionType;
                m_EditorPlayCamera.postProcessing = cc.camera.postProcessing;
                m_EditorPlayCamera.lens          = cc.camera.lens;
            }

            // Resize EditorPlayCamera framebuffer to match the Editor Viewport size
            auto editorPlayTarget = m_SceneRenderer->GetRenderTarget(&m_EditorPlayCamera);
            if (editorPlayTarget)
            {
                const glm::uvec2 framebufferSize = editorPlayTarget->compositeRT->GetSize();
                const glm::uvec2 desiredSize = glm::max(glm::uvec2(0), glm::uvec2(globals::GEditor::EditorViewport.max));
                const bool framebufferNeedsResize = framebufferSize.x != desiredSize.x || framebufferSize.y != desiredSize.y;
                const bool isFramebufferSizeValid = desiredSize.x > 0 && desiredSize.y > 0;

                if (isFramebufferSizeValid)
                {
                    if (framebufferNeedsResize)
                    {
                        m_EditorPlayCamera.UpdateProjection(desiredSize.x, desiredSize.y);
                        m_State.editorPlayResizing = true;
                    }

                    if (m_State.editorPlayResizing)
                    {
                        if (m_State.editorPlayResizingFrame++ >= m_State.STABLE_RESIZE_FRAME)
                        {
                            m_SceneRenderer->ResizeFramebuffer(&m_EditorPlayCamera, desiredSize.x, desiredSize.y);
                            m_State.editorPlayResizing = false;
                            m_State.editorPlayResizingFrame = 0;
                        }
                    }
                }
            }
        }

        // Render to Edit Viewport
        if (m_ScenePanel->m_Data.sceneViewportEditorVisible)
        {
            switch (m_ActiveScene->GetState())
            {
                case ESceneState::Play:
                {
                    if (Entity primaryCam = m_ActiveScene->GetPrimaryCamera())
                    {
                        auto &cc = primaryCam.GetComponent<CameraComponent>();
                        // Copy the live view matrix from the game camera into our editor-side mirror camera.
                        // The projection is already sized to the Editor Viewport, so both viewports are independent.
                        m_EditorPlayCamera.SetView(cc.camera.GetView());
                        m_EditorPlayCamera.position = cc.camera.position;
                        {
                            IGN_PROFILE_SCOPE("SceneRenderer::RenderPlayToEditorViewport");
                            m_SceneRenderer->Render(&m_EditorPlayCamera, false);
                        }
                        break;
                    }
                }
                case ESceneState::Simulate:
                case ESceneState::Stop:
                {
                    IGN_PROFILE_SCOPE("SceneRenderer::RenderEditorTo");
                    m_SceneRenderer->Render(editCamera, true); // enable draw debug
                    break;
                }
            }
        }

        // Render to Game Viewport
        if (m_State.gameplayViewportWindow && m_ScenePanel->m_Data.sceneViewportGameplayVisible)
        {
            if (Entity primaryCam = m_ActiveScene->GetPrimaryCamera())
            {
                auto &cc = primaryCam.GetComponent<CameraComponent>();
                ICamera *gameCamera = &cc.camera;
                {
                    IGN_PROFILE_SCOPE("SceneRenderer::RenderGameplayTo");
                    m_SceneRenderer->Render(gameCamera, false); // disable draw debug
                }
            }
        }

        if (m_State.takeScreenshot)
        {
            auto target = m_SceneRenderer->GetRenderTarget(editCamera);
            if (target)
            {
                auto sceneTexture = target->compositeRT->GetColorAttachment(0)->GetHandle();
                nvrhi::TextureDesc stagingDesc = sceneTexture->getDesc();
                stagingDesc.initialState = nvrhi::ResourceStates::CopyDest;
                auto screenShotStagingTexture = m_Device->createStagingTexture(stagingDesc, nvrhi::CpuAccessMode::Read);

                nvrhi::CommandListHandle cmd = m_Device->createCommandList();
                cmd->open();
                cmd->copyTexture(screenShotStagingTexture, nvrhi::TextureSlice(), sceneTexture, nvrhi::TextureSlice());
                cmd->close();

                m_Device->executeCommandList(cmd);

                if (!screenShotStagingTexture)
                {
                    m_State.takeScreenshot = false;
                }
                else
                {
                    // Map and read the pixel data
                    size_t rowPitch = 0;
                    if (void *mappedData = m_Device->mapStagingTexture(screenShotStagingTexture, nvrhi::TextureSlice(), nvrhi::CpuAccessMode::Read, &rowPitch))
                    {
                        static ImageData image;
                        image.width = screenShotStagingTexture->getDesc().width;
                        image.height = screenShotStagingTexture->getDesc().height;

                        const uint32_t packedStride = image.width * 4u;

                        image.pixels.resize(image.height * packedStride);
                        const auto src = static_cast<const uint8_t *>(mappedData);
                        uint8_t *dst = image.pixels.data();

                        for (uint32_t y = 0u; y < image.height; ++y)
                        {
                            memcpy(dst + y * packedStride, src + y * rowPitch, packedStride);
                        }

                        m_Device->unmapStagingTexture(screenShotStagingTexture);

                        const std::tm local_tm = Timestep::GetLocalTime();

                        std::ostringstream oss;
                        oss << std::put_time(&local_tm, "igite_ss %D-%H-%M-%S");
                        std::string filename = oss.str();
                        stringutils::ReplaceWith(filename, "/", "-");

                        SDL_ShowSaveFileDialog(OnScreenshotSaveFileSelected, &image,
                            Application::GetInstance()->GetWindow()->GetWindowHandle(),
                            kScreenshotFileFilters, IM_ARRAYSIZE(kScreenshotFileFilters),
                            filename.c_str());
                    }

                    m_State.takeScreenshot = false;
                }
            }
        }
    }

    void EditorLayer::OnGuiRender()
    {
        IGN_PROFILE_FUNCTION();
        BindSharedImGuiContext();
        constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar
            | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus
            | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

        const ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGui::Begin("##main_dockspace", nullptr, windowFlags);
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        window->DC.LayoutType = ImGuiLayoutType_Horizontal;
        window->DC.NavLayerCurrent = ImGuiNavLayer_Menu;

        // MAIN MENU BAR
        if (ImGui::BeginMenuBar())
        {
            // FILE MENU
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
                    m_State.popupNewProjectModal = true;
                }

                else if (ImGui::MenuItem("Save Project", nullptr, false, m_ActiveProject != nullptr))
                {
                    SaveProject();
                }

                else if (ImGui::MenuItem("Open Project"))
                {
                    OpenProject();
                }

                else if (ImGui::MenuItem("Close Project", nullptr, false, m_ActiveProject != nullptr))
                {
                    CloseCurrentProject();
                }

                ImGui::EndMenu();
            }

            // VIEW MENU
            if (ImGui::BeginMenu("View"))
            {
                const bool canAddContentBrowser = (GetOpenContentBrowserCount() + m_PendingContentBrowserPanelsToAdd) < 4;
                if (ImGui::MenuItem("Add Content Browser", nullptr, false, canAddContentBrowser))
                {
                    ++m_PendingContentBrowserPanelsToAdd;
                }

                if (ImGui::MenuItem("ImGui Demo", nullptr, false, m_ActiveProject != nullptr))
                {
                    m_State.imguiDemoWindow = true;
                }

                if (ImGui::MenuItem("Game View", nullptr, false, m_ActiveProject != nullptr))
                {
                    m_State.gameplayViewportWindow = true;
                }

                if (ImGui::MenuItem("Settings", nullptr, false, m_ActiveProject != nullptr))
                {
                    m_State.settingsWindow = true;
                }

                if (ImGui::MenuItem("Asset Registry", nullptr, false, m_ActiveProject != nullptr))
                {
                    m_State.assetRegistryWindow = true;
                }

                if (ImGui::MenuItem("Screenshot", nullptr, false, m_ActiveProject != nullptr))
                {
                    m_State.takeScreenshot = true;
                }

                ImGui::EndMenu();
            }

            // BUILD MENU
            if (ImGui::BeginMenu("Script", m_ActiveProject != nullptr))
            {
                if (ImGui::MenuItem("Build Solution"))
                {
                    m_ActiveProject->BuildSolution(true);
                }

                ProjectConfiguration currentConfig = m_ActiveProject->GetConfiguration();
                static const char *configNames[] = { "Debug", "Release", "Shipping" };
                const char *currentConfigName = configNames[static_cast<int>(currentConfig)];

                if (ImGui::BeginMenu("Active Configuration"))
                {
                    for (size_t i = 0; i < std::size(configNames); ++i)
                    {
                        if (ImGui::MenuItem(configNames[i], nullptr, nullptr, i != (size_t)m_ActiveProject->GetConfiguration()))
                        {
                            m_ActiveProject->GetInfo().configuration = static_cast<ProjectConfiguration>(i);
                            AssetWorker::SubmitJob([this]()
                            {
                                SaveProject();
                                m_ActiveProject->BuildSolution(true);
                                m_ActiveProject->GetScriptEngine()->ReloadAssembly();
                            });
                        }
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            
            }

            ImGui::EndMenuBar();
        }

        if (m_State.popupNewProjectModal)
        {
            ImGui::OpenPopup("New Project");
            m_State.popupNewProjectModal = false;
        }

        // Reserve space for status bar
        constexpr float statusBarHeight = 32.0f;
        
        {
            // RENDER TOOL BAR
            ImGui::BeginChild("##toolbar_child", {0.0f, statusBarHeight }, 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar);
            m_ScenePanel->RenderToolbar();
            ImGui::EndChild();
        }
        
        UIProjectCreation();

        ImVec2 avail = ImGui::GetContentRegionAvail();

        // Dockspace tabs area gets everything except status bar
        ImVec2 dockSize = { avail.x, std::max(0.0f, avail.y - statusBarHeight) };
        ImGui::BeginChild("##dockspace_tabs_child", dockSize, 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);
        {
            struct DockWorkspace
            {
                std::string name;
                bool open = true;
                uint32_t dockspaceUid = 0;
            };

            static uint32_t s_NextDockspaceUid = 1;
            static std::vector<DockWorkspace> s_DockTabs = { { "Workspace 1", true, s_NextDockspaceUid++ } };
            static int s_ActiveDockTab = 0;
            static int s_PendingSelectDockTab = 0;

            if (ImGui::BeginTabBar("##dockspace_tabs", ImGuiTabBarFlags_AutoSelectNewTabs))
            {
                for (int i = 0; i < static_cast<int>(s_DockTabs.size()); ++i)
                {
                    ImGuiTabItemFlags tabFlags = (s_PendingSelectDockTab == i) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;

                    bool tabOpen = s_DockTabs[i].open;
                    bool *tabOpenPtr = s_DockTabs.size() > 1 ? &tabOpen : nullptr;
                    if (ImGui::BeginTabItem(s_DockTabs[i].name.c_str(), tabOpenPtr, tabFlags))
                    {
                        s_ActiveDockTab = i;
                        ImGui::EndTabItem();
                    }

                    if (tabOpenPtr)
                    {
                        s_DockTabs[i].open = tabOpen;
                    }
                }

                if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing))
                {
                    const int newTabIndex = static_cast<int>(s_DockTabs.size()) + 1;
                    s_DockTabs.push_back({ "Workspace " + std::to_string(newTabIndex), true, s_NextDockspaceUid++ });
                    s_ActiveDockTab = static_cast<int>(s_DockTabs.size()) - 1;
                    s_PendingSelectDockTab = s_ActiveDockTab;
                }
                else
                {
                    s_PendingSelectDockTab = -1;
                }

                for (int i = static_cast<int>(s_DockTabs.size()) - 1; i >= 0; --i)
                {
                    if (!s_DockTabs[i].open)
                    {
                        s_DockTabs.erase(s_DockTabs.begin() + i);
                        if (s_ActiveDockTab >= i)
                        {
                            s_ActiveDockTab = std::max(0, s_ActiveDockTab - 1);
                        }
                    }
                }

                if (s_DockTabs.empty())
                {
                    s_DockTabs.push_back({ "Workspace 1", true, s_NextDockspaceUid++ });
                    s_ActiveDockTab = 0;
                    s_PendingSelectDockTab = 0;
                }

                s_ActiveDockTab = std::clamp(s_ActiveDockTab, 0, static_cast<int>(s_DockTabs.size()) - 1);

                for (int i = 0; i < static_cast<int>(s_DockTabs.size()); ++i)
                {
                    if (i == s_ActiveDockTab)
                        continue;

                    const std::string dockspaceId = "main_dockspace_" + std::to_string(s_DockTabs[i].dockspaceUid);
                    ImGui::DockSpace(ImGui::GetID(dockspaceId.c_str()), ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_KeepAliveOnly);
                }

                const std::string activeDockspaceId = "main_dockspace_" + std::to_string(s_DockTabs[s_ActiveDockTab].dockspaceUid);
                m_ActiveEditorDockspaceId = ImGui::GetID(activeDockspaceId.c_str());
                ImGui::DockSpace(m_ActiveEditorDockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

                ImGui::EndTabBar();
            }
        }
        ImGui::EndChild();

        const auto bottomStatusBarAvail = ImGui::GetContentRegionMax();
        // Status bar at bottom
        ImGui::BeginChild("##status_bar", { 0.0f, bottomStatusBarAvail.y }, 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar);
        
        ImGui::BeginDisabled((GetOpenContentBrowserCount() + m_PendingContentBrowserPanelsToAdd) >= 4);
        if (ImGui::Button("+ Content Browser"))
        {
            ++m_PendingContentBrowserPanelsToAdd;
        }
        ImGui::EndDisabled();
        
        if (m_LoadingProgress > 0.0f && m_LoadingProgress < 1.0f)
        {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(120.0f);
            ImGui::ProgressBar(m_LoadingProgress, ImVec2(0.0f, 0.0f), "");
        }

        ImGui::SameLine();
        ImGui::TextUnformatted(m_StatusText.c_str());

        const std::string versionStr = std::format("Version: {}", Application::GetVersionString());
        float versionWidth = ImGui::CalcTextSize(versionStr.c_str()).x;
        ImGui::SameLine(ImGui::GetWindowWidth() - versionWidth - 10.0f);
        ImGui::TextUnformatted(versionStr.c_str());

        ImGui::EndChild();

        ImGui::End();

        // ImGui Demo
        if (m_State.imguiDemoWindow)
        {
            ImGui::ShowDemoWindow(&m_State.imguiDemoWindow);
        }

        // Console window
        if (m_State.consoleWindow)
        {
            if (ImGui::Begin("Console", &m_State.consoleWindow))
            {
                if (ImGui::Button("Clear"))
                {
                    Logger::ClearLogs();
                }

                ImGui::Separator();

                if (ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar))
                {
                    const auto& logs = Logger::GetLogs();
                    for (const auto& log : logs)
                    {
                        ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                        switch (log.level)
                        {
                            case spdlog::level::trace: color = ImVec4(0.8f, 0.8f, 0.8f, 1.0f); break;
                            case spdlog::level::debug: color = ImVec4(0.2f, 0.8f, 0.8f, 1.0f); break;
                            case spdlog::level::info:  color = ImVec4(0.2f, 0.8f, 0.2f, 1.0f); break;
                            case spdlog::level::warn:  color = ImVec4(0.8f, 0.8f, 0.2f, 1.0f); break;
                            case spdlog::level::err:   color = ImVec4(0.8f, 0.2f, 0.2f, 1.0f); break;
                            case spdlog::level::critical: color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); break;
                            default: break;
                        }

                        ImGui::PushStyleColor(ImGuiCol_Text, color);
                        ImGui::TextWrapped("%s", log.message.c_str());
                        ImGui::PopStyleColor();
                    }
                }

                if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                {
                    ImGui::SetScrollHereY(1.0f);
                }

                ImGui::EndChild();
            }
            ImGui::End();
        }

        // Draw UI
        UISceneRenderer();

        UISettings();
    }

    void EditorLayer::SetActiveScene(const Ref<Scene> &scene)
    {
        if (m_ActiveScene == scene)
        {
            return;
        }

        constexpr std::string_view kActiveSceneAssetOwner = "editor.active-scene";
        if (m_ActiveProject)
        {
            std::unordered_set<AssetHandle> referencedHandles;
            if (scene)
            {
                referencedHandles = scene->CollectReferencedAssetHandles();
            }

            m_ActiveProject->GetAssetManager()->ReplaceAssetPins(std::string(kActiveSceneAssetOwner), referencedHandles);
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
        
        if (m_ActiveProject)
        {
            m_ActiveProject->SetActiveScene(scene);
        }

        m_SceneRenderer->SetActiveScene(scene);

        GameUISystem::SetSceneContext(scene.get());
    }

    void EditorLayer::RefreshContentBrowsers()
    {
        if (!m_ActiveProject)
        {
            return;
        }

        for (ContentBrowserPanel *panel : m_ContentBrowserPanels)
        {
            if (panel)
            {
                panel->RefreshFiles();
            }
        }
    }

    void EditorLayer::NewScene()
    {
        if (m_EditorScene)
        {
            m_EditorScene->OnStop();
        }

        OnSceneStop();

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
            m_ActiveProject->GetAssetManager()->UnloadUnusedAssets();
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

    void EditorLayer::SaveScene(const ignite::Path &filepath) const
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

    void EditorLayer::OpenScene(const ignite::Path &filepath)
    {
        AssetHandle openSceneHandle = m_ActiveProject->GetAssetManager()->GetAssetHandle(filepath);

        if (m_CurrentSceneHandle == openSceneHandle)
            return;

        m_CurrentSceneHandle = openSceneHandle;

        if (m_EditorScene)
        {
            m_EditorScene->OnStop();
        }

        OnSceneStop();

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
                m_ActiveProject->GetAssetManager()->UnloadUnusedAssets();
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
            m_ActiveProject->Serialize(m_CurrentProjectFilepath);
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

    void EditorLayer::CloseCurrentProject()
    {
        if (m_ActiveProject)
        {
            SaveProject();

            OnSceneStop();

            SetActiveScene(nullptr);

            m_EditorScene.reset();
            m_ActiveScene.reset();

            m_ActiveProject->GetAssetManager()->ClearAllLoadedAssets();

            // Reset everything
            m_ActiveProject.reset();
            m_CurrentProjectFilepath.clear();
            m_CurrentSceneFilePath.clear();
            m_CurrentSceneHandle = AssetHandle(0);
        }
    }

    void EditorLayer::OpenProject(const ignite::Path &filepath)
    {
        if (filepath == m_CurrentProjectFilepath)
        {
            LOG_TRACE("Dismiss opening current project {0}", filepath.generic_string());
            return;
        }

        // Clear old project's loaded assets before opening new project
        if (m_ActiveProject)
        {
            CloseCurrentProject();
        }

        if (const Ref<Project> openedProject = Project::Deserialize(filepath))
        {
            // Subscribe Build Solution callback
            m_ProjectReadySignalToken = SignalBus::Subscribe<SuccessResultSignal>([this](const SuccessResultSignal &signal)
                { OnProjectReadySignal(signal); });

            m_ActiveProject = openedProject;
            m_CurrentProjectFilepath = filepath;
            openedProject->InitScriptEngine();
        }
    }

    bool EditorLayer::OnMouseMovedEvent(MouseMovedEvent &event)
    {
        return false;
    }

    void EditorLayer::OnScenePlay()
    {
        if (m_EditorScene)
            m_EditorScene->OnStop();

        OnSceneStop();

        // copy initial components to new scene
        SetActiveScene(SceneManager::Copy(m_EditorScene));
        m_ActiveScene->OnStart(ESceneState::Play);
    }

    void EditorLayer::OnSceneStop()
    {
        if (m_EditorScene)
            m_EditorScene->OnStop();

        m_ActiveScene->OnStop();
        SetActiveScene(m_EditorScene);
    }

    void EditorLayer::OnSceneSimulate()
    {
        if (m_EditorScene)
            m_EditorScene->OnStop();

        OnSceneStop();

        // copy initial components to new scene
        SetActiveScene(SceneManager::Copy(m_EditorScene));
        m_ActiveScene->OnStart(ESceneState::Simulate);
    }

    void EditorLayer::OnSceneSaveFileSelected(void *userData, const char *const *filelist, int filter)
    {
        auto *editor = (EditorLayer *)userData;

        // Check for errors
        if (editor == nullptr || filelist == nullptr)
        {
            const char *error = SDL_GetError();
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

            editor->m_CurrentSceneFilePath = filepath;

            Application::SubmitToMainThread([editor, file = filepath, userData]()
            {
                SignalBus::Emit<FileImportPayload>(
                    FileImportPayload{ 
                        .type = ImportType::Save, 
                        .status = FileStatus::Success, 
                        .metadata = AssetMetaData(file, AssetType::Scene),
                        .userData = userData
                    }
                );
            });
        }
    }

    void EditorLayer::OnSceneOpenFileSelected(void *userData, const char *const *filelist, int filter)
    {
        EditorLayer *editor = (EditorLayer *)userData;

        // Check for errors
        if (editor == nullptr || filelist == nullptr)
        {
            const char *error = SDL_GetError();
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
            
            Application::SubmitToMainThread([editor, file = filepath, userData]()
            {
                SignalBus::Emit<FileImportPayload>(
                    FileImportPayload{ 
                        .type = ImportType::Open,
                        .status = FileStatus::Success, 
                        .metadata = AssetMetaData(file, AssetType::Scene), 
                        .userData = userData 
                    }
                );
            });
        }
    }

    void EditorLayer::OnProjectSaveFileSelected(void *userData, const char *const *filelist, int filter)
    {
        EditorLayer *editor = (EditorLayer *)userData;

        // Check for errors
        if (editor == nullptr || filelist == nullptr)
        {
            const char *error = SDL_GetError();
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

            Application::SubmitToMainThread([editor, file = filepath, userData]()
            {
                SignalBus::Emit<FileImportPayload>(
                    FileImportPayload{ 
                        .type = ImportType::Save, 
                        .status = FileStatus::Success, 
                        .metadata = AssetMetaData(file, AssetType::Project), 
                        .userData = userData
                    }
                );
            });
        }
    }

    void EditorLayer::OnProjectOpenFileSelected(void *userData, const char *const *filelist, int filter)
    {
        auto *editor = (EditorLayer *)userData;

        // Check for errors
        if (filelist == nullptr)
        {
            const char *error = SDL_GetError();
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
            Application::SubmitToMainThread([editor, file = filepath, userData]()
            {
                SignalBus::Emit<FileImportPayload>(
                    FileImportPayload{
                        .type = ImportType::Open, 
                        .status = FileStatus::Success, 
                        .metadata = AssetMetaData(file, AssetType::Project), 
                        .userData = userData
                    }
                );
            });
        }
    }

    void EditorLayer::OnScreenshotSaveFileSelected(void *userData, const char *const *filelist, int filter)
    {
        if (filelist == nullptr || *filelist == nullptr) return;

        std::string filepath = filelist[0];
        if (!filepath.empty())
        {
            if (!filepath.ends_with(".png"))
            {
                filepath += ".png";
            }

            auto image = static_cast<ImageData *>(userData);
            if (!image->pixels.empty())
            {
                const int channels = 4;
                // stride is width * 4 because we packed it
                stbi_write_png(filepath.c_str(), image->width, image->height, channels, image->pixels.data(), image->width * channels);

                image->pixels.clear();
                image->pixels.shrink_to_fit();
            }
        }
    }

    void EditorLayer::OnProjectFolderSelected(void *userData, const char *const *filelist, int filter)
    {
        if (filelist == nullptr || *filelist == nullptr) return;

        std::string filepath = filelist[0];
        if (!filepath.empty())
        {
            auto *editor = static_cast<EditorLayer *>(userData);
            editor->m_State.projectCreateInfo.filepath = ignite::Path(filepath) / editor->m_State.projectCreateInfo.name; // Append project name
        }
    }

    void EditorLayer::OnProjectReadySignal(const SuccessResultSignal &signal)
    {
        if (signal.isSuccess && signal.type == SignalType::Project)
        {
            // One shot signal
            SignalBus::Unsubscribe<SuccessResultSignal>(m_ProjectReadySignalToken);
            m_ProjectReadySignalToken = kInvalidSignalToken;

            // Reload project files
            ReloadContentBrowserPanels();

            // Get Project default scene (use immediate load for synchronous path)
            AssetHandle defSceneAssetHandle = m_ActiveProject->GetInfo().defaultSceneHandle;
            if (defSceneAssetHandle != AssetHandle(0))
            {
                // Use GetAssetImmediate since we're on main thread and need synchronous load
                if (Ref<Scene> activeScene = m_ActiveProject->GetAssetImmediate<Scene>(defSceneAssetHandle))
                {
                    m_EditorScene = SceneManager::Copy(activeScene);
                    m_EditorScene->SetDirtyFlag(false);
                    SetActiveScene(m_EditorScene);

                    const auto &[assetFilepath, assetType] = m_ActiveProject->GetAssetManager()->GetMetaData(activeScene->handle);

                    m_CurrentSceneFilePath = m_ActiveProject->GetProjectFilepath(assetFilepath);
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
        }
    }

    void EditorLayer::OnFileImport(const FileImportPayload &payload)
    {
        switch (payload.type)
        {
        case ImportType::Open:
        {
            if (payload.metadata.type == AssetType::Scene)
            {
                // Submit scene loading to asset worker
                ignite::Path filepath = payload.metadata.filepath;
                AssetHandle sceneHandle = m_ActiveProject->GetAssetManager()->GetAssetHandle(filepath);

                if (m_CurrentSceneHandle == sceneHandle)
                    break;

                m_CurrentSceneHandle = sceneHandle;

                // Submit heavy I/O work to asset worker
                AssetWorker::SubmitJob([this, filepath, sceneHandle]()
                    {
                        AssetWorker::ReportStatus(std::format("Loading scene {}...", filepath.filename().string()), 0.5f);

                        // Load scene on worker thread (I/O happens here)
                        Ref<Scene> loadedScene = SceneSerializer::Deserialize(filepath, m_ActiveProject.get());
                        if (loadedScene)
                        {
                            // Submit UI update back to main thread
                            Application::SubmitToMainThread([this, loadedScene, filepath]() mutable
                                {
                                    AssetWorker::ReportStatus("Finalizing scene load...", 0.9f);

                                    if (m_EditorScene)
                                    {
                                        m_EditorScene->OnStop();
                                    }

                                    OnSceneStop();

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
                                        m_ActiveProject->GetAssetManager()->UnloadUnusedAssets();
                                    }

                                    // Copy and activate new scene
                                    m_EditorScene = SceneManager::Copy(loadedScene);
                                    m_EditorScene->SetDirtyFlag(false);
                                    SetActiveScene(m_EditorScene);
                                    m_CurrentSceneFilePath = filepath;

                                    AssetWorker::ReportStatus("Ready", 0.0f);
                                });
                        }
                        else
                        {
                            LOG_ERROR("[Editor] Failed to load scene: {}", filepath.generic_string());
                        }
                    });
            }
            else if (payload.metadata.type == AssetType::Project)
            {
                ignite::Path filepath = payload.metadata.filepath;

                if (filepath == m_CurrentProjectFilepath)
                {
                    LOG_TRACE("Dismiss opening current project {0}", filepath.generic_string());
                    break;
                }

                OpenProject(filepath);
            }
            break;
        }
        case ImportType::Save:
        {
            if (payload.metadata.type == AssetType::Scene)
            {
                // Submit scene save to asset worker
                ignite::Path filepath = payload.metadata.filepath;

                // AssetWorker::SubmitJob([this, filepath]()
                Application::SubmitToMainThread([this, filepath]()
                    {
                        SceneSerializer serializer(m_ActiveScene, m_ActiveProject.get());
                        serializer.Serialize(filepath);

                        LOG_INFO("[Editor] Scene saved: {}", filepath.generic_string());

                        RefreshContentBrowsers();
                    });
            }
            else if (payload.metadata.type == AssetType::Project)
            {
                SaveProjectAs();
            }
            break;
        }
        }
    }

    void EditorLayer::UIProjectCreation()
    {
        ImGui::SetNextWindowSizeConstraints({ 640.0f, 320.0f }, { 640.0f, 320.0f });
        if (!ImGui::BeginPopupModal("New Project", nullptr, ImGuiWindowFlags_NoResize))
        {
            return;
        }

        ImGui::Text("Create a brand new project");
        ImGui::Separator();

        static char nameBuffer[128] = {};

        // Use a simple two-column table for labels and controls
        if (ImGui::BeginTable("proj_create_table", 2, ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Project Name");
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##ProjectName", nameBuffer, sizeof(nameBuffer)))
            {
                m_State.projectCreateInfo.name = std::string(nameBuffer);
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Location");
            ImGui::TableNextColumn();

            // Path input with Browse button
            ImGui::PushItemWidth(-120);
            std::string pathStr = m_State.projectCreateInfo.filepath.generic_string();
            char pathBuf[1024] = {};
            if (!pathStr.empty()) strncpy(pathBuf, pathStr.c_str(), sizeof(pathBuf) - 1);
            if (ImGui::InputText("##ProjectLocation", pathBuf, sizeof(pathBuf)))
            {
                m_State.projectCreateInfo.filepath = ignite::Path(pathBuf);
            }
            ImGui::PopItemWidth();
            ImGui::SameLine();
            if (ImGui::Button("Browse"))
            {
                std::string filepath = FileDialogs::SelectFolder();
                if (!filepath.empty())
                {
                    m_State.projectCreateInfo.filepath = ignite::Path(filepath) / m_State.projectCreateInfo.name;
                }
            }

            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::TextWrapped("Note: A sample `Game.cs` will only be created when no scripts exist for the project. Existing user scripts will not be overwritten.");

        ImGui::Spacing();

        // Actions
        const bool isProjectPathValid = !m_State.projectCreateInfo.filepath.empty() && !m_State.projectCreateInfo.name.empty();

        ImGui::Separator();

        ImGui::Spacing();

        ImGui::BeginDisabled(!isProjectPathValid);
        ImGui::SameLine(ImGui::GetWindowWidth() - 220);
        if (ImGui::Button("Create", ImVec2(100, 0)))
        {
            AssetWorker::SubmitJob([this]()
            {
                // sanitize name
                while (m_State.projectCreateInfo.name.find(' ') != std::string::npos)
                {
                    const size_t spacePos = m_State.projectCreateInfo.name.find(' ');
                    m_State.projectCreateInfo.name.replace(spacePos, 1, "");
                }

                m_State.projectCreateInfo.filepath /= (m_State.projectCreateInfo.name + ".ixproj");
                m_State.projectCreateInfo.rootDirectory = m_State.projectCreateInfo.filepath.parent_path();

                if (Ref<Project> newProject = Project::Create(m_State.projectCreateInfo))
                {
                    // Subscribe Build Solution callback
                    m_ProjectReadySignalToken = SignalBus::Subscribe<SuccessResultSignal>([this](const SuccessResultSignal &signal)
                        { OnProjectReadySignal(signal); });

                    // Submit to main thread to sync
                    Application::SubmitToMainThread([this, newProject]()
                    {
                        m_ActiveProject = newProject;
                        m_ActiveProject->InitScriptEngine();

                        // Serialize
                        m_ActiveProject->Serialize(m_State.projectCreateInfo.filepath);

                        // clear modal inputs
                        m_State.projectCreateInfo.filepath.clear();
                        m_State.projectCreateInfo.name.clear();
                        memset(nameBuffer, 0, sizeof(nameBuffer));
                    });
                }
            });

            ImGui::CloseCurrentPopup();
        }

        ImGui::EndDisabled();

        ImGui::SameLine(ImGui::GetWindowWidth() - 110);
        if (ImGui::Button("Cancel", ImVec2(100, 0)))
        {
            m_State.projectCreateInfo.filepath.clear();
            m_State.projectCreateInfo.name.clear();
            memset(nameBuffer, 0, sizeof(nameBuffer));
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    void EditorLayer::UISettings()
    {
        if (m_ActiveScene && m_State.settingsWindow)
        {
            ImGui::Begin("Settings", &m_State.settingsWindow);

            if (ImGui::BeginTabBar("##settings_tabs", ImGuiTabBarFlags_Reorderable))
            {
                if (ImGui::BeginTabItem("Pipeline"))
                {
                    // Raster settings
                    static std::array<const char *, 2>rasterFillStr = { "Solid", "Wireframe" };
                    const char *currentFillMode = rasterFillStr[static_cast<i32>(m_State.rasterFillMode)];
                    if (ImGui::BeginCombo("Fill", currentFillMode))
                    {
                        for (size_t i = 0; i < std::size(rasterFillStr); ++i)
                        {
                            bool isSelected = strcmp(currentFillMode, rasterFillStr[i]) == 0;
                            if (ImGui::Selectable(rasterFillStr[i], isSelected))
                            {
                                m_State.rasterFillMode = static_cast<nvrhi::RasterFillMode>(i);
                                m_SceneRenderer->SetFillMode(m_State.rasterFillMode);
                            }

                            if (isSelected)
                            {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::EndCombo();
                    }
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }

            ImGui::End(); // !settings window
        }


        // ----------------------------------------
        // ASSET REGISTRY & MEMORY MONITOR
        // ----------------------------------------
        if (m_ActiveScene && m_State.assetRegistryWindow)
        {
            auto assetManager = AssetManager::GetInstance();
            AssetRegistry assetRegistry = assetManager->GetAssetAssetRegistry();
            const auto &loadedAssets = assetManager->GetLoadedAssets();

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

            ImGui::SetNextWindowSize(ImVec2(940, 512), ImGuiCond_FirstUseEver);
            ImGui::Begin("Asset Registry & Memory Monitor", &m_State.assetRegistryWindow);

            // === Asset Statistics & Memory Usage ===
            std::unordered_map<AssetType, int> registeredCounts;
            std::unordered_map<AssetType, int> loadedCounts;
            std::unordered_map<AssetType, size_t> memoryUsage;

            // Display table
            constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_Sortable | ImGuiTableFlags_RowBg 
                | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable;
            
            if (ImGui::BeginTable("asset_statistics_memory_usage", 2, ImGuiTableFlags_SizingStretchProp | tableFlags))
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();

                for (const auto &[handle, metadata] : assetRegistry)
                    registeredCounts[metadata.type]++;

                for (const auto &[handle, asset] : loadedAssets)
                {
                    if (asset)
                    {
                        AssetType type = assetManager->GetAssetType(handle);
                        loadedCounts[type]++;
                        // Use on-disk file size as a reasonable memory estimate
                        const AssetMetaData &meta = assetManager->GetMetaData(handle);
                        memoryUsage[type] += assetManager->GetAssetFileSize(meta);
                    }
                }

                // Left column - Asset counts
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

                ImGui::TableNextColumn(); // ------------ NEXT COLUMN ------------

                // Right column - Loaded assets & memory
                int totalLoaded = 0;
                for (const auto &[type, count] : loadedCounts)
                {
                    if (type != AssetType::Invalid)
                    {
                        float percentage = registeredCounts[type] > 0 ? (float)count / registeredCounts[type] * 100.0f : 0.0f;

                        // Color code by type
                        ImVec4 color = ImVec4(0.5f, 0.8f, 0.5f, 1.0f);
                        if (type == AssetType::Texture) color = ImVec4(0.9f, 0.5f, 0.5f, 1.0f);
                        else if (type == AssetType::Mesh) color = ImVec4(0.5f, 0.5f, 0.9f, 1.0f);
                        else if (type == AssetType::Material) color = ImVec4(0.9f, 0.9f, 0.5f, 1.0f);

                        ImGui::TextColored(color, "%s: %d (%.1f%%)", AssetTypeToString(type).c_str(), count, percentage);

                        if (memoryUsage[type] > 0)
                        {
                            const size_t bytes = memoryUsage[type];
                            ImGui::SameLine();
                            if (bytes >= 1024u * 1024u)
                                ImGui::Text("~%.1f MB", bytes / (1024.0f * 1024.0f));
                            else
                                ImGui::Text("~%zu KB", bytes / 1024u);
                        }

                        totalLoaded += count;
                    }
                }

                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Total Loaded: %d", totalLoaded);

                // Memory usage bar
                const float loadRatio = totalRegistered > 0 ? (float)totalLoaded / totalRegistered : 0.0f;
                const auto memoryLoadText = std::format("Memory Load: {} %", (int)(loadRatio * 100));
                const float textWidth = ImGui::CalcTextSize(memoryLoadText.c_str()).x + 16.0f;
                ImGui::ProgressBar(loadRatio, ImVec2(textWidth, 0.0f), memoryLoadText.c_str());
                
                ImGui::EndTable();
            }

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
                const char *typeNames[] = { 
                    "All", 
                    "Scene", 
                    "Texture", 
                    "Material", 
                    "StaticMesh", 
                    "SkeletalMesh", 
                    "Audio", 
                    "Skeleton"
                };

                const AssetType typeValues[] = {
                    AssetType::Invalid, 
                    AssetType::Scene, 
                    AssetType::Texture,
                    AssetType::Material, 
                    AssetType::Mesh, 
                    AssetType::SkeletalMesh, 
                    AssetType::Audio, 
                    AssetType::Skeleton
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

                ImGui::TableNextColumn(); // ------------ NEXT COLUMN ------------

                ImGui::BeginGroup();
                if (ImGui::SmallButton("Refresh Registry"))
                {
                    m_ActiveProject->ValidateAssetRegistry();
                }
                if (ImGui::SmallButton("Unload Unused Assets"))
                {
                    assetManager->UnloadUnusedAssets();
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
                        std::filesystem::absolute(m_ActiveProject->GetProjectFilepath(metadata.filepath).string()).generic_string());

                    if (handleStr.find(findKey) == std::string::npos &&
                        typeStr.find(findKey) == std::string::npos &&
                        filepathStr.find(findKey) == std::string::npos)
                    {
                        continue;
                    }
                }

                filteredAssets.insert({ handle, metadata });
            }

            if (ImGui::BeginTable("asset_registry_table", 5, tableFlags))
            {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("Handle", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed, 120.0f, 0);
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
                        else if (metadata.type == AssetType::Mesh) typeColor = ImVec4(0.5f, 0.5f, 0.9f, 1.0f);
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
                            displayPath = std::filesystem::absolute(m_ActiveProject->GetProjectFilepath(metadata.filepath).string()).generic_string();
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
                        else if (metadata.type == AssetType::Mesh) typeColor = ImVec4(0.5f, 0.5f, 0.9f, 1.0f);
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
                            displayPath = std::filesystem::absolute(m_ActiveProject->GetProjectFilepath(metadata.filepath).string()).generic_string();
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

            ImGui::End();
        }
    }

    void EditorLayer::UISceneRenderer()
    {
        ImGui::Begin("Scene Renderer");

        // Shaders
        if (ImGui::TreeNodeEx("Shaders", ImGuiTreeNodeFlags_Framed))
        {
            for (auto &[key, shader] : Shader::GetShaderCache())
            {
                ImGui::Button(shader->GetName().c_str(), { -1.0f, 0.0f });

            }
            ImGui::TreePop();
        }

        // Render Stats
        if (ImGui::TreeNodeEx("Statistics", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen))
        {
            const auto &stats = Renderer::Stats;

            // Helper lambda to format bytes as KB or MB
            auto fmtBytes = [](size_t bytes, char *buf, size_t bufLen)
            {
                if (bytes >= 1024u * 1024u)
                    snprintf(buf, bufLen, "%.1f MB", bytes / (1024.0f * 1024.0f));
                else
                    snprintf(buf, bufLen, "%zu KB", bytes / 1024u);
            };
            char memBuf[32];

            ImGui::SeparatorText("3D Renderer");
            ImGui::Text("Draw Calls (opaque+transparent): %zu", stats.drawCallCount);
            ImGui::Text("Shadow Draw Calls (CSM) %zu", stats.shadowDrawCallCount);
            ImGui::Text("Static Meshes Drawn: %zu", stats.staticMeshCount);
            ImGui::Text("Skeletal Meshes Drawn: %zu", stats.skeletalMeshCount);
            ImGui::Text("Indices Submitted: %zu", stats.indexCount3D);

            ImGui::Spacing();
            ImGui::SeparatorText("2D Renderer");
            ImGui::Text("Quads: %zu", stats.quadCount);
            ImGui::Text("Lines: %zu", stats.lineCount);
            ImGui::Text("Circles: %zu", stats.circleCount);
            ImGui::Text("Text Glyphs: %zu", stats.textCount);
            ImGui::Text("Point Lights 2D: %zu", stats.pointLight2dCount);

            ImGui::Spacing();
            ImGui::SeparatorText("GPU Buffer Memory");
            fmtBytes(stats.gpuVertexBufferBytes, memBuf, sizeof(memBuf));
            ImGui::Text("Vertex Buffers: %s", memBuf);
            fmtBytes(stats.gpuIndexBufferBytes, memBuf, sizeof(memBuf));
            ImGui::Text("Index Buffers: %s", memBuf);
            fmtBytes(stats.gpuConstantBufferBytes, memBuf, sizeof(memBuf));
            ImGui::Text("Constant Buffers: %s", memBuf);
            const size_t totalGpu = stats.gpuVertexBufferBytes + stats.gpuIndexBufferBytes + stats.gpuConstantBufferBytes;
            fmtBytes(totalGpu, memBuf, sizeof(memBuf));
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "Total GPU Buffers: %s", memBuf);

            ImGui::Spacing();
            ImGui::SeparatorText("2D Batch Buffer Sizes");
            fmtBytes(stats.quadVerticesSize + stats.quadIndicesSize, memBuf, sizeof(memBuf));
            ImGui::Text("Quad VB+IB: %s", memBuf);
            fmtBytes(stats.lineVerticesSize, memBuf, sizeof(memBuf));
            ImGui::Text("Line VB: %s", memBuf);
            fmtBytes(stats.circleVerticesSize + stats.circleIndicesSize, memBuf, sizeof(memBuf));
            ImGui::Text("Circle VB+IB:%s", memBuf);
            fmtBytes(stats.textVerticesSize + stats.textIndicesSize, memBuf, sizeof(memBuf));
            ImGui::Text("Text VB+IB: %s", memBuf);

            ImGui::TreePop();
        }

        // Scene Render
        if (ImGui::TreeNodeEx("Settings Render", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::SeparatorText("Visibility");
            UI::DrawCheckbox("Bounding Box", &m_SceneRenderer->sceneRenderSettings.showBoundingBox);
            UI::DrawCheckbox("Physics Collider", &m_SceneRenderer->sceneRenderSettings.showPhysicsCollider);

            ImGui::SeparatorText("Scene Render");
            static const char *renderModeLabels[] = { "Color", "Diffuse", "Normals", "Metallic", "Roughness" };
            static const char *debugShadowLabels[] = { "Off", "Cascade Colors", "Shadow Term" };
            static const char *tonemapLabels[] = { "Reinhard", "Uncharted2", "Filmic" };

            int renderMode = m_SceneRenderer->GetRenderMode();
            if (UI::DrawComboBox("Render Mode", renderModeLabels, IM_ARRAYSIZE(renderModeLabels), &renderMode))
            {
                m_SceneRenderer->SetRenderMode(renderMode);
            }

            int debugShadow = m_SceneRenderer->GetDebugShadowMode();
            if (UI::DrawComboBox("CSM Debug", debugShadowLabels, IM_ARRAYSIZE(debugShadowLabels), &debugShadow))
            {
                m_SceneRenderer->SetDebugShadowMode(debugShadow);
            }
            
            ImGui::TreePop();
        }

        ImGui::End();
    }

}
