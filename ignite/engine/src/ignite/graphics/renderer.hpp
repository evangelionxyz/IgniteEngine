// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_RENDERER_HPP
#define IGN_RENDERER_HPP

#include "ignite/core/subsystem.hpp"
#include "ignite/core/types.hpp"
#include "graphics_pipeline.hpp"

#include <nvrhi/nvrhi.h>
#include <string>
#include <unordered_map>

namespace ignite
{
#define RENDER_MODE_COLOR 0
#define RENDER_MODE_DIFFUSE 1
#define RENDER_MODE_NORMALS 2
#define RENDER_MODE_METALLIC 3
#define RENDER_MODE_ROUGHNESS 4

    class DeviceManager;
    class Texture;
    class Shader;
	class ConstantBuffer;

    enum class GLayoutMap
    {
        MESH,
        MESH_ANIM, 
        MATERIAL,
        ENVIRONMENT,
    };

    struct RendererStats
    {
        // 2D Statistics
        size_t quadCount = 0;
        size_t lineCount = 0;
        size_t circleCount = 0;
        size_t textCount = 0;
        size_t pointLight2dCount = 0;

        size_t quadVerticesSize = 0;
        size_t quadIndicesSize = 0;
        size_t lineVerticesSize = 0;
        size_t circleVerticesSize = 0;
        size_t circleIndicesSize = 0;
        size_t textVerticesSize = 0;
        size_t textIndicesSize = 0;
    };

    class IGN_API Renderer : public Subsystem
    {
    public:
        Renderer() = default;
        Renderer(DeviceManager *deviceManager, nvrhi::GraphicsAPI api);

        virtual void Shutdown() override;

        static void BeginStats();
        
        static Ref<Texture> GetWhiteTexture();
        static Ref<Texture> GetBlackTexture();
        static Ref<Texture> GetMagentaTexture();

        static nvrhi::GraphicsAPI GetGraphicsAPI();
        static nvrhi::BindingLayoutHandle GetBindingLayout(GLayoutMap type);

        static RendererStats Stats;

    private:
        nvrhi::GraphicsAPI m_GraphicsAPI;

        std::unordered_map<GLayoutMap, nvrhi::BindingLayoutHandle> m_BindingLayouts;

        Ref<Texture> m_WhiteTexture;
        Ref<Texture> m_BlackTexture;
        Ref<Texture> m_MagentaTexture;

        nvrhi::IDevice *m_Device;
        std::vector<std::function<void(nvrhi::ICommandList *)>> m_SubmitFuncs;

        friend class ShaderLibrary;
    };
}

#endif

