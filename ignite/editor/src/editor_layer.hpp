//Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef EDITOR_LAYER_HPP
#define EDITOR_LAYER_HPP

#include <nvrhi/nvrhi.h>
#include "ignite/asset/asset_importer.hpp"
#include "ignite/core/layer.hpp"
#include "ignite/ignite.hpp"
#include "ignite/graphics/renderer/scene_renderer.hpp"
#include "ignite/serializer/serializer.hpp"
#include "ignite/serializer/scene_serializer.hpp"
#include "ignite/core/signals/signals.hpp"
#include "ignite/project/project.hpp"
#include "ignite/scene/scene.hpp"
#include "states.hpp"

#include <future>
#include <optional>
#include <unordered_set>

namespace ignite
{
    class ShaderFactory;
    class ScenePanel;
    class AssetImporterPanel;
    class AssetEditorPanel;
    class ContentBrowserPanel;

    class EditorLayer final : public Layer
    {
    private:
        struct EditorState
        {
            bool debugMode = false;
            bool developerMode = false;
            bool multiSelect = false;
            bool settingsWindow = false;
            bool imguiDemoWindow = false;
            bool popupNewProjectModal = false;
            bool assetRegistryWindow = false;
            bool takeScreenshot = false;
            bool gameplayViewportWindow = false;
            bool consoleWindow = true;

            const int STABLE_RESIZE_FRAME = 12;

            int editorResizingFrame = 0;
            int gameplayResizingFrame = 0;
            bool editorResizing = false;
            bool gameplayResizing = false;

            float assetUnloadTimer = 0.0f;

            uint32_t hoveredEntity = static_cast<uint32_t>(-1);
            ProjectInfo projectCreateInfo;

            ESceneState sceneState = ESceneState::Stop;
            nvrhi::RasterFillMode rasterFillMode = nvrhi::RasterFillMode::Solid;
            nvrhi::RasterCullMode rasterCullMode = nvrhi::RasterCullMode::Front;
        };

    public:
        EditorLayer(const std::string &name);
        ~EditorLayer();

        void OnAttach() override;
        void OnDetach() override;
        void OnUpdate(float deltaTime) override;
        void OnEvent(Event &e) override;
        void OnSDLEvent(SDL_Event *evt) override;

        bool OnKeyPressedEvent(KeyPressedEvent &event);
        bool OnMouseButtonPressed(MouseButtonPressedEvent &event);
		bool OnMouseMovedEvent(MouseMovedEvent &event);

        void OnRender(nvrhi::IFramebuffer *framebuffer) override;
        void OnGuiRender() override;
        void OnScenePlay();
        void OnSceneStop();
        void OnSceneSimulate();
        void NewScene();
        void SaveScene();
        void SaveSceneAs();
        void SaveScene(const ignite::Path &filepath) const;
        void OpenScene();
        void OpenScene(const ignite::Path &filepath);
        
        void SaveProject();
        void SaveProjectAs();
        void OpenProject();
        void CloseCurrentProject();
        void OpenProject(const ignite::Path &filepath);

        void SetActiveScene(const Ref<Scene> &scene);

        Ref<Scene> GetActiveScene() const { return m_ActiveScene; }
        Ref<Project> GetActiveProject() const { return m_ActiveProject; }

        SceneRenderer *GetSceneRenderer() { return m_SceneRenderer.get(); }
        uint32_t GetActiveDockspaceID() const { return m_ActiveEditorDockspaceId; }

        EditorState &GetState() { return m_State; }

        void RefreshContentBrowsers();
        
        void SetStatusText(std::string_view text) { m_StatusText = text; }
        void SetLoadingProgress(float progress) { m_LoadingProgress = progress; }

    private:
        static void OnSceneSaveFileSelected(void *userData, const char *const *filelist, int filter);
        static void OnSceneOpenFileSelected(void *userData, const char *const *filelist, int filter);

        static void OnProjectSaveFileSelected(void *userData, const char *const *filelist, int filter);
        static void OnProjectOpenFileSelected(void *userData, const char *const *filelist, int filter);

        static void OnScreenshotSaveFileSelected(void *userData, const char *const *filelist, int filter);
        static void OnProjectFolderSelected(void *userData, const char *const *filelist, int filter);

        void OnProjectReadySignal(const SuccessResultSignal &signal);

        void ProcessPendingFileLoading();
        void AddContentBrowserPanel();
        void ReloadContentBrowserPanels();
        uint32_t GetOpenContentBrowserCount() const;

        void UIProjectCreation();
        void UISettings();
        void UISceneRenderer();

        ScenePanel *m_ScenePanel;
        AssetImporterPanel *m_AssetImporterPanel;
        ContentBrowserPanel *m_ContentBrowserPanel;
        AssetEditorPanel *m_AssetEditorPanel;
        std::vector<ContentBrowserPanel *> m_ContentBrowserPanels;

        Ref<SceneRenderer> m_SceneRenderer;

        Ref<Scene> m_ActiveScene;
        Ref<Scene> m_EditorScene;
        Ref<Project> m_ActiveProject;
        EditorState m_State;

        ignite::Path m_CurrentSceneFilePath;
    	ignite::Path m_CurrentProjectFilepath;

        std::vector<uint8_t> m_ScreenshotPixelData;
        int m_ScreenshotWidth = 0;
        int m_ScreenshotHeight = 0;

        nvrhi::BufferHandle m_DebugRenderBuffer;
        nvrhi::StagingTextureHandle m_MousePickingStagingTexture;
        nvrhi::StagingTextureHandle m_ScreenshotStagingTexture;
        nvrhi::CommandListHandle m_Cmd;
        glm::vec2 m_CurrentFramebufferSize;
            
        nvrhi::IDevice *m_Device = nullptr;

        int m_SelectedMesh = 0;
        void *m_MeshInstanceData = nullptr;
        std::optional<MeshScene> m_LoadedMeshScene;

        AssetHandle m_CurrentSceneHandle = AssetHandle(0);

        std::queue<PendingFileLoading> m_PendingFileLoading;
        uint32_t m_PendingContentBrowserPanelsToAdd = 0;
        std::unordered_set<ContentBrowserPanel *> m_ContentBrowserPanelsPendingRemoval;
        uint32_t m_NextContentBrowserPanelId = 1;
        uint32_t m_ActiveEditorDockspaceId = 0;

        SignalToken m_ProjectReadySignalToken = kInvalidSignalToken;
        
        std::string m_StatusText;
        float m_LoadingProgress = 0.0f;

        friend class ScenePanel;
        friend class AssetImporterPanel;
    };
}

#endif