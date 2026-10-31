// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_OUTLINE_JFA_HPP
#define IGN_OUTLINE_JFA_HPP

#include "ignite/core/types.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "buffers/constant_buffer.hpp"
#include "binding_set.hpp"

#include <glm/glm.hpp>
#include <array>

namespace ignite
{
    class Shader;
    class Texture;

    struct OutlineJFAParameter
    {
        glm::vec4 outlineColor = glm::vec4(1.0f, 0.5f, 0.1f, 1.0f);
        float outlineWidth = 2.5f;
        uint32_t selectedCount = 0;
        glm::vec2 _padding;
    };

    class OutlineJFA
    {
    public:
        static constexpr size_t kMaxFloodSteps = 7; // Steps: 1, 2, 4, 8, 16, 32, 64

        OutlineJFA();
        ~OutlineJFA();

        void CreatePipeline();
        void UpdateBindingSet(const Ref<Texture> &objectIDTexture);
        void ExecuteCompute(nri::CommandBuffer *commandBuffer, const OutlineJFAParameter &params, uint32_t width, uint32_t height);
        void CreateOutputTexture(uint32_t width, uint32_t height);

        void SetSelectedIDs(const uint32_t *ids, uint32_t count);
        nri::Buffer *GetSelectedIDBuffer() const { return m_SelectedIDBuffer; }
        nri::Descriptor *GetSelectedIDDescriptor() const { return m_SelectedIDDescriptor; }
        Ref<Texture> GetOutputTexture() const { return m_OutputTexture; }

        static Ref<OutlineJFA> Create();

    private:
        Ref<Shader> m_SeedShader;
        Ref<Shader> m_FloodShader;
        Ref<Shader> m_OutlineShader;

        nri::PipelineLayout *m_SeedPipelineLayout = nullptr;
        nri::PipelineLayout *m_FloodPipelineLayout = nullptr;
        nri::PipelineLayout *m_OutlinePipelineLayout = nullptr;

        nri::Pipeline *m_SeedPipeline = nullptr;
        nri::Pipeline *m_FloodPipeline = nullptr;
        nri::Pipeline *m_OutlinePipeline = nullptr;

        Ref<ConstantBuffer> m_SeedCB;
        std::array<Ref<ConstantBuffer>, kMaxFloodSteps> m_FloodCBs;
        Ref<ConstantBuffer> m_OutlineCB;

        // Resources
        nri::Buffer *m_SelectedIDBuffer = nullptr;
        nri::Memory *m_SelectedIDMemory = nullptr;
        nri::Descriptor *m_SelectedIDDescriptor = nullptr;

        BindingLayoutDesc m_SeedBindingLayout;
        BindingLayoutDesc m_FloodBindingLayout;
        BindingLayoutDesc m_OutlineBindingLayout;

        Ref<BindingSet> m_SeedBindingSet;
        std::array<Ref<BindingSet>, kMaxFloodSteps> m_FloodBindingSetPingToPong;
        std::array<Ref<BindingSet>, kMaxFloodSteps> m_FloodBindingSetPongToPing;
        Ref<BindingSet> m_OutlineBindingSetPing;
        Ref<BindingSet> m_OutlineBindingSetPong;

        // Textures
        Ref<Texture> m_JFAPing;
        Ref<Texture> m_JFAPong;
        Ref<Texture> m_OutputTexture;
        nri::Texture *m_CurrentObjectIDTexture = nullptr;
    };
}

#endif
