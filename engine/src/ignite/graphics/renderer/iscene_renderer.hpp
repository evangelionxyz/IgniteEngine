// Copyright (c) 2026 Evangelion Manuhutu

#ifndef ISCENE_RENDERER_HPP
#define ISCENE_RENDERER_HPP

#include "ignite/graphics/buffers/constant_buffer.hpp"
#include "ignite/graphics/bloom.hpp"
#include "ignite/graphics/ssao.hpp"
#include "ignite/graphics/edge_detection.hpp"
#include "ignite/graphics/graphics_pipeline.hpp"
#include "ignite/graphics/render_target.hpp"
#include "ignite/scene/entity.hpp"
#include <nvrhi/nvrhi.h>
#include <unordered_map>

namespace ignite
{
    class Scene;
    class ICamera;
    class Project;
    class RenderTarget;
    class Renderer2D;
    class CascadedShadowMap;
    class MeshPrimitive;
    class StaticMesh;
    class SkeletalMesh;
    class Material;

    struct DebugGridStyle
    {
        bool enabled = true;
        bool enableXAxis = true;
        bool enableYAxis = true;
        bool enableZAxis = true;

        float cellSize = 0.25f;
        float minPixelsBetweenCells = 10.0f;
        float gridSize = 300.0f;
        float majorLineScale = 8.0f;

        glm::vec4 thinColor = glm::vec4(0.789f, 0.789f, 0.789f, 1.0f);
        glm::vec4 thickColor = glm::vec4(0.456f, 0.456f, 0.456f, 1.0f);
        glm::vec4 xAxisColor = glm::vec4(0.96f, 0.29f, 0.29f, 1.0f);
        glm::vec4 yAxisColor = glm::vec4(0.29f, 0.96f, 0.29f, 1.0f);
        glm::vec4 zAxisColor = glm::vec4(0.29f, 0.52f, 0.96f, 1.0f);
    };

    struct DebugGridSettings
    {
        DebugGridStyle world3D;
        DebugGridStyle world2D;

        DebugGridSettings()
        {
            world2D.enableZAxis = false;
            world2D.gridSize = 100.0f;
        }
    };

    class ISceneRenderer
    {
    public:
        ISceneRenderer();
        virtual ~ISceneRenderer();

        virtual void OnUpdate(float deltaTime) { };

        virtual void ResizeFramebuffer(uint32_t width, uint32_t height);
        virtual Ref<Texture> GetEnvironmentMapColorTexture() const;
        virtual Ref<Texture> GetCascadedShadowMapDepthTexture() const;
        virtual Ref<CascadedShadowMap> GetCascadedShadowMap();

    protected:
        void EnsureCompositeVertexBufferUploaded(nvrhi::ICommandList *cmd);

        Ref<CascadedShadowMap> m_CascadedShadowMap;

        Ref<VertexBuffer> m_CompositeVertexBuffer;
        nvrhi::SamplerHandle m_CompositeSampler;

        Ref<Renderer2D> m_Renderer2D;
        Ref<EdgeDetection> m_EdgeDetection;
        Ref<Bloom> m_EditorBloom;
        Ref<SSAO> m_EditorSSAO;

        Ref<Bloom> m_GameplayBloom;
        Ref<SSAO> m_GameplaySSAO;
        
        Ref<ConstantBuffer> m_CompositePostProcessBuffer;
        Ref<ConstantBuffer> m_CascadedShadowMapBuffer;
        Ref<ConstantBuffer> m_SceneBuffer;
        Ref<ConstantBuffer> m_CameraBuffer;

        nvrhi::BindingSetHandle m_MeshBindingSet;
        SceneBufferData m_SceneGPUData;

        std::vector<uint32_t> m_SelectedEntities;
        nvrhi::RasterFillMode m_FillMode = nvrhi::RasterFillMode::Solid;
        Ref<ConstantBuffer> m_DebugGridBuffer;
        DebugGridSettings m_DebugGridSettings;

        nvrhi::IDevice *m_Device = nullptr;
        Ref<Scene> m_Scene;
        Project *m_Project = nullptr;
        bool m_Has2DPreRenderCache = false;
        bool m_CompositeVertexBufferUploadPending = true;
    };
}

#endif