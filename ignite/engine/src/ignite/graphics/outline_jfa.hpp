// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_OUTLINE_JFA_HPP
#define IGN_OUTLINE_JFA_HPP

#include "ignite/core/types.hpp"
#include "buffers/constant_buffer.hpp"

#include <glm/glm.hpp>
#include <nvrhi/nvrhi.h>
#include <nvrhi/utils.h>

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
        OutlineJFA();
        ~OutlineJFA();

        void CreatePipeline();
        void UpdateBindingSet(const Ref<Texture> &objectIDTexture);
        void ExecuteCompute(nvrhi::ICommandList *commandList, const OutlineJFAParameter &params, uint32_t width, uint32_t height);
        void CreateOutputTexture(uint32_t width, uint32_t height);

        nvrhi::BufferHandle GetSelectedIDBuffer() { return m_SelectedIDBuffer; }
        Ref<Texture> GetOutputTexture() const { return m_OutputTexture; }

        static Ref<OutlineJFA> Create();

    private:
        Ref<Shader> m_SeedShader;
        Ref<Shader> m_FloodShader;
        Ref<Shader> m_OutlineShader;

        nvrhi::ComputePipelineHandle m_SeedPipeline;
        nvrhi::ComputePipelineHandle m_FloodPipeline;
        nvrhi::ComputePipelineHandle m_OutlinePipeline;

        Ref<ConstantBuffer> m_SeedCB;
        Ref<ConstantBuffer> m_FloodCB;
        Ref<ConstantBuffer> m_OutlineCB;

        // Resources
        nvrhi::BufferHandle m_SelectedIDBuffer;

        nvrhi::BindingLayoutHandle m_SeedBindingLayout;
        nvrhi::BindingLayoutHandle m_FloodBindingLayout;
        nvrhi::BindingLayoutHandle m_OutlineBindingLayout;

        nvrhi::BindingSetHandle m_SeedBindingSet;
        nvrhi::BindingSetHandle m_FloodBindingSetPingToPong;
        nvrhi::BindingSetHandle m_FloodBindingSetPongToPing;
        nvrhi::BindingSetHandle m_OutlineBindingSetPing;
        nvrhi::BindingSetHandle m_OutlineBindingSetPong;

        // Textures
        Ref<Texture> m_JFAPing;
        Ref<Texture> m_JFAPong;
        Ref<Texture> m_OutputTexture;
        nvrhi::ITexture *m_CurrentObjectIDTexture = nullptr;
    };
}

#endif
