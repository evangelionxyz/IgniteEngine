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
    class StaticMesh;
    class Material;
    class Shader;
	class ConstantBuffer;

    enum class EBindingLayout
    {
        MESH_STATIC = 1,
        MESH_ANIM, 
        MATERIAL,
        ENVIRONMENT,
    };

	enum class EMeshType
	{
		UV_SPHERE = 0,
	};

    struct RendererStats
    {
        // 3D Statistics
        size_t drawCallCount = 0;          // total indexed draw calls (opaque + transparent)
        size_t shadowDrawCallCount = 0;    // draw calls in shadow/CSM passes
        size_t staticMeshCount = 0;        // visible static mesh entities drawn
        size_t skeletalMeshCount = 0;      // visible skeletal mesh entities drawn
        size_t vertexCount3D = 0;          // total vertices submitted (sum of index counts for 3D)
        size_t indexCount3D = 0;           // total indices submitted across all 3D draw calls

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

        // GPU Buffer Memory (bytes)
        size_t gpuVertexBufferBytes = 0;   // sum of all live VertexBuffer allocations
        size_t gpuIndexBufferBytes = 0;    // sum of all live IndexBuffer allocations
        size_t gpuConstantBufferBytes = 0; // sum of all live ConstantBuffer allocations
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

        static Ref<Material> GetDefaultMaterial();

        static Ref<StaticMesh> GetDefaultMesh(EMeshType type);

        static nvrhi::GraphicsAPI GetGraphicsAPI();
        static nvrhi::BindingLayoutHandle GetBindingLayout(EBindingLayout type);

        static RendererStats Stats;

    private:
        nvrhi::GraphicsAPI m_GraphicsAPI;

        std::unordered_map<EBindingLayout, nvrhi::BindingLayoutHandle> m_BindingLayouts;
		std::unordered_map<EMeshType, Ref<StaticMesh>> m_DefaultMeshes;

        Ref<Texture> m_WhiteTexture;
        Ref<Texture> m_BlackTexture;
        Ref<Texture> m_MagentaTexture;

        Ref<Material> m_DefaultMaterial;

        nvrhi::IDevice *m_Device;
        std::vector<std::function<void(nvrhi::ICommandList *)>> m_SubmitFuncs;


        friend class ShaderLibrary;
    };
}

#endif

