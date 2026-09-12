// Copyright (c) 2026 Evangelion Manuhutu

#include "avalonia_layer.h"

#include "ignite/core/input/mouse_event.hpp"

#include "ignite/scene/scene.hpp"
#include "ignite/graphics/renderer/scene_renderer.hpp"
#include "ignite/scene/scene_manager.hpp"
#include "ignite/project/project.hpp"

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
        Ref<Scene> activeScene = GetCurrentScene();
        if (activeScene)
        {
            activeScene->OnUpdateEdit(deltaTime);
        }

        // Native camera update using SDL3 InputSystem
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
        dispatcher.Dispatch<MouseScrolledEvent>([this](MouseScrolledEvent &event) {
            m_EditorCamera.mouse.scroll = { static_cast<int>(event.GetXOffset()), static_cast<int>(event.GetYOffset()) };
            return false;
        });
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
