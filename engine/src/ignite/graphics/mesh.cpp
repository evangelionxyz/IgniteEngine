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

#include "mesh.hpp"
#include "environment.hpp"
#include "scene_renderer.hpp"

namespace ignite
{
    void Mesh::CreateBuffers()
    {
        nvrhi::IDevice *device = Application::GetDeviceManager()->GetDevice();

        // create vertex buffer
        auto vbDesc = nvrhi::BufferDesc();
        vbDesc.isVertexBuffer = true;
        vbDesc.byteSize = sizeof(VertexMesh) * data.vertices.size();
        vbDesc.initialState = nvrhi::ResourceStates::VertexBuffer;
        vbDesc.keepInitialState = true;
        vbDesc.debugName = "[Mesh] vertex buffer";

        m_VertexBuffer = device->createBuffer(vbDesc);
        LOG_ASSERT(m_VertexBuffer, "[Mesh] Failed to create Vertex Buffer");

        // create index buffer
        auto ibDesc = nvrhi::BufferDesc();
        ibDesc.isIndexBuffer = true;
        ibDesc.byteSize = sizeof(uint32_t) * data.indices.size();
        ibDesc.initialState = nvrhi::ResourceStates::IndexBuffer;
        ibDesc.keepInitialState = true;
        ibDesc.debugName = "[Mesh] index buffer";

        m_IndexBuffer = device->createBuffer(ibDesc);
        LOG_ASSERT(m_IndexBuffer, "[Mesh] Failed to create Index Buffer");
    }

    void Mesh::WriteVertexBuffer(uint32_t entityID)
    {
        for (auto &vertex : data.vertices)
            vertex.entityID = entityID;

        nvrhi::IDevice *device = Application::GetDeviceManager()->GetDevice();

        // Write to buffers
        nvrhi::CommandListHandle commandList = device->createCommandList();
        commandList->open();
        commandList->writeBuffer(m_VertexBuffer, data.vertices.data(), sizeof(VertexMesh) * data.vertices.size());
        commandList->writeBuffer(m_IndexBuffer, data.indices.data(), sizeof(uint32_t) * data.indices.size());
        commandList->close();
        device->executeCommandList(commandList);
    }
}
