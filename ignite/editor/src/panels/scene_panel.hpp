//Copyright (c) 2026 Evangelion Manuhutu | IGNITE STUDIO

#pragma once
#ifndef SCENE_PANEL_HPP
#define SCENE_PANEL_HPP

#include "ipanel.hpp"
#include "ignite/graphics/render_target.hpp"
#include "ignite/scene/entity.hpp"
#include "ignite/scene/scene_manager.hpp"
#include "ignite/core/uuid.hpp"
#include "ignite/core/types.hpp"
#include "ignite/graphics/gizmo/gizmo.hpp"
#include "editor_camera.hpp"
#include <optional>
#include <string>
#include <vector>
#include <glm/fwd.hpp>

namespace ignite
{
    class Scene;
    class SceneRenderer;
    class Event;
    class MouseScrolledEvent;
    class MouseMovedEvent;
    class JoystickConnectionEvent;
    class KeyPressedEvent;
    class EditorLayer;

    class ScenePanel final : public IPanel
    {
    public:
        explicit ScenePanel(const char *windowTitle, EditorLayer *editor);
        virtual ~ScenePanel() override;

        void SetActiveScene(Scene *scene);

        void OnUpdate(float deltaTime) override;
        void OnGuiRender() override;

        // Viewports
        void RenderSceneEditViewport();
        void RenderSceneGameViewport();
        void RenderToolbar();

        virtual void OnEvent(Event &event) override;
        bool OnMouseScrolledEvent(MouseScrolledEvent &event);
        bool OnMouseMovedEvent(MouseMovedEvent &event);
        bool OnJoystickConnectionEvent(JoystickConnectionEvent &event);
        bool OnKeyPressedEvent(KeyPressedEvent &event);

        void SetGizmoOperation(GizmoOperation op);
        void SetGizmoMode(GizmoMode mode);
        void SubmitGizmoForRender(SceneRenderer *renderer, ICamera *camera);

        bool IsGizmoBeingUse() const { return m_Data.isGizmoBeingUse; }

        EditorCamera &GetViewportCamera() { return m_EditorCamera; }

        const glm::vec2 &GetViewportMousePos() const { return m_ViewportData.mousePos; }

        void RenderHierarchy();
        Entity ShowEntityContextMenu();
        void RenderEntityNode(Entity entity, const std::string &filterLower = std::string());

        void SetHierarchyDirty() { m_HierarchyDirty = true; }
        void SortHierarchy(std::vector<Entity>& entities);
        int GetMaxParentDepth(Entity entity);

        void RenderInspector();
        void UpdateCameraInput(float deltaTime);
        void DestroyEntity(Entity entity);
        void DuplicateSelectedEntity();
        void ClearSelection(bool recordUndo = true);

        Entity SetSelectedEntity(Entity entity);
        void SetSelectedEntities(const std::vector<UUID> &entityUUIDs, UUID trackingUUID = UUID(0), bool recordUndo = true);
        Entity GetSelectedEntity();

        Entity CreateFolderInHierarchy(Entity parent = {});
        void GroupSelectedEntitiesInFolder();

        const std::unordered_map<UUID, Entity> &GetSelectedEntities() { return m_SelectedEntities; }

        template<typename T, typename UIFunction>
        void RenderComponent(const std::string &name, Entity entity, UIFunction uiFunction, bool allowedToRemove = true);

        AABB ComputeEntityLocalBounds(Entity entity) const;

    private:
        void ApplySelectionInternal(const std::vector<UUID> &entityUUIDs, UUID trackingUUID);
        std::vector<UUID> GetOrderedSelectedUUIDs() const;
        bool MatchesHierarchyFilter(Entity entity, const std::string &filterLower) const;
        bool HasSelectedDescendant(Entity current) const;

        struct PendingHierarchyDrop
        {
            std::vector<UUID> sourceUUIDs;
            UUID targetUUID = UUID(0);
            HierarchyDropPosition position = HierarchyDropPosition::Inside;
            bool toRoot = false;
        };

        std::optional<PendingHierarchyDrop> m_PendingHierarchyDrop;
        std::vector<UUID> m_PendingDeleteUUIDs;
        bool m_PendingGroupSelectionInFolder = false;
        UUID m_PendingUngroupFolderUUID = UUID(0);

        UUID m_RenamingEntityUUID = UUID(0);
        char m_RenameBuffer[256] = {};
        bool m_FocusRenameInput = false;
        char m_HierarchySearchBuffer[128] = {};
        UUID m_LastAutoExpandedSelection = UUID(0);

        EditorCamera m_EditorCamera;
        std::optional<EditorCamera> m_EditorCamera2D;
        std::optional<EditorCamera> m_EditorCamera3D;

        Scene *m_Scene;
        Gizmo m_Gizmo;
        std::unordered_map<UUID, Entity> m_SelectedEntities;
        std::unordered_map<std::string, Ref<Texture>> m_Icons;

        static UUID m_TrackingSelectedEntity;

		struct ViewportData
		{
			glm::vec2 mousePos = glm::vec2(0.0f);
            // Individual operation snap for TRANSLATE, ROTATION, SCALE
            // - Rotation in Degree
            std::array<float,3> snapValues = { 0.5f, 1.0f, 0.25f };
            float panSnapValue = 0.0025f;
			bool wantMouseDragging = false;
		} m_ViewportData;

		struct Data
		{
			bool isGizmoManipulating = false;
			bool isGizmoBeingUse = false;
			bool blockGizmoUntilMouseRelease = false;
            bool sceneViewportGameplayVisible = false;
            bool sceneViewportEditorVisible = false;
            float gamePreviewZoom = 1.0f;
            glm::vec2 gamePreviewPan = glm::vec2(0.0f);
            GizmoOperation gizmoOp = GizmoOperation::NONE;
		} m_Data;

        bool m_SceneFocused;
        int m_SceneFocusCooldown;
        bool m_EditorCameraMouseLocked = false;
        bool m_EditorCameraLockJustEngaged = false;

        bool m_HierarchyDirty = true;
        size_t m_LastEntityCount = 0;
        Scene *m_LastSyncedScene = nullptr;
        uint32_t m_HierarchySyncFrameCounter = 0;
        std::vector<Entity> m_RootEntities;

        friend class EditorLayer;
    };
}

#endif
