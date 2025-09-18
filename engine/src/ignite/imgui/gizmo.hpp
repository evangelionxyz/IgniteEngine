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

#include <imgui.h>
#include <ImGuizmo.h>

#include "ignite/scene/icamera.hpp"

#include "ignite/math/math.hpp"

namespace ignite {
    
    struct GizmoInfo
    {
        glm::mat4 cameraView;
        glm::mat4 cameraProjection;
        ProjectionType cameraType;

        Rect viewRect;
        float snapValue = 0.25f;
        bool isSnapping = true;
    };

    class Gizmo
    {
    public:
        Gizmo() = default;

        void SetInfo(const GizmoInfo &info);

        void SetOperation(ImGuizmo::OPERATION op);
        void SetMode(ImGuizmo::MODE mode);

        ImGuizmo::MODE GetMode() { return m_Mode; }
        ImGuizmo::OPERATION GetOperation() { return m_Operation; }

        void Manipulate(glm::mat4 &inOutMatrix);
        void DrawGrid(float gridSize = 10.0f, const glm::mat4 &gridMatrix = glm::mat4(1.0f));

        bool IsManipulating() const;
        bool IsHovered() const;

    private:
        GizmoInfo m_GizmoInfo;
        ImGuizmo::MODE m_Mode = ImGuizmo::MODE::LOCAL;
        ImGuizmo::OPERATION m_Operation = ImGuizmo::OPERATION::NONE;
    };
}
