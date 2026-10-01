// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_AVALONIA_LAYER_H
#define IGN_AVALONIA_LAYER_H

#include "ignite/core/layer.hpp"
#include "ignite/scene/editor_camera.hpp"
#include "ignite/scene/entity.hpp"

#include "ignite/imgui/gizmo.hpp"

namespace ignite
{
    class Scene;
    class SceneRenderer;

    class AvaloniaLayer : public Layer
    {
    public:
        AvaloniaLayer(uint32_t width, uint32_t height);

        void OnAttach() override;
        void OnDetach() override;
        void Resize(uint32_t width, uint32_t height);
        void OnUpdate(float deltaTime) override;
        void OnRender(nvrhi::IFramebuffer *mainFramebuffer) override;
        void OnGuiRender() override;

        // Scene Management
        void SetActiveScene(const Ref<Scene> &scene);
        bool SceneNew();
        bool SceneOpen(const std::filesystem::path &filepath);
        bool SceneSave();
        bool SceneSave(const std::filesystem::path &filepath);
        AssetHandle GetCurrentSceneHandle() const { return m_CurrentSceneHandle; }
        AssetHandle GetActiveSceneHandle() const;
        const std::filesystem::path &GetCurrentSceneFilePath() const { return m_CurrentSceneFilepath; }
        bool IsCurrentSceneSaved() const;

        void ScenePlay();
        void SceneSimulate();
        void SceneStop();
        void ScenePause();
        void SceneStepFrame(int frames);
        int SceneGetState();
        Ref<Scene> GetActiveScene();

        void OnEvent(Event &e) override;
        void SetNavigationMode(int mode);
        int GetNavigationMode() const;
        EditorCamera &GetEditorCamera() { return m_EditorCamera; }

        UUID PickEntity(float mouseX, float mouseY, uint32_t viewportWidth, uint32_t viewportHeight, bool isDoubleClick = false, bool isShiftDown = false);
        // Viewport-originated selection changes notify the managed UI.
        void SelectSingleEntityFromViewport(UUID uuid);
        void AddEntityToSelectionFromViewport(UUID uuid);
        void DeselectEntityFromViewport(UUID uuid);
        void ClearSelectionFromViewport();

        // Managed hierarchy synchronization is silent and never emits callbacks.
        void SyncSingleSelection(UUID uuid);
        void SyncSelectedEntities(const uint64_t *uuids, uint32_t count);
        void SyncClearSelection();
        bool IsEntitySelected(UUID uuid) const;
        uint32_t GetSelectedEntities(uint64_t *outUuids, uint32_t maxCount) const;
        uint32_t GetSelectedEntityCount() const;

        void SetMultiSelect(bool multiSelect) { m_MultiSelect = multiSelect; }
        bool IsMultiSelect() const { return m_MultiSelect; }

        Entity GetSelectedEntity() { return m_SelectedEntities.empty() ? Entity{} : m_SelectedEntities.begin()->second; }
        UUID GetSelectedEntityUUID() { return m_SelectedEntities.empty() ? UUID(0) : m_SelectedEntities.begin()->first; }

        using EntitySelectedCallback = void (*)(uint64_t uuid, bool isMultiSelect);
        void SetEntitySelectedCallback(EntitySelectedCallback callback) { m_EntitySelectedCallback = callback; }

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

        using GizmoOperationChangedCallback = void (*)(int op);
        void SetGizmoOperationChangedCallback(GizmoOperationChangedCallback callback) { m_GizmoOperationChangedCallback = callback; }

        // Project
        bool ProjectSave();
        bool ProjectClose();

        void OnOpenProject();

    private:

        void DrawGizmo();
        void ApplyGizmoOperation();
        Entity ApplySingleSelection(Entity entity);

    private:
        // Project
        Ref<Project> m_ActiveProject;
        std::filesystem::path m_CurrentProjectFilepath;
        AssetHandle m_CurrentProjectAssetHandle = AssetHandle(0);

        // Scenes
        Ref<Scene> m_EditorScene;
        Ref<Scene> m_ActiveScene;
        Ref<Scene> m_MainSceneBeforeIsolation;

        Ref<SceneRenderer> m_SceneRenderer;
        std::filesystem::path m_CurrentSceneFilepath;
        AssetHandle m_CurrentSceneHandle = AssetHandle(0);

        // Prefab
        Ref<Prefab> m_EditingPrefab;
        AssetHandle m_EditingPrefabHandle = AssetHandle(0);
        bool m_IsInPrefabIsolationMode = false;

        std::unordered_map<UUID, Entity> m_SelectedEntities;

        // Camera & Gizmo
        EditorCamera m_EditorCamera;
        uint32_t m_Width = 1280;
        uint32_t m_Height = 720;

        Gizmo m_Gizmo;
        GizmoOperation m_GizmoOperation = GizmoOperation::NONE;
        bool m_SnapEnabled = true;
        bool m_IsGizmoManipulating = false;
        bool m_IsGizmoBeingUse = false;
        bool m_IsCameraNavigating = false;
        std::array<float, 3> m_SnapValues = { 0.5f, 15.0f, 0.25f };
        float m_PanningSnapValue = 0.0025f;

        EntitySelectedCallback m_EntitySelectedCallback = nullptr;
        GizmoOperationChangedCallback m_GizmoOperationChangedCallback = nullptr;
        std::chrono::steady_clock::time_point m_LastClickTime{};
        glm::ivec2 m_LastClickPos{ 0, 0 };
        bool m_MultiSelect = false;
    };
}

#endif
