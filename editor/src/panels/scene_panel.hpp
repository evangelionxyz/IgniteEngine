//Copyright (c) 2026 Evangelion Manuhutu | IGNITE STUDIO

#pragma once

#include "ipanel.hpp"

#include "ignite/graphics/render_target.hpp"
#include "ignite/scene/entity.hpp"
#include "ignite/core/uuid.hpp"
#include "ignite/core/types.hpp"
#include "ignite/imgui/gizmo.hpp"
#include "../editor_camera.hpp"
#include <string>
#include <glm/fwd.hpp>

namespace ignite
{
    class Scene;
    class Event;
    class MouseScrolledEvent;
    class MouseMovedEvent;
    class JoystickConnectionEvent;
    class EditorLayer;

    class ScenePanel final : public IPanel
    {
    public:
        explicit ScenePanel(const char *windowTitle, EditorLayer *editor);
        virtual ~ScenePanel() override;
        
        void SetActiveScene(const Ref<Scene> &scene);

        void OnUpdate(float deltaTime) override;
        void OnGuiRender() override;

        // Viewports
        void RenderSceneEditViewport();
        void RenderSceneGameViewport();
        void RenderToolbar();

        void ViewportEditResize(uint32_t width, uint32_t height);
        void ViewportGameResize(uint32_t width, uint32_t height);

        void OnEvent(Event &event);
        bool OnMouseScrolledEvent(MouseScrolledEvent &event);
        bool OnMouseMovedEvent(MouseMovedEvent &event);
        bool OnJoystickConnectionEvent(JoystickConnectionEvent &event);

        void SetGizmoOperation(GizmoOperation op);
        void SetGizmoMode(ImGuizmo::MODE mode);

        bool IsGizmoBeingUse() const { return m_Data.isGizmoBeingUse; }
        
        EditorCamera &GetViewportCamera() { return m_EditorCamera; }

        const glm::vec2 &GetViewportMousePos() const { return m_ViewportData.mousePos; }

        void RenderHierarchy();
        Entity ShowEntityContextMenu();
        void RenderEntityNode(Entity entity);
        
        void RenderInspector();
        void UpdateCameraInput(float deltaTime);
        void DestroyEntity(Entity entity);
        void DuplicateSelectedEntity();
        void ClearSelection();

        Entity SetSelectedEntity(Entity entity);
        Entity GetSelectedEntity();

        glm::vec2 GetViewportEditSize() const { return m_ViewportEditRT.rect.GetSize(); }
        glm::vec2 GetViewportGameSize() const { return m_ViewportGameRT.rect.GetSize(); }

        const std::unordered_map<UUID, Entity> &GetSelectedEntities() { return m_SelectedEntities; }

        const Ref<RenderTarget> &GetViewportEditSceneRT() { return m_ViewportEditRT.scene; }
        const Ref<RenderTarget> &GetViewportEditUIRT() { return m_ViewportEditRT.ui; }
        const Ref<RenderTarget> &GetViewportEditCompRT() { return m_ViewportEditRT.composite; }

		const Ref<RenderTarget> &GetViewportGameSceneRT() { return m_ViewportGameRT.scene; }
		const Ref<RenderTarget> &GetViewportGameUIRT() { return m_ViewportGameRT.ui; }
		const Ref<RenderTarget> &GetViewportGameCompRT() { return m_ViewportGameRT.composite; }
        
        template<typename T, typename UIFunction>
        void RenderComponent(const std::string &name, Entity entity, UIFunction uiFunction, bool allowedToRemove = true);

        void Render2DBoundsSizing();
        bool Is2DResizableEntity(Entity entity) const;
        glm::vec3 ScreenToWorldOnPlane(const glm::vec2 &screenPos, float planeZ, bool *isValid = nullptr);

    private:
        EditorCamera m_EditorCamera;
        std::optional<EditorCamera> m_EditorCamera2D;
        std::optional<EditorCamera> m_EditorCamera3D;

        Ref<Scene> m_Scene;
        Gizmo m_Gizmo;
        std::unordered_map<UUID, Entity> m_SelectedEntities;
        std::unordered_map<std::string, Ref<Texture>> m_Icons;

        static UUID m_TrackingSelectedEntity;

        struct ViewportRenderTarget
        {
            Ref<RenderTarget> scene;
            Ref<RenderTarget> ui;
            Ref<RenderTarget> composite;

            Rect rect;
        };

        ViewportRenderTarget m_ViewportEditRT;
        ViewportRenderTarget m_ViewportGameRT;

		struct ViewportData
		{
			glm::vec2 mousePos = glm::vec2(0.0f);
            float snapValue = 0.05f;
            float panSnapValue = 0.0025f;
			bool wantMouseDragging = false;
		};

        ViewportData m_ViewportData;

		struct Data
		{
			bool settingsWindow = true;
			bool isGizmoManipulating = false;
			bool isGizmoBeingUse = false;

            bool sceneViewportGameplayVisible = false;
            bool sceneViewportEditorVisible = false;

            float gamePreviewZoom = 1.0f;
            glm::vec2 gamePreviewPan = glm::vec2(0.0f);

            bool is2DBoundsSizing = false;
            bool is2DBoundsHovered = false;
            int active2DCorner = -1;
            UUID active2DEntity = UUID(0);
            float active2DPlaneZ = 0.0f;
            glm::vec3 active2DAxisX = glm::vec3(1.0f, 0.0f, 0.0f);
            glm::vec3 active2DAxisY = glm::vec3(0.0f, 1.0f, 0.0f);
            glm::vec3 active2DOppositeWorld = glm::vec3(0.0f);
            GizmoOperation gizmoOp;
            TransformComponent before2DResize;
		} m_Data;

        friend class EditorLayer;
    };
}
