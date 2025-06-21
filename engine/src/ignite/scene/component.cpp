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

        // transformBufferHandle = other.transformBufferHandle; //buffer handle should not to be copied

        meshSource = other.meshSource;
        meshIndex = other.meshIndex;

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
