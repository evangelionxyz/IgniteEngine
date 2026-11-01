// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_ENVIRONMENT_HPP
#define IGN_ENVIRONMENT_HPP

#include "ignite/graphics/texture.hpp"
#include "ignite/graphics/binding_set.hpp"
#include "ignite/graphics/buffers/index_buffer.hpp"
#include "ignite/graphics/buffers/vertex_buffer.hpp"

#include <glm/glm.hpp>

#include <array>
#include <string>

namespace ignite
{
    class GraphicsPipeline;
    class ConstantBuffer;
    class RenderTarget;
    class Scene;
    class ICamera;
    class ProceduralSky;
    enum class SkyType : uint32_t;

    class IGN_API Environment : public Asset
    {
    public:
        Environment();
        virtual ~Environment() override;

        void Draw(nri::CommandBuffer *cmd, RenderTarget *rt, const Ref<GraphicsPipeline> &gp,
            const ConstantBuffer &cameraBuffer, const ConstantBuffer &sceneBuffer);
        void Draw(nri::CommandBuffer *cmd, RenderTarget *rt, const Ref<GraphicsPipeline> &gp,
            const Ref<ConstantBuffer> &cameraBuffer, const Ref<ConstantBuffer> &sceneBuffer);

        void LoadTexture(const std::string &filepath);
        void SetTexture(const Ref<Texture> &texture);
        void WriteBuffer(nri::CommandBuffer *cmd = nullptr);

        static Ref<Environment> Create();
        static BindingLayoutDesc GetBindingLayoutDesc();

        Ref<Texture> GetHDRTexture() { return m_HDRTexture; }

        SkyType GetSkyType() const { return m_SkyType; }
        void SetSkyType(SkyType type) { m_SkyType = type; }

        Ref<ProceduralSky> &GetProceduralSky() { return m_ProceduralSky; }

    private:

        void CreateVerticesIndices();

        Ref<VertexBuffer> m_VertexBuffer;
        Ref<IndexBuffer> m_IndexBuffer;
        Ref<Texture> m_HDRTexture;
        Ref<ProceduralSky> m_ProceduralSky;
        SkyType m_SkyType;
        nri::Descriptor *m_Sampler = nullptr;

        std::array<glm::vec3, 24> m_Vertices;
        std::array<uint32_t, 36> m_Indices;
    };
}

#endif
