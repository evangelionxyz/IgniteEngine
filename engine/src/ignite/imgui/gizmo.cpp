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

#include "gizmo.hpp"

#include <ImGuizmo.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace ignite {

    void Gizmo::SetInfo(const GizmoInfo &info)
    {
        m_GizmoInfo = info;

        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(
            m_GizmoInfo.viewRect.min.x,
            m_GizmoInfo.viewRect.min.y,
            m_GizmoInfo.viewRect.GetSize().x,
            m_GizmoInfo.viewRect.GetSize().y);

        ImGuizmo::SetOrthographic(m_GizmoInfo.cameraType == ProjectionType::Orthographic);
    }

    void Gizmo::SetOperation(ImGuizmo::OPERATION op)
    {
        m_Operation = op;
    }

    void Gizmo::SetMode(ImGuizmo::MODE mode)
    {
        m_Mode = mode;
    }

    void Gizmo::Manipulate(glm::mat4 &inOutMatrix)
    {
        // X, Y, Z axes
        f32 snapValues[] = { m_GizmoInfo.snapValue, m_GizmoInfo.snapValue, m_GizmoInfo.snapValue };

        ImGuizmo::Manipulate(glm::value_ptr(m_GizmoInfo.cameraView),
            glm::value_ptr(m_GizmoInfo.cameraProjection),
            m_Operation, m_Mode,
            glm::value_ptr(inOutMatrix), // matrix 
            nullptr, // delta matrix
            m_GizmoInfo.isSnapping ? snapValues : nullptr, // snap
            nullptr, // local bound
            nullptr // bound snap
        );
    }

    void Gizmo::DrawGrid(float gridSize, const glm::mat4 &gridMatrix)
    {
        ImGuizmo::DrawGrid(
            glm::value_ptr(m_GizmoInfo.cameraView),
            glm::value_ptr(m_GizmoInfo.cameraProjection),
            glm::value_ptr(gridMatrix),
            10.0f
        );
    }

    bool Gizmo::IsManipulating() const
    {
        return ImGuizmo::IsUsing();
    }

    bool Gizmo::IsHovered() const
    {
        return ImGuizmo::IsOver();
    }

}
