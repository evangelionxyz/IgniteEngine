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

#include "ipanel.hpp"

#include "ignite/graphics/render_target.hpp"
#include "ignite/scene/entity.hpp"
#include "ignite/core/uuid.hpp"
#include "ignite/core/base.hpp"
#include "ignite/core/types.hpp"
#include "ignite/imgui/gizmo.hpp"

#include "../editor_camera.hpp"

#include <string>
#include <glm/fwd.hpp>
#include <nvrhi/nvrhi.h>

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
        ~ScenePanel();
        
        void SetActiveScene(Scene *scene);

        void OnUpdate(f32 deltaTime) override;
        void OnGuiRender() override;
        void RenderViewport();

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
        void CameraSettingsUI();
        void UpdateCameraInput(f32 deltaTime);
        void DestroyEntity(Entity entity);
        void DuplicateSelectedEntity();
        void ClearSelection();

        Entity SetSelectedEntity(Entity entity);
        Entity GetSelectedEntity();

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

        void DebugRender();

        struct Data
        {
            bool settingsWindow = true;
            bool isGizmoManipulating = false;
            bool isGizmoBeingUse = false;
        } m_Data;

        EditorCamera m_Camera;
        EditorLayer *m_Editor;

        Scene *m_Scene = nullptr;
        Gizmo m_Gizmo;

        std::unordered_map<UUID, Entity> m_SelectedEntities;

        static UUID m_TrackingSelectedEntity;

        struct CameraData
        {
            f32 moveSpeed = 6.0f;
            const f32 maxMoveSpeed = 500.0f;
            const f32 rotationSpeed = 0.8f;
            glm::vec3 lastPosition = { 0.0f, 0.0f, 0.0f };
        } m_CameraData;

        struct ViewportData
        {
            Rect rect = { 0, 0, 1, 1 };
            glm::vec2 mousePos = glm::vec2(0.0f);
            bool wantMouseDragging = false;
        } m_ViewportData;

        std::unordered_map<std::string, Ref<Texture>> m_Icons;

        // For viewport
        Ref<RenderTarget> m_SceneViewportRT;
        Ref<RenderTarget> m_UIViewportRT;
        Ref<RenderTarget> m_CompositeViewportRT;
        
        // For camera preview
        Ref<RenderTarget> m_SceneCameraRT;
        Ref<RenderTarget> m_CompositeCameraRT;
        Ref<RenderTarget> m_UICameraRT;
    };
}
