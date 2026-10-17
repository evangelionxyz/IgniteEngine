// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_RUNTIME_LAYER_HPP
#define IGN_RUNTIME_LAYER_HPP

#include <ignite/core/layer.hpp>
#include <ignite/core/types.hpp>
#include <ignite/core/input/app_event.hpp>

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
        explicit RuntimeLayer(const std::string &name);
        virtual ~RuntimeLayer() override;

        virtual void OnAttach() override;
        virtual void OnDetach() override;
        virtual void OnUpdate(float deltaTime) override;
        virtual void OnEvent(Event &e) override;

        virtual void OnRender(nvrhi::IFramebuffer *framebuffer) override;

    private:
        bool OnFramebufferResize(const FramebufferResizeEvent &event);

        void ProcessCameraViewportResize(ICamera *camera, const glm::uvec2 &desiredSize);

        void OpenProject(const std::filesystem::path &filepath);
        void OnOpenProject();
        void CloseCurrentProject();

    private:
        Ref<Scene> m_ActiveScene;
        Ref<Project> m_ActiveProject;
        Ref<SceneRenderer> m_SceneRenderer;

        glm::uvec2 m_FramebufferSize = { 0, 0 };
        int m_ResizingFrame = 0;
    };
}

#endif
