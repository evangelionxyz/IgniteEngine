/* MIT License
* 
* Copyright (c) 2025 Evangelion Manuhutu | IGNITE STUDIO
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#pragma once

#include "ignite/core/types.hpp"
#include "ignite/core/layer.hpp"
#include "ignite/graphics/scene_renderer.hpp"
#include "ignite/graphics/command_list.hpp"

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
