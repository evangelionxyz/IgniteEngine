// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_NATIVE_EMBEDDED_APP_H
#define IGN_NATIVE_EMBEDDED_APP_H

#include "ignite/core/application.hpp"

namespace ignite
{
    class AvaloniaLayer;
    class Scene;

    class NativeEmbeddedApp final : public Application
    {
    public:
        explicit NativeEmbeddedApp(const ApplicationCreateInfo &createInfo);
        void ResizeEmbedded(uint32_t width, uint32_t height);
        void SetCameraNavigationMode(int mode);
        int GetCameraNavigationMode() const;

        // Scene Management
        void SetActiveScene(const Ref<Scene> &scene);
        bool SceneNew();
        bool SceneOpen(const std::filesystem::path &filepath);
        bool SceneSave();
        bool SceneSave(const std::filesystem::path &filepath);
        uint64_t GetCurrentSceneHandle() const;
        uint64_t GetActiveSceneHandle() const;
        const std::filesystem::path &GetCurrentSceneFilePath() const;
        bool IsCurrentSceneSaved() const;

        void ScenePlay();
        void SceneSimulate();
        void SceneStop();
        void ScenePause();
        void SceneStepFrame(int frames);
        int SceneGetState() const;

        // Project
        bool ProjectSave();
        bool ProjectClose();

        uint64_t PickEntity(float mouseX, float mouseY, uint32_t viewportWidth, uint32_t viewportHeight, bool isDoubleClick, bool isShiftDown);
        void SelectSingleEntityFromViewport(uint64_t uuid);
        void SyncSingleSelection(uint64_t uuid);
        void AddEntityToSelectionFromViewport(uint64_t uuid);
        void DeselectEntityFromViewport(uint64_t uuid);
        bool IsEntitySelected(uint64_t uuid) const;
        void ClearSelectionFromViewport();
        void SyncClearSelection();
        void SyncSelectedEntities(const uint64_t *uuids, uint32_t count);
        uint32_t GetSelectedEntities(uint64_t *outUuids, uint32_t maxCount) const;
        uint32_t GetSelectedEntityCount() const;
        uint64_t GetSelectedEntity() const;
        void SetEntitySelectedCallback(void (*callback)(uint64_t uuid, bool isMultiSelect));

        // Gizmo & Snapping
        void SetGizmoOperation(int op);
        int GetGizmoOperation() const;
        void CycleGizmoOperation();
        void SetGizmoMode(int mode);
        int GetGizmoMode() const;
        void SetSnapEnabled(bool enabled);
        bool GetSnapEnabled() const;
        void SetSnapValue(int op, float value);
        float GetSnapValue(int op) const;
        void SetGizmoOperationChangedCallback(void (*callback)(int op));

        AvaloniaLayer *GetAvaloniaLayer() { return m_AvaloniaLayer; }

    private:
        AvaloniaLayer *m_AvaloniaLayer = nullptr;
    };
}

#endif
