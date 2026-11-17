// Copyright (c) 2026 Evangelion Manuhutu

#include "pch.hpp"

#include <ignite/asset/asset_manager.hpp>
#include <ignite/core/agent_controller.hpp>
#include <ignite/core/application.hpp>
#include <ignite/core/input/input_system.hpp>

#include <ignite/graphics/renderer.hpp>
#include <ignite/graphics/renderer/scene_renderer.hpp>
#include <ignite/graphics/window.hpp>
#include <ignite/graphics/ui/game_ui_system.hpp>

#include <ignite/project/project.hpp>

#include <ignite/scene/entity.hpp>
#include <ignite/scene/scene_camera.hpp>
#include <ignite/scene/scene.hpp>
#include <ignite/scene/scene_manager.hpp>
#include <ignite/scene/component.hpp>

#include <ignite/net/network_manager.hpp>
#include <SDL3/SDL_mouse.h>
#include <imgui.h>
#include "runtime_layer.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace ignite
{
    namespace
    {
        std::filesystem::path GetExecutableDirectory()
        {
#ifdef _WIN32
            wchar_t buffer[MAX_PATH]{};
            const DWORD len = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
            if (len > 0 && len < MAX_PATH)
            {
                return std::filesystem::path(buffer).parent_path();
            }
#endif
            return std::filesystem::current_path();
        }

        std::filesystem::path FindProjectInDirectory(const std::filesystem::path &dir)
        {
            std::error_code ec;
            if (dir.empty() || !std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec))
                return {};

            for (const auto &entry : std::filesystem::directory_iterator(dir, ec))
            {
                if (!ec && entry.is_regular_file(ec) && entry.path().extension() == ".ixproj")
                {
                    return entry.path();
                }
            }
            return {};
        }

        std::filesystem::path DiscoverProjectPath(const std::filesystem::path &explicitPath)
        {
            std::error_code ec;
            if (!explicitPath.empty() && std::filesystem::exists(explicitPath, ec))
                return explicitPath;

            if (auto found = FindProjectInDirectory(std::filesystem::current_path(ec)); !found.empty())
                return found;

            if (auto found = FindProjectInDirectory(GetExecutableDirectory()); !found.empty())
                return found;

            const std::filesystem::path devFallback = "D:/Dev/IgniteProject/Hello/Hello.ixproj";
            if (std::filesystem::exists(devFallback, ec))
                return devFallback;

            return {};
        }

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
    }

    RuntimeLayer::RuntimeLayer(const std::string &name, const ApplicationCommandLineArgs &args)
        : Layer(name)
    {
        auto *app = Application::GetInstance();

        ParseCommandLineArgs(args);
        AgentController::Get().InitializeFromArgs(args);

        m_SceneRenderer = CreateRef<SceneRenderer>();
        const std::filesystem::path projectPath = DiscoverProjectPath(m_InitialProjectPath);
        if (!projectPath.empty())
        {
            OpenProject(projectPath);
        }

        if (app && app->GetWindow() && app->GetCreateInfo().useGui)
        {
            app->GetWindow()->Show();
        }

        if (m_AutoConnect)
        {
            if (auto *net = NetworkManager::GetInstance())
            {
                net->Connect(m_HostAddress, static_cast<uint16_t>(m_HostPort), m_PlayerName);
            }
        }
    }

    RuntimeLayer::~RuntimeLayer()
    {
    }

    void RuntimeLayer::OnAttach()
    {
        BindSharedImGuiContext();
    }

    void RuntimeLayer::OnDetach()
    {
        CloseCurrentProject();
    }

    void RuntimeLayer::SyncActiveSceneFromProject()
    {
        if (!m_ActiveProject)
            return;

        Ref<Scene> projectScene = m_ActiveProject->LockActiveScene();
        if (!projectScene || projectScene == m_ActiveScene)
            return;

        m_ActiveScene = projectScene;
        if (m_SceneRenderer)
        {
            m_SceneRenderer->SetActiveScene(m_ActiveScene);
        }
        GameUISystem::SetSceneContext(m_ActiveScene.get());
        if (!m_ActiveScene->IsPlaying() && !m_ActiveScene->IsSimulating())
        {
            m_ActiveScene->OnStart(ESceneState::Play);
        }
        if (auto *net = NetworkManager::GetInstance())
        {
            net->SetActiveScene(m_ActiveScene.get());
        }
        m_FixedAccumulator = 0.0f;
    }

    void RuntimeLayer::OnUpdate(float deltaTime)
    {
        if (auto *assetManager = AssetManager::GetInstance())
        {
            assetManager->OnUpdate(deltaTime);
        }

        SyncActiveSceneFromProject();

        ICamera *activeCamera = nullptr;
        if (m_ActiveScene && m_ActiveProject)
        {
            float mouseX = 0.0f;
            float mouseY = 0.0f;
            SDL_GetMouseState(&mouseX, &mouseY);
            InputSystem::SetGameplayMousePosition(mouseX, mouseY, true);

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
            SyncActiveSceneFromProject();

            if (Entity primaryCam = m_ActiveScene->GetPrimaryCamera(); primaryCam && primaryCam.HasComponent<CameraComponent>())
            {
                activeCamera = reinterpret_cast<ICamera *>(&primaryCam.GetComponent<CameraComponent>().camera);
            }
        }

        AgentExecutionContext agentCtx;
        agentCtx.scene = m_ActiveScene.get();
        agentCtx.project = m_ActiveProject.get();
        agentCtx.sceneRenderer = m_SceneRenderer.get();
        agentCtx.camera = activeCamera;
        agentCtx.onSceneReplaced = [this](const Ref<Scene> &newScene)
        {
            if (m_ActiveScene && m_ActiveScene != newScene)
            {
                m_ActiveScene->OnStop();
            }
            m_ActiveScene = newScene;
            if (m_ActiveProject)
            {
                m_ActiveProject->SetActiveScene(m_ActiveScene);
            }
            if (m_SceneRenderer)
            {
                m_SceneRenderer->SetActiveScene(m_ActiveScene);
            }
            GameUISystem::SetSceneContext(m_ActiveScene.get());
            if (m_ActiveScene && !m_ActiveScene->IsPlaying())
            {
                m_ActiveScene->OnStart(ESceneState::Play);
            }
            if (auto *net = NetworkManager::GetInstance())
            {
                net->SetActiveScene(m_ActiveScene.get());
            }
            m_FixedAccumulator = 0.0f;
        };
        AgentController::Get().Update(deltaTime, agentCtx);
    }

    void RuntimeLayer::OnRender(RenderTarget *framebuffer)
    {
        SyncActiveSceneFromProject();
        if (!m_ActiveScene || !m_ActiveProject)
            return;

        Entity primaryCam = m_ActiveScene->GetPrimaryCamera();
        if (!primaryCam || !primaryCam.HasComponent<CameraComponent>())
            return;

        auto &cc = primaryCam.GetComponent<CameraComponent>();

        const glm::uvec2 desiredSize = glm::max(glm::uvec2(0), m_FramebufferSize);
        if (desiredSize.x == 0 || desiredSize.y == 0)
            return;

        ProcessCameraViewportResize((ICamera *)&cc.camera, desiredSize);

        FrameContext *frameContext = Renderer::GetCurrentFrameContext();
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
                if (primaryCamera.HasComponent<CameraComponent>())
                {
                    const auto &cc = primaryCamera.GetComponent<CameraComponent>();
                    if (camera == &cc.camera)
                    {
                        postProcessing = cc.camera.postProcessing;
                    }
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
                m_SceneRenderer->ResizeFramebuffer(camera, desiredSize.x, desiredSize.y);
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
                if (auto *net = NetworkManager::GetInstance()) net->SetActiveScene(m_ActiveScene.get());
            }
        }
        LOG_ASSERT(m_ActiveScene, "[Runtime] Failed to load default scene!");
    }

    void RuntimeLayer::CloseCurrentProject()
    {
        if (!m_ActiveProject)
            return;

        GameUISystem::SetSceneContext(nullptr);
        if (auto *net = NetworkManager::GetInstance()) net->SetActiveScene(nullptr);

        if (m_ActiveScene)
            m_ActiveScene->OnStop();

        if (m_SceneRenderer)
            m_SceneRenderer->SetActiveScene(nullptr);

        if (m_ActiveProject)
            m_ActiveProject->SetActiveScene(nullptr);

        m_ActiveScene.reset();
        m_SceneRenderer.reset();

        Project::CloseActive();
        m_ActiveProject.reset();

        if (auto *am = AssetManager::GetInstance())
        {
            am->Reset();
        }
    }

    void RuntimeLayer::OnEvent(Event &e)
    {
        BindSharedImGuiContext();

        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<FramebufferResizeEvent>(BIND_CLASS_EVENT_FN(RuntimeLayer::OnFramebufferResize));
        dispatcher.Dispatch<KeyPressedEvent>(BIND_CLASS_EVENT_FN(RuntimeLayer::OnKeyPressed));
    }

    bool RuntimeLayer::OnKeyPressed(const KeyPressedEvent &event)
    {
        if (event.GetKeyCode() == Key::F1 || event.GetKeyCode() == Key::GraveAccent)
        {
            m_ShowNetworkOverlay = !m_ShowNetworkOverlay;
            return true;
        }
        return false;
    }

    void RuntimeLayer::ParseCommandLineArgs(const ApplicationCommandLineArgs &args)
    {
        auto parseConnect = [this](const std::string &connStr)
        {
            const auto colonPos = connStr.find(':');
            if (colonPos != std::string::npos)
            {
                strncpy(m_HostAddress, connStr.substr(0, colonPos).c_str(), sizeof(m_HostAddress) - 1);
                m_HostAddress[sizeof(m_HostAddress) - 1] = '\0';
                m_HostPort = std::stoi(connStr.substr(colonPos + 1));
            }
            else
            {
                strncpy(m_HostAddress, connStr.c_str(), sizeof(m_HostAddress) - 1);
                m_HostAddress[sizeof(m_HostAddress) - 1] = '\0';
            }
            m_AutoConnect = true;
            if (!m_HideOverlay)
            {
                m_ShowNetworkOverlay = true;
            }
        };

        for (int i = 1; i < args.count; i++)
        {
            const std::string_view arg(args[i]);
            if (arg == "--project" && i + 1 < args.count)
            {
                m_InitialProjectPath = args[++i];
            }
            else if (arg.starts_with("--project="))
            {
                m_InitialProjectPath = std::string(arg.substr(10));
            }
            else if (arg == "--connect" && i + 1 < args.count)
            {
                parseConnect(args[++i]);
            }
            else if (arg.starts_with("--connect="))
            {
                parseConnect(std::string(arg.substr(10)));
            }
            else if (arg == "--port" && i + 1 < args.count)
            {
                m_HostPort = std::stoi(args[++i]);
            }
            else if (arg == "--host" && i + 1 < args.count)
            {
                strncpy(m_HostAddress, args[++i], sizeof(m_HostAddress) - 1);
                m_HostAddress[sizeof(m_HostAddress) - 1] = '\0';
            }
            else if (arg == "--hide-overlay")
            {
                m_HideOverlay = true;
                m_ShowNetworkOverlay = false;
            }
            else if (arg == "--show-overlay")
            {
                m_HideOverlay = false;
                m_ShowNetworkOverlay = true;
            }
        }
    }

    void RuntimeLayer::OnGuiRender()
    {
        BindSharedImGuiContext();

        if (m_HideOverlay)
            return;

        if (m_DiagnosticsOpen)
        {
            if (auto *net = NetworkManager::GetInstance())
            {
                net->OnImGuiRender(&m_DiagnosticsOpen);
            }
        }

        if (!m_ShowNetworkOverlay)
            return;

        auto *net = NetworkManager::GetInstance();
        const bool isConnected = net && net->IsConnected();
        const bool isConnecting = net && net->IsConnecting();

        ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(380.0f, 0.0f), ImGuiCond_FirstUseEver);

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse;
        if (ImGui::Begin("Runtime Live-Edit & Network [F1 to toggle]", &m_ShowNetworkOverlay, windowFlags))
        {
            // Status banner
            if (isConnected)
            {
                ImGui::TextColored(ImVec4(0.2f, 0.95f, 0.3f, 1.0f), "[CONNECTED] Live Edit Interop Active");
                ImGui::Text("Client ID: %u | Server Tick: %u", net->GetClientId(), net->GetClientServerTick());
                NetStats stats = net->GetStats();
                ImGui::Text("RTT: %d ms | Loss: %.1f%%", stats.rttMs, stats.packetLossPercent);
                ImGui::Text("Bandwidth: In: %.1f KB/s | Out: %.1f KB/s", stats.inKBytesPerSec, stats.outKBytesPerSec);
            }
            else if (isConnecting)
            {
                ImGui::TextColored(ImVec4(0.95f, 0.85f, 0.2f, 1.0f), "[CONNECTING] Handshaking with Host...");
            }
            else
            {
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "[OFFLINE] Standalone Runtime");
            }

            ImGui::Separator();

            // Host & Port Configuration
            ImGui::BeginDisabled(isConnected || isConnecting);
            ImGui::InputText("Host Address", m_HostAddress, sizeof(m_HostAddress));
            ImGui::InputInt("Host Port", &m_HostPort);
            ImGui::InputText("Player Name", m_PlayerName, sizeof(m_PlayerName));
            ImGui::EndDisabled();

            ImGui::Spacing();

            if (!isConnected && !isConnecting)
            {
                if (ImGui::Button("Connect to Editor", ImVec2(160, 0)))
                {
                    if (net)
                    {
                        net->Connect(m_HostAddress, static_cast<uint16_t>(m_HostPort), m_PlayerName);
                    }
                }
            }
            else
            {
                if (ImGui::Button("Disconnect", ImVec2(120, 0)))
                {
                    if (net)
                    {
                        net->Disconnect();
                    }
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Diagnostics", ImVec2(100, 0)))
            {
                m_DiagnosticsOpen = !m_DiagnosticsOpen;
            }

            ImGui::Separator();
            ImGui::TextDisabled("Press F1 or ~ to toggle this overlay");
        }
        ImGui::End();
    }
}
