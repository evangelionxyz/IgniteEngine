//Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef EDITOR_LAYER_HPP
#define EDITOR_LAYER_HPP

#include "ignite/core/layer.hpp"
#include "ignite/project/project.hpp"
#include "ignite/core/input/event.hpp"
#include "ignite/core/input/mouse_event.hpp"
#include "ignite/core/input/app_event.hpp"
#include "ignite/core/input/key_event.hpp"

#include "ignite/core/signal_bus.hpp"
#include "ignite/core/signals/signals.hpp"
#include "ignite/core/signals/asset_signal.hpp"

#include "ignite/core/application.hpp"
#include "ignite/graphics/texture.hpp"
#include "ignite/graphics/renderer.hpp"

#include "ignite/graphics/renderer/scene_renderer.hpp"
#include "ignite/graphics/renderer/asset_scene_renderer.hpp"

#include <nvrhi/nvrhi.h>

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
        friend class ScenePanel;
        friend class AssetImporterPanel;
        friend class AssetEditorPanel;
        friend class ContentBrowserPanel;

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
            int editorPlayResizingFrame = 0;
            bool editorResizing = false;
            bool gameplayResizing = false;
            bool editorPlayResizing = false;

            bool editorRequestToResize = false;
            bool gameplayRequestToResize = false;

            uint32_t hoveredEntity = static_cast<uint32_t>(-1);
            ProjectInfo projectCreateInfo;

            nvrhi::RasterFillMode rasterFillMode = nvrhi::RasterFillMode::Solid;
            nvrhi::RasterCullMode rasterCullMode = nvrhi::RasterCullMode::Front;
        };

    public:
        explicit EditorLayer(const std::string &name);
        virtual ~EditorLayer() override;

        virtual void OnAttach() override;
        virtual void OnDetach() override;
        virtual void OnUpdate(float deltaTime) override;
        virtual void OnEvent(Event &e) override;

        bool OnKeyPressedEvent(KeyPressedEvent &event);
	    bool OnMouseMovedEvent(MouseMovedEvent &event);

        virtual void OnRender(nvrhi::IFramebuffer *framebuffer) override;
        virtual void OnGuiRender() override;

        void OnScenePlay();
        void OnSceneStop();
        void OnSceneSimulate();
        void NewScene();
        void SaveScene();
        void SaveSceneAs();
        void SaveScene(const std::filesystem::path &filepath) const;
        void OpenScene();
        void OpenScene(const std::filesystem::path &filepath);

        void SaveProject();
        void OpenProject();
        void CloseCurrentProject();
        void OpenProject(const std::filesystem::path &filepath);

        void SetActiveScene(const Ref<Scene> &scene);

        void EnterPrefabIsolation(AssetHandle prefabHandle);
        void ExitPrefabIsolation(bool save = true);

        bool IsInPrefabIsolation() const { return m_InPrefabIsolationMode; }
        [[nodiscard]] Ref<Prefab> GetEditingPrefab() const { return m_EditingPrefab; }
        [[nodiscard]] AssetHandle GetEditingPrefabHandle() const { return m_EditingPrefabHandle; }

        static EditorLayer *GetInstance() { return s_Instance; }

        [[nodiscard]] Ref<Scene> GetActiveScene() const { return m_ActiveScene; }
        [[nodiscard]] Ref<Project> GetActiveProject() const { return m_ActiveProject; }

        SceneRenderer *GetSceneRenderer() { return m_SceneRenderer.get(); }
        ICamera *GetEditorPlayCamera() { return &m_EditorPlayCamera; }
        uint32_t GetActiveDockspaceID() const { return m_ActiveEditorDockspaceId; }

        EditorState &GetState() { return m_State; }

        void RefreshContentBrowsers();

        void SetStatusText(const std::string_view text) { m_StatusText = text; }
        void SetLoadingProgress(const float progress) { m_LoadingProgress = progress; }

    private:
        static void OnSceneSaveFileSelected(void *userData, const char *const *filelist, int filter);
        static void OnSceneOpenFileSelected(void *userData, const char *const *filelist, int filter);

        static void OnProjectSaveFileSelected(void *userData, const char *const *filelist, int filter);
        static void OnProjectOpenFileSelected(void *userData, const char *const *filelist, int filter);

        static void OnScreenshotSaveFileSelected(void *userData, const char *const *filelist, int filter);
        static void OnProjectFolderSelected(void *userData, const char *const *filelist, int filter);

        void OnProjectReadySignal(const SuccessResultSignal &signal);

        void OnFileImport(const FileImportPayload &payload);
        void AddContentBrowserPanel();
        void ReloadContentBrowserPanels() const;
        uint32_t GetOpenContentBrowserCount() const;

        void UIProjectCreation();
        void UISettings();
        void UISceneRenderer();

        void ProcessCameraViewportResize(ICamera *camera, const glm::uvec2 &desiredSize, bool &isResizing, int &resizingFrame, bool &requestToResize) const;

        ScenePanel *m_ScenePanel;
        AssetImporterPanel *m_AssetImporterPanel;
        ContentBrowserPanel *m_ContentBrowserPanel;
        AssetEditorPanel *m_AssetEditorPanel;
        std::vector<ContentBrowserPanel *> m_ContentBrowserPanels;

        Ref<SceneRenderer> m_SceneRenderer;

        Ref<Scene> m_ActiveScene;
        Ref<Scene> m_EditorScene;
        Ref<Scene> m_MainSceneBeforeIsolation;
        Ref<Prefab> m_EditingPrefab;
        AssetHandle m_EditingPrefabHandle = AssetHandle(0);
        bool m_InPrefabIsolationMode = false;
        Ref<Project> m_ActiveProject;
        EditorState m_State;

        ICamera m_EditorPlayCamera;

        std::filesystem::path m_CurrentSceneFilePath;
    	std::filesystem::path m_CurrentProjectFilepath;

        nvrhi::BufferHandle m_DebugRenderBuffer;
        nvrhi::StagingTextureHandle m_MousePickingStagingTexture;
        nvrhi::CommandListHandle m_Cmd;
        glm::vec2 m_CurrentFramebufferSize;

        nvrhi::IDevice *m_Device = nullptr;

        AssetHandle m_CurrentSceneHandle = AssetHandle(0);

        uint32_t m_PendingContentBrowserPanelsToAdd = 0;
        std::unordered_set<ContentBrowserPanel *> m_ContentBrowserPanelsPendingRemoval;
        uint32_t m_NextContentBrowserPanelId = 1;
        uint32_t m_ActiveEditorDockspaceId = 0;

        SignalToken m_FileImportSignalToken = kInvalidSignalToken;
        SignalToken m_ProjectReadySignalToken = kInvalidSignalToken;

        std::string m_StatusText;
        float m_LoadingProgress = 0.0f;

        static EditorLayer *s_Instance;

        friend class ScenePanel;
        friend class AssetImporterPanel;
    };
}

#endif
