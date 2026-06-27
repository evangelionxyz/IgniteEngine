// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_ENVIRONMENT_HPP
#define IGN_ENVIRONMENT_HPP

#include "ignite/graphics/texture.hpp"
#include "ignite/asset/asset.hpp"
#include "ignite/graphics/buffers/index_buffer.hpp"
#include "ignite/graphics/buffers/vertex_buffer.hpp"
#include "ignite/core/base.hpp"

#include <array>
#include <string>
#include <nvrhi/nvrhi.h>

namespace ignite
{
    class GraphicsPipeline;
    class ConstantBuffer;
    class Scene;
    class ICamera;

    class IGN_API Environment : public Asset
    {
    public:
        Environment();
    	virtual ~Environment() override;

        void Draw(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *fb, const Ref<GraphicsPipeline> &gp);

        bool UpdateBindingSet(const Ref<ConstantBuffer> &cameraBuffer, const Ref<ConstantBuffer> &sceneBuffer);

        void LoadTexture(const std::string &filepath);
        void SetTexture(const Ref<Texture> &texture);
        void WriteBuffer(nvrhi::ICommandList *cmd);

        static Ref<Environment> Create();
        static nvrhi::BindingLayoutDesc GetBindingLayoutDesc();

        Ref<Texture> GetHDRTexture() { return m_HDRTexture; }
    private:

        void CreateVerticesIndices();

        Ref<VertexBuffer> m_VertexBuffer;
        Ref<IndexBuffer> m_IndexBuffer;
        Ref<Texture> m_HDRTexture;
    	nvrhi::SamplerHandle m_Sampler;

        std::array<glm::vec3, 24> m_Vertices;
        std::array<uint32_t, 36> m_Indices;

        nvrhi::BindingSetHandle m_BindingSet;
    };
}

#endif
