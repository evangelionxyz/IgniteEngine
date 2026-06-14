// Copyright (c) 2026 Evangelion Manuhutu | IGNITE STUDIO

#pragma once
#ifndef IGN_RUNTIME_LAYER_HPP
#define IGN_RUNTIME_LAYER_HPP

#include "engine/core/types.hpp"
#include "engine/core/layer.hpp"
#include "renderer/graphics/scene_renderer.hpp"
#include "renderer/graphics/command_list.hpp"

#include <nvrhi/nvrhi.h>
#include <filesystem>

namespace ignite
{
    class Scene;
    class Project;
    class RenderTarget;
    class GraphicsPipeline;
    class VertexBuffer;

    class RuntimeLayer : public Layer
    {
    public:
        RuntimeLayer(const std::string &name);
        ~RuntimeLayer();

        virtual void OnAttach() override;
        virtual void OnDetach() override;
        virtual void OnUpdate(float deltaTime) override;
        virtual void OnRender(nvrhi::IFramebuffer *framebuffer) override;
        virtual void OnEvent(Event &e) override;
        virtual void OnGuiRender() override;

    private:
        bool OnResizeEvent(FramebufferResizeEvent &event);
        void CreatePipeline(nvrhi::IFramebuffer *framebuffer);
        void UpdateBindingSet();

        Ref<Project> OpenProject();
        Ref<Project> OpenProject(const std::filesystem::path &filepath);

        Ref<Project> m_ActiveProject;
        Ref<Scene> m_ActiveScene;
        Ref<VertexBuffer> m_ScreenVertexBuffer;
        nvrhi::BindingSetHandle m_BindingSet;

        Ref<RenderTarget> m_SceneRT;
        Ref<RenderTarget> m_UIRT;
        Ref<RenderTarget> m_CompositeRT;

        Ref<GraphicsPipeline> m_CompositePipeline;

        SceneRenderer m_SceneRenderer;

        std::filesystem::path m_CurrentProjectPath;
        std::filesystem::path m_CurrentScenePath;

        nvrhi::DeviceHandle m_Device;
        Ref<CommandList> m_CommandList;

        struct
        {
            glm::vec2 position;
            glm::vec2 size;
            glm::vec2 mousePosition;
        } m_ViewportData;
    };
}

#endif
