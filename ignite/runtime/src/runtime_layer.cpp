// Copyright (c) 2026 Evangelion Manuhutu

#include "pch.hpp"

#include <ignite/core/application.hpp>
#include <ignite/core/input/input_system.hpp>

#include <ignite/graphics/renderer/scene_renderer.hpp>
#include <ignite/graphics/window.hpp>
#include <ignite/graphics/ui/game_ui_system.hpp>

#include <ignite/project/project.hpp>

#include <ignite/scene/entity.hpp>
#include <ignite/scene/scene_camera.hpp>
#include <ignite/scene/scene.hpp>
#include <ignite/scene/scene_manager.hpp>
#include <ignite/scene/component.hpp>

#include "runtime_layer.hpp"

namespace ignite
{
    RuntimeLayer::RuntimeLayer(const std::string &name)
        : Layer(name)
    {
        auto *app = Application::GetInstance();

        m_SceneRenderer = CreateRef<SceneRenderer>();
        OpenProject("D:/Dev/IgniteProject/Hello/Hello.ixproj");

        app->GetWindow()->Show();
    }

    RuntimeLayer::~RuntimeLayer()
    {
    }

    void RuntimeLayer::OnAttach()
    {
    }

    void RuntimeLayer::OnDetach()
    {
        CloseCurrentProject();
    }

    void RuntimeLayer::OnUpdate(float deltaTime)
    {
        if (!m_ActiveScene || !m_ActiveProject)
            return;

        m_ActiveScene->OnFixedUpdateRuntimeSimulate();
        m_ActiveScene->OnUpdateRuntimeSimulate(deltaTime);
    }

    void RuntimeLayer::OnRender(nvrhi::IFramebuffer *framebuffer)
    {
        if (!m_ActiveScene || !m_ActiveProject)
            return;

        FrameContext *frameContext = Renderer::GetCurrentFrameContext();
        Entity primaryCam = m_ActiveScene->GetPrimaryCamera();
        auto &cc = primaryCam.GetComponent<CameraComponent>();

        const glm::uvec2 desiredSize = glm::max(glm::uvec2(0), m_FramebufferSize);
        ProcessCameraViewportResize((ICamera *)&cc.camera, desiredSize);

        m_SceneRenderer->Render((ICamera *)&cc.camera, frameContext, false, framebuffer);
    }

    bool RuntimeLayer::OnFramebufferResize(const FramebufferResizeEvent &event)
    {
        m_FramebufferSize = { event.GetWidth(), event.GetHeight() };
        return false;
    }

    void RuntimeLayer::ProcessCameraViewportResize(ICamera *camera, const glm::uvec2 &desiredSize)
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
        float globalScale = m_SceneRenderer ? m_SceneRenderer->sceneRenderSettings.renderScale : 1.0f;
        postProcessing.renderScale = glm::clamp(postProcessing.renderScale * globalScale, 0.25f, 1.0f);

        const uint32_t expectedRenderWidth = std::max(1u, static_cast<uint32_t>(std::round(static_cast<float>(desiredSize.x) * postProcessing.renderScale)));
        const uint32_t expectedRenderHeight = std::max(1u, static_cast<uint32_t>(static_cast<float>(desiredSize.y) * postProcessing.renderScale));

        const glm::uvec2 sceneRTSize = target ? target->sceneRT->GetSize() : glm::uvec2(0);

        const bool cameraNeedsResize = (desiredSize.x != cameraSize.x || desiredSize.y != cameraSize.y);
        const bool compositeNeedsResize = (!target || desiredSize.x != targetSize.x || desiredSize.y != targetSize.y);
        const bool sceneRTNeedsResize = (!target || expectedRenderWidth != sceneRTSize.x || expectedRenderHeight != sceneRTSize.y);

        const bool needsResize = cameraNeedsResize || compositeNeedsResize || sceneRTNeedsResize;

        if (needsResize)
        {
            camera->UpdateProjection(desiredSize.x, desiredSize.y);

            if (m_SceneRenderer)
            {
                if (!target || targetSize.x == 0 || targetSize.y == 0 || sceneRTSize.x == 0 || sceneRTSize.y == 0)
                {
                    m_SceneRenderer->ResizeFramebuffer(camera, desiredSize.x, desiredSize.y);
                }
                else if (m_ResizingFrame++ >= 12)
                {
                    m_SceneRenderer->ResizeFramebuffer(camera, desiredSize.x, desiredSize.y);
                    m_ResizingFrame = 0;
                }
            }
        }
    }

    void RuntimeLayer::OpenProject(const std::filesystem::path &filepath)
    {
        if (m_ActiveProject)
        {
            CloseCurrentProject();
        }

        if (const Ref<Project> openedProject = Project::Open(filepath))
        {
            m_ActiveProject = openedProject;
            OnOpenProject();
        }
    }

    void RuntimeLayer::OnOpenProject()
    {
        // Get Project default scene (use immediate load for synchronous path)
        AssetHandle defSceneAssetHandle = m_ActiveProject->GetInfo().defaultSceneHandle;
        if (defSceneAssetHandle != AssetHandle(0))
        {
            // Use GetAssetImmediate since we're on main thread and need synchronous load
            if (Ref<Scene> activeScene = AssetManager::GetInstance()->GetAssetImmediate<Scene>(defSceneAssetHandle))
            {
                m_ActiveScene = SceneManager::Copy(activeScene);
                m_SceneRenderer->SetActiveScene(m_ActiveScene);
                m_ActiveProject->SetActiveScene(m_ActiveScene);

                GameUISystem::SetSceneContext(m_ActiveScene.get());

                m_ActiveScene->OnStart(ESceneState::Play);
            }
        }
        LOG_ASSERT(m_ActiveScene, "[Runtime] Failed to load default scene!");
    }

    void RuntimeLayer::CloseCurrentProject()
    {
        if (!m_ActiveProject)
            return;

        if (m_ActiveScene)
            m_ActiveScene->OnStop();

        AssetManager::GetInstance()->Reset();

        m_ActiveProject.reset();
        Project::CloseActive();
    }

    void RuntimeLayer::OnEvent(Event &e)
    {
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<FramebufferResizeEvent>(BIND_CLASS_EVENT_FN(RuntimeLayer::OnFramebufferResize));
    }
}
