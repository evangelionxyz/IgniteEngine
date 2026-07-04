// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_ISCENE_RENDERER_HPP
#define IGN_ISCENE_RENDERER_HPP

#include "ignite/asset/asset.hpp"
#include "ignite/core/base.hpp"
#include "ignite/graphics/gpu_data.hpp"
#include "ignite/graphics/bloom.hpp"
#include "ignite/graphics/ssao.hpp"
#include "ignite/graphics/edge_detection.hpp"
#include "ignite/graphics/render_target.hpp"
#include "ignite/graphics/buffers/constant_buffer.hpp"

#include "ignite/graphics/objects/mesh.hpp"
#include "ignite/graphics/objects/material.hpp"
#include "ignite/graphics/objects/material_2d.hpp"
#include "ignite/graphics/objects/shadow_map.hpp"
#include "ignite/graphics/texture.hpp"

#include <nvrhi/nvrhi.h>
#include <unordered_map>

namespace ignite
{
    class Scene;
    class ICamera;
    class Renderer2D;
    class CascadedShadowMap;
    class WorldEnvironment;

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

    struct SceneRenderSettings
    {
        DebugGridStyle worldGrid3D;
        DebugGridStyle worldGrid2D;

        bool showBoundingBox = false;
        bool showPhysicsCollider = true;

        SceneRenderSettings()
        {
            worldGrid2D.enableZAxis = false;
            worldGrid2D.gridSize = 100.0f;
        }
    };

    struct DebugGrid_GPUData
    {
        glm::vec4 thinColor = glm::vec4(0.0f);
        glm::vec4 thickColor = glm::vec4(0.0f);
        glm::vec4 xAxisColor = glm::vec4(0.0f);
        glm::vec4 yAxisColor = glm::vec4(0.0f);
        glm::vec4 zAxisColor = glm::vec4(0.0f);
        glm::vec4 settings0 = glm::vec4(0.0f); // x=cellSize y=minPixelsBetweenCells z=gridSize w=majorLineScale
        glm::vec4 settings1 = glm::vec4(0.0f); // x=planeMode(0:XZ, 1:XY) y=enableX z=enableY w=enableZ
    };

    struct CompositePostProcess_GPUData
    {
        glm::vec4 flags = glm::vec4(0.0f); // x=enableBloom, y=bloomIntensity, z=enableVignette, w=enableChromAb
        glm::vec4 vignetteParams = glm::vec4(0.0f); // x=radius, y=softness, z=intensity, w=chromAbAmount
        glm::vec4 chromAbParams = glm::vec4(0.0f); // x=chromAbRadial, y=enableSSAO, z=ssaoIntensity
        glm::vec4 vignetteColor = glm::vec4(0.0f);
        
        int tonemapMode = 0;
        float exposure = 1.1f;
        float gamma = 2.2f;
        int enableDOF = 0;

        float focalLength = 120.0f;
        float focalDistance = 5.5f;
        float fStop = 1.4f;
        float focusRange = 5.0f;

        float blurAmount = 1.0f;
        float padding_dof[3] = { 0.0f, 0.0f, 0.0f };

        glm::vec4 fogColor = glm::vec4(0.5f, 0.6f, 0.7f, 1.0f);
        float fogDensity = 0.0f;
        float fogStart = 10.0f;
        float fogEnd = 100.0f;
        float padding_fog = 0.0f;

        glm::mat4 projectionInv = glm::mat4(1.0f);
    };

    class IGN_API ISceneRenderer
    {
    public:
        ISceneRenderer();
        virtual ~ISceneRenderer();

        virtual void ResizeFramebuffer(ICamera *camera, uint32_t width, uint32_t height);

        int GetRenderMode() const { return m_SceneGPUData.renderMode; }
        void SetRenderMode(int renderMode) { m_SceneGPUData.renderMode = renderMode; }

        int GetDebugShadowMode() const { return m_SceneGPUData.debugShadow; }
        void SetDebugShadowMode(int debugShadow) { m_SceneGPUData.debugShadow = debugShadow; }

        virtual Ref<Texture> GetEnvironmentMapColorTexture() const;
        virtual Ref<Texture> GetCascadedShadowMapDepthTexture() const;
        virtual Ref<CascadedShadowMap> GetCascadedShadowMap();

		template<typename T>
		Ref<T> ResolveAsset(AssetHandle handle)
		{
            auto assetManager = AssetManager::GetInstance();
			if (!assetManager || handle == AssetHandle(0))
				return nullptr;

			AssetResolveKey key{ assetManager->GetProject(), handle};
			auto it = m_ResolvedAssetsCache.find(key);
			if (it != m_ResolvedAssetsCache.end())
				return it->second->As<T>();

			Ref<T> asset = assetManager->GetAsset<T>(handle);
			if (asset)
				m_ResolvedAssetsCache.emplace(key, asset);

			return asset;
		}

		SceneRenderSettings sceneRenderSettings;

    protected:
		void EnsureSceneEnvironmentMap();

        virtual void AddAssetPin(AssetHandle handle) = 0;
        virtual std::string_view BuildAssetPinName(AssetHandle handle) = 0;
        void ClearPinnedAssets();

        Ref<CascadedShadowMap> m_CascadedShadowMap;

        Ref<VertexBuffer> m_CompositeVertexBuffer;
        nvrhi::SamplerHandle m_CompositeSampler;

        Ref<Renderer2D> m_Renderer2D;
        Ref<EdgeDetection> m_EdgeDetection;
        
        Ref<ConstantBuffer> m_CompositePostProcessBuffer;

        Ref<ConstantBuffer> m_SceneBuffer;
        Ref<ConstantBuffer> m_CameraBuffer;
        Ref<ConstantBuffer> m_CascadedShadowMapBuffer;
        Ref<ConstantBuffer> m_CSMPerCascadeBuffers[NUM_CASCADES];
        Ref<ConstantBuffer> m_PointLightBuffer;
        Ref<ConstantBuffer> m_SpotLightBuffer;

		std::unordered_map<AssetResolveKey, Ref<Asset>, AssetResolveKeyHash> m_ResolvedAssetsCache;

		WorldEnvironment *m_WorldEnvironment = nullptr;

        nvrhi::BindingSetHandle m_MeshBindingSet;
        SceneBufferData m_SceneGPUData;

        std::vector<AssetHandle> m_PinnedAssetHandles;

        std::vector<uint32_t> m_SelectedEntities;
        nvrhi::RasterFillMode m_FillMode = nvrhi::RasterFillMode::Solid;
        Ref<ConstantBuffer> m_DebugGridBuffer;

        nvrhi::IDevice *m_Device = nullptr;
        
        Ref<Scene> m_Scene;

        bool m_Has2DPreRenderCache = false;
        bool m_CompositeVertexBufferUploadPending = true;
    };
}

#endif