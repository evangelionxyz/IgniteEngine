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

#include <nvrhi/nvrhi.h>
#include <glm/glm.hpp>
#include "ignite/scene/icamera.hpp"
#include "ignite/core/types.hpp"

namespace ignite
{
    struct VertexMesh
    {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec3 tangent;
        glm::vec3 bitangent;
        glm::vec2 uv;
    };

    struct VertexMesh_Anim
    {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec3 tangent;
        glm::vec3 bitangent;
        glm::vec2 uv;
        uint32_t boneIDs[4] = { 0 };
        float weights[4] = { 0.0f };

        static nvrhi::BindingLayoutDesc GetBindingLayoutDesc()
        {
            return nvrhi::BindingLayoutDesc()
                .setRegisterSpace(0) // set 0
                .setRegisterSpaceIsDescriptorSet(true)
                .setVisibility(nvrhi::ShaderType::All)
                .addItem(nvrhi::BindingLayoutItem::ConstantBuffer(0))         // Camera
                .addItem(nvrhi::BindingLayoutItem::ConstantBuffer(1))         // Object
                .addItem(nvrhi::BindingLayoutItem::ConstantBuffer(2))         // Skeleton
                .addItem(nvrhi::BindingLayoutItem::ConstantBuffer(3))         // Scene
                .addItem(nvrhi::BindingLayoutItem::ConstantBuffer(4));        // CSM
        }
    };

    struct VertexScreen
    {
        glm::vec2 position;
        glm::vec2 texCoord;
    };

    struct Vertex2DQuad
    {
        glm::vec3 position;
        glm::vec2 texCoord;
        glm::vec2 tilingFactor;
        glm::vec4 color;
        glm::vec4 additiveColor;
        uint32_t texIndex;
        uint32_t materialType;
        uint32_t objectID;
    };

    struct Vertex2DLine
    {
        glm::vec3 position;
        glm::vec4 color;
    };

    struct Vertex2DCircle
    {
        glm::vec4 position;
        glm::vec2 localPosition;
        glm::vec4 color;
        uint32_t objectID;
    };

    struct VertexText
    {
        glm::vec3 position;
        glm::vec4 color;
        glm::vec2 texCoord;
        uint32_t texIndex;
        uint32_t objectID;
    };

    struct VertexWidgetQuad
    {
        glm::vec3 position;
        glm::vec2 texCoord;
        glm::vec2 tilingFactor;
        glm::vec4 color;
        uint32_t texIndex;
    };

    struct VertexWidgetText
    {
        glm::vec3 position;
        glm::vec4 color;
        glm::vec2 texCoord;
        uint32_t texIndex;
    };
}
