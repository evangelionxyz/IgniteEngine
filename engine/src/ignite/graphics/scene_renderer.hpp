/* MIT License
* 
* Copyright (c) 2025 Evangelion Manuhutu | IGNITE STUDIO
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#pragma once

#include "ignite/graphics/buffers/constant_buffer.hpp"
#include "bloom.hpp"
#include "edge_detection.hpp"
#include "graphics_pipeline.hpp"
#include "render_target.hpp"
#include "ignite/scene/entity.hpp"

#include <nvrhi/nvrhi.h>
#include <unordered_map>

namespace ignite
{
    class Scene;
    class ICamera;
    class Project;
    class RenderTarget;
    class UIRenderer;
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

    class SceneRenderer
    {
    public:
        SceneRenderer();
        ~SceneRenderer();

        void BeginFrame();
        
        void SetActiveScene(const Ref<Scene> &scene);
        
        void RenderEditorTo(ICamera *camera, const Ref<RenderTarget> &sceneRT, const Ref<RenderTarget> &uiRT, const Ref<RenderTarget> &compositeRT, bool renderEnvironment = true);
        void RenderGameplayTo(ICamera *camera, const Ref<RenderTarget> &sceneRT, const Ref<RenderTarget> &uiRT, const Ref<RenderTarget> &compositeRT, bool renderEnvironment = true);
        
        void SetFillMode(nvrhi::RasterFillMode mode);

        void SetSelectedEntity(const Entity& entity);
        void UnselectEntity(const Entity& entity);
        void ClearSelectedEntities();

        // UI Input handling
        void UpdateUIInput(const glm::vec2& viewportMousePos, const glm::vec2& viewportPos, const glm::vec2& viewportSize, bool mousePressed);

		Ref<Texture> GetEnvironmentMapColorTexture() const;
		Ref<Texture> GetCascadedShadowMapDepthTexture() const;

        Ref<CascadedShadowMap> GetCascadedShadowMap();
        Ref<UIRenderer> &GetUIRenderer() { return m_UIRenderer; }
        Ref<Renderer2D> &GetRenderer2D() { return m_Renderer2D; }

        DebugGridSettings &GetDebugGridSettings() { return m_DebugGridSettings; }
        const DebugGridSettings &GetDebugGridSettings() const { return m_DebugGridSettings; }
        void SetDebugGridSettings(const DebugGridSettings &settings) { m_DebugGridSettings = settings; }

    private:
        struct AssetResolveKey
        {
            Project *project = nullptr;
            AssetHandle handle = AssetHandle(0);

            bool operator==(const AssetResolveKey &other) const noexcept
            {
                return project == other.project && handle == other.handle;
            }
        };

        struct AssetResolveKeyHash
        {
            size_t operator()(const AssetResolveKey &key) const noexcept
            {
                size_t h = std::hash<const void *>{}(key.project);
                h ^= (std::hash<AssetHandle>{}(key.handle) + 0x9e3779b9 + (h << 6) + (h >> 2));
                return h;
            }
        };

        Ref<StaticMesh> ResolveStaticMesh(Project *project, AssetHandle handle);
        Ref<SkeletalMesh> ResolveSkeletalMesh(Project *project, AssetHandle handle);
        Ref<Material> ResolveMaterial(Project *project, AssetHandle handle);
        void Clear3DAssetResolveCache();

        void ShadowPass(nvrhi::ICommandList *cmd, ICamera *camera);
        void ColorPass(nvrhi::ICommandList *cmd, ICamera *camera, nvrhi::IFramebuffer *framebuffer);
        void CompositePass(nvrhi::ICommandList *cmd, ICamera *camera, nvrhi::IFramebuffer *framebuffer,
            Ref<Texture> sceneTexture, Ref<Texture> uiTexture, Ref<Texture> edgeTexture = nullptr, Ref<Texture> bloomTexture = nullptr);

        void DrawDebugGrid(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer, const DebugGridStyle &style, bool is2D);
        void DrawDebug2DPhysics(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer);
        void DrawDebug3DPhysics(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer);
        
    private:
		Ref<CascadedShadowMap> m_CascadedShadowMap;

        // Composite
        Ref<VertexBuffer> m_CompositeVertexBuffer;
		nvrhi::SamplerHandle m_CompositeSampler;

        Ref<Renderer2D> m_Renderer2D;
        Ref<UIRenderer> m_UIRenderer;
        Ref<EdgeDetection> m_EdgeDetection;
        Ref<Bloom> m_Bloom;
        Ref<ConstantBuffer> m_CompositePostProcessBuffer;

        std::vector<uint32_t> m_SelectedEntities;
        nvrhi::RasterFillMode m_FillMode = nvrhi::RasterFillMode::Solid;
        Ref<ConstantBuffer> m_DebugGridBuffer;
        DebugGridSettings m_DebugGridSettings;

        nvrhi::IDevice *m_Device = nullptr;
        Ref<Scene> m_Scene;

        std::unordered_map<AssetResolveKey, Ref<StaticMesh>, AssetResolveKeyHash> m_StaticMeshResolveCache;
        std::unordered_map<AssetResolveKey, Ref<SkeletalMesh>, AssetResolveKeyHash> m_SkeletalMeshResolveCache;
        std::unordered_map<AssetResolveKey, Ref<Material>, AssetResolveKeyHash> m_MaterialResolveCache;

        bool m_Has2DPreRenderCache = false;

    };
}
