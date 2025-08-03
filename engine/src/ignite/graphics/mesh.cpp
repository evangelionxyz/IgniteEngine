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

        m_VertexBuffer->SetData(Buffer(data.vertices.data(), sizeof(VertexMesh_Anim) * data.vertices.size()));
        m_IndexBuffer->SetData(Buffer(data.indices.data(), sizeof(uint32_t) * data.indices.size()));
    }

    void MeshInstance::UpdateBindingSet()
    {
        // TODO: Remove this code
        constant.transformation = glm::mat4(1.0f);
        constant.normal = glm::mat4(1.0f);

        nvrhi::IDevice* device = Application::GetGraphicsDevice();;

        // create per Mesh constant buffers
        auto bufferDesc = nvrhi::BufferDesc();
        bufferDesc.setIsConstantBuffer(true);
        bufferDesc.setIsVolatile(true);
        bufferDesc.setMaxVersions(16);
        bufferDesc.setInitialState(nvrhi::ResourceStates::ConstantBuffer);
        bufferDesc.setDebugName("MeshConstantBuffer");
        bufferDesc.setByteSize(sizeof(SkinnedMeshConstants));
        constantBuffer = device->createBuffer(bufferDesc);
        LOG_ASSERT(constantBuffer, "[MeshRenderer] Failed to create mesh constant buffer");

        // Create binding set
        auto desc = nvrhi::BindingSetDesc();
        desc.addItem(nvrhi::BindingSetItem::PushConstants(0, sizeof(CameraConstants)));
        desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(1, constantBuffer));
        desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(2, SceneRenderer::GetActive()->GetEnvironment()->GetDirLightBuffer()));
        desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(3, SceneRenderer::GetActive()->GetEnvironment()->GetParamsBuffer()));

        const auto newBindingSet = device->createBindingSet(desc, Renderer::GetBindingLayout(GLayoutMap::MESH_ANIM));
        LOG_ASSERT(newBindingSet, "Failed to create binding set");

        if (newBindingSet)
        {
            bindingSet = newBindingSet;
        }
    }

    void MeshInstance::SetMaterial(const Ref<Material>& mat)
    {
        material = mat;
    }

    std::vector<Ref<MeshInstance>> MeshAsset::Create()
    {
        // create meshes
        std::vector<Ref<MeshInstance>> instances(meshesData.size());

        for (size_t i = 0; i < meshesData.size(); ++i)
        {
            // Create mesh instance
            instances[i] = CreateRef<MeshInstance>();
            auto& m = instances[i];

            m->mesh.data = meshesData[i];
            m->mesh.CreateBuffers();

            // Material
            m->SetMaterial(materials[m->mesh.data.materialIndex]);
            m->UpdateBindingSet();
        }

        return instances;
    }

    

}
