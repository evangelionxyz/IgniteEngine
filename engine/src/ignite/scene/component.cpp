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

#include "component.hpp"
#include "ignite/graphics/mesh.hpp"
#include "ignite/graphics/scene_renderer.hpp"

namespace ignite
{

    MeshRenderer::MeshRenderer(const MeshRenderer& other)
        : IComponent(other)
    {
        if (!other.mesh)
            return;

        mesh = CreateRef<Mesh>(*other.mesh.get());

        cullMode = other.cullMode;
        fillMode = other.fillMode;
        material = other.material;
        transformData = other.transformData;

        root = other.root;
    }

    void  MeshRenderer::Create(bool _isSkinnedMesh)
    {
        isSkinnedMesh = _isSkinnedMesh;

        if (isSkinnedMesh)
        {
            nvrhi::IDevice *device = Application::GetGraphicsDevice();

            // create per Mesh constant buffers
            auto bufferDesc = nvrhi::BufferDesc();
            bufferDesc.setIsConstantBuffer(true);
            bufferDesc.setIsVolatile(true);
            bufferDesc.setMaxVersions(16);
            bufferDesc.setInitialState(nvrhi::ResourceStates::ConstantBuffer);
            bufferDesc.setDebugName("MeshConstantBuffer");
            bufferDesc.setByteSize(sizeof(SkinnedMeshConstants));
            transformBufferHandle = device->createBuffer(bufferDesc);
            LOG_ASSERT(transformBufferHandle, "[MeshRenderer] Failed to create mesh constant buffer");
        }
        else
        {
        }

        UpdateBindingSet();
    }

    void MeshRenderer::UpdateBindingSet()
    {
        nvrhi::IDevice *device = Application::GetGraphicsDevice();

        auto desc = nvrhi::BindingSetDesc();
        desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, Renderer::GetCameraBufferHandle()));
        desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(1, transformBufferHandle));
        desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(2, SceneRenderer::GetActive()->GetEnvironment()->GetDirLightBuffer()));
        desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(3, SceneRenderer::GetActive()->GetEnvironment()->GetParamsBuffer()));

        const auto newBindingSet = device->createBindingSet(desc, Renderer::GetBindingLayout(GLayoutMap::MESH));
        LOG_ASSERT(newBindingSet, "Failed to create binding set");

        if (newBindingSet)
        {
            bindingSet = newBindingSet;
        }
    }

    void MeshRenderer::WriteTransformBuffer(nvrhi::ICommandList *commandList) const
    {
        commandList->writeBuffer(transformBufferHandle, &transformData, sizeof(transformData));
    }
}
