// Copyright (c) 2026 Evangelion Manuhutu

#include "avalonia_layer.h"

#include "ignite/core/input/mouse_event.hpp"
#include "ignite/core/input/key_event.hpp"
#include "ignite/core/input/input_system.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/scene/entity.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/graphics/renderer/scene_renderer.hpp"
#include "ignite/scene/scene_manager.hpp"
#include "ignite/project/project.hpp"

#include <chrono>
#include <algorithm>

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
    }

    void AvaloniaLayer::OnAttach()
    {
        m_SceneRenderer = CreateRef<SceneRenderer>();
        m_FallbackScene = Scene::Create(nullptr);
    }

    void AvaloniaLayer::OnDetach()
    {
        m_SceneRenderer = nullptr;
        m_FallbackScene = nullptr;
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
        if (Ref<Scene> activeScene = GetCurrentScene())
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
        }

        // Camera update using SDL3 InputSystem
        if (!ImGui::GetIO().WantCaptureMouse && !ImGui::GetIO().WantCaptureKeyboard)
        {
            m_EditorCamera.UpdateMouseState();
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
        m_EditorCamera.UpdateCameraPosition(deltaTime);
        m_EditorCamera.UpdateView();
    }

    void AvaloniaLayer::OnRender(nvrhi::IFramebuffer *mainFramebuffer)
    {
        if (!m_SceneRenderer)
            return;

        Ref<Scene> activeScene = GetCurrentScene();
        if (!activeScene)
            return;

        ICamera *cameraToUse = &m_EditorCamera;

        m_SceneRenderer->ResizeFramebuffer(cameraToUse, m_Width, m_Height);
        m_SceneRenderer->SetActiveScene(activeScene);
        m_SceneRenderer->BeginFrame();

        FrameContext *frameContext = Renderer::GetCurrentFrameContext();
        m_SceneRenderer->Render(cameraToUse, frameContext, false, mainFramebuffer);
    }

    void AvaloniaLayer::OnGuiRender()
    {
        ImGui::ShowDemoWindow(nullptr);
    }

    void AvaloniaLayer::Play()
    {
        Stop();
        if (Ref<Scene> editorScene = GetEditorScene())
        {
            m_RuntimeScene = SceneManager::Copy(editorScene);
            m_RuntimeScene->OnStart(ESceneState::Play);
        }
    }

    void AvaloniaLayer::Simulate()
    {
        Stop();
        if (Ref<Scene> editorScene = GetEditorScene())
        {
            m_RuntimeScene = SceneManager::Copy(editorScene);
            m_RuntimeScene->OnStart(ESceneState::Simulate);
        }
    }

    void AvaloniaLayer::Stop()
    {
        if (m_RuntimeScene)
        {
            m_RuntimeScene->OnStop();
            m_RuntimeScene = nullptr;
        }
    }

    void AvaloniaLayer::Pause()
    {
        if (m_RuntimeScene)
        {
            m_RuntimeScene->Pause();
        }
    }

    void AvaloniaLayer::StepFrame(int frames)
    {
        if (m_RuntimeScene)
        {
            m_RuntimeScene->StepFrame(frames);
        }
    }

    int AvaloniaLayer::GetState()
    {
        if (m_RuntimeScene)
            return static_cast<int>(m_RuntimeScene->GetState());
        return static_cast<int>(ESceneState::Stop);
    }

    Ref<Scene> AvaloniaLayer::GetCurrentScene()
    {
        if (Ref<Project> activeProj = Project::GetActive())
        {
            if (Ref<Scene> projScene = activeProj->LockActiveScene())
            {
                return projScene;
            }
        }
        return m_FallbackScene;
    }

    Ref<Scene> AvaloniaLayer::GetEditorScene()
    {
        return m_RuntimeScene ? m_RuntimeScene : GetCurrentScene();
    }

    void AvaloniaLayer::OnEvent(Event &e)
    {
        EventDispatcher dispatcher(e);

        // =============================
        // Keyboard Event
        // =============================
        dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent &event)
        {

            if (ImGui::GetIO().WantTextInput)
                return false;

            const bool control = InputSystem::IsModifierPressed(KeyMod::Control);
            const bool shift = InputSystem::IsModifierPressed(KeyMod::LeftShift);

            m_MultiSelect = shift;

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
        dispatcher.Dispatch<MouseScrolledEvent>([this](MouseScrolledEvent &event)
        {
            if (ImGui::GetIO().WantCaptureMouse)
                return false;

            m_EditorCamera.mouse.scroll = { static_cast<int>(event.GetXOffset()), static_cast<int>(event.GetYOffset()) };
            return false;
        });

        // =============================
        // Mouse Button Event
        // =============================
        dispatcher.Dispatch<MouseButtonPressedEvent>([this](MouseButtonPressedEvent &event)
        {
            if (event.GetButton() == Mouse::ButtonLeft)
            {
                auto scene = GetCurrentScene();

                // Picking is disabled while playing scene
                if (!scene || GetState() == static_cast<int>(ESceneState::Play) || ImGui::GetIO().WantCaptureMouse)
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

                const bool isShift = InputSystem::IsModifierPressed(KeyMod::Shift) || InputSystem::IsModifierPressed(KeyMod::LeftShift) || InputSystem::IsModifierPressed(KeyMod::RightShift);
                const bool isCtrl = InputSystem::IsModifierPressed(KeyMod::Control) || InputSystem::IsModifierPressed(KeyMod::LeftControl) || InputSystem::IsModifierPressed(KeyMod::RightControl);
                m_MultiSelect = isShift || isCtrl;

                const uint64_t pickedUuid = static_cast<uint64_t>(PickEntity(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y),
                    m_Width, m_Height, isDoubleClick, m_MultiSelect));

                Entity pickedEntity = (pickedUuid != 0xFFFFFFFFu && pickedUuid != 0)
                    ? SceneManager::GetEntity(scene.get(), UUID(pickedUuid))
                    : Entity{};

                if (!m_MultiSelect)
                {
                    if (pickedEntity.IsValid())
                    {
                        ClearSelectedEntities();
                        SetSelectedEntity(pickedEntity);
                        if (m_EntitySelectedCallback)
                            m_EntitySelectedCallback(static_cast<uint64_t>(pickedEntity.GetUUID()), false);
                    }
                    else
                    {
                        ClearSelectedEntities();
                        if (m_EntitySelectedCallback)
                            m_EntitySelectedCallback(0, false);
                    }
                }
                else
                {
                    if (pickedEntity.IsValid())
                    {
                        SetSelectedEntity(pickedEntity);
                        if (m_EntitySelectedCallback)
                            m_EntitySelectedCallback(static_cast<uint64_t>(pickedEntity.GetUUID()), true);
                    }
                }
            }
            return false;
        });
    }

    UUID AvaloniaLayer::PickEntity(float mouseX, float mouseY, uint32_t viewportWidth, uint32_t viewportHeight, bool isDoubleClick, bool isShiftDown)
    {
        Ref<Scene> scene = GetEditorScene();
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

    Entity AvaloniaLayer::SetSelectedEntity(Entity entity)
    {
        if (!entity.IsValid())
        {
            ClearSelectedEntities();
            return Entity{};
        }

        // multi select
        if (m_MultiSelect)
        {
            if (auto it = m_SelectedEntities.find(entity.GetUUID()); it != m_SelectedEntities.end())
            {
                // de-select
                if (m_SceneRenderer)
                    m_SceneRenderer->UnselectEntity(it->second);
                it = m_SelectedEntities.erase(it);

                if (!m_SelectedEntities.empty())
                {
                    return m_SelectedEntities.begin()->second;
                }
                return Entity{};
            }
            else
            {
                m_SelectedEntities[entity.GetUUID()] = entity;
                if (m_SceneRenderer)
                    m_SceneRenderer->SetSelectedEntity(entity);
            }
        }
        else // single select
        {
            m_SelectedEntities.clear();
            if (m_SceneRenderer)
                m_SceneRenderer->ClearSelectedEntities();

            m_SelectedEntities[entity.GetUUID()] = entity;
            if (m_SceneRenderer)
                m_SceneRenderer->SetSelectedEntity(entity);
        }

        return entity;
    }

    Entity AvaloniaLayer::SetSelectedEntity(UUID uuid)
    {
        Ref<Scene> scene = GetCurrentScene();
        if (!scene)
            return Entity{};

        Entity entity = SceneManager::GetEntity(scene.get(), uuid);
        return SetSelectedEntity(entity);
    }

    void AvaloniaLayer::SelectEntity(UUID uuid, bool multiSelect)
    {
        Ref<Scene> scene = GetCurrentScene();
        if (!scene)
            return;

        Entity entity = SceneManager::GetEntity(scene.get(), uuid);
        if (!entity.IsValid())
        {
            if (!multiSelect)
                ClearSelectedEntities();
            return;
        }

        if (!multiSelect)
        {
            m_SelectedEntities.clear();
            if (m_SceneRenderer)
                m_SceneRenderer->ClearSelectedEntities();
        }

        m_SelectedEntities[uuid] = entity;
        if (m_SceneRenderer)
            m_SceneRenderer->SetSelectedEntity(entity);
    }

    void AvaloniaLayer::DeselectEntity(UUID uuid)
    {
        auto it = m_SelectedEntities.find(uuid);
        if (it != m_SelectedEntities.end())
        {
            if (m_SceneRenderer)
                m_SceneRenderer->UnselectEntity(it->second);
            m_SelectedEntities.erase(it);
        }
    }

    void AvaloniaLayer::ClearSelectedEntities()
    {
        m_SelectedEntities.clear();
        if (m_SceneRenderer)
        {
            m_SceneRenderer->ClearSelectedEntities();
        }
    }

    void AvaloniaLayer::SetSelectedEntities(const uint64_t *uuids, uint32_t count)
    {
        m_SelectedEntities.clear();
        if (m_SceneRenderer)
            m_SceneRenderer->ClearSelectedEntities();

        if (!uuids || count == 0)
            return;

        Ref<Scene> scene = GetCurrentScene();
        if (!scene)
            return;

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
}
