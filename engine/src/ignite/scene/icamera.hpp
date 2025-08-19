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

#include "ignite/core/types.hpp"
#include <nvrhi/nvrhi.h>
#include <string>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

namespace ignite
{
    struct CameraConstants
    {
        glm::mat4 viewProjection;
        glm::vec4 position;
    };

    class ICamera
    {
    public:
        enum class Type
        {
            Orthographic, Perspective
        };

        ICamera();
        ~ICamera() { }

        void CreateOrthographic(f32 width, f32 height, f32 zoom, f32 nearClip, f32 farClip);
        void CreatePerspective(f32 fov, f32 width, f32 height, f32 nearClip, f32 farClip);

        void UpdateProjectionMatrix();
        void UpdateViewMatrix();

        void SetSize(f32 w, f32 h);
        glm::vec2 GetSize();
        glm::mat4 GetViewProjectionMatrix() const { return projectionMatrix * viewMatrix; }

        glm::vec3 GetUpDirection() const;
        glm::vec3 GetRightDirection() const;
        glm::vec3 GetForwardDirection() const;
        f32 GetAspectRatio() const { return m_AspectRatio; }

        f32 zoom, width, height;
        f32 yaw, pitch, fov;
        f32 nearClip, farClip;

        glm::vec3 position;
        glm::mat4 viewMatrix;
        glm::mat4 projectionMatrix;
        Type projectionType;

    protected:
        f32 m_MinOrthoZoom = 1.5f;
        f32 m_MaxOrthoZoom = 100.0f;
        f32 m_AspectRatio = 1.0f;
    };
}
