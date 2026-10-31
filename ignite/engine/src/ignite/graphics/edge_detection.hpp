// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_EDGE_DETECTION_HPP
#define IGN_EDGE_DETECTION_HPP

#include "ignite/core/types.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "buffers/constant_buffer.hpp"
#include "binding_set.hpp"

#include <glm/glm.hpp>

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
        void ExecuteCompute(nri::CommandBuffer *commandBuffer, const EdgeDetectionParameter &params, uint32_t width, uint32_t height);
        void CreateOutputTexture(uint32_t width, uint32_t height);

        void SetSelectedIDs(const uint32_t *ids, uint32_t count);
        nri::Buffer *GetSelectedIDBuffer() const { return m_SelectedIDBuffer; }
        nri::Descriptor *GetSelectedIDDescriptor() const { return m_SelectedIDDescriptor; }
        Ref<Texture> GetOutputTexture() const { return m_OutputTexture; }

        static Ref<EdgeDetection> Create();

    private:
        Ref<Shader> m_Shader;
        nri::PipelineLayout *m_PipelineLayout = nullptr;
        nri::Pipeline *m_Pipeline = nullptr;

        Ref<ConstantBuffer> m_ConstantBuffer;

        // Resources
        nri::Buffer *m_SelectedIDBuffer = nullptr;
        nri::Memory *m_SelectedIDMemory = nullptr;
        nri::Descriptor *m_SelectedIDDescriptor = nullptr;

        BindingLayoutDesc m_BindingLayout;
        Ref<BindingSet> m_BindingSet;

        // Texture
        Ref<Texture> m_OutputTexture;
        nri::Descriptor *m_Sampler = nullptr;
    };
}

#endif
