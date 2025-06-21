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

        vertexBuffer = device->createBuffer(vbDesc);
        LOG_ASSERT(vertexBuffer, "[Mesh] Failed to create Vertex Buffer");

        // create index buffer
        auto ibDesc = nvrhi::BufferDesc();
        ibDesc.isIndexBuffer = true;
        ibDesc.byteSize = sizeof(uint32_t) * data.indices.size();
        ibDesc.initialState = nvrhi::ResourceStates::IndexBuffer;
        ibDesc.keepInitialState = true;
        ibDesc.debugName = "[Mesh] index buffer";

        indexBuffer = device->createBuffer(ibDesc);
        LOG_ASSERT(indexBuffer, "[Mesh] Failed to create Index Buffer");
    }

    void Mesh::WriteVertexBuffer(uint32_t entityID)
    {
        for (auto &vertex : data.vertices)
            vertex.entityID = entityID;

        nvrhi::IDevice *device = Application::GetDeviceManager()->GetDevice();

        // Write to buffers
        nvrhi::CommandListHandle commandList = device->createCommandList();
        commandList->open();
        commandList->writeBuffer(vertexBuffer, data.vertices.data(), sizeof(VertexMesh) * data.vertices.size());
        commandList->writeBuffer(indexBuffer, data.indices.data(), sizeof(uint32_t) * data.indices.size());
        commandList->close();
        device->executeCommandList(commandList);
    }
}
