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

#include <nvrhi/nvrhi.h>
#include "ignite/core/layer.hpp"
#include "ignite/ignite.hpp"
#include "ignite/graphics/command_list.hpp"
#include "ignite/graphics/scene_renderer.hpp"
#include "ignite/serializer/serializer.hpp"
#include "ignite/project/project.hpp"
#include "states.hpp"
#include <future>

namespace ignite
{
    class ShaderFactory;
    class ScenePanel;
    class ContentBrowserPanel;
    class ModelViewerPanel;

    class EditorLayer final : public Layer
    {
    private:
        struct EditorData
        {
            bool debugMode = false;
            bool developerMode = false;
            bool multiSelect = false;
            bool settingsWindow = false;
            bool popupNewProjectModal = false;
            bool assetRegistryWindow = false;
            bool isPickingEntity = false;
            bool takeScreenshot = false;

            uint32_t hoveredEntity = static_cast<uint32_t>(-1);

            ProjectInfo projectCreateInfo;

            State sceneState = State::SceneEdit;
            nvrhi::RasterFillMode rasterFillMode = nvrhi::RasterFillMode::Solid;
            nvrhi::RasterCullMode rasterCullMode = nvrhi::RasterCullMode::Front;
        };

    public:
        EditorLayer(const std::string &name);

        void OnAttach() override;
        void OnDetach() override;
        void OnUpdate(f32 deltaTime) override;
        void OnEvent(Event& e) override;

        bool OnKeyPressedEvent(KeyPressedEvent &event);
        bool OnMouseButtonPressed(MouseButtonPressedEvent &event);

        void OnRender(nvrhi::IFramebuffer *framebuffer) override;
        void OnGuiRender() override;

        void SetActiveScene(Scene *scene);

        Scene *GetActiveScene() const { return m_ActiveScene.get(); }
        Project *GetActiveProject() const { return m_ActiveProject.get(); }
        SceneRenderer *GetSceneRenderer() { return &m_SceneRenderer; }

        EditorData &GetState() { return m_Data; }

    private:
        void NewScene();
        void SaveScene();
        void SaveSceneAs();
        void SaveScene(const std::filesystem::path &filepath) const;
        void OpenScene();
        void OpenScene(const std::filesystem::path &filepath);
        
        void SaveProject();
        void SaveProjectAs();
        Ref<Project> OpenProject();
        Ref<Project> OpenProject(const std::filesystem::path &filepath);

        void OnScenePlay();
        void OnSceneStop();
        void OnSceneSimulate();

        void SettingsUI();

        Ref<ScenePanel> m_ScenePanel;
        Ref<ContentBrowserPanel> m_ContentBrowserPanel;
        Ref<ModelViewerPanel> m_ModelViewerPanel;
        SceneRenderer m_SceneRenderer;

        Ref<Scene> m_ActiveScene;
        Ref<Scene> m_EditorScene;
        Ref<Project> m_ActiveProject;
        EditorData m_Data;

        std::filesystem::path m_CurrentSceneFilePath;
        nvrhi::BufferHandle m_DebugRenderBuffer;
        nvrhi::StagingTextureHandle m_MousePickingStagingTexture;
        nvrhi::StagingTextureHandle m_ScreenshotStagingTexture;
        Ref<CommandList > m_CommandList;
            
        nvrhi::IDevice *m_Device = nullptr;

        friend class ScenePanel;
    };
}
