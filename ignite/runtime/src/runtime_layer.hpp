// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_RUNTIME_LAYER_HPP
#define IGN_RUNTIME_LAYER_HPP

#include <ignite/core/layer.hpp>
#include <ignite/core/types.hpp>
#include <ignite/core/input/app_event.hpp>
#include <ignite/core/input/key_event.hpp>
#include <ignite/core/application.hpp>

namespace ignite
{
    // Forward Declaration
    class Scene;
    class Project;
    class SceneRenderer;
    class ICamera;

    class RuntimeLayer final : public Layer
    {
    public:
        explicit RuntimeLayer(const std::string &name, const ApplicationCommandLineArgs &args = {});
        virtual ~RuntimeLayer() override;

        virtual void OnAttach() override;
        virtual void OnDetach() override;
        virtual void OnUpdate(float deltaTime) override;
        virtual void OnEvent(Event &e) override;
        virtual void OnGuiRender() override;

        virtual void OnRender(RenderTarget *framebuffer) override;

    private:
        bool OnFramebufferResize(const FramebufferResizeEvent &event);
        bool OnKeyPressed(const KeyPressedEvent &event);

        void ProcessCameraViewportResize(ICamera *camera, const glm::uvec2 &desiredSize);

        void OpenProject(const std::filesystem::path &filepath);
        void OnOpenProject();
        void CloseCurrentProject();
        void ParseCommandLineArgs(const ApplicationCommandLineArgs &args);

    private:
        Ref<Scene> m_ActiveScene;
        Ref<Project> m_ActiveProject;
        Ref<SceneRenderer> m_SceneRenderer;

        glm::uvec2 m_FramebufferSize = { 0, 0 };
        int m_ResizingFrame = 0;

        // Network & Live Edit UI State
        bool m_ShowNetworkOverlay = true;
        char m_HostAddress[128] = "127.0.0.1";
        int m_HostPort = 8192;
        char m_PlayerName[64] = "RuntimeClient";
        bool m_AutoConnect = false;
        bool m_DiagnosticsOpen = false;
        std::filesystem::path m_InitialProjectPath;
    };
}

#endif
