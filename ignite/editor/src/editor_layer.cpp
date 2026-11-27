// Copyright (c) 2026 Evangelion Manuhutu | IGNITE STUDIO

#include "pch.hpp"
#include "editor_layer.hpp"
#include "ignite/graphics/gi/gi_baker.hpp"
#include <ignite/net/network_manager.hpp>
#include "ext/editor_ui.hpp"
#include "panels/scene_panel.hpp"
#include "panels/content_browser_panel.hpp"
#include "panels/asset_importer_panel.hpp"
#include "panels/asset_editor_panel.hpp"
#include "ignite/asset/asset_worker.hpp"
#include "ignite/asset/asset.hpp"
#include "ignite/asset/asset_importer.hpp"
#include "ignite/core/agent_controller.hpp"
#include "ignite/core/command.hpp"
#include "ignite/core/input/input_system.hpp"
#include "ignite/core/platform_utils.hpp"
#include "ignite/core/time.hpp"
#include "ignite/core/profiler/profiler.hpp"
#include "ignite/project/game_packager.hpp"
#include "ignite/scene/prefab.hpp"
#include "ignite/scene/scene_manager.hpp"
#include "ignite/scripting/script_engine.hpp"
#include "ignite/imgui/imgui_layer.hpp"
#include "ignite/imgui/imgui_nri.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/shader.hpp"
#include "ignite/graphics/thumbnail_generator.hpp"
#include "ignite/graphics/ui/game_ui_system.hpp"
#include "ignite/graphics/window.hpp"
#include "ignite/globals/globals.hpp"
#include "ignite/serializer/scene_serializer.hpp"
#include "editor_context.hpp"

#include "stb_image_write.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <string_view>
#include <unordered_set>

#include <SDL3/SDL_dialog.h>
#include <spdlog/spdlog.h>

namespace ignite
{
    namespace
    {
        bool BindSharedImGuiContext()
        {
            if (auto *app = Application::GetInstance(); app && !app->GetCreateInfo().useGui)
            {
                return false;
            }

            ImGuiContext *sharedContext = Application::GetImGuiContext();
            LOG_ASSERT(sharedContext, "Engine ImGui context has not been created");

            if (sharedContext && ImGui::GetCurrentContext() != sharedContext)
            {
                ImGui::SetCurrentContext(sharedContext);
            }
            return sharedContext != nullptr;
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

    EditorLayer *EditorLayer::s_Instance = nullptr;

    EditorLayer::EditorLayer(const std::string &name)
        : Layer(name), m_StatusText("Ready")
    {
        s_Instance = this;
        EditorContext::Init();
    }

    EditorLayer::~EditorLayer()
    {
        EditorContext::Shutdown();
    }

    void EditorLayer::OnAttach()
    {
        Layer::OnAttach();
        BindSharedImGuiContext();

        auto *app = Application::GetInstance();

        m_FileImportSignalToken = SignalBus::Subscribe<FileImportPayload>(
            [this](const FileImportPayload &payload)
            {
                OnFileImport(payload);
            }
        );

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
                {
                    SetLoadingProgress(progress);
                }
            });
        });

        // create render target framebuffer
        m_SceneRenderer = CreateRef<SceneRenderer>();
        m_GIBaker = CreateScope<GIBaker>();

        const auto &cmdArgs = Application::GetInstance()->GetCreateInfo().cmdLineArgs;
        AgentController::Get().InitializeFromArgs(cmdArgs);
        for (int i = 1; i < cmdArgs.count; ++i)
        {
            const std::string_view arg(cmdArgs[i]);
            constexpr std::string_view kProjectPrefix = "--project=";
            if (arg.starts_with(kProjectPrefix))
            {
                OpenProject(std::string(arg.substr(kProjectPrefix.size())));
            }
            else if (arg == "--project" && i + 1 < cmdArgs.count)
            {
                OpenProject(cmdArgs[++i]);
            }
        }

        SceneManager::SetStopCallback([this]() { OnSceneStop(); });

        if (Application::GetInstance()->GetCreateInfo().useGui && Application::GetInstance()->GetWindow())
        {
            Application::GetInstance()->GetWindow()->Show();
        }
    }

    void EditorLayer::OnDetach()
    {
        Layer::OnDetach();
        AssetWorker::SetStatusCallback(nullptr);
        SceneManager::SetStopCallback(nullptr);

        if (s_Instance == this)
        {
            s_Instance = nullptr;
        }

        // Close project
        CloseProject();
        ContentBrowserPanel::ReleaseSharedResources();

        m_ContentBrowserPanels.clear();
        m_ContentBrowserPanelsPendingRemoval.clear();
        m_ContentBrowserPanel = nullptr;

        m_SceneRenderer.reset();
        m_GIBaker.reset();

        // Unsubscribe signals
        SignalBus::Unsubscribe<SuccessResultSignal>(m_FileImportSignalToken);
        m_FileImportSignalToken = kInvalidSignalToken;
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
            panel->LoadProjectFiles();
        }
    }

    void EditorLayer::ReloadContentBrowserPanels() const
    {
        if (!m_ActiveProject)
        {
            return;
        }

        for (ContentBrowserPanel *panel : m_ContentBrowserPanels)
        {
            if (panel)
            {
                panel->LoadProjectFiles();
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

    void EditorLayer::RevealAssetInContentBrowser(AssetHandle handle)
    {
        if (handle == AssetHandle(0))
        {
            return;
        }

        for (ContentBrowserPanel *panel : m_ContentBrowserPanels)
        {
            if (panel && panel->IsOpen())
            {
                panel->RevealAsset(handle);
                return;
            }
        }

        if (m_ContentBrowserPanel)
        {
            m_ContentBrowserPanel->RevealAsset(handle);
        }
    }

    void EditorLayer::OnUpdate(const float deltaTime)
    {
        Layer::OnUpdate(deltaTime);

        // using int type, to prevent unsigned values
        for (int i = static_cast<int>(m_ContentBrowserPanels.size()) - 1; i >= 0; --i)
        {
            ContentBrowserPanel *panel = m_ContentBrowserPanels[static_cast<size_t>(i)];
            if (panel == nullptr)
            {
                m_ContentBrowserPanels.erase(m_ContentBrowserPanels.begin() + i);
                continue;
            }

            // Remove closed content browser
            if (!panel->IsOpen())
            {
                if (m_ContentBrowserPanelsPendingRemoval.insert(panel).second)
                {
                if (auto it = std::ranges::find(m_ContentBrowserPanels, panel);
                    it != m_ContentBrowserPanels.end())
                    {
                        // Erase then pop the layer
                        it = m_ContentBrowserPanels.erase(it);
                        Application::GetInstance()->PopLayer(panel);
                    }
                    m_ContentBrowserPanelsPendingRemoval.erase(panel);
                }
            }
        }

        {
            // Reopen the content browser if we accidentally close the last one
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
                AddContentBrowserPanel();
                --m_PendingContentBrowserPanelsToAdd;
            }
        }

        AssetManager::GetInstance()->OnUpdate(deltaTime);

        SyncActiveSceneFromProject();

        const bool isImporting = m_AssetImporterPanel && m_AssetImporterPanel->IsImporting();
        if (!isImporting)
        {
            if (m_State.isLoading)
            {
                const float workerProgress = AssetWorker::GetProgress();
                SetLoadingProgress(workerProgress > 0.0f ? workerProgress : 0.15f);
                if (m_StatusText == "Ready")
                {
                    SetStatusText("Loading project...");
                }
                m_LoadingSceneAssets = true;
            }
            else if (m_ActiveScene)
            {
                if (m_LastAssetProgressScene != m_ActiveScene.get())
                {
                    m_LastAssetProgressScene = m_ActiveScene.get();
                    m_CachedAllSceneAssetsReady = false;
                    m_AssetProgressCheckCounter = 0;
                }

                const bool shouldPollProgress = !m_CachedAllSceneAssetsReady
                    || m_LoadingSceneAssets
                    || AssetWorker::IsBusy()
                    || ((++m_AssetProgressCheckCounter % 30u) == 0u);

                if (shouldPollProgress)
                {
                    const SceneAssetLoadProgress assetProgress = m_ActiveScene->GetAssetLoadProgress();
                    m_CachedAllSceneAssetsReady = assetProgress.allReady;
                    if (!assetProgress.allReady)
                    {
                        const float clampedProgress = std::clamp(assetProgress.progress, 0.02f, 0.99f);
                        SetLoadingProgress(clampedProgress);
                        SetStatusText(std::format("Loading scene assets ({}/{})...", assetProgress.readyAssets, assetProgress.totalAssets));
                        m_LoadingSceneAssets = true;
                    }
                    else if (AssetWorker::IsBusy())
                    {
                        const float workerProgress = AssetWorker::GetProgress();
                        if (m_LoadingProgress <= 0.0f || m_LoadingProgress >= 1.0f || m_LoadingSceneAssets)
                        {
                            SetLoadingProgress(workerProgress > 0.0f ? workerProgress : 0.1f);
                        }
                        m_LoadingSceneAssets = false;
                    }
                    else if (m_LoadingSceneAssets || (m_LoadingProgress > 0.0f && m_LoadingProgress < 1.0f))
                    {
                        SetLoadingProgress(0.0f);
                        SetStatusText("Ready");
                        m_LoadingSceneAssets = false;
                    }
                }
            }
            else if (AssetWorker::IsBusy())
            {
                const float workerProgress = AssetWorker::GetProgress();
                if (m_LoadingProgress <= 0.0f || m_LoadingProgress >= 1.0f)
                {
                    SetLoadingProgress(workerProgress > 0.0f ? workerProgress : 0.1f);
                }
            }
            else if (m_LoadingSceneAssets || (m_LoadingProgress > 0.0f && m_LoadingProgress < 1.0f))
            {
                SetLoadingProgress(0.0f);
                SetStatusText("Ready");
                m_LoadingSceneAssets = false;
            }
        }

        // update panels
        if (m_ActiveScene)
        {
            // multi-select entity
            m_State.multiSelect = InputSystem::IsModifierPressed(KeyMod::LeftShift);

            switch (m_ActiveScene->GetState())
            {
                case ESceneState::Simulate:
                case ESceneState::Play:
                {
                    constexpr float kFixedStep = 1.0f / 60.0f;
                    constexpr int kMaxSubsteps = 8;
                    m_FixedAccumulator += std::min(std::max(deltaTime, 0.0f), 0.25f);
                    int substeps = 0;
                    while (m_FixedAccumulator >= kFixedStep && substeps < kMaxSubsteps)
                    {
                        m_ActiveScene->OnFixedUpdateRuntimeSimulate();
                        m_FixedAccumulator -= kFixedStep;
                        ++substeps;
                    }
                    m_ActiveScene->OnUpdateRuntimeSimulate(deltaTime);
                    break;
                }
                case ESceneState::Stop:
                {
                    m_FixedAccumulator = 0.0f;
                    m_ActiveScene->OnFixedUpdateEdit();
                    m_ActiveScene->OnUpdateEdit(deltaTime);
                    break;
                }
                case ESceneState::None:
                default: break;
            }

            m_ScenePanel->OnUpdate(deltaTime);
        }

        if (!m_State.isLoading)
        {
            AgentExecutionContext agentCtx;
            agentCtx.scene = m_ActiveScene.get();
            agentCtx.project = m_ActiveProject.get();
            agentCtx.sceneRenderer = m_SceneRenderer.get();
            agentCtx.giBaker = m_GIBaker.get();
            if (m_ActiveScene && m_ActiveScene->IsPlaying())
            {
                agentCtx.camera = &m_EditorPlayCamera;
            }
            else if (m_ScenePanel)
            {
                agentCtx.camera = &m_ScenePanel->GetViewportCamera();
            }
            agentCtx.onSceneReplaced = [this](const Ref<Scene> &newScene)
            {
                m_EditorScene = newScene;
                SetActiveScene(newScene);
            };
            agentCtx.onPlay = [this]() { OnScenePlay(); };
            agentCtx.onStop = [this]() { OnSceneStop(); };
            agentCtx.onSimulate = [this]() { OnSceneSimulate(); };
            AgentController::Get().Update(deltaTime, agentCtx);

            if (m_GIBaker)
            {
                const bool isBaking = m_GIBaker->IsBaking();
                if (m_WasGIBaking && !isBaking)
                {
                    m_WasGIBaking = false;
                    const bool ok = m_GIBaker->DidFinishSuccessfully();
                    if (ok && m_SceneRenderer)
                    {
                        Ref<IrradianceVolume> uploadedVol = CreateRef<IrradianceVolume>();
                        if (uploadedVol->UploadFromData(m_GIBaker->GetLastBakedData()))
                        {
                            m_SceneRenderer->SetIrradianceVolume(uploadedVol);
                        }
                        else
                        {
                            m_SceneRenderer->ClearIrradianceVolume();
                        }
                    }
                    if (m_BakeReflectionProbesAfterGI)
                    {
                        m_BakeReflectionProbesAfterGI = false;
                        if (ok && m_ActiveScene && m_ActiveProject)
                        {
                            const std::string sceneName = m_CurrentSceneFilePath.empty()
                                ? "Scene" : m_CurrentSceneFilePath.stem().string();
                            const std::filesystem::path outputDir = m_ActiveProject->GetDirectory() / "Baked" / sceneName;
                            m_GIBaker->BakeReflectionProbes(
                                m_ActiveScene.get(),
                                m_ActiveScene->GetAssetManager(),
                                m_SceneRenderer ? m_SceneRenderer->GetIrradianceVolume().get() : nullptr,
                                outputDir);
                            if (m_SceneRenderer)
                            {
                                m_SceneRenderer->ClearReflectionProbeSet();
                            }
                        }
                    }
                }
                else if (isBaking)
                {
                    m_WasGIBaking = true;
                }
            }
        }
    }

    void EditorLayer::OnEvent(Event &e)
    {
        BindSharedImGuiContext();
        Layer::OnEvent(e);

        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<KeyPressedEvent>(BIND_CLASS_EVENT_FN(EditorLayer::OnKeyPressedEvent));
        dispatcher.Dispatch<WindowCloseEvent>(BIND_CLASS_EVENT_FN(EditorLayer::OnWindowCloseEvent));
    }

    bool EditorLayer::OnKeyPressedEvent(KeyPressedEvent &event)
    {
        bool control = InputSystem::IsModifierPressed(KeyMod::Control);
        bool shift = InputSystem::IsModifierPressed(KeyMod::Shift)
            || InputSystem::IsModifierPressed(KeyMod::LeftShift)
            || InputSystem::IsModifierPressed(KeyMod::RightShift);

        if (ImGui::GetIO().WantTextInput)
        {
            return false;
        }

        switch (event.GetKeyCode())
        {
            case Key::F:
            {
                if (Entity entity = m_ScenePanel->GetSelectedEntity())
                {
                    auto &cam = m_ScenePanel->GetViewportCamera();
                    auto &tr = entity.GetComponent<TransformComponent>();

                    glm::vec3 focusCenter = tr.world.translation;
                    glm::vec3 halfExtents = glm::abs(tr.world.scale) * 0.5f;

                    if (entity.HasComponent<StaticMeshComponent>())
                    {
                      if (const auto &smc = entity.GetComponent<StaticMeshComponent>();
                          smc.handle != AssetHandle(0))
                        {
                            if (auto mesh = AssetManager::GetInstance()->GetAsset<StaticMesh>(smc.handle))
                            {
                                const auto &[min, max] = smc.worldAABB;
                                focusCenter = (min + max) * 0.5f;
                                halfExtents = glm::abs(max - min) * 0.5f;
                            }
                        }
                    }
                    else if (entity.HasComponent<SkeletalMeshComponent>())
                    {
                        if (const auto &smc = entity.GetComponent<SkeletalMeshComponent>();
                            smc.handle != AssetHandle(0))
                        {
                            if (auto mesh = AssetManager::GetInstance()->GetAsset<SkeletalMesh>(smc.handle))
                            {
                                const auto &[min, max] = smc.worldAABB;
                                focusCenter = (min + max) * 0.5f;
                                halfExtents = glm::abs(max - min) * 0.5f;
                            }
                        }
                    }
                    else if (entity.HasComponent<TerrainComponent>())
                    {
                        const auto &tc = entity.GetComponent<TerrainComponent>();

                        auto minPos = glm::vec3(std::numeric_limits<float>::max());
                        auto maxPos = glm::vec3(std::numeric_limits<float>::min());

                        for (size_t idx = 0; idx < tc.chunks.size(); ++idx)
                        {
                            auto &chunk = tc.chunks[idx];
                            if (!chunk.primitive || !chunk.primitive->vertexBuffer || !chunk.primitive->indexBuffer)
                            {
                                continue;
                            }

                            auto [min, max] = chunk.bounds.Transform(tr.world.GetMatrix());
                            minPos = glm::min(minPos, min);
                            maxPos = glm::max(maxPos, max);
                        }

                        focusCenter = (minPos + maxPos) * 0.5f;
                        halfExtents = glm::abs(maxPos - minPos) * 0.5f;
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
                    SaveProject();
                }
                break;
            }
            case Key::Q:
            {
                if (!InputSystem::IsMouseButtonPressed(Mouse::ButtonRight))
                {
                    m_ScenePanel->SetGizmoOperation(GizmoOperation::BOUND_SIZING_2D);
                }
                break;
            }
            case Key::B:
            {
                if (!InputSystem::IsMouseButtonPressed(Mouse::ButtonRight))
                {
                    m_ScenePanel->SetGizmoOperation(GizmoOperation::BOUND_SIZING_3D);
                }
                break;
            }
            case Key::T:
            {
                if (!InputSystem::IsMouseButtonPressed(Mouse::ButtonRight))
                {
                    m_ScenePanel->SetGizmoOperation(GizmoOperation::TRANSLATE);
                }
                break;
            }
            case Key::R:
            {
                if (!InputSystem::IsMouseButtonPressed(Mouse::ButtonRight))
                {
                    m_ScenePanel->SetGizmoOperation(GizmoOperation::ROTATE);
                }
                break;
            }
            case Key::E:
            {
                if (!InputSystem::IsMouseButtonPressed(Mouse::ButtonRight))
                {
                    m_ScenePanel->SetGizmoOperation(GizmoOperation::SCALE);
                }
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
                {
                    m_ScenePanel->DuplicateSelectedEntity();
                }
                break;
            }
            case Key::Y:
            {
                if (control)
                {
                    if (m_AssetEditorPanel && m_AssetEditorPanel->HasFocusedAssetEditor())
                    {
                        m_AssetEditorPanel->RedoFocusedAsset();
                        return true;
                    }
                    if (m_ActiveScene && !m_ActiveScene->IsRunning())
                    {
                        Application::GetCommandManager()->Redo();
                        return true;
                    }
                }
                break;
            }
            case Key::Z:
            {
                if (control)
                {
                    if (m_AssetEditorPanel && m_AssetEditorPanel->HasFocusedAssetEditor())
                    {
                        if (shift)
                            m_AssetEditorPanel->RedoFocusedAsset();
                        else
                            m_AssetEditorPanel->UndoFocusedAsset();
                        return true;
                    }
                    if (m_ActiveScene && !m_ActiveScene->IsRunning())
                    {
                        if (shift)
                            Application::GetCommandManager()->Redo();
                        else
                            Application::GetCommandManager()->Undo();
                        return true;
                    }
                }
                break;
            }
            default: break;
        }
        return false;
    }

    void EditorLayer::OnRender(RenderTarget *mainFramebuffer)
    {
        IGN_PROFILE_FUNCTION();
        Layer::OnRender(mainFramebuffer);

        ThumbnailGenerator::Get().ProcessFrame(Renderer::GetCurrentFrameContext());

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
            const glm::uvec2 desiredSize = glm::max(glm::uvec2(0), glm::uvec2(globals::GEditor::EditorViewport.max));
            ProcessCameraViewportResize(editCamera, desiredSize, m_State.editorResizing, m_State.editorResizingFrame, m_State.editorRequestToResize);
        }

        // Resizing game-play camera (Game Viewport)
        if (Entity primaryCam = m_ActiveScene->GetPrimaryCamera())
        {
            auto &cc = primaryCam.GetComponent<CameraComponent>();
            const glm::uvec2 desiredSize = glm::max(glm::uvec2(0), glm::uvec2(globals::GEditor::GameViewport.max));
            ProcessCameraViewportResize(&cc.camera, desiredSize, m_State.gameplayResizing, m_State.gameplayResizingFrame, m_State.gameplayRequestToResize);
        }

        // Resize the EditorPlayCamera (mirror camera for Play mode -> Editor Viewport).
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

            const glm::uvec2 desiredSize = glm::max(glm::uvec2(0), glm::uvec2(globals::GEditor::EditorViewport.max));
            bool dummyRequest = false;
            ProcessCameraViewportResize(&m_EditorPlayCamera, desiredSize, m_State.editorPlayResizing, m_State.editorPlayResizingFrame, dummyRequest);
        }

        FrameContext *frameContext = Renderer::GetCurrentFrameContext();
        const bool hasSeparateGameplayView = m_State.gameplayViewportWindow && m_ScenePanel->m_Data.sceneViewportGameplayVisible;

        // Render to Edit Viewport
        if (m_ScenePanel->m_Data.sceneViewportEditorVisible)
        {
            switch (m_ActiveScene->GetState())
            {
                case ESceneState::Play:
                {
                    if (!hasSeparateGameplayView)
                    {
                        if (Entity primaryCam = m_ActiveScene->GetPrimaryCamera())
                        {
                            auto &cc = primaryCam.GetComponent<CameraComponent>();
                            // Copy the live view matrix from the game camera into our editor-side mirror camera.
                            // The projection is already sized to the Editor Viewport, so both viewports are independent.
                            m_EditorPlayCamera.SetView(cc.camera.GetView());
                            m_EditorPlayCamera.position = cc.camera.position;
                            m_SceneRenderer->ClearGizmoDrawList();
                            {
                                IGN_PROFILE_SCOPE("SceneRenderer::RenderPlayToEditorViewport");
                                m_SceneRenderer->Render(&m_EditorPlayCamera, frameContext, false);
                            }
                            break;
                        }
                    }
                    [[fallthrough]];
                }
                case ESceneState::Simulate:
                case ESceneState::Stop:
                {
                    IGN_PROFILE_SCOPE("SceneRenderer::RenderEditorTo");
                    m_ScenePanel->SubmitGizmoForRender(m_SceneRenderer.get(), editCamera);
                    m_SceneRenderer->Render(editCamera, frameContext, true); // enable draw debug
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
                m_SceneRenderer->ClearGizmoDrawList();
                {
                    IGN_PROFILE_SCOPE("SceneRenderer::RenderGameplayTo");
                    m_SceneRenderer->Render(gameCamera, frameContext, false); // disable draw debug
                }
            }
        }

        if (m_State.takeScreenshot)
        {
            auto target = m_SceneRenderer->GetRenderTarget(editCamera);
            if (target)
            {
                auto sceneTexture = target->compositeRT->GetColorAttachment(0);
                if (sceneTexture)
                {
                    static ImageData image;
                    image.width = sceneTexture->GetWidth();
                    image.height = sceneTexture->GetHeight();

                    const uint32_t packedStride = image.width * 4u;
                    image.pixels.resize(image.height * packedStride);

                    if (sceneTexture->ReadPixelData(image.pixels.data(), image.pixels.size()))
                    {
                        const std::tm local_tm = Timestep::GetLocalTime();

                        std::ostringstream oss;
                        oss << std::put_time(&local_tm, "screenshot-ignite-%D-%H-%M-%S");
                        std::string filename = oss.str();
                        stringutils::ReplaceWith(filename, "/", "-");

                        SDL_ShowSaveFileDialog(OnScreenshotSaveFileSelected, &image,
                            Application::GetInstance()->GetWindow()->GetWindowHandle(),
                            kScreenshotFileFilters, IM_ARRAYSIZE(kScreenshotFileFilters),
                            filename.c_str());
                    }
                }
            }
            m_State.takeScreenshot = false;
        }
    }

    void EditorLayer::OnGuiRender()
    {
        IGN_PROFILE_FUNCTION();
        BindSharedImGuiContext();
        constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus
            | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

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
            if (ImGui::BeginMenu("File", !m_State.isLoading))
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
                    SerializeEditorState();
                    SaveProject();
                }

                else if (ImGui::MenuItem("Open Project"))
                {
                    OpenProject();
                }

                else if (ImGui::MenuItem("Close Project", nullptr, false, m_ActiveProject != nullptr))
                {
                    SerializeEditorState();
                    CloseProject();
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Package Game...", nullptr, false, m_ActiveProject != nullptr))
                {
                    SDL_ShowOpenFolderDialog(OnPackageFolderSelected, this,
                        Application::GetInstance()->GetWindow()->GetWindowHandle(),
                        m_ActiveProject->GetDirectory().string().c_str(), false);
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Lighting", m_ActiveProject != nullptr && m_ActiveScene != nullptr && !m_State.isLoading))
            {
                Entity bakeVolume;
                if (m_ScenePanel)
                    bakeVolume = m_ScenePanel->GetSelectedEntity();
                if (!bakeVolume || !bakeVolume.HasComponent<IrradianceVolumeComponent>())
                {
                    for (entt::entity e : m_ActiveScene->registry->view<IrradianceVolumeComponent>())
                    {
                        bakeVolume = Entity(e, m_ActiveScene.get());
                        break;
                    }
                }

                const std::string sceneName = m_CurrentSceneFilePath.empty()
                    ? "Scene" : m_CurrentSceneFilePath.stem().string();
                const std::filesystem::path bakedDir = m_ActiveProject->GetDirectory() / "Baked" / sceneName;
                const bool isBaking = m_GIBaker && m_GIBaker->IsBaking();
                const bool canBake = bakeVolume.IsValid() && !isBaking;

                if (ImGui::MenuItem("Bake GI", nullptr, false, canBake))
                {
                    const std::filesystem::path output = bakedDir
                        / (std::to_string(static_cast<uint64_t>(bakeVolume.GetUUID())) + ".igi");
                    m_BakeReflectionProbesAfterGI = false;
                    if (m_GIBaker->StartBake(m_ActiveScene.get(), m_ActiveScene->GetAssetManager(), bakeVolume, output))
                        m_State.giBakeModal = true;
                }
                if (ImGui::MenuItem("Bake Reflection Probes", nullptr, false, m_GIBaker != nullptr && !isBaking))
                {
                    m_GIBaker->BakeReflectionProbes(
                        m_ActiveScene.get(),
                        m_ActiveScene->GetAssetManager(),
                        m_SceneRenderer ? m_SceneRenderer->GetIrradianceVolume().get() : nullptr,
                        bakedDir);
                    if (m_SceneRenderer)
                        m_SceneRenderer->ClearReflectionProbeSet();
                }
                if (ImGui::MenuItem("Bake All", nullptr, false, m_GIBaker != nullptr && !isBaking))
                {
                    if (bakeVolume.IsValid())
                    {
                        const std::filesystem::path output = bakedDir
                            / (std::to_string(static_cast<uint64_t>(bakeVolume.GetUUID())) + ".igi");
                        m_BakeReflectionProbesAfterGI = true;
                        if (m_GIBaker->StartBake(m_ActiveScene.get(), m_ActiveScene->GetAssetManager(), bakeVolume, output))
                            m_State.giBakeModal = true;
                    }
                    else
                    {
                        m_GIBaker->BakeReflectionProbes(
                            m_ActiveScene.get(),
                            m_ActiveScene->GetAssetManager(),
                            m_SceneRenderer ? m_SceneRenderer->GetIrradianceVolume().get() : nullptr,
                            bakedDir);
                        if (m_SceneRenderer)
                            m_SceneRenderer->ClearReflectionProbeSet();
                    }
                }
                if (ImGui::MenuItem("Clear Baked Data", nullptr, false, !isBaking))
                {
                    for (entt::entity e : m_ActiveScene->registry->view<IrradianceVolumeComponent>())
                    {
                        auto &vol = m_ActiveScene->registry->get<IrradianceVolumeComponent>(e);
                        vol.bakedAssetPath.clear();
                        vol.bakeHash = 0;
                    }
                    for (entt::entity e : m_ActiveScene->registry->view<ReflectionProbeComponent>())
                    {
                        auto &rp = m_ActiveScene->registry->get<ReflectionProbeComponent>(e);
                        rp.bakedAssetPath.clear();
                        rp.bakeHash = 0;
                    }
                    if (m_SceneRenderer)
                    {
                        m_SceneRenderer->ClearIrradianceVolume();
                        m_SceneRenderer->ClearReflectionProbeSet();
                    }
                }
                if (ImGui::MenuItem("Cancel GI Bake", nullptr, false, isBaking))
                    m_GIBaker->CancelBake();

                ImGui::Separator();
                if (ImGui::MenuItem("Show GI Probes", nullptr, m_SceneRenderer->sceneRenderSettings.showGIProbes))
                {
                    m_SceneRenderer->sceneRenderSettings.showGIProbes = !m_SceneRenderer->sceneRenderSettings.showGIProbes;
                }

                if (ImGui::BeginMenu("Debug Mode"))
                {
                    static constexpr int kGIModes[] = { RENDER_MODE_GI_DIFFUSE, RENDER_MODE_GI_SPECULAR,
                        RENDER_MODE_AO, RENDER_MODE_PROBE_WEIGHTS };
                    static constexpr const char *kGINames[] = { "GI Diffuse", "GI Specular", "AO", "Probe Weights" };
                    for (int i = 0; i < 4; ++i)
                    {
                        if (ImGui::MenuItem(kGINames[i], nullptr, m_SceneRenderer->GetRenderMode() == kGIModes[i]))
                            m_SceneRenderer->SetRenderMode(kGIModes[i]);
                    }
                    if (ImGui::MenuItem("Color", nullptr, m_SceneRenderer->GetRenderMode() == RENDER_MODE_COLOR))
                        m_SceneRenderer->SetRenderMode(RENDER_MODE_COLOR);
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }

            // EDIT MENU
            if (ImGui::BeginMenu("Edit", !m_State.isLoading))
            {
                CommandHistory *activeHistory = nullptr;
                if (m_AssetEditorPanel && m_AssetEditorPanel->HasFocusedAssetEditor())
                {
                    activeHistory = m_AssetEditorPanel->GetFocusedAssetHistory();
                }
                else if (CommandManager *cmdMgr = Application::GetCommandManager())
                {
                    activeHistory = &cmdMgr->GetSceneHistory();
                }

                const bool canUndo = activeHistory && activeHistory->CanUndo() && (!m_ActiveScene || !m_ActiveScene->IsRunning() || (m_AssetEditorPanel && m_AssetEditorPanel->HasFocusedAssetEditor()));
                const bool canRedo = activeHistory && activeHistory->CanRedo() && (!m_ActiveScene || !m_ActiveScene->IsRunning() || (m_AssetEditorPanel && m_AssetEditorPanel->HasFocusedAssetEditor()));

                const std::string undoLabel = (activeHistory && activeHistory->CanUndo())
                    ? std::format("Undo {}", activeHistory->GetUndoName())
                    : std::string("Undo");
                const std::string redoLabel = (activeHistory && activeHistory->CanRedo())
                    ? std::format("Redo {}", activeHistory->GetRedoName())
                    : std::string("Redo");

                if (ImGui::MenuItem(undoLabel.c_str(), "Ctrl+Z", false, canUndo))
                {
                    activeHistory->Undo();
                }
                if (ImGui::MenuItem(redoLabel.c_str(), "Ctrl+Shift+Z", false, canRedo))
                {
                    activeHistory->Redo();
                }
                ImGui::Separator();

                const std::string vsyncLabel = DeviceManager::GetInstance()->IsVsyncEnabled() ? "Disable Vsync" : "Enable Vsync";
                if (ImGui::MenuItem(vsyncLabel.c_str(), nullptr, nullptr))
                {
                    Application::SubmitToMainThread([]()
                    {
                        if (DeviceManager::GetInstance()->IsVsyncEnabled())
                            DeviceManager::GetInstance()->DisableVsync();
                        else
                            DeviceManager::GetInstance()->EnableVsync();
                    });
                }
                if (ImGui::MenuItem("Screenshot", nullptr, false, m_ActiveProject != nullptr))
                {
                    m_State.takeScreenshot = true;
                }
                ImGui::EndMenu();
            }

            // VIEW MENU
            if (ImGui::BeginMenu("View", !m_State.isLoading))
            {
                ImGui::MenuItem("Game", nullptr, &m_State.gameplayViewportWindow, m_ActiveProject != nullptr);
                ImGui::MenuItem("Scene Renderer", nullptr, &m_State.sceneRendererWindow, m_ActiveProject &&m_SceneRenderer != nullptr);
                ImGui::MenuItem("Network", nullptr, &m_State.networkWindow, m_ActiveProject != nullptr);
                ImGui::MenuItem("Settings", nullptr, &m_State.settingsWindow, m_ActiveProject != nullptr);
                ImGui::MenuItem("Asset Registry", nullptr, &m_State.assetRegistryWindow, m_ActiveProject != nullptr);

                const bool canAddContentBrowser = (GetOpenContentBrowserCount() + m_PendingContentBrowserPanelsToAdd) < 4;
                if (ImGui::MenuItem("Add Content Browser", nullptr, false, canAddContentBrowser))
                {
                    ++m_PendingContentBrowserPanelsToAdd;
                }

                ImGui::MenuItem("UI Showcase", nullptr, &m_State.uiShowcaseWindow);
                ImGui::MenuItem("ImGui Demo", nullptr, &m_State.imguiDemoWindow);

                ImGui::EndMenu();
            }

            // BUILD MENU
            if (ImGui::BeginMenu("Script", m_ActiveProject != nullptr && !m_State.isLoading))
            {
                if (ImGui::MenuItem("Build Solution"))
                {
                    AssetWorker::SubmitJob([this]()
                    {
                        m_ActiveProject->BuildSolution(false);
                    });
                }

                if (ImGui::MenuItem("Rebuild Solution"))
                {
                    AssetWorker::SubmitJob([this]()
                    {
                        m_ActiveProject->BuildSolution(true);
                    });
                }

                ProjectConfiguration currentConfig = m_ActiveProject->GetConfiguration();
                static const char *configNames[] = { "Debug", "Release", "Shipping" };
                const char *currentConfigName = configNames[static_cast<int>(currentConfig)];

                if (ImGui::BeginMenu("Active Configuration"))
                {
                    for (size_t i = 0; i < std::size(configNames); ++i)
                    {
                        if (ImGui::MenuItem(configNames[i], nullptr, nullptr, i != static_cast<size_t>(m_ActiveProject->GetConfiguration())))
                        {
                            m_ActiveProject->GetInfo().configuration = static_cast<ProjectConfiguration>(i);
                            AssetWorker::SubmitJob([this]()
                            {
                                SaveProject();
                                m_ActiveProject->BuildSolution(false);
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
        const float scale = std::max(UI::GetDpiScale(), 1.0f);
        const float barHeight = glm::round(36.0f * scale);
        const float buttonHeight = glm::round(24.0f * scale);

        // RENDER TOOL BAR
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 8.0f * scale, 5.0f * scale });
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 5.0f * scale, 0.0f });
        ImGui::BeginChild("##toolbar_child", { 0.0f, barHeight }, 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar);
        m_ScenePanel->RenderToolbar();
        ImGui::EndChild();
        ImGui::PopStyleVar(2);

        // Render Project Creation Pop Up
        UIProjectCreation();

        // Draw Loading Popup Modal
        if (m_LoadingProgress > 0.0f && m_LoadingProgress < 1.0f)
            ImGui::OpenPopup("Please Wait...");

        if (m_LoadingProgress > 0.0f && m_LoadingProgress < 1.0f)
        {
            if (ImGui::BeginPopupModal("Please Wait...", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::TextUnformatted(m_StatusText.c_str());
                UI::ProgressBar(m_LoadingProgress, ImVec2(320.0f, 24.0f));
                ImGui::EndPopup();
            }
        }

        // Draw Baking Popup Modal
        if (m_GIBaker && m_GIBaker->IsBaking())
            ImGui::OpenPopup("Bake GI");

        if (m_State.giBakeModal && ImGui::BeginPopupModal("Bake GI", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            const uint32_t total = m_GIBaker ? m_GIBaker->GetTotalProbes() : 0u;
            const uint32_t completed = m_GIBaker ? m_GIBaker->GetCompletedProbes() : 0u;
            const float progress = m_GIBaker ? m_GIBaker->GetProgress() : 0.0f;

            ImGui::TextUnformatted("Time-sliced irradiance bake");
            UI::ProgressBar(progress, ImVec2(320.0f, 24.0f));
            ImGui::Text("Probes: %u / %u", completed, total);

            if (m_GIBaker && m_GIBaker->IsBaking())
            {
                if (ImGui::Button("Cancel"))
                    m_GIBaker->CancelBake();
            }
            else if (ImGui::Button("Close"))
            {
                m_State.giBakeModal = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const ImVec2 dockSize = { avail.x, glm::max(0.0f, avail.y - barHeight) };
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

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 8.0f * scale, 5.0f * scale });
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 5.0f * scale, 0.0f });
        if (ImGui::BeginChild("##status_bar", { 0.0f, barHeight }, 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar))
        {
            ImGui::BeginDisabled((GetOpenContentBrowserCount() + m_PendingContentBrowserPanelsToAdd) >= 4);
            if (UI::Button("Content Browser", UI::ButtonVariant::Outline, UI::ButtonSize::Small, ImVec2(0.0f, 24.0f), ICON_LC_PLUS))
            {
                ++m_PendingContentBrowserPanelsToAdd;
            }
            ImGui::EndDisabled();

            if (m_ActiveScene && m_ActiveProject && !m_State.isLoading && m_CachedAllSceneAssetsReady && IsSceneGIOutOfDate(m_ActiveScene.get()))
            {
                ImGui::SameLine();
                const bool isBaking = m_GIBaker && m_GIBaker->IsBaking();
                if (UI::Button("GI Out of Date - Rebake", UI::ButtonVariant::Primary, UI::ButtonSize::Small, ImVec2(0.0f, 24.0f), nullptr, isBaking, isBaking))
                {
                    Entity bakeVolume;
                    if (m_ScenePanel)
                        bakeVolume = m_ScenePanel->GetSelectedEntity();
                    if (!bakeVolume || !bakeVolume.HasComponent<IrradianceVolumeComponent>())
                    {
                        for (entt::entity e : m_ActiveScene->registry->view<IrradianceVolumeComponent>())
                        {
                            bakeVolume = Entity(e, m_ActiveScene.get());
                            break;
                        }
                    }
                    if (bakeVolume.IsValid() && m_GIBaker)
                    {
                        const std::string sceneName = m_CurrentSceneFilePath.empty()
                            ? "Scene"
                            : m_CurrentSceneFilePath.stem().string();

                        const std::filesystem::path output = m_ActiveProject->GetDirectory()
                            / "Baked" / sceneName / (std::to_string(static_cast<uint64_t>(bakeVolume.GetUUID())) + ".igi");

                        m_BakeReflectionProbesAfterGI = false;
                        if (m_GIBaker->StartBake(m_ActiveScene.get(), m_ActiveScene->GetAssetManager(), bakeVolume, output))
                            m_State.giBakeModal = true;
                    }
                }
            }

            const std::string versionStr = std::format("FPS: {:.7} | Version: {}", ImGui::GetIO().Framerate, Application::GetVersionString());
            float versionWidth = ImGui::CalcTextSize(versionStr.c_str()).x;
            ImGui::SameLine(ImGui::GetWindowWidth() - versionWidth - 10.0f);
            ImGui::TextUnformatted(versionStr.c_str());
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(2);

        ImGui::End();

        // ImGui Demo
        if (m_State.imguiDemoWindow)
        {
            ImGui::ShowDemoWindow(&m_State.imguiDemoWindow);
        }

        // Modern UI Showcase
        if (m_State.uiShowcaseWindow)
        {
            UI::ShowShowcaseWindow(&m_State.uiShowcaseWindow);
        }

        // Console window
        if (m_State.consoleWindow)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 6.0f * scale, 6.0f * scale });
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 6.0f * scale, 6.0f * scale });
            if (ImGui::Begin("Console", &m_State.consoleWindow))
            {
                const auto &logs = Logger::GetLogs();

                // Allocate enough memory for log searching
                static char buffer[256] = { 0 };
                static std::vector<size_t> s_FilteredLogIndices;
                static std::string s_LastConsoleSearch;
                static size_t s_LastConsoleLogCount = 0;
                static std::string s_LastConsoleBackMsg;
                static bool s_ConsoleFilterDirty = true;

                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 8.0f * scale, 5.0f * scale });
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 5.0f * scale, 0.0f });
                if (ImGui::BeginChild("##console_control_bar", { 0.0f, barHeight }, ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
                {
                    // Clear Logs and Search Query
                    if (UI::Button("Clear", UI::ButtonVariant::Outline, UI::ButtonSize::Small, ImVec2(0.0f, buttonHeight), ICON_LC_TRASH_2))
                    {
                        std::memset(buffer, 0, sizeof(buffer));
                        s_LastConsoleSearch.clear();
                        s_FilteredLogIndices.clear();
                        s_LastConsoleLogCount = 0;
                        s_LastConsoleBackMsg.clear();
                        s_ConsoleFilterDirty = true;

                        Logger::ClearLogs();
                    }

                    // Query search string
                    ImGui::SameLine();

                    if (UI::TextField(
                        "##console_search",
                        buffer,
                        sizeof(buffer),
                        "Filter logs...",
                        ImVec2(glm::round(280.0f * scale), buttonHeight),
                        ICON_LC_SEARCH,
                        true,
                        ImGuiInputTextFlags_EscapeClearsAll | ImGuiInputTextFlags_NoHorizontalScroll))
                    {
                        s_ConsoleFilterDirty = true;
                    }

                    const std::string_view currentSearchView(buffer);
                    const bool logsChanged = (logs.size() != s_LastConsoleLogCount)
                        || (!logs.empty() && logs.back().message != s_LastConsoleBackMsg);
                    if (s_ConsoleFilterDirty || logsChanged || currentSearchView != s_LastConsoleSearch)
                    {
                        s_LastConsoleSearch.assign(currentSearchView);
                        s_LastConsoleLogCount = logs.size();
                        s_LastConsoleBackMsg = logs.empty() ? std::string() : logs.back().message;
                        s_ConsoleFilterDirty = false;
                        s_FilteredLogIndices.clear();

                        if (!s_LastConsoleSearch.empty())
                        {
                            const std::string searchLower = stringutils::ToLower(s_LastConsoleSearch);
                            s_FilteredLogIndices.reserve(logs.size());
                            for (size_t i = 0; i < logs.size(); ++i)
                            {
                                const std::string messageLower = stringutils::ToLower(logs[i].message);
                                if (messageLower.find(searchLower) != std::string::npos)
                                {
                                    s_FilteredLogIndices.push_back(i);
                                }
                            }
                        }
                    }
                }
                ImGui::EndChild();
                ImGui::PopStyleVar(2);
                

                ImGui::Separator();
                if (ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar))
                {
                    const auto printLogFn = [](const LogMessage &log)
                    {
                        ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                        switch (log.level)
                        {
                            case spdlog::level::trace:    color = ImVec4(0.8f, 0.8f, 0.8f, 1.0f); break;
                            case spdlog::level::debug:    color = ImVec4(0.2f, 0.8f, 0.8f, 1.0f); break;
                            case spdlog::level::info:     color = ImVec4(0.2f, 0.8f, 0.2f, 1.0f); break;
                            case spdlog::level::warn:     color = ImVec4(0.8f, 0.8f, 0.2f, 1.0f); break;
                            case spdlog::level::err:      color = ImVec4(0.8f, 0.2f, 0.2f, 1.0f); break;
                            case spdlog::level::critical: color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); break;
                            default: break;
                        }

                        if (log.level >= 0 && log.level < spdlog::level::n_levels)
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, color);
                            ImGui::TextWrapped("%s", log.message.c_str());
                            ImGui::PopStyleColor();
                        }
                    };

                    // Print with search or default
                    if (s_LastConsoleSearch.empty())
                    {
                        for (const auto &log : logs)
                        {
                            printLogFn(log);
                        }
                    }
                    else
                    {
                        for (const size_t logIndex : s_FilteredLogIndices)
                        {
                            if (logIndex < logs.size())
                            {
                                printLogFn(logs[logIndex]);
                            }
                        }
                    }
                }

                // Scroll to the bottom
                if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                {
                    ImGui::SetScrollHereY(1.0f);
                }

                ImGui::EndChild();
            }
            ImGui::End();
            ImGui::PopStyleVar(2);
        }

        // Draw UI
        UISceneRenderer();
        UISettings();
    }

    void EditorLayer::SetActiveScene(const Ref<Scene> &scene)
    {
        if (m_ActiveScene == scene)
            return;

        // Update active scene
        if (m_ActiveProject)
        {
            if (scene)
                scene->PreloadReferencedAssets();
        }

        // Clear references in all systems before changing active scene
        if (m_ScenePanel)
            m_ScenePanel->SetActiveScene(nullptr);

        m_SceneRenderer->SetActiveScene(nullptr);

        if (m_ActiveProject)
            m_ActiveProject->SetActiveScene(nullptr);

        // Set new scene
        m_ActiveScene = scene;
        if (auto *net = NetworkManager::GetInstance())
        {
            net->SetActiveScene(m_ActiveScene.get());
        }

        // Update all systems with new scene
        if (m_ScenePanel)
            m_ScenePanel->SetActiveScene(scene.get());

        if (m_ActiveProject)
            m_ActiveProject->SetActiveScene(scene);

        m_SceneRenderer->SetActiveScene(scene);
        GameUISystem::SetSceneContext(scene.get());
    }

    void EditorLayer::SyncActiveSceneFromProject()
    {
        if (!m_ActiveProject)
            return;

        Ref<Scene> projectScene = m_ActiveProject->LockActiveScene();
        if (!projectScene || projectScene == m_ActiveScene)
            return;

        // Only auto-sync when a runtime transition occurs while playing/simulating.
        if (!m_ActiveScene || (!m_ActiveScene->IsRunning() && !projectScene->IsRunning()))
            return;

        SetActiveScene(projectScene);
        if (m_ActiveScene && !m_ActiveScene->IsRunning())
        {
            m_ActiveScene->OnStart(ESceneState::Play);
        }
        m_FixedAccumulator = 0.0f;
    }

    void EditorLayer::EnterPrefabIsolation(AssetHandle prefabHandle)
    {
        if (m_InPrefabIsolationMode)
        {
            ExitPrefabIsolation(true);
        }

        AssetWorker::SubmitJob([this, handle = prefabHandle]()
        {
            auto assetManager = AssetManager::GetInstance();
            if (!assetManager)
                return;

            Ref<Prefab> prefab = assetManager->GetAssetImmediate<Prefab>(handle);
            if (!prefab || !prefab->GetPrefabScene())
            {
                LOG_ERROR("[Editor] Failed to load prefab for isolation editing!");
                return;
            }

            m_MainSceneBeforeIsolation = m_ActiveScene;
            m_EditingPrefab = prefab;
            m_EditingPrefabHandle = handle;
            m_InPrefabIsolationMode = true;

            Application::SubmitToMainThread([this, pr = prefab, assetManager, handle]()
            {
                if (auto *cmdMgr = Application::GetCommandManager())
                {
                    cmdMgr->ClearSceneHistory();
                }
                SetActiveScene(pr->GetPrefabScene());
                // SetStatusText(fmt::format("Editing Prefab: {}", assetManager->GetAssetDisplayName(handle)));
            });
        });
    }

    void EditorLayer::ExitPrefabIsolation(bool save)
    {
        if (!m_InPrefabIsolationMode)
            return;

        auto assetManager = AssetManager::GetInstance();
        bool isDirty = m_EditingPrefab && m_EditingPrefab->GetPrefabScene() && m_EditingPrefab->GetPrefabScene()->IsDirty();

        if (save && isDirty && m_EditingPrefab && m_EditingPrefabHandle != AssetHandle(0))
        {
            const auto &meta = assetManager->GetMetaData(m_EditingPrefabHandle);
            if (!meta.filepath.empty())
            {
                auto project = m_ActiveProject;
                std::filesystem::path fullPath = project ? project->GetProjectFilepath(meta.filepath) : meta.filepath;
                m_EditingPrefab->Serialize(fullPath);
                SetStatusText(fmt::format("Saved Prefab: {}", meta.filepath.filename().string()));
            }
        }

        Ref<Scene> previousMainScene = m_MainSceneBeforeIsolation;
        AssetHandle editingHandle = m_EditingPrefabHandle;

        m_InPrefabIsolationMode = false;
        m_EditingPrefab = nullptr;
        m_EditingPrefabHandle = AssetHandle(0);
        m_MainSceneBeforeIsolation = nullptr;

        if (auto *cmdMgr = Application::GetCommandManager())
        {
            cmdMgr->ClearSceneHistory();
        }

        if (previousMainScene)
        {
            if (save && isDirty && editingHandle != AssetHandle(0))
            {
                SceneManager::SyncAllPrefabInstances(previousMainScene.get(), editingHandle);
            }
            SetActiveScene(previousMainScene);
        }
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
        OnSceneStop();

        if (auto *cmdMgr = Application::GetCommandManager())
        {
            cmdMgr->ClearSceneHistory();
        }

        m_CurrentSceneFilePath.clear();

        // Unload current scene from RAM, so we can load directly from filepath (JSON)
        if (m_CurrentSceneHandle != AssetHandle(0))
        {
            AssetManager::GetInstance()->UnloadAsset(m_CurrentSceneHandle);
            m_CurrentSceneHandle = AssetHandle(0);
        }

        // Clear active scene first to release references in renderer and panels
        SetActiveScene(nullptr);

        // Reset scenes - this should trigger destructors
        m_EditorScene.reset();
        m_ActiveScene.reset();

        // Unload unused assets (assets not referenced by anything else)
        if (m_ActiveProject)
        {
            AssetManager::GetInstance()->UnloadUnusedAssets();
            ImGui_NRI::ClearTextureCache();

            // Create new editor scene
            m_EditorScene = Scene::Create(m_ActiveProject.get());

            // Set as active scene
            SetActiveScene(m_EditorScene);
        }

        LOG_ASSERT(m_ActiveProject, "Invalid project!");
        LOG_ASSERT(m_ActiveScene, "Failed to create new scene!");
    }

    void EditorLayer::SaveScene()
    {
        // Prefab Scene
        if (m_InPrefabIsolationMode)
        {
            if (m_EditingPrefabHandle != AssetHandle(0))
            {
                // Get filepath to serialize
                const auto &filepath = AssetManager::GetInstance()->GetFilepath(m_EditingPrefabHandle);
                const auto &absPath = m_ActiveProject->GetProjectFilepath(filepath);
                if (std::filesystem::exists(absPath))
                    m_EditingPrefab->Serialize(absPath);
            }
        }

        // Main Scene
        else
        {
            // 1. Save current scene if the Current Scene Handle is the same with the current scene
            const AssetHandle activeSceneHandle = m_ActiveScene ? m_ActiveScene->handle : AssetHandle(0);
            if (m_CurrentSceneHandle != AssetHandle(0) && m_CurrentSceneHandle == activeSceneHandle && !m_CurrentSceneFilePath.empty())
            {
                SaveScene(m_CurrentSceneFilePath);
            }
            else
            {
                // 2. Open a File Save Dialog to save current scene if the Current Scene Handle is different (not saved yet to disk)
                SaveSceneAs();
            }
        }
    }

    void EditorLayer::SaveScene(const std::filesystem::path &filepath)
    {
        Ref<Scene> sceneToSave = m_EditorScene ? m_EditorScene : m_ActiveScene;
        if (!sceneToSave || !m_ActiveProject)
            return;

        std::error_code ec;
        const auto normDest = std::filesystem::weakly_canonical(filepath, ec);
        const auto normCurr = m_CurrentSceneFilePath.empty() ? std::filesystem::path() : std::filesystem::weakly_canonical(m_CurrentSceneFilePath, ec);
        const bool savingToDifferentFile = !m_CurrentSceneFilePath.empty() && !normDest.empty() && (normDest != normCurr);

        if (AssetManager *am = AssetManager::GetInstance())
        {
            const AssetHandle existingHandle = am->GetAssetHandle(filepath);
            if (savingToDifferentFile)
            {
                if (existingHandle != AssetHandle(0) && existingHandle != m_CurrentSceneHandle)
                {
                    sceneToSave->handle = existingHandle;
                }
                else
                {
                    sceneToSave->handle = AssetHandle();
                }

                const auto relPath = m_ActiveProject->GetProjectRelativeFilepath(filepath);
                AssetMetaData meta(relPath, AssetType::Scene);
                am->AssignMetaData(sceneToSave->handle, meta);
                am->AssignAsset(sceneToSave->handle, sceneToSave);
            }
            else
            {
                if (sceneToSave->handle == AssetHandle(0))
                {
                    sceneToSave->handle = AssetHandle();
                }

                if (existingHandle != AssetHandle(0))
                {
                    sceneToSave->handle = existingHandle;
                }
                else
                {
                    const auto relPath = m_ActiveProject->GetProjectRelativeFilepath(filepath);
                    AssetMetaData meta(relPath, AssetType::Scene);
                    am->AssignMetaData(sceneToSave->handle, meta);
                    am->AssignAsset(sceneToSave->handle, sceneToSave);
                }
            }
        }
        else if (sceneToSave->handle == AssetHandle(0))
        {
            sceneToSave->handle = AssetHandle();
        }

        SceneSerializer serializer(sceneToSave, m_ActiveProject.get());
        serializer.Serialize(filepath);

        if (m_EditorScene)
        {
            m_EditorScene->handle = sceneToSave->handle;
            m_EditorScene->SetDirtyFlag(false);
        }
        if (m_ActiveScene)
        {
            m_ActiveScene->handle = sceneToSave->handle;
            m_ActiveScene->SetDirtyFlag(false);
        }

        m_CurrentSceneFilePath = filepath;
        m_CurrentSceneHandle = sceneToSave->handle;
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
        AssetHandle sceneHandle = AssetManager::GetInstance()->GetAssetHandle(filepath);
        if (m_CurrentSceneHandle == sceneHandle)
        {
            LOG_WARN("Dismiss opening current scene {0}", filepath.generic_string());
            return;
        }

        // Unload current scene from RAM, so we can load directly from filepath (JSON)
        if (m_CurrentSceneHandle != AssetHandle(0))
        {
            AssetManager::GetInstance()->UnloadAsset(m_CurrentSceneHandle);
        }

        m_CurrentSceneHandle = sceneHandle;

        // Submit heavy I/O work to asset worker
        AssetWorker::SubmitJob([this, filepath, sceneHandle]()
        {
            AssetWorker::ReportStatus(std::format("Loading scene {}...", filepath.filename().string()), 0.5f);

            // Load scene on worker thread (I/O happens here)
            Ref<Scene> loadedScene = AssetManager::GetInstance()->GetAssetImmediate<Scene>(sceneHandle);
            if (loadedScene)
            {
                // Submit UI update back to main thread
                Application::SubmitToMainThread([this, loadedScene, filepath, sceneHandle]() mutable
                {
                    // Stop scene
                    if (m_EditorScene)
                        m_EditorScene->OnStop();

                    if (m_ActiveScene)
                        m_ActiveScene->OnStop();

                    // Clear active scene references
                    SetActiveScene(nullptr);

                    if (auto *cmdMgr = Application::GetCommandManager())
                    {
                        cmdMgr->ClearSceneHistory();
                    }

                    // Reset old scene
                    m_EditorScene.reset();
                    m_ActiveScene.reset();

                    // Unload unused assets
                    if (m_ActiveProject)
                        AssetManager::GetInstance()->UnloadUnusedAssets();

                    // Copy and activate new scene
                    m_EditorScene = SceneManager::Copy(loadedScene);
                    m_EditorScene->SetDirtyFlag(false);

                    SetActiveScene(m_EditorScene);

                    m_CurrentSceneFilePath = filepath;
                    m_CurrentSceneHandle = sceneHandle;

                    AssetWorker::ReportStatus("Ready", 0.0f);
                });
            }
            else
            {
                LOG_ERROR("[Editor] Failed to load scene: {}", filepath.generic_string());
                AssetWorker::ReportStatus("Ready", 0.0f);
            }
        });
    }

    void EditorLayer::SaveProject()
    {
        if (m_ActiveProject)
        {
            // Ignore triggering SaveAs(...) call
            // Auto save if there is a valid scene path
            if (!m_CurrentSceneFilePath.empty())
            {
                SaveScene();
            }
            m_ActiveProject->Serialize(m_CurrentProjectFilepath);
        }
    }

    void EditorLayer::OpenProject()
    {
        SDL_ShowOpenFileDialog(OnProjectOpenFileSelected, this,
            Application::GetInstance()->GetWindow()->GetWindowHandle(),
            kProjectFileFilters, IM_ARRAYSIZE(kProjectFileFilters),
            nullptr, false);
    }

    void EditorLayer::CloseProject()
    {
        if (!m_ActiveProject)
            return;

        if (IsInPrefabIsolation())
        {
            ExitPrefabIsolation(true);
        }

        // Stop scene before saving
        if (m_EditorScene)
            m_EditorScene->OnStop();

        if (m_ActiveScene)
            m_ActiveScene->OnStop();

        SaveProject();

        SetActiveScene(nullptr);

        m_EditorScene.reset();
        m_ActiveScene.reset();

        if (m_AssetEditorPanel)
        {
            m_AssetEditorPanel->CloseAllAssetEditors();
        }

        if (auto *cmdMgr = Application::GetCommandManager())
        {
            cmdMgr->ClearAll();
        }

        ContentBrowserPanel::ReleaseSharedResources();

        ThumbnailGenerator::Get().ClearPending();
        AssetManager::GetInstance()->Reset();

        // Reset everything
        m_ActiveProject.reset();
        Project::CloseActive();

        m_CurrentProjectFilepath.clear();
        m_MainSceneBeforeIsolation.reset();
        m_EditingPrefab.reset();
        m_InPrefabIsolationMode = false;
        m_CurrentSceneFilePath.clear();

        m_EditingPrefabHandle = AssetHandle(0);
        m_CurrentSceneHandle = AssetHandle(0);
    }

    void EditorLayer::OpenProject(const std::filesystem::path &filepath)
    {
        if (filepath == m_CurrentProjectFilepath)
        {
            LOG_WARN("Dismiss opening current project {0}", filepath.generic_string());
            return;
        }

        m_State.isLoading = true;

        // Clear old project's loaded assets before opening new project
        if (m_ActiveProject)
        {
            CloseProject();
        }

        AssetWorker::SubmitJob([this, filepath]()
        {
            if (const Ref<Project> openedProject = Project::Open(filepath))
            {
                Application::SubmitToMainThread([this, openedProject, filepath]()
                {
                    m_ActiveProject = openedProject;
                    m_CurrentProjectFilepath = filepath;
                    OnOpenProject();
                });
            }
            else
            {
                Application::SubmitToMainThread([this]()
                {
                    m_State.isLoading = false;
                });
                LOG_ASSERT(false, "Failed to open project!");
            }
        });
    }

    bool EditorLayer::OnWindowCloseEvent(WindowCloseEvent &event)
    {
        SerializeEditorState();
        return false;
    }

    void EditorLayer::OnScenePlay()
    {
        if (!m_EditorScene)
        {
            LOG_WARN("[EditorLayer] Cannot play: m_EditorScene is null");
            return;
        }

        if (!m_EditorScene->IsReadyToPlay())
        {
            const SceneAssetLoadProgress progress = m_EditorScene->GetAssetLoadProgress();
            LOG_WARN("[EditorLayer] Cannot play: scene assets are not ready yet ({}/{})", progress.readyAssets, progress.totalAssets);
            SetStatusText(std::format("Loading scene assets ({}/{})...", progress.readyAssets, progress.totalAssets));
            return;
        }

        OnSceneStop();

        // copy initial components to new scene
        Ref<Scene> playScene = SceneManager::Copy(m_EditorScene);
        if (playScene)
        {
            SetActiveScene(playScene);
            m_ActiveScene->OnStart(ESceneState::Play);
        }
    }

    void EditorLayer::OnSceneStop()
    {
        if (m_EditorScene)
            m_EditorScene->OnStop();

        if (m_ActiveScene)
            m_ActiveScene->OnStop();

        InputSystem::ResetCursorState();

        SetActiveScene(m_EditorScene);
    }

    void EditorLayer::OnSceneSimulate()
    {
        if (!m_EditorScene)
        {
            LOG_WARN("[EditorLayer] Cannot simulate: m_EditorScene is null");
            return;
        }

        if (!m_EditorScene->IsReadyToPlay())
        {
            const SceneAssetLoadProgress progress = m_EditorScene->GetAssetLoadProgress();
            LOG_WARN("[EditorLayer] Cannot simulate: scene assets are not ready yet ({}/{})", progress.readyAssets, progress.totalAssets);
            SetStatusText(std::format("Loading scene assets ({}/{})...", progress.readyAssets, progress.totalAssets));
            return;
        }

        if (m_EditorScene)
            m_EditorScene->OnStop();

        OnSceneStop();

        // copy initial components to new scene
        Ref<Scene> simScene = SceneManager::Copy(m_EditorScene);
        if (simScene)
        {
            SetActiveScene(simScene);
            m_ActiveScene->OnStart(ESceneState::Simulate);
        }
    }

    void EditorLayer::OnSceneSaveFileSelected(void *userData, const char *const *filelist, int filter)
    {
        auto *editor = static_cast<EditorLayer*>(userData);

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
        auto editor = static_cast<EditorLayer*>(userData);

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
        EditorLayer *editor = static_cast<EditorLayer*>(userData);

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
        auto *editor = static_cast<EditorLayer*>(userData);

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

        if (std::string filepath = filelist[0]; !filepath.empty())
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
            editor->m_State.projectCreateInfo.filepath = std::filesystem::path(filepath) / editor->m_State.projectCreateInfo.name; // Append project name
        }
    }

    void EditorLayer::OnPackageFolderSelected(void *userData, const char *const *filelist, int filter)
    {
        if (filelist == nullptr || *filelist == nullptr) return;

        const std::string filepath = filelist[0];
        if (filepath.empty()) return;

        auto *editor = static_cast<EditorLayer *>(userData);
        if (!editor || !editor->m_ActiveProject) return;

        AssetWorker::SubmitJob([editor, f = filepath]()
        {
            PackageBuildConfig config;
            config.outputDirectory = std::filesystem::path(f);
            config.includeDedicatedServer = true;

            const PackageBuildResult result = GamePackager::PackageProject(editor->m_ActiveProject.get(), config);
            if (result.success)
            {
                editor->SetStatusText("Packaged game to " + result.outputDirectory.string());
                LOG_INFO("[Editor] Game packaged successfully: {}", result.executablePath.string());
            }
            else
            {
                editor->SetStatusText("Package failed: " + result.errorMessage);
                LOG_ERROR("[Editor] Package failed: {}", result.errorMessage);
            }
        });
    }

    void EditorLayer::SerializeEditorState()
    {
        if (m_ActiveProject)
        {
            const auto filepath = m_ActiveProject->GetDirectory() / "EditorStates.ign";
            Serializer sr(filepath);

            sr.BeginMap(); // START

            sr.BeginMap("EditorStates");
            {
                // Windows
                sr.BeginMap("Windows");
                {
                    sr.AddKeyValue("Console", m_State.consoleWindow);
                    sr.AddKeyValue("Network", m_State.networkWindow);
                    sr.AddKeyValue("SceneRenderer", m_State.sceneRendererWindow);
                    sr.AddKeyValue("GameplayViewport", m_State.gameplayViewportWindow);
                }
                sr.EndMap();

                // Camera
                if (m_ScenePanel)
                {
                    sr.BeginMap("Camera");
                    {
                        const auto &camera = m_ScenePanel->GetViewportCamera();
                        sr.AddKeyValue("Translation", camera.position);
                        sr.AddKeyValue("Yaw", camera.yaw);
                        sr.AddKeyValue("Pitch", camera.pitch);
                        sr.AddKeyValue("Fov", camera.fov);
                        sr.AddKeyValue("FarPlane", camera.farPlane);
                        sr.AddKeyValue("NearPlane", camera.nearPlane);
                        sr.AddKeyValue("OrthoSize", camera.orthoSize);
                        sr.AddKeyValue("Distance", camera.GetDistance());
                        sr.AddKeyValue("FocusTarget", camera.GetTarget());
                        sr.AddKeyValue("ProjectionType", static_cast<int>(camera.projectionType));
                    }
                    sr.EndMap();
                }

                if (m_SceneRenderer)
                {
                    sr.BeginMap("SceneRenderer");
                    {
                        const auto &renderSettings = m_SceneRenderer->sceneRenderSettings;
                        sr.AddKeyValue("ShowBoundingBox", renderSettings.showBoundingBox);
                        sr.AddKeyValue("ShowPhysicsCollider", renderSettings.showPhysicsCollider);
                        sr.AddKeyValue("ShowEntityNames", renderSettings.showEntityNames);
                    }
                    sr.EndMap();
                }
            }
            sr.EndMap();

            sr.EndMap(); // END

            sr.Serialize();
        }

    }

    void EditorLayer::DeserializeEditorState()
    {
        const auto filepath = m_ActiveProject->GetDirectory() / "EditorStates.ign";
        if (!std::filesystem::exists(filepath))
            return;

        JsonNode stateFileNode = Serializer::Deserialize(filepath);
        JsonNode stateNode = stateFileNode["EditorStates"];
        if (!stateNode)
            return;

        if (JsonNode windowsNode = stateNode["Windows"])
        {
            if (auto n = windowsNode["Console"]) m_State.consoleWindow = n.as<bool>();
            if (auto n = windowsNode["Network"]) m_State.networkWindow = n.as<bool>();
            if (auto n = windowsNode["SceneRenderer"]) m_State.sceneRendererWindow = n.as<bool>();
            if (auto n = windowsNode["GameplayViewport"]) m_State.gameplayViewportWindow = n.as<bool>();
        }

        if (m_ScenePanel)
        {
            if (JsonNode cameraNode = stateNode["Camera"])
            {
                auto &camera = m_ScenePanel->GetViewportCamera();
                if (auto n = cameraNode["Translation"]) camera.position = n.as<glm::vec3>();
                if (auto n = cameraNode["Yaw"]) camera.yaw = n.as<float>();
                if (auto n = cameraNode["Pitch"]) camera.pitch = n.as<float>();
                if (auto n = cameraNode["Fov"]) camera.fov = n.as<float>();
                if (auto n = cameraNode["FarPlane"]) camera.farPlane = n.as<float>();
                if (auto n = cameraNode["NearPlane"]) camera.nearPlane = n.as<float>();
                if (auto n = cameraNode["OrthoSize"]) camera.orthoSize = n.as<float>();
                if (auto n = cameraNode["Distance"]) camera.SetDistance(n.as<float>());
                if (auto n = cameraNode["FocusTarget"]) camera.SetTarget(n.as<glm::vec3>());
                if (auto n = cameraNode["ProjectionType"]) camera.projectionType = static_cast<ProjectionType>(n.as<int>());
                camera.UpdateCameraPosition(0.0f);
                camera.UpdateView();
            }
        }

        if (m_SceneRenderer)
        {
            if (JsonNode sceneRendererNode = stateNode["SceneRenderer"])
            {
                auto &renderSettings = m_SceneRenderer->sceneRenderSettings;
                if (auto n = sceneRendererNode["ShowBoundingBox"]) renderSettings.showBoundingBox = n.as<bool>();
                if (auto n = sceneRendererNode["ShowPhysicsCollider"]) renderSettings.showPhysicsCollider = n.as<bool>();
                if (auto n = sceneRendererNode["ShowEntityNames"]) renderSettings.showEntityNames = n.as<bool>();
            }
        }
    }

    void EditorLayer::OnOpenProject()
    {
        // Apply project undo limit setting
        if (m_ActiveProject)
        {
            if (auto *cmdMgr = Application::GetCommandManager())
            {
                cmdMgr->SetMaxHistory(m_ActiveProject->GetInfo().editorSettings.undoHistoryLimit);
            }
            if (m_SceneRenderer)
            {
                const std::filesystem::path gameSettingsPath = m_ActiveProject->GetDirectory() / "GameSettings.ign";
                if (m_SceneRenderer->sceneRenderSettings.DeserializeFromFile(gameSettingsPath))
                {
                    m_State.editorRequestToResize = true;
                    m_State.gameplayRequestToResize = true;
                }
            }
        }

        // Reload project files
        ReloadContentBrowserPanels();

        AssetWorker::SubmitJob([this]()
        {
            // Get Project default scene (use immediate load for synchronous path)
            AssetHandle defSceneAssetHandle = m_ActiveProject->GetInfo().defaultSceneHandle;
            if (defSceneAssetHandle != AssetHandle(0))
            {
                // Use GetAssetImmediate since we're on main thread and need synchronous load
                if (Ref<Scene> activeScene = AssetManager::GetInstance()->GetAssetImmediate<Scene>(defSceneAssetHandle))
                {
                    m_EditorScene = SceneManager::Copy(activeScene);
                    m_EditorScene->SetDirtyFlag(false);
                    SetActiveScene(m_EditorScene);

                    const auto &[assetFilepath, assetType] = AssetManager::GetInstance()->GetMetaData(activeScene->handle);

                    m_CurrentSceneFilePath = m_ActiveProject->GetProjectFilepath(assetFilepath);
                    m_CurrentSceneHandle = activeScene->handle;
                    m_PendingSceneThumbnailCapture = true;
                    m_SceneThumbnailWaitFrames = 0;
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

            DeserializeEditorState();
            if (m_SceneRenderer && m_ActiveProject && std::filesystem::exists(m_ActiveProject->GetDirectory() / "GameSettings.ign"))
            {
                m_SceneRenderer->sceneRenderSettings.SyncToScene(m_ActiveScene.get(), m_ScenePanel ? &m_ScenePanel->GetViewportCamera() : nullptr);
            }
            m_State.isLoading = false;
        });

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
                OpenScene(payload.metadata.filepath);
            }
            else if (payload.metadata.type == AssetType::Project)
            {
                std::filesystem::path filepath = payload.metadata.filepath;
                OpenProject(filepath);
            }
            break;
        }
        case ImportType::Save:
        {
            if (payload.metadata.type == AssetType::Scene)
            {
                // Submit scene save to asset worker
                std::filesystem::path filepath = payload.metadata.filepath;

                // AssetWorker::SubmitJob([this, filepath]()
                Application::SubmitToMainThread([this, filepath]()
                {
                    SaveScene(filepath);
                    LOG_INFO("[Editor] Scene saved: {}", filepath.generic_string());
                    RefreshContentBrowsers();
                });
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

            ImGui::Spacing();

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Location");
            ImGui::TableNextColumn();

            // Path input with Browse button
            ImGui::PushItemWidth(-120);
            std::string pathStr = m_State.projectCreateInfo.filepath.generic_string();

            std::array<char, 1024> pathBuf = {};
            if (!pathStr.empty())
            {
                strncpy(pathBuf.data(), pathStr.c_str(), sizeof(pathBuf) - 1);
            }

            if (ImGui::InputText("##ProjectLocation", pathBuf.data(), sizeof(pathBuf)))
            {
                m_State.projectCreateInfo.filepath = pathBuf.data();
            }

            ImGui::PopItemWidth();
            ImGui::SameLine();
            if (ImGui::Button("Browse"))
            {
                std::string filepath = FileDialogs::SelectFolder();
                if (!filepath.empty())
                {
                    m_State.projectCreateInfo.filepath = std::filesystem::path(filepath) / m_State.projectCreateInfo.name;
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
            // Close current project before creating new one
            if (m_ActiveProject)
            {
                CloseProject();
            }

            const std::string projName = m_State.projectCreateInfo.name;
            const std::filesystem::path parentDir = m_State.projectCreateInfo.filepath.parent_path();

            AssetWorker::SubmitJob([this, projName, parentDir]()
            {
                if (Ref<Project> newProject = Project::New(projName, parentDir))
                {
                    // Submit to main thread to sync
                    Application::SubmitToMainThread([this, newProject]()
                    {
                        m_ActiveProject = newProject;
                        m_CurrentProjectFilepath = newProject->GetFilepath();

                        // clear modal inputs
                        m_State.projectCreateInfo.filepath.clear();
                        m_State.projectCreateInfo.name.clear();
                        memset(nameBuffer, 0, sizeof(nameBuffer));

                        OnOpenProject();
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
        if (m_State.networkWindow)
        {
            if (auto *net = NetworkManager::GetInstance())
            {
                net->OnImGuiRender(&m_State.networkWindow);
            }
        }

        if (m_ActiveScene && m_State.settingsWindow)
        {
            ImGui::Begin("Settings", &m_State.settingsWindow);

            if (ImGui::BeginTabBar("##settings_tabs", ImGuiTabBarFlags_Reorderable))
            {
                if (ImGui::BeginTabItem("Physics"))
                {
                    if (auto assetManager = AssetManager::GetInstance())
                    {
                        if (auto project = assetManager->LockActiveProject())
                        {
                            auto &physSettings = project->GetInfo().physicsSettings;

                            ImGui::Text("Global Settings");
                            if (ImGui::DragFloat3("Gravity", &physSettings.gravity.x, 0.1f))
                            {
                                project->SetDirtyFlag(true);
                            }

                            constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersOuter
                                                                 | ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX
                                                                 | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_HighlightHoveredColumn;
                            constexpr ImGuiTableColumnFlags columnFlags = ImGuiTableColumnFlags_AngledHeader | ImGuiTableColumnFlags_NoHeaderWidth;

                            ImGui::Spacing();
                            ImGui::Separator();
                            ImGui::Spacing();

                            ImGui::Text("Physics Layers (%u / %u slots)", physSettings.layerCount, physics::MAX_PHYSICS_LAYERS);

                            ImGui::SameLine();
                            if (ImGui::Button("Add Layer Slot") && physSettings.layerCount < physics::MAX_PHYSICS_LAYERS)
                            {
                                physSettings.layerCount++;
                                project->SetDirtyFlag(true);
                            }

                            ImGui::SameLine();
                            if (ImGui::Button("Remove Layer Slot") && physSettings.layerCount > 1)
                            {
                                physSettings.layerCount--;
                                project->SetDirtyFlag(true);
                            }

                            if (ImGui::TreeNodeEx("Layer Names", ImGuiTreeNodeFlags_DefaultOpen))
                            {
                                ImGui::BeginChild("##physics_layer_names", ImVec2(-1.0f, 256.0f));
                                for (uint32_t i = 0; i < physSettings.layerCount; ++i)
                                {
                                    char buf[128];
                                    snprintf(buf, sizeof(buf), "%s", physSettings.layerNames[i].c_str());
                                    std::string label = std::format("Layer {}##layer_name_{}", i, i);
                                    if (ImGui::InputText(label.c_str(), buf, sizeof(buf)))
                                    {
                                        physSettings.layerNames[i] = buf;
                                        project->SetDirtyFlag(true);
                                    }
                                }
                                ImGui::EndChild();
                                ImGui::TreePop();
                            }

                            ImGui::Spacing();
                            if (ImGui::TreeNodeEx("Collision Matrix", ImGuiTreeNodeFlags_DefaultOpen))
                            {
                                const uint32_t count = physSettings.layerCount;
                                if (ImGui::BeginTable("##collision_matrix_table", count, tableFlags))
                                {
                                    // Layer label
                                    ImGui::TableSetupColumn("Layer", ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_NoReorder);

                                    // Iterate the second layer to compare with the first
                                    for (uint32_t j = 1; j < count; ++j)
                                        ImGui::TableSetupColumn(physSettings.layerNames[j].c_str(), columnFlags);

                                    constexpr int frozenRows = 2;
                                    constexpr int frozenCols = 1;
                                    ImGui::TableSetupScrollFreeze(frozenCols, frozenRows);

                                    ImGui::TableAngledHeadersRow();
                                    ImGui::TableHeadersRow();

                                    for (uint32_t row = 0; row < count; ++row)
                                    {
                                        ImGui::TableNextRow();
                                        ImGui::TableSetColumnIndex(0);
                                        ImGui::TextUnformatted(physSettings.layerNames[row].c_str());

                                        for (uint32_t column = 1; column < count; ++column)
                                        {
                                            ImGui::TableSetColumnIndex(column);
                                            bool canCollide = physSettings.CanLayersCollide(row, column);
                                            std::string checkId = std::format("##col_{}_{}", row, column);
                                            if (ImGui::Checkbox(checkId.c_str(), &canCollide))
                                            {
                                                physSettings.SetLayerCollision(row, column, canCollide);
                                                project->SetDirtyFlag(true);
                                            }
                                        }
                                    }
                                    ImGui::EndTable();
                                }
                                ImGui::TreePop();
                            }
                        }
                    }
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Server"))
                {
                    if (auto assetManager = AssetManager::GetInstance())
                    {
                        if (auto project = assetManager->LockActiveProject())
                        {
                            auto &serverSettings = project->GetInfo().serverSettings;

                            ImGui::Text("Server Configuration");
                            ImGui::Spacing();

                            int port = static_cast<int>(serverSettings.port);
                            if (ImGui::InputInt("Port", &port))
                            {
                                serverSettings.port = static_cast<uint16_t>(std::clamp(port, 1024, 65535));
                                project->SetDirtyFlag(true);
                            }

                            int tickRate = static_cast<int>(serverSettings.tickRate);
                            if (ImGui::SliderInt("Tick Rate (Hz)", &tickRate, 10, 128))
                            {
                                serverSettings.tickRate = static_cast<uint16_t>(tickRate);
                                project->SetDirtyFlag(true);
                            }

                            int snapshotRate = static_cast<int>(serverSettings.snapshotRate);
                            if (ImGui::SliderInt("Snapshot Rate (Hz)", &snapshotRate, 5, 64))
                            {
                                serverSettings.snapshotRate = static_cast<uint16_t>(snapshotRate);
                                project->SetDirtyFlag(true);
                            }

                            int maxClients = static_cast<int>(serverSettings.maxClients);
                            if (ImGui::SliderInt("Max Clients", &maxClients, 1, 256))
                            {
                                serverSettings.maxClients = static_cast<uint32_t>(maxClients);
                                project->SetDirtyFlag(true);
                            }

                            float handshakeTimeout = static_cast<float>(serverSettings.handshakeTimeoutSec);
                            if (ImGui::DragFloat("Handshake Timeout (s)", &handshakeTimeout, 0.25f, 1.0f, 60.0f, "%.2f"))
                            {
                                serverSettings.handshakeTimeoutSec = static_cast<double>(handshakeTimeout);
                                project->SetDirtyFlag(true);
                            }

                            float chatInterval = static_cast<float>(serverSettings.chatMinIntervalSec);
                            if (ImGui::DragFloat("Chat Interval Limit (s)", &chatInterval, 0.05f, 0.0f, 5.0f, "%.2f"))
                            {
                                serverSettings.chatMinIntervalSec = static_cast<double>(chatInterval);
                                project->SetDirtyFlag(true);
                            }

                            if (ImGui::Checkbox("Auto-Start Server on Load", &serverSettings.autoStart))
                            {
                                project->SetDirtyFlag(true);
                            }
                        }
                    }
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Editor / Undo"))
                {
                    if (auto assetManager = AssetManager::GetInstance())
                    {
                        if (auto project = assetManager->LockActiveProject())
                        {
                            auto &editorSettings = project->GetInfo().editorSettings;
                            CommandManager *cmdMgr = Application::GetCommandManager();

                            ImGui::Text("Undo / Redo History");
                            ImGui::Spacing();

                            int historyLimit = static_cast<int>(editorSettings.undoHistoryLimit);
                            ImGui::SetNextItemWidth(220.0f);
                            if (ImGui::SliderInt("Undo History Limit", &historyLimit, 10, 2000))
                            {
                                editorSettings.undoHistoryLimit = static_cast<uint32_t>(std::clamp(historyLimit, 10, 2000));
                                if (cmdMgr)
                                {
                                    cmdMgr->SetMaxHistory(editorSettings.undoHistoryLimit);
                                }
                                project->SetDirtyFlag(true);
                            }
                            ImGui::SameLine();
                            if (ImGui::Button("Reset (200)"))
                            {
                                editorSettings.undoHistoryLimit = 200;
                                if (cmdMgr)
                                {
                                    cmdMgr->SetMaxHistory(200);
                                }
                                project->SetDirtyFlag(true);
                            }

                            if (ImGui::Checkbox("Record Entity Selection in Undo", &editorSettings.recordSelectionUndo))
                            {
                                project->SetDirtyFlag(true);
                            }

                            ImGui::Spacing();
                            ImGui::SeparatorText("Estimated Memory Usage");

                            if (cmdMgr)
                            {
                                auto formatBytes = [](size_t bytes) -> std::string
                                {
                                    if (bytes >= 1024u * 1024u)
                                        return std::format("{:.2f} MB", bytes / (1024.0f * 1024.0f));
                                    if (bytes >= 1024u)
                                        return std::format("{:.1f} KB", bytes / 1024.0f);
                                    return std::format("{} B", bytes);
                                };

                                const CommandHistory &sceneHist = cmdMgr->GetSceneHistory();
                                const size_t sceneBytes = cmdMgr->GetSceneEstimatedMemoryUsage();
                                const size_t assetBytes = cmdMgr->GetAssetHistoriesEstimatedMemoryUsage();
                                const size_t totalBytes = sceneBytes + assetBytes;
                                const size_t sceneSteps = sceneHist.GetUndoCount() + sceneHist.GetRedoCount();
                                const size_t totalSteps = sceneSteps + cmdMgr->GetTotalAssetCommandCount();
                                const size_t avgStepBytes = totalSteps > 0 ? (totalBytes / totalSteps) : 2048u;
                                const size_t projectedBytes = avgStepBytes * static_cast<size_t>(editorSettings.undoHistoryLimit);

                                ImGui::Text("Scene History: Undo %zu | Redo %zu | Limit %u | Est. %s",
                                    sceneHist.GetUndoCount(), sceneHist.GetRedoCount(), editorSettings.undoHistoryLimit, formatBytes(sceneBytes).c_str());
                                ImGui::Text("Asset Editor Histories (%zu open): %zu steps | Est. %s",
                                    cmdMgr->GetAssetHistoryCount(), cmdMgr->GetTotalAssetCommandCount(), formatBytes(assetBytes).c_str());
                                ImGui::TextColored(ImVec4(0.45f, 0.90f, 0.55f, 1.0f), "Total Undo Memory: %s", formatBytes(totalBytes).c_str());
                                ImGui::TextDisabled("Projected Max Budget (%u steps): ~%s", editorSettings.undoHistoryLimit, formatBytes(projectedBytes).c_str());

                                const float usageRatio = editorSettings.undoHistoryLimit > 0
                                    ? std::clamp(static_cast<float>(sceneHist.GetUndoCount()) / static_cast<float>(editorSettings.undoHistoryLimit), 0.0f, 1.0f)
                                    : 0.0f;
                                const std::string barLabel = std::format("Scene Slots: {} / {}", sceneHist.GetUndoCount(), editorSettings.undoHistoryLimit);
                                ImGui::ProgressBar(usageRatio, ImVec2(280.0f, 0.0f), barLabel.c_str());

                                ImGui::Spacing();
                                if (ImGui::Button("Clear Scene History"))
                                {
                                    cmdMgr->ClearSceneHistory();
                                }
                                ImGui::SameLine();
                                if (ImGui::Button("Clear Asset Histories"))
                                {
                                    cmdMgr->ClearAllAssetHistories();
                                }
                                ImGui::SameLine();
                                if (ImGui::Button("Clear All History"))
                                {
                                    cmdMgr->ClearAll();
                                }

                                ImGui::Spacing();
                                if (ImGui::TreeNode("Scene Undo Stack"))
                                {
                                    const auto &stack = sceneHist.GetUndoStack();
                                    if (stack.empty())
                                    {
                                        ImGui::TextDisabled("No recorded scene undo steps.");
                                    }
                                    else
                                    {
                                        ImGui::BeginChild("##scene_undo_stack_list", ImVec2(-1.0f, 180.0f), ImGuiChildFlags_Borders);
                                        size_t displayIdx = stack.size();
                                        for (auto it = stack.rbegin(); it != stack.rend(); ++it)
                                        {
                                            if (*it)
                                            {
                                                ImGui::Text("#%zu: %s (%s)", displayIdx, (*it)->GetName().c_str(), formatBytes((*it)->GetMemoryUsage()).c_str());
                                            }
                                            --displayIdx;
                                        }
                                        ImGui::EndChild();
                                    }
                                    ImGui::TreePop();
                                }
                            }
                        }
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
                        float percentage = registeredCounts[type] > 0 ? static_cast<float>(count) / registeredCounts[type] * 100.0f : 0.0f;

                        // Color code by type
                        ImVec4 color = ImVec4(0.5f, 0.8f, 0.5f, 1.0f);
                        if (type == AssetType::Texture) color = ImVec4(0.9f, 0.5f, 0.5f, 1.0f);
                        else if (type == AssetType::StaticMesh) color = ImVec4(0.5f, 0.5f, 0.9f, 1.0f);
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
                const float loadRatio = totalRegistered > 0 ? static_cast<float>(totalLoaded) / totalRegistered : 0.0f;
                const auto memoryLoadText = std::format("Memory Load: {} %", static_cast<int>(loadRatio * 100));
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
                    "SkeletalAnimation",
                    "BlendSpace",
                    "Prefab",
                    "Audio",
                    "Skeleton"
                };

                const AssetType typeValues[] = {
                    AssetType::Invalid,
                    AssetType::Scene,
                    AssetType::Texture,
                    AssetType::Material,
                    AssetType::StaticMesh,
                    AssetType::SkeletalMesh,
                    AssetType::SkeletalAnimation,
                    AssetType::BlendSpace,
                    AssetType::Prefab,
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

            if (ImGui::BeginTable("asset_registry_table", 6, tableFlags))
            {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("Handle", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed, 140.0f, 0);
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 100.0f, 1);
                ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 80.0f, 2);
                ImGui::TableSetupColumn("Refs", ImGuiTableColumnFlags_WidthFixed, 50.0f, 3);
                ImGui::TableSetupColumn("Pins", ImGuiTableColumnFlags_WidthFixed, 50.0f, 4);
                ImGui::TableSetupColumn("Filepath", ImGuiTableColumnFlags_WidthStretch, 0.0f, 5);
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

                auto drawAssetRegistryRow = [&](AssetHandle handle, const AssetMetaData &metadata)
                {
                    ImGui::TableNextRow();

                    // Column 0: Handle (TreeNode)
                    ImGui::TableNextColumn();
                    ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                    ImGui::TreeNodeEx((void*)static_cast<uint64_t>(handle), treeNodeFlags, "%llu", static_cast<uint64_t>(handle));

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

                    // Column 4: Reserved
                    ImGui::TableNextColumn();
                    ImGui::Text("-");

                    // Column 5: Filepath
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
                };

                // Iterate through appropriate asset collection
                if (useFiltered)
                {
                    for (const auto &[handle, metadata] : filteredAssets)
                    {
                        drawAssetRegistryRow(handle, metadata);
                    }
                }
                else
                {
                    for (const auto &[handle, metadata] : assetRegistry)
                    {
                        drawAssetRegistryRow(handle, metadata);
                    }
                }
                ImGui::EndTable();
            }

            ImGui::End();
        }
    }

    void EditorLayer::UISceneRenderer()
    {
        if (!m_State.sceneRendererWindow)
            return;

        ImGui::Begin("Scene Renderer", &m_State.sceneRendererWindow);

        // Scene Render
        if (ImGui::TreeNodeEx("Render Settings", ImGuiTreeNodeFlags_Framed))
        {
            auto &renderSettings = m_SceneRenderer->sceneRenderSettings;
            bool settingsChanged = false;

            if (ImGui::TreeNodeEx("Visibility"))
            {
                UI::DrawCheckbox("Bounding Box", &renderSettings.showBoundingBox);
                UI::DrawCheckbox("Physics Collider", &renderSettings.showPhysicsCollider);
                UI::DrawCheckbox("Entity Names", &renderSettings.showEntityNames);
                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx("Render Properties", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (ImGui::TreeNodeEx("Sampling"))
                {
                    static const char *sampleMethodLabels[] = { "None", "TAA", "MSAA", "FXAA" };
                    int currentMethod = static_cast<int>(renderSettings.sampleMethod);
                    if (UI::DrawComboBox("Method", sampleMethodLabels, IM_ARRAYSIZE(sampleMethodLabels), &currentMethod))
                    {
                        m_SceneRenderer->SetSampleMethod(static_cast<SampleMethod>(currentMethod));
                        settingsChanged = true;
                    }

                    if (renderSettings.sampleMethod == SampleMethod::TAA)
                    {
                        if (UI::DrawFloatControl("Blend Factor", &renderSettings.taaProperties.blendFactor, 0.01f, 0.02f, 1.0f, 0.10f))
                            settingsChanged = true;
                    }
                    else if (renderSettings.sampleMethod == SampleMethod::MSAA)
                    {
                        if (UI::DrawIntControl("Samples", &renderSettings.msaaProperties.sampleCount, 1, 2, 8))
                        {
                            if (renderSettings.msaaProperties.sampleCount <= 2) renderSettings.msaaProperties.sampleCount = 2;
                            else if (renderSettings.msaaProperties.sampleCount <= 4) renderSettings.msaaProperties.sampleCount = 4;
                            else renderSettings.msaaProperties.sampleCount = 8;
                            settingsChanged = true;
                        }
                    }
                    else if (renderSettings.sampleMethod == SampleMethod::FXAA)
                    {
                        if (UI::DrawFloatControl("Subpixel Quality", &renderSettings.fxaaProperties.subpixQuality, 0.02f, 0.0f, 1.0f, 0.75f))
                            settingsChanged = true;
                        if (UI::DrawFloatControl("Edge Threshold", &renderSettings.fxaaProperties.edgeThreshold, 0.01f, 0.063f, 0.333f, 0.125f))
                            settingsChanged = true;
                        if (UI::DrawFloatControl("Min Threshold", &renderSettings.fxaaProperties.edgeThresholdMin, 0.005f, 0.015f, 0.0833f, 0.0417f))
                            settingsChanged = true;
                    }

                    if (settingsChanged)
                    {
                        m_State.editorRequestToResize = true;
                        m_State.gameplayRequestToResize = true;
                        if (m_ActiveProject)
                            renderSettings.SerializeToFile(m_ActiveProject->GetDirectory() / "GameSettings.ign");
                    }

                    ImGui::TreePop();
                }

                if (ImGui::TreeNodeEx("Render scale"))
                {
                    if (UI::DrawFloatControl("Factor", &renderSettings.renderScale, 0.025f, 0.25f, 1.0f, 1.0f))
                        settingsChanged = true;
                    if (UI::DrawButtonWithColumn("", "Apply"))
                    {
                        m_State.gameplayRequestToResize = true;
                        m_State.editorRequestToResize = true;
                        settingsChanged = true;
                    }
                    ImGui::TreePop();
                }

                if (ImGui::TreeNodeEx("Shading"))
                {
                    static const char *renderModeLabels[] = {
                        "Color", "Diffuse", "Normals", "Metallic", "Roughness",
                        "Subsurface", "Transmission", "Coat", "Fuzz",
                        "GI Diffuse", "GI Specular", "AO", "Probe Weights"
                    };
                    static const char *debugShadowLabels[] = { "Off", "Cascade Colors", "Shadow Term" };
                    static const char *tonemapLabels[] = { "Reinhard", "Uncharted2", "ACES Filmic" };
                    static const char *fillModeLabels[]{ "Solid", "Wireframe" };

                    int fillMode = static_cast<int>(renderSettings.fillMode);
                    if (UI::DrawComboBox("Fill Mode", fillModeLabels, IM_ARRAYSIZE(fillModeLabels), &fillMode))
                    {
                        m_SceneRenderer->SetFillMode(static_cast<nri::FillMode>(fillMode));
                    }

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

                if (ImGui::TreeNodeEx("Global Illumination & AO"))
                {
                    static const char *giModeLabels[] = { "Volume Default", "Off", "Fast", "Leak-Resistant" };
                    int giIdx = std::clamp(renderSettings.giModeOverride + 1, 0, 3);
                    if (UI::DrawComboBox("GI Mode", giModeLabels, IM_ARRAYSIZE(giModeLabels), &giIdx))
                    {
                        renderSettings.giModeOverride = giIdx - 1;
                        settingsChanged = true;
                    }

                    static const char *aoModeLabels[] = { "NONE", "SSAO", "GTAO" };
                    int aoIdx = static_cast<int>(renderSettings.scalability.ambientOcclusion);
                    if (UI::DrawComboBox("Ambient Occlusion", aoModeLabels, IM_ARRAYSIZE(aoModeLabels), &aoIdx))
                    {
                        renderSettings.scalability.ambientOcclusion = static_cast<AOMode>(std::clamp(aoIdx, 0, 2));
                        renderSettings.aoModeOverride = static_cast<int>(renderSettings.scalability.ambientOcclusion);
                        renderSettings.ApplyIndividualScalability();
                        settingsChanged = true;
                    }

                    static const char *shadowQualLabels[] = { "Low (512)", "Medium (1024)", "High (2048)", "Ultra (4096)", "Ultimate (8192)" };
                    int shadowQualIdx = std::clamp(static_cast<int>(renderSettings.shadowQuality), 0, 4);
                    if (UI::DrawComboBox("Shadow Quality", shadowQualLabels, IM_ARRAYSIZE(shadowQualLabels), &shadowQualIdx))
                    {
                        renderSettings.SyncFromDirectionalLight(static_cast<uint32_t>(shadowQualIdx));
                        settingsChanged = true;
                    }

                    if (UI::DrawCheckbox("Depth+Normal Prepass", &renderSettings.enableDepthPrepass))
                        settingsChanged = true;
                    if (UI::DrawCheckbox("Specular AA", &renderSettings.enableSpecularAA))
                        settingsChanged = true;
                    if (UI::DrawCheckbox("Bloom", &renderSettings.enableBloom))
                        settingsChanged = true;
                    if (UI::DrawCheckbox("Screen-Space Reflections (SSR)", &renderSettings.enableSSR))
                        settingsChanged = true;
                    if (renderSettings.enableSSR)
                    {
                        if (UI::DrawFloatControl("SSR Intensity", &renderSettings.ssrIntensity, 0.025f, 0.0f, 2.0f, 1.0f))
                            settingsChanged = true;
                        if (UI::DrawFloatControl("SSR Max Roughness", &renderSettings.ssrMaxRoughness, 0.025f, 0.05f, 1.0f, 0.6f))
                            settingsChanged = true;
                    }
                    if (UI::DrawCheckbox("Show GI Probes", &renderSettings.showGIProbes))
                        settingsChanged = true;

                    ImGui::TreePop();
                }

                ImGui::TreePop();
            }

            if (settingsChanged)
            {
                renderSettings.SyncToScene(m_ActiveScene.get(), m_ScenePanel ? &m_ScenePanel->GetViewportCamera() : nullptr);
                m_State.editorRequestToResize = true;
                m_State.gameplayRequestToResize = true;
                if (m_ActiveProject)
                {
                    renderSettings.SerializeToFile(m_ActiveProject->GetDirectory() / "GameSettings.ign");
                }
            }

            ImGui::TreePop();
        }

        // Render Stats
        if (ImGui::TreeNodeEx("Statistics", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen))
        {
            const auto &stats = Renderer::Stats;

            // Helper lambda to format bytes as KB or MB
            auto fmtBytes = [ ](size_t bytes, char *buf, size_t bufLen)
            {
                if (bytes >= 1024u * 1024u)
                    snprintf(buf, bufLen, "%.1f MB", bytes / (1024.0f * 1024.0f));
                else
                    snprintf(buf, bufLen, "%zu KB", bytes / 1024u);
            };
            char memBuf[32];

            ImGui::SeparatorText("GPU Pass Timings (ms)");
            const auto &timings = m_SceneRenderer->GetLastGPUTimingsMs();
            const auto &rs = m_SceneRenderer->sceneRenderSettings;
            const bool gtaoEnabled = (rs.aoModeOverride == static_cast<int>(AOMode::GTAO))
                || (rs.aoModeOverride < 0 && rs.scalability.ambientOcclusion == AOMode::GTAO);
            const bool prepassEnabled = rs.enableDepthPrepass && (rs.enableSSR || gtaoEnabled);

            auto isPinnedZone = [&](const std::string &name) -> bool
            {
                if (name == "ShadowPass" || name == "ColorPass" || name == "SkyPass" || name == "CompositePass")
                    return true;
                if (rs.enableSSR && (name == "SSR" || name == "SSRCopyPrevColor"))
                    return true;
                if (rs.enableBloom && name == "Bloom")
                    return true;
                if (gtaoEnabled && name == "GTAO")
                    return true;
                if (prepassEnabled && name == "DepthNormalPrepass")
                    return true;
                return false;
            };

            struct HeldZoneTiming
            {
                double lastMs = 0.0;
                int framesSinceSeen = 0;
            };
            static std::unordered_map<std::string, HeldZoneTiming> s_HeldTimings;
            for (auto &[_, entry] : s_HeldTimings)
            {
                entry.framesSinceSeen++;
            }
            for (const auto &[zoneName, ms] : timings)
            {
                s_HeldTimings[zoneName] = { ms, 0 };
            }

            static const std::array<const char *, 15> kCanonicalOrder = {
                "ShadowPass",
                "ProceduralSkyLUT",
                "SkyIBL",
                "DepthNormalPrepass",
                "GTAO",
                "SSR",
                "ColorPass",
                "SkyPass",
                "DebugPass",
                "MSAAResolve",
                "SSRCopyPrevColor",
                "Bloom",
                "UIPass",
                "CompositePass",
                "FXAA"
            };

            for (const char *zoneName : kCanonicalOrder)
            {
                if (isPinnedZone(zoneName) && !s_HeldTimings.contains(zoneName))
                {
                    s_HeldTimings.emplace(zoneName, HeldZoneTiming{ 0.0, 1 });
                }
            }

            for (auto it = s_HeldTimings.begin(); it != s_HeldTimings.end();)
            {
                if (!isPinnedZone(it->first) && it->second.framesSinceSeen > 120)
                    it = s_HeldTimings.erase(it);
                else
                    ++it;
            }

            if (s_HeldTimings.empty())
            {
                ImGui::Text("No GPU timestamps available");
            }
            else
            {
                double totalMs = 0.0;
                std::unordered_set<std::string> printedZones;
                for (const char *zoneName : kCanonicalOrder)
                {
                    auto it = s_HeldTimings.find(zoneName);
                    if (it != s_HeldTimings.end())
                    {
                        const double ms = (it->second.framesSinceSeen == 0) ? it->second.lastMs : 0.0;
                        totalMs += ms;
                        ImGui::Text("%s: %.3f ms", zoneName, ms);
                        printedZones.insert(zoneName);
                    }
                }

                std::vector<std::string> extraZones;
                for (const auto &[zoneName, _] : s_HeldTimings)
                {
                    if (!printedZones.contains(zoneName))
                        extraZones.push_back(zoneName);
                }
                std::sort(extraZones.begin(), extraZones.end());
                for (const std::string &zoneName : extraZones)
                {
                    const auto &entry = s_HeldTimings.at(zoneName);
                    const double ms = (entry.framesSinceSeen == 0) ? entry.lastMs : 0.0;
                    totalMs += ms;
                    ImGui::Text("%s: %.3f ms", zoneName.c_str(), ms);
                }

                ImGui::Separator();
                ImGui::Text("Total Timed: %.3f ms", totalMs);
            }

            ImGui::Spacing();
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

            ImGui::Spacing();
            ImGui::SeparatorText("Active Graphics Pipelines");
            if (stats.pipelineCounts.empty())
            {
                ImGui::Text("None");
            }
            else
            {
                for (const auto &[name, count] : stats.pipelineCounts)
                {
                    ImGui::Text("%s: %zu", name.c_str(), count);
                }
            }

            ImGui::Spacing();
            ImGui::SeparatorText("Active Binding Sets");
            if (stats.bindingSetCounts.empty())
            {
                ImGui::Text("None");
            }
            else
            {
                for (const auto &[name, count] : stats.bindingSetCounts)
                {
                    ImGui::Text("%s: %zu", name.c_str(), count);
                }
            }
            ImGui::TreePop();
        }

        // Shaders
        if (ImGui::TreeNodeEx("Shaders", ImGuiTreeNodeFlags_Framed))
        {
            for (auto &[key, shader] : Shader::GetShaderCache())
            {
                constexpr float buttonW = 24.0f;
                ImGui::PushID(key.filename.c_str());
                ImGui::SetNextItemWidth(ImGui::GetContentRegionMax().x - buttonW);
                ImGui::Text(shader->GetName().c_str());
                ImGui::SameLine();

                if (UI::Button("##recompile_shader", UI::ButtonVariant::Ghost, UI::ButtonSize::IconMd, ImVec2(buttonW, 0.0f), ICON_LC_ROTATE_CW))
                {
                    shader->Recompile();
                }

                ImGui::PopID();

            }
            ImGui::TreePop();
        }

        ImGui::End();
    }

    void EditorLayer::ProcessCameraViewportResize(ICamera *camera, const glm::uvec2 &desiredSize, bool &isResizing, int &resizingFrame, bool &requestToResize) const
    {
        if (!camera || desiredSize.x == 0 || desiredSize.y == 0)
        {
            return;
        }

        auto target = m_SceneRenderer ? m_SceneRenderer->GetRenderTarget(camera) : nullptr;
        const glm::uvec2 cameraSize = camera->GetViewportSize();
        const glm::uvec2 targetSize = target ? target->compositeRT->GetSize() : glm::uvec2(0);

        PostProcessing postProcessing = camera->postProcessing;
        if (m_ActiveScene)
        {
            if (Entity primaryCamera = m_ActiveScene->GetPrimaryCamera())
            {
                const auto &cc = primaryCamera.GetComponent<CameraComponent>();
                if (camera == &cc.camera)
                {
                    postProcessing = cc.camera.postProcessing;
                }
            }
        }
        const float globalScale = m_SceneRenderer ? m_SceneRenderer->sceneRenderSettings.renderScale : 1.0f;
        const float effectiveRenderScale = glm::clamp(globalScale, 0.25f, 1.0f);

        const uint32_t expectedRenderWidth = std::max(1u, static_cast<uint32_t>(std::round(static_cast<float>(desiredSize.x) * effectiveRenderScale)));
        const uint32_t expectedRenderHeight = std::max(1u, static_cast<uint32_t>(std::round(static_cast<float>(desiredSize.y) * effectiveRenderScale)));

        const glm::uvec2 sceneRTSize = target ? target->sceneRT->GetSize() : glm::uvec2(0);

        const bool cameraNeedsResize = (desiredSize.x != cameraSize.x || desiredSize.y != cameraSize.y);
        const bool compositeNeedsResize = (!target || desiredSize.x != targetSize.x || desiredSize.y != targetSize.y);
        const bool sceneRTNeedsResize = (!target || expectedRenderWidth != sceneRTSize.x || expectedRenderHeight != sceneRTSize.y);

        const bool needsResize = cameraNeedsResize || compositeNeedsResize || sceneRTNeedsResize || requestToResize;
        const bool immediateRequest = requestToResize;

        if (needsResize)
        {
            // A new viewport size restarts the debounce. Without this, a long
            // interactive drag crosses STABLE_RESIZE_FRAME and reallocates every
            // subsequent frame while the user is still resizing.
            if (cameraNeedsResize || requestToResize)
            {
                resizingFrame = 0;
            }
            camera->UpdateProjection(desiredSize.x, desiredSize.y);
            isResizing = true;
            requestToResize = false;
        }

        if (isResizing && m_SceneRenderer)
        {
            if (immediateRequest || !target || targetSize.x == 0 || targetSize.y == 0 || sceneRTSize.x == 0 || sceneRTSize.y == 0)
            {
                m_SceneRenderer->ResizeFramebuffer(camera, desiredSize.x, desiredSize.y);
                isResizing = false;
                resizingFrame = 0;
            }
            else if (resizingFrame++ >= m_State.STABLE_RESIZE_FRAME)
            {
                m_SceneRenderer->ResizeFramebuffer(camera, desiredSize.x, desiredSize.y);
                isResizing = false;
                resizingFrame = 0;
            }
        }
    }

}
