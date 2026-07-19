// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_RENDERER_HPP
#define IGN_RENDERER_HPP

#include "ignite/core/subsystem.hpp"
#include "ignite/core/types.hpp"
#include "frame_context.hpp"

#include <string>
#include <unordered_map>
#include <mutex>

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

        // Live pipeline and binding set tracking
        std::unordered_map<std::string, size_t> pipelineCounts;
        std::unordered_map<std::string, size_t> bindingSetCounts;
    };

    class IGN_API Renderer : public Subsystem
    {
    public:
        Renderer(DeviceManager *deviceManager, nvrhi::GraphicsAPI api);

        virtual void Shutdown() override;
        void BeginFrame(const uint64_t frameIndex);
        void ResetStatistics();

		static FrameContext *GetCurrentFrameContext();
        
        static Ref<Texture> GetWhiteTexture();
        static Ref<Texture> GetBlackTexture();
        static Ref<Texture> GetMagentaTexture();
        static Ref<Texture> GetBlackUIntTexture();

        static Ref<Material> GetDefaultMaterial();

        static Ref<StaticMesh> GetDefaultMesh(EMeshType type);

        static nvrhi::GraphicsAPI GetGraphicsAPI();
        static nvrhi::BindingLayoutHandle GetBindingLayout(EBindingLayout type);

        static void IncrementPipelineCount(const std::string& name);
        static void DecrementPipelineCount(const std::string& name);
        static void IncrementBindingSetCount(const std::string& name);
        static void DecrementBindingSetCount(const std::string& name);

        static RendererStats Stats;

    private:
        nvrhi::GraphicsAPI m_GraphicsAPI;

        std::unordered_map<EBindingLayout, nvrhi::BindingLayoutHandle> m_BindingLayouts;
		std::unordered_map<EMeshType, Ref<StaticMesh>> m_DefaultMeshes;

        Ref<Texture> m_WhiteTexture;
        Ref<Texture> m_BlackTexture;
        Ref<Texture> m_MagentaTexture;
        Ref<Texture> m_BlackUIntTexture;
        Ref<Material> m_DefaultMaterial;

        std::vector<std::function<void(nvrhi::ICommandList *)>> m_SubmitFuncs;
		std::vector<FrameContext> m_Frames; // Triple buffering
		uint64_t m_MaxFramesInFlight = 3;
        uint64_t m_FrameCounter = 0;

        friend class ShaderLibrary;
    };
}

#endif

