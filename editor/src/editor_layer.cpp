//Copyright (c) 2026 Evangelion Manuhutu | IGNITE STUDIO

#include "editor_layer.hpp"
#include "panels/scene_panel.hpp"
#include "panels/content_browser_panel.hpp"
#include "panels/asset_importer_panel.hpp"
#include "panels/asset_editor_panel.hpp"
#include "ext/editor_ui.hpp"
#include "ignite/core/application.hpp"
#include "ignite/core/command.hpp"
#include "ignite/graphics/renderer/renderer_2d.hpp"
#include "ignite/asset/asset.hpp"
#include "ignite/asset/asset_importer.hpp"
#include "ignite/scripting/script_engine.hpp"
#include "ignite/graphics/objects/shadow_map.hpp"
#include "ignite/core/platform_utils.hpp"
#include "ignite/core/profiler/profiler.hpp"
#include "ignite/imgui/imgui_nvrhi.hpp"
#include "ignite/scene/scene_manager.hpp"
#include "stb_image_write.h"

#include <algorithm>
#include <mutex>
#include <spdlog/spdlog.h>
#include <cmath>
#include <format>
#include <limits>
#include <ranges>
#include <string_view>
#include <unordered_set>
#include <QCursor>
#include <SDL3/SDL_dialog.h>

namespace ignite
{
    static EditorLayer *s_EditorLayerInstance = nullptr;

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

    EditorLayer::EditorLayer(const std::string &name)
        : Layer(name)
    {
        s_EditorLayerInstance = this;
    }

    EditorLayer::~EditorLayer()
    {
        if (s_EditorLayerInstance == this)
        {
            s_EditorLayerInstance = nullptr;
        }
    }

    EditorLayer *EditorLayer::GetInstance()
    {
        return s_EditorLayerInstance;
    }

    void EditorLayer::SetQtSceneHierarchyRefreshCallback(std::function<void()> callback)
    {
        m_QtSceneHierarchyRefreshCallback = std::move(callback);
    }

    void EditorLayer::NotifyQtSceneHierarchyChanged()
    {
        if (m_QtSceneHierarchyRefreshCallback)
        {
            Application::SubmitToMainThread([callback = m_QtSceneHierarchyRefreshCallback]()
            {
                if (callback)
                {
                    callback();
                }
            });
        }
    }

    void EditorLayer::ClearSelection(bool notifyQt)
    {
        std::scoped_lock lock(m_SelectionMutex);
        m_State.selectedEntities.clear();
        m_State.trackingSelectedEntity = UUID(0);

        if (notifyQt)
        {
            NotifyQtSceneHierarchyChanged();
        }
    }

    Entity EditorLayer::SetSelectedEntity(Entity entity, bool appendSelection, bool notifyQt)
    {
        if (!entity.IsValid())
        {
            ClearSelection(notifyQt);
            return {};
        }

        std::scoped_lock lock(m_SelectionMutex);

        if (!appendSelection)
        {
            m_State.selectedEntities.clear();

            m_State.selectedEntities[entity.GetUUID()] = entity;
        }
        else
        {
            if (auto it = m_State.selectedEntities.find(entity.GetUUID()); it != m_State.selectedEntities.end())
            {
                m_State.selectedEntities.erase(it);

                if (m_State.selectedEntities.empty())
                {
                    m_State.trackingSelectedEntity = UUID(0);

                    if (notifyQt)
                    {
                        NotifyQtSceneHierarchyChanged();
                    }
                    return {};
                }

                m_State.trackingSelectedEntity = m_State.selectedEntities.begin()->first;
                if (notifyQt)
                {
                    NotifyQtSceneHierarchyChanged();
                }
                return m_State.selectedEntities.begin()->second;
            }

            m_State.selectedEntities[entity.GetUUID()] = entity;
        }

        m_State.trackingSelectedEntity = entity.GetUUID();

        if (notifyQt)
        {
            NotifyQtSceneHierarchyChanged();
        }

        return entity;
    }

    void EditorLayer::SetSelectedEntities(const std::vector<Entity> &entities, UUID trackingEntity, bool notifyQt)
    {
        std::scoped_lock lock(m_SelectionMutex);
        m_State.selectedEntities.clear();
        m_State.trackingSelectedEntity = UUID(0);

        for (const Entity &entity : entities)
        {
            if (!entity.IsValid())
            {
                continue;
            }

            m_State.selectedEntities[entity.GetUUID()] = entity;
        }

        if (!m_State.selectedEntities.empty())
        {
            if (trackingEntity != UUID(0) && m_State.selectedEntities.contains(trackingEntity))
            {
                m_State.trackingSelectedEntity = trackingEntity;
            }
            else
            {
                m_State.trackingSelectedEntity = m_State.selectedEntities.begin()->first;
            }
        }

        if (notifyQt)
        {
            NotifyQtSceneHierarchyChanged();
        }
    }

    Entity EditorLayer::GetSelectedEntity() const
    {
        std::scoped_lock lock(m_SelectionMutex);
        if (m_State.trackingSelectedEntity != UUID(0))
        {
            if (auto it = m_State.selectedEntities.find(m_State.trackingSelectedEntity); it != m_State.selectedEntities.end())
            {
                return it->second;
            }
        }

        return m_State.selectedEntities.empty() ? Entity{} : m_State.selectedEntities.begin()->second;
    }

    std::unordered_map<UUID, Entity> &EditorLayer::GetSelectedEntities()
    {
        std::scoped_lock lock(m_SelectionMutex);
        return m_State.selectedEntities;
    }

    size_t EditorLayer::GetSelectedEntityCount() const
    {
        std::scoped_lock lock(m_SelectionMutex);
        return m_State.selectedEntities.size();
    }

    UUID EditorLayer::GetTrackingSelectedEntity() const
    {
        std::scoped_lock lock(m_SelectionMutex);
        return m_State.trackingSelectedEntity;
    }

    void EditorLayer::DuplicateSelectedEntities()
    {
        const auto selectedEntities = GetSelectedEntities();
        for (const Entity &entity : selectedEntities | std::views::values)
        {
            if (entity.IsValid())
            {
                SceneManager::DuplicateEntity(m_ActiveScene.get(), entity);
            }
        }
    }

    void EditorLayer::OnAttach()
    {
        Layer::OnAttach();

        m_Device = DeviceManager::GetInstance()->GetDevice();

        auto *app = Application::GetInstance();

        m_ScenePanel = new ScenePanel("Scene Panel", this);
        app->PushLayer(m_ScenePanel);

        m_AssetImporterPanel = new AssetImporterPanel("AssetImporter Panel", this);
        m_AssetEditorPanel = new AssetEditorPanel("Animation Panel", this);
        app->PushLayer(m_AssetImporterPanel);
        app->PushLayer(m_AssetEditorPanel);
        AddContentBrowserPanel();
        
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

        m_ContentBrowserPanels.clear();
        m_ContentBrowserPanelsPendingRemoval.clear();
        m_ContentBrowserPanel = nullptr;
        ContentBrowserPanel::ReleaseSharedResources();

        m_ActiveProject.reset();
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

        ProcessPendingFileLoading();

        for (ContentBrowserPanel *contentBrowserPanel : m_ContentBrowserPanels)
        {
            if (contentBrowserPanel && contentBrowserPanel->IsOpen())
            {
                contentBrowserPanel->OnUpdate(deltaTime);
            }
        }

        // update panels
        if (m_ActiveScene)
        {
            m_SceneRenderer->OnUpdate(deltaTime);

            // multi select entity
            m_State.multiSelect = Input::IsModifierPressed(KeyMod::LeftShift);

            switch (m_State.sceneState)
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
        
        m_SceneRenderer->HandleNuklearEvent(&modifiedEvent);
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
                    auto &cam = m_ScenePanel->GetViewportCamera();
                    auto &tr = entity.GetComponent<TransformComponent>();

                    glm::vec3 focusCenter = tr.translation;
                    glm::vec3 halfExtents = glm::abs(tr.scale) * 0.5f;

                    auto tryFocusFromAABB = [&](const AABB &meshAABB)
                    {
                        const glm::vec3 corners[8] =
                        {
                            { meshAABB.min.x, meshAABB.min.y, meshAABB.min.z },
                            { meshAABB.max.x, meshAABB.min.y, meshAABB.min.z },
                            { meshAABB.min.x, meshAABB.max.y, meshAABB.min.z },
                            { meshAABB.max.x, meshAABB.max.y, meshAABB.min.z },
                            { meshAABB.min.x, meshAABB.min.y, meshAABB.max.z },
                            { meshAABB.max.x, meshAABB.min.y, meshAABB.max.z },
                            { meshAABB.min.x, meshAABB.max.y, meshAABB.max.z },
                            { meshAABB.max.x, meshAABB.max.y, meshAABB.max.z },
                        };

                        const glm::mat4 worldTransform = tr.GetWorldMatrix();
                        glm::vec3 worldMin(std::numeric_limits<float>::max());
                        glm::vec3 worldMax(std::numeric_limits<float>::lowest());

                        for (const glm::vec3 &corner : corners)
                        {
                            const glm::vec4 worldPos = worldTransform * glm::vec4(corner, 1.0f);
                            worldMin = glm::min(worldMin, glm::vec3(worldPos));
                            worldMax = glm::max(worldMax, glm::vec3(worldPos));
                        }

                        focusCenter = (worldMin + worldMax) * 0.5f;
                        halfExtents = glm::abs(worldMax - worldMin);
                    };

                    if (entity.HasComponent<MeshComponent>())
                    {
                        const auto &smc = entity.GetComponent<MeshComponent>();
                        if (smc.handle != AssetHandle(0))
                        {
                            if (Ref<Mesh> mesh = m_ActiveProject->GetAsset<Mesh>(smc.handle))
                            {
                                tryFocusFromAABB(mesh->aabb);
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
                if (!Input::IsMouseButtonPressed(Mouse::ButtonRight))
                    m_ScenePanel->SetGizmoOperation(GizmoOperation::BOUND_SIZING_2D);
                break;
            }
            case Key::T:
            {
                if (!Input::IsMouseButtonPressed(Mouse::ButtonRight))
                    m_ScenePanel->SetGizmoOperation(GizmoOperation::TRANSLATE);
                break;
            }
            case Key::R:
            {
                if (!Input::IsMouseButtonPressed(Mouse::ButtonRight))
                    m_ScenePanel->SetGizmoOperation(GizmoOperation::ROTATE);
                break;
            }
            case Key::E:
            {
                if (!Input::IsMouseButtonPressed(Mouse::ButtonRight))
                    m_ScenePanel->SetGizmoOperation(GizmoOperation::SCALE);
                break;
            }
            case Key::F5:
            {
                (m_State.sceneState == State::SceneEdit || m_State.sceneState == State::SceneSimulate)
                    ? OnScenePlay()
                    : OnSceneStop();

                break;
            }
            case Key::F6:
            {
                (m_State.sceneState == State::SceneEdit || m_State.sceneState == State::ScenePlay)
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
            m_SceneRenderer->ClearSelectedEntities();

            const auto selectedEntities = GetSelectedEntities();
            for (const Entity &entity : selectedEntities | std::views::values)
            {
                if (entity.IsValid())
                {
                    m_SceneRenderer->SetSelectedEntity(entity);
                }
            }
        }

        // Resizing editor camera
        ICamera *editCamera = &m_ScenePanel->GetViewportCamera();
        if (editCamera)
        {
            // Resize Edit Viewport Framebuffer
            const glm::vec2 framebufferSize = m_SceneRenderer->GetCompositeRT()->GetSize();
            const glm::vec2 currentViewportSize = editCamera->viewportSize;
            const glm::vec2 desiredSize = glm::max(glm::vec2(0.0f), currentViewportSize);
            const bool framebufferNeedsResize = framebufferSize.x != desiredSize.x || framebufferSize.y != desiredSize.y;

            // Resize camera
            if (framebufferNeedsResize && desiredSize.x > 0.0f && desiredSize.y > 0.0f)
            {
                m_ScenePanel->GetViewportCamera().UpdateProjection(desiredSize.x, desiredSize.y);
                m_State.editorResizing = true;
            }

            // Resize framebuffer when in stable frame
            if (m_State.editorResizing && desiredSize.x > 0.0f && desiredSize.y > 0.0f)
            {
                if (m_State.editorResizingFrame++ >= m_State.STABLE_RESIZE_FRAME)
                {
                    m_SceneRenderer->ResizeFramebuffer(static_cast<uint32_t>(desiredSize.x), static_cast<uint32_t>(desiredSize.y));
                    m_State.editorResizing = false;
                    m_State.editorResizingFrame = 0;
                }
            }
        }

        // Resizing gameplay camera
        if (Entity primaryCam = m_ActiveScene->GetPrimaryCamera())
        {
            auto &cc = primaryCam.GetComponent<CameraComponent>();
            ICamera *gameCamera = &cc.camera;
            {
                // Resize Game Viewport Framebuffer
                const glm::vec2 framebufferSize = m_SceneRenderer->GetGameplayCompositeRT()->GetSize();
                const glm::vec2 currentViewportSize = gameCamera->viewportSize;
                const glm::vec2 desiredSize = glm::max(glm::vec2(0.0f), currentViewportSize);
                const bool framebufferNeedsResize = framebufferSize.x != desiredSize.x || framebufferSize.y != desiredSize.y;

                if (framebufferNeedsResize && desiredSize.x > 0.0f && desiredSize.y > 0.0f)
                {
                    gameCamera->UpdateProjection(desiredSize.x, desiredSize.y);
                    m_State.gameplayResizing = true;
                }

                if (m_State.gameplayResizing && desiredSize.x > 0.0f && desiredSize.y > 0.0f)
                {
                    if (m_State.gameplayResizingFrame++ >= m_State.STABLE_RESIZE_FRAME)
                    {
                        m_SceneRenderer->ResizeGameplayFramebuffer(static_cast<uint32_t>(desiredSize.x), static_cast<uint32_t>(desiredSize.y));
                        m_State.gameplayResizing = false;
                        m_State.gameplayResizingFrame = 0;
                    }
                }
            }
        }

        // Render to Edit Viewport
        if (m_ScenePanel->m_Data.sceneViewportEditorVisible)
        {
            switch (m_State.sceneState)
            {
                case State::SceneSimulate:
                case State::SceneEdit:
                case State::ScenePlay:
                {
                    ICamera *editCamera = &m_ScenePanel->GetViewportCamera();
                    if (editCamera)
                    {
                        IGN_PROFILE_SCOPE("SceneRenderer::RenderEditorTo");
                        m_SceneRenderer->RenderEditorTo(editCamera);
                    }
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
                    m_SceneRenderer->RenderGameplayTo(gameCamera);
                }
            }
        }

        m_Cmd->open();

        if (m_State.takeScreenshot)
        {
            auto sceneTexture = m_SceneRenderer->GetCompositeRT()->GetColorAttachment(0)->GetHandle();
            nvrhi::TextureDesc stagingDesc = sceneTexture->getDesc();
            stagingDesc.initialState = nvrhi::ResourceStates::CopyDest;
            m_ScreenshotStagingTexture = m_Device->createStagingTexture(stagingDesc, nvrhi::CpuAccessMode::Read);
            m_Cmd->copyTexture(m_ScreenshotStagingTexture, nvrhi::TextureSlice(), sceneTexture, nvrhi::TextureSlice());
        }

        m_Cmd->close();
        Application::SubmitWorkerCommandList(m_Cmd);

        if (m_State.takeScreenshot)
        {
            // Map and read the pixel data
            size_t rowPitch = 0;
            if (void *mappedData = m_Device->mapStagingTexture(m_ScreenshotStagingTexture, nvrhi::TextureSlice(), nvrhi::CpuAccessMode::Read, &rowPitch))
            {
                m_ScreenshotWidth = static_cast<int>(m_ScreenshotStagingTexture->getDesc().width);
                m_ScreenshotHeight = static_cast<int>(m_ScreenshotStagingTexture->getDesc().height);

                size_t packedStride = m_ScreenshotWidth * 4;
                m_ScreenshotPixelData.resize(m_ScreenshotHeight * packedStride);

                const auto src = static_cast<const uint8_t *>(mappedData);
                uint8_t *dst = m_ScreenshotPixelData.data();

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
            m_State.takeScreenshot = false;
        }
    }

    void EditorLayer::OnGuiRender()
    {
        IGN_PROFILE_FUNCTION();

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

            ImGui::EndMenuBar();
        }

        if (m_State.popupNewProjectModal)
        {
            ImGui::OpenPopup("New Project");
            m_State.popupNewProjectModal = false;
        }
        
        {
            // RENDER TOOL BAR
            ImGui::BeginChild("##toolbar_child", {0.0f, 32.0f }, 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar);
            m_ScenePanel->RenderToolbar();
            ImGui::EndChild();
        }
        
        UIProjectCreation();

        // Reserve space for status bar
        constexpr float statusBarHeight = 32.0f;
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

        // Status bar at bottom
        ImGui::BeginChild("##status_bar", { 0.0f, statusBarHeight }, 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar);
        ImGui::BeginDisabled((GetOpenContentBrowserCount() + m_PendingContentBrowserPanelsToAdd) >= 4);
        if (ImGui::SmallButton("+ Content Browser"))
        {
            ++m_PendingContentBrowserPanelsToAdd;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::Text("Version: %s", ENGINE_VERSION);
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
                        ImGui::TextUnformatted(log.message.c_str());
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
        UISettings();
    }

    void EditorLayer::SetActiveScene(const Ref<Scene> &scene)
    {
        if (m_ActiveScene == scene)
        {
            return;
        }

        ClearSelection(false);

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
        NotifyQtSceneHierarchyChanged();
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

        if (m_State.sceneState == State::ScenePlay)
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
        AssetHandle openSceneHandle = m_ActiveProject->GetAssetManager()->GetAssetHandle(filepath);

        if (m_CurrentSceneHandle == openSceneHandle)
            return;

        m_CurrentSceneHandle = openSceneHandle;

        if (m_EditorScene)
        {
            m_EditorScene->OnStop();
        }

        if (m_State.sceneState == State::ScenePlay)
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

            m_ActiveProject->GetAssetManager()->ClearAllLoadedAssets();
        }

        if (const Ref<Project> openedProject = Project::Deserialize(filepath))
        {
            m_ActiveProject = openedProject;
            m_CurrentProjectFilepath = filepath;

            // Reload project files
            ReloadContentBrowserPanels();

            // Get Project default scene (use immediate load for synchronous path)
            if (m_ActiveProject->GetInfo().defaultSceneHandle != AssetHandle(0))
            {
                // Use GetAssetImmediate since we're on main thread and need synchronous load
                if (Ref<Scene> activeScene = m_ActiveProject->GetAssetImmediate<Scene>(m_ActiveProject->GetInfo().defaultSceneHandle))
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

    bool EditorLayer::OnMouseMovedEvent(MouseMovedEvent &event)
    {
        return false;
    }

    void EditorLayer::OnScenePlay()
    {
        if (m_EditorScene)
        {
            m_EditorScene->OnStop();
        }

        m_ScenePanel->SetGizmoOperation(GizmoOperation::NONE);

        if (m_State.sceneState != State::SceneEdit)
            OnSceneStop();

        m_State.sceneState = State::ScenePlay;

        // copy initial components to new scene
        SetActiveScene(SceneManager::Copy(m_EditorScene));
        m_ActiveScene->OnStart();
    }

    void EditorLayer::OnSceneStop()
    {
        m_State.sceneState = State::SceneEdit;

        m_ActiveScene->OnStop();
        SetActiveScene(m_EditorScene);
    }

    void  EditorLayer::OnSceneSimulate()
    {
        if (m_State.sceneState != State::SceneEdit)
            OnSceneStop();

        m_State.sceneState = State::SceneSimulate;

        // copy initial components to new scene
        SetActiveScene(SceneManager::Copy(m_EditorScene));
        m_ActiveScene->OnStart();
    }

    void EditorLayer::OnSceneSaveFileSelected(void *userData, const char *const *filelist, int filter)
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
            if (!filepath.ends_with(".ixscene"))
            {
                filepath += ".ixscene";
            }

            editor->m_CurrentSceneFilePath = filepath;

            PendingFileLoading pf = { PendingFileLoading::Save, AssetMetaData(filepath, AssetType::Scene), userData };
            editor->m_PendingFileLoading.push(pf);
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
            PendingFileLoading pf = { PendingFileLoading::Open, AssetMetaData(filepath, AssetType::Scene), userData };
            editor->m_PendingFileLoading.push(pf);
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

            PendingFileLoading pf = { PendingFileLoading::Open, AssetMetaData(filepath, AssetType::Project), userData };
            editor->m_PendingFileLoading.push(pf);
        }
    }

    void EditorLayer::OnProjectOpenFileSelected(void *userData, const char *const *filelist, int filter)
    {
        EditorLayer *editor = (EditorLayer *)userData;

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
            PendingFileLoading pf = { PendingFileLoading::Open, AssetMetaData(filepath, AssetType::Project), userData };
            editor->m_PendingFileLoading.push(pf);
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

            EditorLayer *editor = static_cast<EditorLayer *>(userData);
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

    void EditorLayer::OnProjectFolderSelected(void *userData, const char *const *filelist, int filter)
    {
        if (filelist == nullptr || *filelist == nullptr) return;

        std::string filepath = filelist[0];
        if (!filepath.empty())
        {
            EditorLayer *editor = static_cast<EditorLayer *>(userData);
            editor->m_State.projectCreateInfo.filepath = std::filesystem::path(filepath) / editor->m_State.projectCreateInfo.name; // Append project name
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
                        AssetHandle sceneHandle = m_ActiveProject->GetAssetManager()->GetAssetHandle(filepath);

                        if (m_CurrentSceneHandle == sceneHandle)
                            break;

                        m_CurrentSceneHandle = sceneHandle;

                        // Submit heavy I/O work to asset worker
                        m_ActiveProject->GetAssetManager()->SubmitJob([this, filepath, sceneHandle]()
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

                                    if (m_State.sceneState == State::ScenePlay)
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
                                        m_ActiveProject->GetAssetManager()->UnloadUnusedAssets();
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

                        Ref<Project> loadedProject = Project::Deserialize(filepath);
                        if (loadedProject)
                        {
                            // Submit UI update back to main thread
                            Application::SubmitToMainThread([this, loadedProject, filepath]() mutable
                            {
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
                                    m_ActiveProject->GetAssetManager()->ClearAllLoadedAssets();
                                }


                                m_ActiveProject = loadedProject;
                                m_CurrentProjectFilepath = filepath;

                                // Reload content browser
                                ReloadContentBrowserPanels();

                                // Load default scene
                                if (m_ActiveProject->GetInfo().defaultSceneHandle != AssetHandle(0))
                                {
                                    if (Ref<Scene> activeScene = m_ActiveProject->GetAssetImmediate<Scene>(m_ActiveProject->GetInfo().defaultSceneHandle))
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

                        m_ActiveProject->GetAssetManager()->SubmitJob([this, filepath]()
                        {
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
                m_State.projectCreateInfo.filepath = std::filesystem::path(pathBuf);
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
            // sanitize name
            while (m_State.projectCreateInfo.name.find(' ') != std::string::npos)
            {
                const size_t spacePos = m_State.projectCreateInfo.name.find(' ');
                m_State.projectCreateInfo.name.replace(spacePos, 1, "");
            }

            m_State.projectCreateInfo.filepath /= (m_State.projectCreateInfo.name + ".ixproj");

            if (Ref<Project> newProject = Project::Create(m_State.projectCreateInfo))
            {
                m_ActiveProject = newProject;

                // Serialize
                m_ActiveProject->Serialize(m_State.projectCreateInfo.filepath);

                // Reload content browser
                ReloadContentBrowserPanels();


                if (m_ActiveProject->GetInfo().defaultSceneHandle != AssetHandle(0))
                {
                    if (Ref<Scene> activeScene = m_ActiveProject->GetAssetImmediate<Scene>(m_ActiveProject->GetInfo().defaultSceneHandle))
                    {
                        m_EditorScene = SceneManager::Copy(activeScene);
                        m_EditorScene->SetDirtyFlag(false);

                        SetActiveScene(m_EditorScene);

                        AssetMetaData metadata = m_ActiveProject->GetAssetManager()->GetMetaData(activeScene->handle);
                        auto scenePath = m_ActiveProject->GetProjectFilepath(metadata.filepath);
                        m_CurrentSceneFilePath = scenePath;
                    }
                }
                else
                {
                    NewScene();
                }

                // clear modal inputs
                m_State.projectCreateInfo.filepath.clear();
                m_State.projectCreateInfo.name.clear();
                memset(nameBuffer, 0, sizeof(nameBuffer));

                ImGui::CloseCurrentPopup();
            }
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
            if (ImGui::Begin("Settings", &m_State.settingsWindow))
            {
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
            }
            ImGui::End(); // !settings window
        }

        if (m_State.assetRegistryWindow)
        {
            AssetRegistry assetRegistry = m_ActiveProject->GetAssetManager()->GetAssetAssetRegistry();
            const auto &loadedAssets = m_ActiveProject->GetAssetManager()->GetLoadedAssets();

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
            ImGui::Begin("Asset Registry & Memory Monitor", &m_State.assetRegistryWindow);
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
                    AssetType type = m_ActiveProject->GetAssetManager()->GetAssetType(handle);
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
                    float percentage = registeredCounts[type] > 0 ? (float)count / registeredCounts[type] * 100.0f : 0.0f;

                    // Color code by type
                    ImVec4 color = ImVec4(0.5f, 0.8f, 0.5f, 1.0f);
                    if (type == AssetType::Texture) color = ImVec4(0.9f, 0.5f, 0.5f, 1.0f);
                    else if (type == AssetType::Mesh) color = ImVec4(0.5f, 0.5f, 0.9f, 1.0f);
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
                    AssetType::Material, AssetType::Mesh, AssetType::Audio, AssetType::Skeleton
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
                    m_ActiveProject->GetAssetManager()->UnloadUnusedAssets();
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
                        std::filesystem::absolute(m_ActiveProject->GetProjectFilepath(metadata.filepath)).generic_string());

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
                            displayPath = std::filesystem::absolute(m_ActiveProject->GetProjectFilepath(metadata.filepath)).generic_string();
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
                            displayPath = std::filesystem::absolute(m_ActiveProject->GetProjectFilepath(metadata.filepath)).generic_string();
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
