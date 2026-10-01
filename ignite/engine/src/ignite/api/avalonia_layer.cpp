// Copyright (c) 2026 Evangelion Manuhutu

#include "avalonia_layer.h"

#include "ignite/core/input/mouse_event.hpp"
#include "ignite/core/input/key_event.hpp"
#include "ignite/core/input/input_system.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/application.hpp"
#include "ignite/imgui/imgui_layer.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/scene/entity.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/graphics/renderer/scene_renderer.hpp"
#include "ignite/scene/scene_manager.hpp"
#include "ignite/asset/asset_worker.hpp"
#include "ignite/serializer/scene_serializer.hpp"
#include "ignite/project/project.hpp"
#include "ignite/scene/prefab.hpp"
#include "ignite/core/command.hpp"
#include "ignite/scene/component_property_command.hpp"
#include "ignite/graphics/ui/game_ui_system.hpp"

#ifndef IMGUI_DEFINE_MATH_OPERATORS
    #define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include <imgui.h>
#include <imgui_internal.h>
#include <glm/glm.hpp>

#include <chrono>
#include <algorithm>
#include <SDL3/SDL_keyboard.h>

namespace ignite
{
    AvaloniaLayer::AvaloniaLayer(uint32_t width, uint32_t height)
        : Layer("AvaloniaLayer")
        , m_Width(width)
        , m_Height(height)
        , m_EditorCamera("Embedded Viewport Camera")
    {
        m_EditorCamera.SetTarget(glm::vec3(0.0f));
        m_EditorCamera.SetDistance(24.0f);
        m_EditorCamera.yaw = glm::radians(45.0f);
        m_EditorCamera.pitch = glm::radians(25.0f);
        m_EditorCamera.farPlane = 1000.0f;
        m_EditorCamera.UpdateSphericalPosition();
        m_EditorCamera.UpdateView();
        m_EditorCamera.UpdateProjection(width, height);
        m_EditorCamera.SetNavigationMode(EditorCamera::NavigationMode::Orbit);
        m_Gizmo.SetOperation(ImGuizmo::OPERATION::NONE);

        if (auto *imguiLayer = Application::GetInstance()->GetImGuiLayer())
        {
            imguiLayer->SetBlock(false);
        }
    }

    void AvaloniaLayer::OnAttach()
    {
        m_SceneRenderer = CreateRef<SceneRenderer>();
    }

    void AvaloniaLayer::OnDetach()
    {
        m_SceneRenderer = nullptr;
    }

    void AvaloniaLayer::Resize(uint32_t width, uint32_t height)
    {
        m_Width = width;
        m_Height = height;
        m_EditorCamera.UpdateProjection(width, height);
    }

    void AvaloniaLayer::OnUpdate(float deltaTime)
    {
        // Update Scene
        if (Ref<Scene> activeScene = GetActiveScene())
        {
            switch (activeScene->GetState())
            {
                case ESceneState::Simulate:
                case ESceneState::Play:
                {
                    activeScene->OnFixedUpdateRuntimeSimulate();
                    activeScene->OnUpdateRuntimeSimulate(deltaTime);
                    break;
                }
                case ESceneState::Stop:
                {
                    activeScene->OnFixedUpdateEdit();
                    activeScene->OnUpdateEdit(deltaTime);
                    break;
                }
            }

            m_EditorCamera.UpdateMouseState();

            auto *imguiLayer = Application::GetInstance()->GetImGuiLayer();
            const bool blockMouse = imguiLayer ? imguiLayer->WantCaptureMouse() : false;
            const bool blockKeyboard = imguiLayer ? imguiLayer->WantCaptureKeyboard() : false;
            const bool gizmoEnabled = m_GizmoOperation != GizmoOperation::NONE && !m_SelectedEntities.empty();
            const bool isGizmoManipulating = gizmoEnabled && (m_Gizmo.IsManipulating() || m_IsGizmoManipulating);

            // Camera navigation: user can orbit (RMB) or pan (MMB).
            // Navigation can only begin if mouse is not blocked by ImGui and gizmo is not manipulating.
            const bool wantsNavigate = m_EditorCamera.mouse.rightButtonDown || m_EditorCamera.mouse.middleButtonDown;
            if (!m_IsCameraNavigating)
            {
                if (wantsNavigate && !blockMouse && !isGizmoManipulating)
                {
                    m_IsCameraNavigating = true;
                }
            }
            else
            {
                if (!wantsNavigate || isGizmoManipulating)
                {
                    m_IsCameraNavigating = false;
                }
            }

            if ((!blockMouse || m_IsCameraNavigating) && !isGizmoManipulating)
            {
                switch (m_EditorCamera.GetNavigationMode())
                {
                case EditorCamera::NavigationMode::Fly:
                    if (!blockKeyboard)
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

            if (!isGizmoManipulating)
            {
                m_EditorCamera.ApplyInertia(deltaTime);
            }
            m_EditorCamera.UpdateCameraPosition(deltaTime);
            m_EditorCamera.UpdateView();
        }
    }

    void AvaloniaLayer::OnRender(nvrhi::IFramebuffer *mainFramebuffer)
    {
        Ref<Scene> activeScene = GetActiveScene();
        if (!m_SceneRenderer || !activeScene)
            return;

        ICamera *cameraToUse = &m_EditorCamera;
        m_SceneRenderer->ResizeFramebuffer(cameraToUse, m_Width, m_Height);
        m_SceneRenderer->SetActiveScene(activeScene);
        m_SceneRenderer->BeginFrame();

        constexpr bool drawDebug = true;
        FrameContext *frameContext = Renderer::GetCurrentFrameContext();
        m_SceneRenderer->Render(cameraToUse, frameContext, drawDebug, mainFramebuffer);
    }

    void AvaloniaLayer::OnGuiRender()
    {
        if (m_ActiveScene)
        {
            DrawGizmo();
            ImGui::ShowDemoWindow();
        }
    }

    void AvaloniaLayer::SetActiveScene(const Ref<Scene> &scene)
    {
        if (!scene || m_ActiveScene == scene)
            return;

        // Update active scene
        if (auto project = Project::GetActive())
        {
            scene->PreloadReferencedAssets();
        }

        // Clear references in all systems before changing active scene
        m_SceneRenderer->SetActiveScene(nullptr); // null will clear
        if (auto project = Project::GetActive())
        {
            project->SetActiveScene(scene);
        }

        // Set new scene
        m_ActiveScene = scene;

        // Update all systems with new scene
        m_SceneRenderer->SetActiveScene(scene);
        GameUISystem::SetSceneContext(scene.get());
    }

    bool AvaloniaLayer::SceneNew()
    {
        SceneStop();

        m_CurrentSceneFilepath.clear();
        m_CurrentSceneHandle = AssetHandle(0);

        // Clear active scene first to release references in renderer
        SetActiveScene(nullptr);

        // Reset scenes - this should trigger destructor
        m_EditorScene.reset();
        m_ActiveScene.reset();

        // Unload unused assets (assets not referenced by anything else)
        if (m_ActiveProject)
        {
            AssetManager::GetInstance()->UnloadUnusedAssets();

            // Create new editor scene
            m_EditorScene = Scene::Create(m_ActiveProject.get());
            m_EditorScene->handle = AssetHandle();

            // Set as active scene
            SetActiveScene(m_EditorScene);
            return true;
        }

        LOG_ASSERT(m_ActiveProject, "Invalid project!");
        LOG_ASSERT(m_ActiveScene, "Failed to create new scene!");

        return false;
    }

    bool AvaloniaLayer::SceneOpen(const std::filesystem::path &filepath)
    {
        const AssetHandle sceneHandle = AssetManager::GetInstance()->GetAssetHandle(filepath);
        if (m_CurrentSceneHandle == sceneHandle && m_CurrentSceneHandle != AssetHandle(0))
        {
            LOG_WARN("Dismiss opening current scene {0}", filepath.generic_string());
            return false;
        }

        m_CurrentSceneHandle = sceneHandle;

        // Submit heavy I/O work to asset worker
        AssetWorker::SubmitJob([this, filepath, sceneHandle]()
        {
            AssetWorker::ReportStatus(std::format("Loading scene {}...", filepath.filename().string()), 0.5f);

            // Load scene on worker thread (I/O happens here)
            Ref<Scene> loadedScene = SceneSerializer::Deserialize(filepath, m_ActiveProject.get());
            if (loadedScene)
            {
                loadedScene->handle = sceneHandle;

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

                    // Reset old scene
                    m_EditorScene.reset();
                    m_ActiveScene.reset();

                    // Unload unused assets
                    if (m_ActiveProject)
                    {
                        AssetManager::GetInstance()->UnloadUnusedAssets();
                    }

                    // Copy and activate new scene
                    m_EditorScene = SceneManager::Copy(loadedScene);
                    m_EditorScene->handle = sceneHandle;
                    m_EditorScene->SetDirtyFlag(false);

                    SetActiveScene(m_EditorScene);

                    m_CurrentSceneFilepath = filepath;
                    m_CurrentSceneHandle = sceneHandle;

                    AssetWorker::ReportStatus("Ready", 0.0f);
                });
            }
            else
            {
                LOG_ASSERT(false, "Failed to load scene: {}", filepath.generic_string());
            }
        });

        return true;
    }

    bool AvaloniaLayer::SceneSave()
    {
        // Save Prefab Scene
        if (m_IsInPrefabIsolationMode)
        {
            if (m_EditingPrefabHandle != AssetHandle(0))
            {
                // Get filepath to serialize
                const auto &filepath = AssetManager::GetInstance()->GetFilepath(m_EditingPrefabHandle);
                const auto &absPath = m_ActiveProject->GetProjectFilepath(filepath);

                if (std::filesystem::exists(absPath))
                {
                    return m_EditingPrefab->Serialize(absPath);
                }
            }

            return false;
        }

        // Main Scene:
        // 1. Save current scene if the Current Scene Handle is the same with the current scene
        Ref<Scene> currentScene = m_EditorScene ? m_EditorScene : m_ActiveScene;
        const AssetHandle activeSceneHandle = currentScene ? currentScene->handle : AssetHandle(0);

        if (m_CurrentSceneHandle != AssetHandle(0)
            && m_CurrentSceneHandle == activeSceneHandle
            && !m_CurrentSceneFilepath.empty())
        {
            return SceneSave(m_CurrentSceneFilepath);
        }

        // 2. Open a File Save Dialog to save current scene if the Current Scene Handle is different (not saved yet to disk)
        return false;
    }

    bool AvaloniaLayer::SceneSave(const std::filesystem::path &filepath)
    {
        if (filepath.empty())
        {
            LOG_ERROR("[AvaloniaLayer] Cannot save scene: filepath is empty");
            return false;
        }

        std::filesystem::path path = filepath;
        if (path.extension() != ".ixscene")
        {
            path += ".ixscene";
        }

        Ref<Scene> sceneToSave = m_EditorScene ? m_EditorScene : m_ActiveScene;
        if (!sceneToSave)
        {
            LOG_ERROR("[AvaloniaLayer] Cannot save scene: no active scene");
            return false;
        }

        if (!m_ActiveProject)
        {
            m_ActiveProject = Project::GetActive();
        }

        if (!m_ActiveProject)
        {
            LOG_ERROR("[AvaloniaLayer] Cannot save scene: no active project");
            return false;
        }

        SceneSerializer serializer(sceneToSave, m_ActiveProject.get());
        if (!serializer.Serialize(path))
        {
            LOG_ERROR("[AvaloniaLayer] Failed to serialize scene to: {}", path.generic_string());
            return false;
        }

        if (sceneToSave->handle == AssetHandle(0))
        {
            sceneToSave->handle = AssetHandle();
        }

        if (AssetManager *am = AssetManager::GetInstance())
        {
            AssetHandle existingHandle = am->GetAssetHandle(path);
            if (existingHandle != AssetHandle(0))
            {
                sceneToSave->handle = existingHandle;
            }
            else
            {
                const auto relPath = m_ActiveProject->GetProjectRelativeFilepath(path);
                AssetMetaData meta(relPath, AssetType::Scene);
                am->AssignMetaData(sceneToSave->handle, meta);
                am->AssignAsset(sceneToSave->handle, sceneToSave);
            }
        }

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

        m_CurrentSceneFilepath = path;
        m_CurrentSceneHandle = sceneToSave->handle;

        LOG_INFO("[AvaloniaLayer] Scene saved successfully: {}", path.generic_string());
        return true;
    }

    AssetHandle AvaloniaLayer::GetActiveSceneHandle() const
    {
        Ref<Scene> currentScene = m_EditorScene ? m_EditorScene : m_ActiveScene;
        return currentScene ? currentScene->handle : AssetHandle(0);
    }

    bool AvaloniaLayer::IsCurrentSceneSaved() const
    {
        Ref<Scene> currentScene = m_EditorScene ? m_EditorScene : m_ActiveScene;
        const AssetHandle activeSceneHandle = currentScene ? currentScene->handle : AssetHandle(0);
        return m_CurrentSceneHandle != AssetHandle(0)
            && m_CurrentSceneHandle == activeSceneHandle
            && !m_CurrentSceneFilepath.empty();
    }

    void AvaloniaLayer::ScenePlay()
    {
        SceneStop();
        SetActiveScene(SceneManager::Copy(m_EditorScene));
        m_ActiveScene->OnStart(ESceneState::Play);
    }

    void AvaloniaLayer::SceneSimulate()
    {
        SceneStop();
        SetActiveScene(SceneManager::Copy(m_EditorScene));
        m_ActiveScene->OnStart(ESceneState::Simulate);
    }

    void AvaloniaLayer::SceneStop()
    {
        if (m_EditorScene)
            m_EditorScene->OnStop();

        if (m_ActiveScene)
            m_ActiveScene->OnStop();

        SetActiveScene(m_EditorScene);
    }

    void AvaloniaLayer::ScenePause()
    {
        if (m_ActiveScene)
        {
            m_ActiveScene->Pause();
        }
    }

    void AvaloniaLayer::SceneStepFrame(int frames)
    {
        if (m_ActiveScene)
        {
            m_ActiveScene->StepFrame(frames);
        }
    }

    int AvaloniaLayer::SceneGetState()
    {
        if (m_ActiveScene)
        {
            return static_cast<int>(m_ActiveScene->GetState());
        }

        return static_cast<int>(ESceneState::Stop);
    }

    Ref<Scene> AvaloniaLayer::GetActiveScene()
    {
        return m_ActiveScene;
    }

    void AvaloniaLayer::OnEvent(Event &e)
    {
        EventDispatcher dispatcher(e);

        // =============================
        // Keyboard Event
        // =============================
        dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent &event)
        {
            auto *imguiLayer = Application::GetInstance()->GetImGuiLayer();
            if (imguiLayer && imguiLayer->WantCaptureKeyboard())
                return false;

            const bool control = InputSystem::IsModifierPressed(KeyMod::Control);
            const bool shift = InputSystem::IsModifierPressed(KeyMod::LeftShift) || InputSystem::IsModifierPressed(KeyMod::RightShift);
            const bool alt = InputSystem::IsModifierPressed(KeyMod::LeftAlt) || InputSystem::IsModifierPressed(KeyMod::RightAlt);

            m_MultiSelect = shift;

            if (event.GetRepeatCount() == 0 && !control && !shift && !alt && !m_EditorCamera.mouse.rightButtonDown && !m_EditorCamera.mouse.middleButtonDown)
            {
                switch (event.GetKeyCode())
                {
                case Key::Space:
                    CycleGizmoOperation();
                    return true;
                case Key::Q:
                    SetGizmoOperation(static_cast<int>(GizmoOperation::NONE));
                    return true;
                case Key::W:
                    SetGizmoOperation(static_cast<int>(GizmoOperation::TRANSLATE));
                    return true;
                case Key::E:
                    SetGizmoOperation(static_cast<int>(GizmoOperation::ROTATE));
                    return true;
                case Key::R:
                    SetGizmoOperation(static_cast<int>(GizmoOperation::SCALE));
                    return true;
                case Key::T:
                    SetGizmoOperation(static_cast<int>(GizmoOperation::BOUND_SIZING_2D));
                    return true;
                default:
                    break;
                }
            }

            switch (event.GetKeyCode())
            {
            case Key::F:
            {
                if (Entity entity = GetSelectedEntity())
                {
                    auto &cam = m_EditorCamera;
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
            }
            return false;
        });

        // =============================
        // Mouse Scrolled Event
        // =============================
        dispatcher.Dispatch<MouseScrolledEvent>([this, &e](MouseScrolledEvent &event)
        {
            if (e.GetSource() != EventSource::Viewport)
                return false;

            auto *imguiLayer = Application::GetInstance()->GetImGuiLayer();
            if (imguiLayer && imguiLayer->WantCaptureMouse())
                return false;

            if (m_GizmoOperation != GizmoOperation::NONE && (m_Gizmo.IsManipulating() || m_IsGizmoManipulating))
                return false;

            m_EditorCamera.mouse.scroll = { static_cast<int>(event.GetXOffset()), static_cast<int>(event.GetYOffset()) };
            return false;
        });

        // =============================
        // Mouse Button Event
        // =============================
        dispatcher.Dispatch<MouseButtonPressedEvent>([this, &e](MouseButtonPressedEvent &event)
        {
            if (e.GetSource() != EventSource::Viewport)
                return false;

            if (event.GetButton() == Mouse::ButtonLeft)
            {
                auto scene = GetActiveScene();

                // Picking is disabled while playing scene
                if (!scene || SceneGetState() == static_cast<int>(ESceneState::Play))
                    return false;

                auto *imguiLayer = Application::GetInstance()->GetImGuiLayer();
                if (imguiLayer && imguiLayer->WantCaptureMouse())
                    return false;

                if (m_IsGizmoManipulating || m_IsGizmoBeingUse)
                    return false;

                // Don't pick if right or middle mouse button is pressed (orbit/pan)
                if (InputSystem::IsMouseButtonPressed(Mouse::ButtonRight) || InputSystem::IsMouseButtonPressed(Mouse::ButtonMiddle))
                    return false;

                const glm::ivec2 mousePos = InputSystem::GetMousePosition();
                if (mousePos.x < 0 || mousePos.y < 0 || mousePos.x >= static_cast<int>(m_Width) || mousePos.y >= static_cast<int>(m_Height))
                    return false;

                const auto now = std::chrono::steady_clock::now();
                const float elapsedMs = std::chrono::duration<float, std::milli>(now - m_LastClickTime).count();
                const float clickDist = glm::distance(glm::vec2(mousePos), glm::vec2(m_LastClickPos));
                const bool isDoubleClick = (elapsedMs < 350.0f) && (clickDist < 5.0f);
                m_LastClickTime = now;
                m_LastClickPos = mousePos;

                // Read SDL's live modifier state as well as the cached input state. The
                // viewport is hosted in a native child window, so a key event can be
                // delivered through a different focus path than the mouse event.
                const SDL_Keymod liveModifiers = SDL_GetModState();
                const bool isShift = (liveModifiers & SDL_KMOD_SHIFT) != 0
                    || InputSystem::IsModifierPressed(KeyMod::Shift)
                    || InputSystem::IsModifierPressed(KeyMod::LeftShift)
                    || InputSystem::IsModifierPressed(KeyMod::RightShift);
                const bool isCtrl = (liveModifiers & SDL_KMOD_CTRL) != 0
                    || InputSystem::IsModifierPressed(KeyMod::Control)
                    || InputSystem::IsModifierPressed(KeyMod::LeftControl)
                    || InputSystem::IsModifierPressed(KeyMod::RightControl);
                m_MultiSelect = isShift || isCtrl;

                const uint64_t pickedUuid = static_cast<uint64_t>(PickEntity(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y),
                    m_Width, m_Height, isDoubleClick, m_MultiSelect));

                Entity pickedEntity = (pickedUuid != 0xFFFFFFFFu && pickedUuid != 0)
                    ? SceneManager::GetEntity(scene.get(), UUID(pickedUuid))
                    : Entity{};

                if (!m_MultiSelect)
                {
                    // A plain click on the selected entity toggles it off. Clicking
                    // empty space also clears the complete selection.
                    if (pickedEntity.IsValid() && IsEntitySelected(pickedEntity.GetUUID()))
                        ClearSelectionFromViewport();
                    else
                        SelectSingleEntityFromViewport(
                            pickedEntity.IsValid()
                            ? pickedEntity.GetUUID()
                            : UUID(0)
                        );
                }
                else if (pickedEntity.IsValid())
                {
                    // Shift/Ctrl both use the same toggle behavior in the viewport:
                    // add an unselected entity and remove an already selected one.
                    if (IsEntitySelected(pickedEntity.GetUUID()))
                        DeselectEntityFromViewport(pickedEntity.GetUUID());
                    else
                        AddEntityToSelectionFromViewport(pickedEntity.GetUUID());
                }
            }

            return false;
        });
    }

    UUID AvaloniaLayer::PickEntity(float mouseX, float mouseY, uint32_t viewportWidth, uint32_t viewportHeight, bool isDoubleClick, bool isShiftDown)
    {
        auto *imguiLayer = Application::GetInstance()->GetImGuiLayer();
        if (imguiLayer && imguiLayer->WantCaptureMouse())
            return UUID(0);

        if (m_GizmoOperation != GizmoOperation::NONE && (m_Gizmo.IsManipulating() || m_Gizmo.IsHovered() || m_IsGizmoBeingUse))
            return UUID(0);

        Ref<Scene> scene = GetActiveScene();
        if (!scene || !scene->registry || !m_SceneRenderer)
            return UUID(0);

        auto target = m_SceneRenderer->GetRenderTarget(&m_EditorCamera);
        if (!target || !target->sceneRT)
            return UUID(0);

        Ref<Texture> objectIdTexture = target->sceneRT->GetColorAttachment(1);

        if (!objectIdTexture || !objectIdTexture->GetHandle())
            return UUID(0);

        const int texWidth = objectIdTexture->GetWidth();
        const int texHeight = objectIdTexture->GetHeight();

        if (viewportWidth == 0 || viewportHeight == 0 || texWidth <= 0 || texHeight <= 0)
            return UUID(0);

        const int pixelX = std::clamp(static_cast<int>((mouseX / static_cast<float>(viewportWidth)) * static_cast<float>(texWidth)), 0, texWidth - 1);
        const int pixelY = std::clamp(static_cast<int>((mouseY / static_cast<float>(viewportHeight)) * static_cast<float>(texHeight)), 0, texHeight - 1);

        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();
        if (!device)
            return UUID(0);

        nvrhi::TextureDesc stagingDesc = objectIdTexture->GetHandle()->getDesc();
        stagingDesc.initialState = nvrhi::ResourceStates::CopyDest;
        nvrhi::StagingTextureHandle stagingTexture = device->createStagingTexture(stagingDesc, nvrhi::CpuAccessMode::Read);
        if (!stagingTexture)
            return UUID(0);

        nvrhi::CommandListHandle copyCmd = device->createCommandList();
        copyCmd->open();
        copyCmd->copyTexture(stagingTexture, nvrhi::TextureSlice(), objectIdTexture->GetHandle(), nvrhi::TextureSlice());
        copyCmd->close();

        {
            auto &queueMutex = GPUUploadSync::GetQueueMutex();
            std::lock_guard<std::mutex> lock(queueMutex);
            device->executeCommandList(copyCmd);
        }

        size_t rowPitch = 0;
        uint32_t pickedObjectId = 0xFFFFFFFFu;
        if (void *mapped = device->mapStagingTexture(stagingTexture, nvrhi::TextureSlice(), nvrhi::CpuAccessMode::Read, &rowPitch))
        {
            const auto pixelData = static_cast<const uint32_t *>(mapped);
            pickedObjectId = pixelData[pixelY * (rowPitch / sizeof(uint32_t)) + pixelX];
            device->unmapStagingTexture(stagingTexture);
        }

        if (pickedObjectId == 0xFFFFFFFFu)
        {
            return UUID(0);
        }

        Entity pickedEntity = {};
        scene->registry->view<IDComponent>().each([&](const entt::entity e, const auto &id)
        {
            if (pickedEntity.IsValid())
                return;

            const auto objectId = static_cast<uint32_t>(static_cast<uint64_t>(id.uuid));
            if (objectId == pickedObjectId)
            {
                pickedEntity = Entity{ e, scene.get() };
            }
        });

        if (!pickedEntity.IsValid())
            return UUID(0);

        Entity targetSelection = pickedEntity;

        // Single click: prefer selecting the direct parent group first.
        // Double click: select the exact clicked entity.
        if (!isDoubleClick && !isShiftDown)
        {
            const auto parent = pickedEntity.GetParentUUID();
            if (parent != UUID(0))
            {
                if (Entity parentEntity = SceneManager::GetEntity(scene.get(), parent); parentEntity.IsValid())
                {
                    // If the parent is already selected, subsequent click selects the clicked child
                    if (IsEntitySelected(parentEntity.GetUUID()))
                    {
                        targetSelection = pickedEntity;
                    }
                    else
                    {
                        targetSelection = parentEntity;
                    }
                }
            }
        }

        return targetSelection.GetUUID();
    }

    bool AvaloniaLayer::IsEntitySelected(UUID uuid) const
    {
        return m_SelectedEntities.find(uuid) != m_SelectedEntities.end();
    }

    Entity AvaloniaLayer::ApplySingleSelection(Entity entity)
    {
        if (!entity.IsValid())
        {
            SyncClearSelection();
            return Entity{};
        }

        // This API is also used by the hierarchy-to-native synchronization.
        // It must be idempotent; viewport click toggling is handled explicitly
        // in OnEvent before calling the explicit viewport toggle methods.
        if (m_SelectedEntities.size() == 1 && IsEntitySelected(entity.GetUUID()))
        {
            ApplyGizmoOperation();
            return entity;
        }

        if (entity.IsValid())
        {
            m_SelectedEntities.clear();
            if (m_SceneRenderer)
                m_SceneRenderer->ClearSelectedEntities();

            m_SelectedEntities[entity.GetUUID()] = entity;
            if (m_SceneRenderer)
                m_SceneRenderer->SetSelectedEntity(entity);
        }

        ApplyGizmoOperation();

        return entity;
    }

    void AvaloniaLayer::SelectSingleEntityFromViewport(UUID uuid)
    {
        Ref<Scene> scene = GetActiveScene();
        if (!scene)
            return;

        Entity entity = ApplySingleSelection(SceneManager::GetEntity(scene.get(), uuid));
        if (m_EntitySelectedCallback)
        {
            m_EntitySelectedCallback(
                entity.IsValid() ? static_cast<uint64_t>(entity.GetUUID()) : 0,
                false
            );
        }
    }

    void AvaloniaLayer::SyncSingleSelection(UUID uuid)
    {
        Ref<Scene> scene = GetActiveScene();
        if (scene)
            ApplySingleSelection(SceneManager::GetEntity(scene.get(), uuid));
    }

    void AvaloniaLayer::AddEntityToSelectionFromViewport(UUID uuid)
    {
        Ref<Scene> scene = GetActiveScene();
        if (!scene)
            return;

        Entity entity = SceneManager::GetEntity(scene.get(), uuid);
        if (!entity.IsValid())
            return;

        m_SelectedEntities[uuid] = entity;
        if (m_SceneRenderer)
            m_SceneRenderer->SetSelectedEntity(entity);

        ApplyGizmoOperation();

        if (m_EntitySelectedCallback)
            m_EntitySelectedCallback(static_cast<uint64_t>(uuid), true);
    }

    void AvaloniaLayer::DeselectEntityFromViewport(UUID uuid)
    {
        auto it = m_SelectedEntities.find(uuid);
        if (it != m_SelectedEntities.end())
        {
            if (m_SceneRenderer)
                m_SceneRenderer->UnselectEntity(it->second);
            m_SelectedEntities.erase(it);

            // Any explicit deselection returns to the safe picking mode.
            SetGizmoOperation(static_cast<int>(GizmoOperation::NONE));

            if (m_EntitySelectedCallback)
                m_EntitySelectedCallback(static_cast<uint64_t>(uuid), true);
        }
    }

    void AvaloniaLayer::ClearSelectionFromViewport()
    {
        m_SelectedEntities.clear();
        m_IsGizmoManipulating = false;
        m_IsGizmoBeingUse = false;

        // Clear GIZMO Operation and notify the Avalonia toolbar.
        SetGizmoOperation(static_cast<int>(GizmoOperation::NONE));

        if (m_SceneRenderer)
            m_SceneRenderer->ClearSelectedEntities();

        if (m_EntitySelectedCallback)
            m_EntitySelectedCallback(0, false);
    }

    void AvaloniaLayer::SyncClearSelection()
    {
        m_SelectedEntities.clear();
        m_IsGizmoManipulating = false;
        m_IsGizmoBeingUse = false;

        // Managed synchronization must not change the user's active tool.
        // The stored operation is restored by ApplyGizmoOperation when the
        // next entity is selected.
        if (m_SceneRenderer)
            m_SceneRenderer->ClearSelectedEntities();
    }

    void AvaloniaLayer::SyncSelectedEntities(const uint64_t *uuids, uint32_t count)
    {
        m_SelectedEntities.clear();
        if (m_SceneRenderer)
            m_SceneRenderer->ClearSelectedEntities();

        if (!uuids || count == 0)
        {
            m_Gizmo.SetOperation(ImGuizmo::OPERATION::NONE);
            return;
        }

        Ref<Scene> scene = GetActiveScene();
        if (!scene)
        {
            m_Gizmo.SetOperation(ImGuizmo::OPERATION::NONE);
            return;
        }

        for (uint32_t i = 0; i < count; ++i)
        {
            UUID uuid(uuids[i]);
            Entity entity = SceneManager::GetEntity(scene.get(), uuid);
            if (entity.IsValid())
            {
                m_SelectedEntities[uuid] = entity;
                if (m_SceneRenderer)
                    m_SceneRenderer->SetSelectedEntity(entity);
            }
        }

        if (m_SelectedEntities.empty())
        {
            m_Gizmo.SetOperation(ImGuizmo::OPERATION::NONE);
        }
        else
        {
            ApplyGizmoOperation();
        }
    }

    uint32_t AvaloniaLayer::GetSelectedEntities(uint64_t *outUuids, uint32_t maxCount) const
    {
        if (!outUuids || maxCount == 0)
            return static_cast<uint32_t>(m_SelectedEntities.size());

        uint32_t count = 0;
        for (const auto &[uuid, entity] : m_SelectedEntities)
        {
            if (count >= maxCount)
                break;
            outUuids[count++] = static_cast<uint64_t>(uuid);
        }
        return count;
    }

    uint32_t AvaloniaLayer::GetSelectedEntityCount() const
    {
        return static_cast<uint32_t>(m_SelectedEntities.size());
    }

    bool AvaloniaLayer::ProjectSave()
    {
        // Automatically save current scene
        return Project::SaveActive();
    }

    bool AvaloniaLayer::ProjectClose()
    {
        if (!m_ActiveProject)
            return false;

        if (m_IsInPrefabIsolationMode)
        {
            // ExitPrefabIsolation(true);
        }

        // Stop scene before saving
        if (m_EditorScene)
            m_EditorScene->OnStop();

        if (m_ActiveScene)
            m_ActiveScene->OnStop();

        // Save Project
        ProjectSave();

        SetActiveScene(nullptr);

        m_EditorScene.reset();
        m_ActiveScene.reset();

        AssetManager::GetInstance()->Reset();

        // Reset everything
        m_ActiveProject.reset();
        Project::CloseActive();

        m_CurrentProjectFilepath.clear();
        m_MainSceneBeforeIsolation.reset();
        m_EditingPrefab.reset();
        m_IsInPrefabIsolationMode = false;
        m_CurrentSceneFilepath.clear();

        m_EditingPrefabHandle = AssetHandle(0);
        m_CurrentSceneHandle = AssetHandle(0);

        return true;
    }

    void AvaloniaLayer::OnOpenProject()
    {
        // TODO: Reload project files

        // Get Project default scene (use immediate load for synchronous path)
        m_ActiveProject = Project::GetActive();
        LOG_ASSERT(m_ActiveProject, "Invalid active project!");
        if (!m_ActiveProject)
        {
            return;
        }

        AssetHandle defaultSceneHandle = m_ActiveProject->GetInfo().defaultSceneHandle;
        if (defaultSceneHandle != AssetHandle(0))
        {
            if (Ref<Scene> activeScene = AssetManager::GetInstance()->GetAssetImmediate<Scene>(defaultSceneHandle))
            {
                m_EditorScene = SceneManager::Copy(activeScene);
                m_EditorScene->handle = activeScene->handle;
                m_EditorScene->SetDirtyFlag(false);
                SetActiveScene(m_EditorScene);

                const auto &[assetFilepath, assetType] = AssetManager::GetInstance()->GetMetaData(defaultSceneHandle);

                m_CurrentSceneFilepath = m_ActiveProject->GetProjectFilepath(assetFilepath);
                m_CurrentSceneHandle = activeScene->handle;
            }
            else
            {
                // Create a default scene if load failed
                SceneNew();
            }
        }
        else
        {
            // Create a default scene
            SceneNew();
        }
    }

    void AvaloniaLayer::DrawGizmo()
    {
        constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus
            | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;

        const ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::SetNextWindowBgAlpha(0.0f);

        ImGui::Begin("##main_dockspace", nullptr, windowFlags);
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        window->DC.LayoutType = ImGuiLayoutType_Horizontal;
        window->DC.NavLayerCurrent = ImGuiNavLayer_Menu;

        // Begin GIZMO Manipulation
        auto view = m_EditorCamera.GetView();
        auto &projection = m_EditorCamera.GetProjection();

#if 0
        // Draw orientation just for Perspective projection
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
#endif

        // Setup gizmo
        GizmoInfo gizmoInfo;
        gizmoInfo.cameraView = view;
        gizmoInfo.cameraProjection = projection;
        gizmoInfo.cameraType = m_EditorCamera.projectionType;
        gizmoInfo.viewRect = Rect({ 0, 0 }, { viewport->Size.x, viewport->Size.y });

        gizmoInfo.isSnapping = m_SnapEnabled;

        switch (m_GizmoOperation)
        {
            case GizmoOperation::TRANSLATE: gizmoInfo.snapValue = m_SnapValues[0]; break;
            case GizmoOperation::ROTATE:    gizmoInfo.snapValue = m_SnapValues[1]; break;
            case GizmoOperation::SCALE:     gizmoInfo.snapValue = m_SnapValues[2]; break;
            default:                        gizmoInfo.snapValue = m_SnapValues[0]; break;
        }

        m_Gizmo.SetInfo(gizmoInfo);

        // Start manipulation: Fired only on the first frame of interaction
        // const bool allowGizmoManipulation = !m_Data.is2DBoundsSizing;
        // bool isManipulatingNow = allowGizmoManipulation && m_Gizmo.IsManipulating();
        const bool gizmoEnabled = m_GizmoOperation != GizmoOperation::NONE && !m_SelectedEntities.empty();
        bool isManipulatingNow = gizmoEnabled && m_Gizmo.IsManipulating();

        static std::unordered_map<UUID, TransformComponent> initialTransforms;

        if (isManipulatingNow && !m_IsGizmoManipulating)
        {
            initialTransforms.clear();
            for (const auto &[uuid, entity] : m_SelectedEntities)
            {
                // Store the original transform of each selected entity
                initialTransforms[uuid] = entity.GetTransform();
            }
        }

        // Capture PREVIOUS frame value before overwriting — needed for the release-commit below
        bool isPlayOrSimulate = m_ActiveScene->GetState() == ESceneState::Play || m_ActiveScene->GetState() == ESceneState::Simulate;
        bool wasManipulating = m_IsGizmoManipulating;

        m_IsGizmoManipulating = isManipulatingNow;
        m_IsGizmoBeingUse = gizmoEnabled && (isManipulatingNow || m_Gizmo.IsHovered());

        if (gizmoEnabled && m_SelectedEntities.size() > 1)
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

            if (m_IsGizmoManipulating)
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
                        Entity parent = SceneManager::GetEntity(m_ActiveScene.get(), entity.GetParentUUID());
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

                    if (isPlayOrSimulate)
                    {
                        Application::SubmitToMainThread([scene = m_ActiveScene.get(), entity, translation = tr.local.translation, rotation = tr.local.rotation]() mutable {
                            if (entity.IsValid())
                            {
                                if (entity.HasComponent<RigidbodyComponent>())
                                {
                                    auto &rb = entity.GetComponent<RigidbodyComponent>();
                                    rb.isGizmoDragging = true;
                                    if (auto dynActor = rb.dynamicActor.lock())
                                    {
                                        dynActor->SetPosition(translation, true);
                                        dynActor->SetRotation(rotation, true);
                                        dynActor->SetLinearVelocity(glm::vec3(0.0f));
                                        dynActor->SetAngularVelocity(glm::vec3(0.0f));
                                    }
                                    else if (auto statActor = rb.staticActor.lock())
                                    {
                                        statActor->SetPosition(translation, true);
                                        statActor->SetRotation(rotation, true);
                                    }
                                }
                                if (entity.HasComponent<Rigidbody2DComponent>())
                                {
                                    auto &rb2D = entity.GetComponent<Rigidbody2DComponent>();
                                    rb2D.isGizmoDragging = true;
                                    if (scene->GetPhysics2D() && scene->GetPhysics2D()->IsValidBody(rb2D.bodyId))
                                    {
                                        scene->GetPhysics2D()->SetPosition(rb2D.bodyId, translation);
                                        scene->GetPhysics2D()->SetRotation(rb2D.bodyId, glm::eulerAngles(rotation).z);
                                        scene->GetPhysics2D()->SetLinearVelocity(rb2D.bodyId, glm::vec2(0.0f));
                                        scene->GetPhysics2D()->SetAngularVelocity(rb2D.bodyId, 0.0f);
                                        scene->GetPhysics2D()->SetAwake(rb2D.bodyId, true);
                                    }
                                }
                            }
                            });
                    }
                }
            }

            // Commit commands when the multi-entity gizmo is released
            if (!isManipulatingNow && wasManipulating)
            {
                if (isPlayOrSimulate)
                {
                    for (auto &[uuid, entity] : m_SelectedEntities)
                    {
                        Application::SubmitToMainThread([scene = m_ActiveScene.get(), entity]() mutable {
                            if (entity.IsValid())
                            {
                                if (entity.HasComponent<RigidbodyComponent>())
                                    entity.GetComponent<RigidbodyComponent>().isGizmoDragging = false;
                                if (entity.HasComponent<Rigidbody2DComponent>())
                                    entity.GetComponent<Rigidbody2DComponent>().isGizmoDragging = false;
                            }
                            });
                    }
                }

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
                    CommandManager::AddCommand(CreateScope<ComponentPropertyBatchCommand<TransformComponent>>(m_ActiveScene.get(), std::move(entries)));
                }
            }
        }
        else if (gizmoEnabled && GetSelectedEntity().IsValid())
        {
            Entity entity = GetSelectedEntity();
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
                    Entity parent = SceneManager::GetEntity(m_ActiveScene.get(), entity.GetParentUUID());
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

                // check if the entity has rigidbody and update its position and rotation in the physics engine
                if (isPlayOrSimulate)
                {
                    Application::SubmitToMainThread([scene = m_ActiveScene.get(), entity, translation = tr.local.translation, rotation = tr.local.rotation]() mutable {
                        if (entity.IsValid())
                        {
                            if (entity.HasComponent<RigidbodyComponent>())
                            {
                                auto &rb = entity.GetComponent<RigidbodyComponent>();
                                rb.isGizmoDragging = true;
                                if (auto dynActor = rb.dynamicActor.lock())
                                {
                                    dynActor->SetPosition(translation, true);
                                    dynActor->SetRotation(rotation, true);
                                    dynActor->SetLinearVelocity(glm::vec3(0.0f));
                                    dynActor->SetAngularVelocity(glm::vec3(0.0f));
                                }
                                else if (auto statActor = rb.staticActor.lock())
                                {
                                    statActor->SetPosition(translation, true);
                                    statActor->SetRotation(rotation, true);
                                }
                            }
                            if (entity.HasComponent<Rigidbody2DComponent>())
                            {
                                auto &rb2D = entity.GetComponent<Rigidbody2DComponent>();
                                rb2D.isGizmoDragging = true;
                                if (scene->GetPhysics2D() && scene->GetPhysics2D()->IsValidBody(rb2D.bodyId))
                                {
                                    scene->GetPhysics2D()->SetPosition(rb2D.bodyId, translation);
                                    scene->GetPhysics2D()->SetRotation(rb2D.bodyId, glm::eulerAngles(rotation).z);
                                    scene->GetPhysics2D()->SetLinearVelocity(rb2D.bodyId, glm::vec2(0.0f));
                                    scene->GetPhysics2D()->SetAngularVelocity(rb2D.bodyId, 0.0f);
                                    scene->GetPhysics2D()->SetAwake(rb2D.bodyId, true);
                                }
                            }
                        }
                        });
                }
            }

            // Commit a single command when the gizmo is released (single entity)
            if (!isManipulatingNow && wasManipulating)
            {
                if (isPlayOrSimulate)
                {
                    Application::SubmitToMainThread([scene = m_ActiveScene.get(), entity]() mutable {
                        if (entity.IsValid())
                        {
                            if (entity.HasComponent<RigidbodyComponent>())
                                entity.GetComponent<RigidbodyComponent>().isGizmoDragging = false;
                            if (entity.HasComponent<Rigidbody2DComponent>())
                                entity.GetComponent<Rigidbody2DComponent>().isGizmoDragging = false;
                        }
                        });
                }

                if (auto it = initialTransforms.find(entity.GetUUID()); it != initialTransforms.end())
                {
                    CommandManager::AddCommand(CreateScope<ComponentPropertyCommand<TransformComponent>>(m_ActiveScene.get(), entity.GetUUID(), it->second, entity.GetTransform()));
                }
            }

        }
        

        m_IsGizmoManipulating = gizmoEnabled && m_Gizmo.IsManipulating();
        m_IsGizmoBeingUse = gizmoEnabled && (m_IsGizmoManipulating || m_Gizmo.IsHovered());

        ImGui::End();
    }

    void AvaloniaLayer::SetNavigationMode(int mode)
    {
        m_EditorCamera.SetNavigationMode(static_cast<EditorCamera::NavigationMode>(mode));
        m_EditorCamera.UpdateView();
        m_EditorCamera.UpdateProjection(m_Width, m_Height);
    }

    int AvaloniaLayer::GetNavigationMode() const
    {
        return static_cast<int>(m_EditorCamera.GetNavigationMode());
    }

    void AvaloniaLayer::ApplyGizmoOperation()
    {
        if (m_SelectedEntities.empty())
        {
            m_IsGizmoManipulating = false;
            m_IsGizmoBeingUse = false;
            m_Gizmo.SetOperation(ImGuizmo::OPERATION::NONE);
            return;
        }

        if (m_GizmoOperation == GizmoOperation::NONE)
        {
            m_IsGizmoManipulating = false;
            m_IsGizmoBeingUse = false;
        }

        switch (m_GizmoOperation)
        {
        case GizmoOperation::TRANSLATE:
            m_Gizmo.SetOperation(ImGuizmo::OPERATION::TRANSLATE);
            break;
        case GizmoOperation::ROTATE:
            m_Gizmo.SetOperation(ImGuizmo::OPERATION::ROTATE);
            break;
        case GizmoOperation::SCALE:
            m_Gizmo.SetOperation(ImGuizmo::OPERATION::SCALE);
            break;
        default:
            m_Gizmo.SetOperation(ImGuizmo::OPERATION::NONE);
            break;
        }
    }

    void AvaloniaLayer::SetGizmoOperation(int op)
    {
        m_GizmoOperation = static_cast<GizmoOperation>(op);
        if (m_GizmoOperation == GizmoOperation::NONE)
        {
            m_IsGizmoManipulating = false;
            m_IsGizmoBeingUse = false;
        }
        ApplyGizmoOperation();

        if (m_GizmoOperationChangedCallback)
        {
            m_GizmoOperationChangedCallback(static_cast<int>(m_GizmoOperation));
        }
    }

    int AvaloniaLayer::GetGizmoOperation() const
    {
        return static_cast<int>(m_GizmoOperation);
    }

    void AvaloniaLayer::CycleGizmoOperation()
    {
        GizmoOperation nextOp;
        switch (m_GizmoOperation)
        {
        case GizmoOperation::TRANSLATE:
            nextOp = GizmoOperation::ROTATE;
            break;
        case GizmoOperation::ROTATE:
            nextOp = GizmoOperation::SCALE;
            break;
        case GizmoOperation::SCALE:
            nextOp = GizmoOperation::TRANSLATE;
            break;
        default:
            nextOp = GizmoOperation::TRANSLATE;
            break;
        }
        SetGizmoOperation(static_cast<int>(nextOp));
    }

    void AvaloniaLayer::SetGizmoMode(int mode)
    {
        m_Gizmo.SetMode(mode == 1 ? ImGuizmo::MODE::WORLD : ImGuizmo::MODE::LOCAL);
    }

    int AvaloniaLayer::GetGizmoMode() const
    {
        return m_Gizmo.GetMode() == ImGuizmo::MODE::WORLD ? 1 : 0;
    }

    void AvaloniaLayer::SetSnapEnabled(bool enabled)
    {
        m_SnapEnabled = enabled;
    }

    bool AvaloniaLayer::GetSnapEnabled() const
    {
        return m_SnapEnabled;
    }

    void AvaloniaLayer::SetSnapValue(int op, float value)
    {
        if (op >= 0 && op < static_cast<int>(m_SnapValues.size()))
        {
            m_SnapValues[op] = value;
        }
    }

    float AvaloniaLayer::GetSnapValue(int op) const
    {
        if (op >= 0 && op < static_cast<int>(m_SnapValues.size()))
        {
            return m_SnapValues[op];
        }
        return 0.0f;
    }
}
