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

#include "icamera.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/core/application.hpp"

namespace ignite
{
    ICamera::ICamera()
    : position({0.0f, 0.0f, 0.0f})
    , m_AspectRatio(16.0f / 9.0f)
    , zoom(1.0f)
    , yaw(0.0f)
    , pitch(0.0f)
    , projectionMatrix(glm::mat4(1.0f))
    , viewMatrix(glm::mat4(1.0f))
    , nearClip(0.1f)
    , farClip(300.0f)
    , width(1280.0f)
    , height(720.0f)
    , fov(45.0f)
    , projectionType(Type::Orthographic)
    {
        auto device = Application::GetDeviceManager()->GetDevice();

        // Create camera constant buffer
        nvrhi::BufferDesc cameraConstantBufferDesc;
        cameraConstantBufferDesc.byteSize = sizeof(CameraConstants);
        cameraConstantBufferDesc.isConstantBuffer = true;
        cameraConstantBufferDesc.isVolatile = true;
        cameraConstantBufferDesc.debugName = "Camera constant buffer";
        cameraConstantBufferDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
        cameraConstantBufferDesc.keepInitialState = true;
        cameraConstantBufferDesc.maxVersions = 16;

        m_Buffer = device->createBuffer(cameraConstantBufferDesc);
    }

    void ICamera::CreateOrthographic(f32 width, f32 height, f32 zoom, f32 nearClip, f32 farClip)
    {
        projectionType = Type::Orthographic;

        this->nearClip = nearClip;
        this->farClip = farClip;
        this->zoom = zoom;

        SetSize(width, height);
        UpdateProjectionMatrix();
        UpdateViewMatrix();
    }

    void ICamera::CreatePerspective(f32 fov, f32 width, f32 height, f32 nearClip, f32 farClip)
    {
        projectionType = Type::Perspective;
        this->fov = fov;
        this->nearClip = nearClip;
        this->farClip = farClip;

        SetSize(width, height);
        UpdateProjectionMatrix();
        UpdateViewMatrix();
    }
    void ICamera::SetSize(const f32 w, const f32 h)
    {
        width = w;
        height = h;
        m_AspectRatio = width / height;
    }

    void ICamera::UpdateProjectionMatrix()
    {
        switch (projectionType)
        {
            case Type::Orthographic:
            {
                f32 orthoWidth = zoom * m_AspectRatio / 2.0f;
                f32 orthoHeight = zoom / 2.0f;
                projectionMatrix = glm::ortho(-orthoWidth, orthoWidth, -orthoHeight, orthoHeight, nearClip, farClip);
                break;
            }
            case Type::Perspective:
            default:
            {
                projectionMatrix = glm::perspectiveZO(glm::radians(fov), m_AspectRatio, nearClip, farClip);
                break;
            }
        }
    }

    void ICamera::UpdateViewMatrix()
    {
        switch (projectionType)
        {
            case Type::Orthographic:
            default:
                viewMatrix = glm::translate(glm::mat4(1.0f), position);
            break;
            case Type::Perspective:
                viewMatrix = glm::translate(glm::mat4(1.0f), position) * glm::toMat4(glm::quat({ -pitch, -yaw, 0.0f }));
            break;
        }
        viewMatrix = glm::inverse(viewMatrix);
    }

    glm::vec2 ICamera::GetSize()
    {
        return { width, height };
    }

    glm::vec3 ICamera::GetUpDirection() const
    {
        return glm::rotate(glm::quat({ -pitch, -yaw, 0.0f }), { 0.0f, 1.0f, 0.0f });
    }

    glm::vec3 ICamera::GetRightDirection() const
    {
        return glm::rotate(glm::quat({ -pitch, -yaw, 0.0f }), { 1.0f, 0.0f, 0.0f });
    }

    glm::vec3 ICamera::GetForwardDirection() const
    {
        return glm::rotate(glm::quat({ -pitch, -yaw, 0.0f }), { 0.0f, 0.0f, -1.0f });
    }
}
