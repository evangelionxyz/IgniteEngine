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

#include "edge_detection.hpp"

#include "shader.hpp"

#include "renderer.hpp"
#include "ignite/core/application.hpp"
#include "ignite/core/logger.hpp"

namespace ignite
{

    EdgeDetection::EdgeDetection()
    {
        nvrhi::IDevice *device = Application::GetGraphicsDevice();

        auto bufferDesc = nvrhi::BufferDesc();
        bufferDesc.byteSize = sizeof(EdgeDetectionParameter);
        bufferDesc.isConstantBuffer = true;
        bufferDesc.isVolatile = true;
        bufferDesc.debugName = "Edge detection constant buffer";
        bufferDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
        bufferDesc.keepInitialState = true;
        bufferDesc.maxVersions = 16;
        m_ConstantBuffer = device->createBuffer(bufferDesc);

        bufferDesc = nvrhi::BufferDesc();
        bufferDesc.byteSize = sizeof(uint32_t) * 100; // allocate enough memory for selection
        bufferDesc.structStride = sizeof(uint32_t);
        bufferDesc.cpuAccess = nvrhi::CpuAccessMode::None;
        bufferDesc.debugName = "Selected Object IDs";
        bufferDesc.keepInitialState = true;
        bufferDesc.initialState = nvrhi::ResourceStates::ShaderResource;
        m_SelectedIDBuffer = device->createBuffer(bufferDesc);

        // Create linear sampler
        nvrhi::SamplerDesc samplerDesc;
        samplerDesc.minFilter = true;
        samplerDesc.magFilter = true;
        samplerDesc.addressU = nvrhi::SamplerAddressMode::Clamp;
        samplerDesc.addressV = nvrhi::SamplerAddressMode::Clamp;
        m_LinearSampler = device->createSampler(samplerDesc);

        // Create binding layout
        nvrhi::BindingLayoutDesc layoutDesc;
        layoutDesc.visibility = nvrhi::ShaderType::All;
        layoutDesc.bindings =
        {
            // constant buffer
            nvrhi::BindingLayoutItem::VolatileConstantBuffer(0),

            // Textures
            nvrhi::BindingLayoutItem::Texture_SRV(0), // Scene texture
            nvrhi::BindingLayoutItem::Texture_SRV(1), // Object ID texture
            nvrhi::BindingLayoutItem::Texture_SRV(2), // Depth texture
            nvrhi::BindingLayoutItem::StructuredBuffer_SRV(3), // Object ID texture

            // Sampler
            nvrhi::BindingLayoutItem::Sampler(0),

            // Output texture (for Compute)
            nvrhi::BindingLayoutItem::Texture_UAV(0)
        };

        m_BindingLayout = device->createBindingLayout(layoutDesc);

        m_Shader = Shader::Create("resources/shaders/sobel_edge_detection.compute.hlsl", ShaderType::Compute);
    }

    EdgeDetection::~EdgeDetection()
    {
    }

    void EdgeDetection::CreatePipeline()
    {
        nvrhi::IDevice *device = Application::GetGraphicsDevice();

        // Create compute pipeline
        nvrhi::ComputePipelineDesc computeDesc;
        computeDesc.CS = m_Shader->GetHandle();
        computeDesc.bindingLayouts = { m_BindingLayout };
        m_Pipeline = device->createComputePipeline(computeDesc);
    }

    void EdgeDetection::UpdateBindingSet(const nvrhi::TextureHandle &sceneTexture, const nvrhi::TextureHandle &objectIDTexture, const nvrhi::TextureHandle &depth)
    {
        nvrhi::IDevice *device = Application::GetGraphicsDevice();

        nvrhi::BindingSetDesc desc;
        desc.bindings =
        {
            nvrhi::BindingSetItem::ConstantBuffer(0, m_ConstantBuffer),
            nvrhi::BindingSetItem::Texture_SRV(0, sceneTexture),
            nvrhi::BindingSetItem::Texture_SRV(1, objectIDTexture),
            nvrhi::BindingSetItem::Texture_SRV(2, depth),
            nvrhi::BindingSetItem::StructuredBuffer_SRV(3, m_SelectedIDBuffer),
            nvrhi::BindingSetItem::Sampler(0, m_LinearSampler),
            nvrhi::BindingSetItem::Texture_UAV(0, m_OutputTexture),
        };

        m_BindingSet = device->createBindingSet(desc, m_BindingLayout);
        LOG_ASSERT(m_BindingSet, "[Edge Detection] Failed to create binding set");
    }

    void EdgeDetection::ExecuteCompute(nvrhi::ICommandList *commandList, const EdgeDetectionParameter &params, uint32_t width, uint32_t height)
    {
        // Update constant buffer
        commandList->writeBuffer(m_ConstantBuffer, &params, sizeof(params));

        // Transition output texture to UAV state before compute
        commandList->setTextureState(m_OutputTexture, nvrhi::TextureSubresourceSet(), nvrhi::ResourceStates::UnorderedAccess);
        commandList->commitBarriers(); // commit the barriers

        // Set compute pipeline
        nvrhi::ComputeState computeState;
        computeState.pipeline = m_Pipeline;
        computeState.bindings = { m_BindingSet };
        commandList->setComputeState(computeState);

        // Dispatch compute shader
        uint32_t groupsX = (width + 7) / 8; // 8x8 threads groups
        uint32_t groupsY = (height + 7) / 8;
        commandList->dispatch(groupsX, groupsY, 1);

        // Transition edge detection output from UAV to ShaderResource for composite pass
        commandList->setTextureState(m_OutputTexture, nvrhi::TextureSubresourceSet(), nvrhi::ResourceStates::ShaderResource);
        commandList->commitBarriers(); // Commit the barrier before composite pass
    }

    Ref<EdgeDetection> EdgeDetection::Create()
    {
        return CreateRef<EdgeDetection>();
    }

    void EdgeDetection::CreateOutputTexture(uint32_t width, uint32_t height)
    {
        nvrhi::IDevice *device = Application::GetGraphicsDevice();

        // Texture creation -> Common state
        // First use in Compute: Common -> UnorderedAccess (for writing)
        // After compute       : UnorderedAccess -> ShaderResource (for reading in composite pass)

        nvrhi::TextureDesc desc;
        desc.width = width;
        desc.height = height;
        desc.format = nvrhi::Format::RGBA8_UNORM;
        desc.initialState = nvrhi::ResourceStates::ShaderResource;
        desc.isUAV = true;
        desc.debugName = "SobelDetection Output Texture";
        m_OutputTexture = device->createTexture(desc);
    }

}
