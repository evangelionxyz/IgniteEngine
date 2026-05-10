/* MIT License
*
* Copyright (c) 2025 Evangelion Manuhutu
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

#ifndef EDGE_DETECTION_HPP
#define EDGE_DETECTION_HPP

#include "ignite/core/types.hpp"
#include "buffers/constant_buffer.hpp"

#include <glm/glm.hpp>
#include <nvrhi/nvrhi.h>
#include <nvrhi/utils.h>

namespace ignite
{
    class Shader;
    class Texture;

    struct EdgeDetectionParameter
    {
        glm::vec2 texelSize;
        float edgeThreshold = 0.1f;
        float outlineWidth = 2.0f;
        glm::vec4 outlineColor = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f);
        float depthSensitivity = 100.0f;
        int useObjectID = 1;
        uint32_t selectedCount = 0;
        uint32_t _padding;
    };

    class EdgeDetection
    {
    public:
        EdgeDetection();
        ~EdgeDetection();

        void CreatePipeline();
        void UpdateBindingSet(const Ref<Texture> &sceneTexture, const Ref<Texture> &objectIDTexture, const Ref<Texture> &depth);
        void ExecuteCompute(nvrhi::ICommandList *commandList, const EdgeDetectionParameter &params, uint32_t width, uint32_t height);
        void CreateOutputTexture(uint32_t width, uint32_t height);

        nvrhi::BufferHandle GetSelectedIDBuffer() { return m_SelectedIDBuffer; }
        Ref<Texture> GetOutputTexture() const { return m_OutputTexture; }

        static Ref<EdgeDetection> Create();

    private:
        Ref<Shader> m_Shader;
        nvrhi::ComputePipelineHandle m_Pipeline;

        Ref<ConstantBuffer> m_ConstantBuffer;

        // Resources
        nvrhi::BufferHandle m_SelectedIDBuffer;
        nvrhi::BindingLayoutHandle m_BindingLayout;
        nvrhi::BindingSetHandle m_BindingSet;

        // Texture
        Ref<Texture> m_OutputTexture;
    	nvrhi::SamplerHandle m_Sampler;
    };
}

#endif
