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
        m_VertexBuffer = VertexBuffer::Create(sizeof(VertexMesh_Anim) * data.vertices.size());
        m_IndexBuffer = IndexBuffer::Create(sizeof(uint32_t) * data.indices.size());
    }

    void Mesh::WriteVertexBuffer(uint32_t entityID)
    {
        // for (auto &vertex : data.vertices)
        //     vertex.entityID = entityID;

        m_VertexBuffer->SetData(Buffer(data.vertices.data(), sizeof(VertexMesh_Anim) * data.vertices.size()));
        m_IndexBuffer->SetData(Buffer(data.indices.data(), sizeof(uint32_t) * data.indices.size()));
    }
}
