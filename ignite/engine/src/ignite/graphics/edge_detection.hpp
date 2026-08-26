// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_EDGE_DETECTION_HPP
#define IGN_EDGE_DETECTION_HPP

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
