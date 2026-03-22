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
        explicit ScenePanel(const char *windowTitle);
        virtual ~ScenePanel() override;
        
        void SetActiveScene(const Ref<Scene> &scene);

        void OnUpdate(f32 deltaTime) override;
        void OnGuiRender() override;
        void RenderViewport();

        void ResizeFramebuffer(uint32_t width, uint32_t height);

        void OnEvent(Event &event);
        bool OnMouseScrolledEvent(MouseScrolledEvent &event);
        bool OnMouseMovedEvent(MouseMovedEvent &event);
        bool OnJoystickConnectionEvent(JoystickConnectionEvent &event);

        void SetGizmoOperation(ImGuizmo::OPERATION op);
        void SetGizmoMode(ImGuizmo::MODE mode);

        bool IsGizmoBeingUse() const { return m_Data.isGizmoBeingUse; }
        
        EditorCamera &GetViewportCamera() { return m_Camera; }

        const glm::vec2 &GetViewportMousePos() const { return m_ViewportData.mousePos; }

        void RenderHierarchy();
        Entity ShowEntityContextMenu();
        void RenderEntityNode(Entity entity);
        
        void RenderInspector();
        void UISettings();
        void UpdateCameraInput(f32 deltaTime);
        void DestroyEntity(Entity entity);
        void DuplicateSelectedEntity();
        void ClearSelection();

        Entity SetSelectedEntity(Entity entity);
        Entity GetSelectedEntity();

        glm::vec2 GetViewportSize() const { return m_ViewportData.rect.GetSize(); }

        const std::unordered_map<UUID, Entity> &GetSelectedEntities() { return m_SelectedEntities; }

        const Ref<RenderTarget> &GetSceneViewportRT() { return m_SceneViewportRT; }
        const Ref<RenderTarget> &GetCompositeViewportRT() { return m_CompositeViewportRT; }
        const Ref<RenderTarget> &GetUIViewportRT() { return m_UIViewportRT; }
        const Ref<RenderTarget> &GetUICameratRT() { return m_UICameraRT; }
        
        const Ref<RenderTarget> &GetSceneCameraRT() { return m_SceneCameraRT; }
        const Ref<RenderTarget> &GetCompositeCameraRT() { return m_CompositeCameraRT; }

        template<typename T, typename UIFunction>
        void RenderComponent(const std::string &name, Entity entity, UIFunction uiFunction, bool allowedToRemove = true);

    private:
        EditorCamera m_Camera;
        Ref<Scene> m_Scene;
        Gizmo m_Gizmo;
        std::unordered_map<UUID, Entity> m_SelectedEntities;
        std::unordered_map<std::string, Ref<Texture>> m_Icons;

        static UUID m_TrackingSelectedEntity;

        // For viewport
        Ref<RenderTarget> m_SceneViewportRT;
        Ref<RenderTarget> m_UIViewportRT;
        Ref<RenderTarget> m_CompositeViewportRT;
        
        // For camera preview
        Ref<RenderTarget> m_SceneCameraRT;
        Ref<RenderTarget> m_CompositeCameraRT;
        Ref<RenderTarget> m_UICameraRT;

		struct ViewportData
		{
			Rect rect = { 0, 0, 1, 1 };
			glm::vec2 mousePos = glm::vec2(0.0f);
            float snapValue = 0.05f;
			bool wantMouseDragging = false;
		};

        ViewportData m_ViewportData;

		struct Data
		{
			bool settingsWindow = true;
			bool isGizmoManipulating = false;
			bool isGizmoBeingUse = false;
		} m_Data;
    };
}
