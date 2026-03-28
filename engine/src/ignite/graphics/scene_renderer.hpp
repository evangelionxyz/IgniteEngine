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

#include "ignite/graphics/objects/environment.hpp"
#include "ignite/graphics/buffers/constant_buffer.hpp"
#include "edge_detection.hpp"
#include "graphics_pipeline.hpp"
#include "render_target.hpp"
#include "ignite/scene/entity.hpp"

#include <nvrhi/nvrhi.h>

namespace ignite
{
    class Scene;
    class ICamera;
    class RenderTarget;
    class UIRenderer;
    class Renderer2D;
	class CascadedShadowMap;
    class MeshPrimitive;

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
            world2D.gridSize = 150.0f;
        }
    };

    class SceneRenderer
    {
    public:
        SceneRenderer();
        ~SceneRenderer();
        
        void SetActiveScene(const Ref<Scene> &scene);
        void RenderTo(ICamera *camera, const Ref<RenderTarget> &sceneRT, const Ref<RenderTarget> &uiRT, const Ref<RenderTarget> &compositeRT, bool renderEnvironment = true);
        void SetFillMode(nvrhi::RasterFillMode mode);

        void SetSelectedEntity(const Entity& entity);
        void UnselectEntity(const Entity& entity);
        void ClearSelectedEntities();

        // UI Input handling
        void UpdateUIInput(const glm::vec2& viewportMousePos, const glm::vec2& viewportPos, const glm::vec2& viewportSize, bool mousePressed);

		Ref<Texture> GetEnvironmentMapColorTexture() const;
		Ref<Texture> GetCascadedShadowMapDepthTexture() const;

        Ref<CascadedShadowMap> GetCascadedShadowMap();
        Ref<Environment> &GetEnvironment() { return m_Environment; }
        Ref<UIRenderer> &GetUIRenderer() { return m_UIRenderer; }
        Ref<Renderer2D> &GetRenderer2D() { return m_Renderer2D; }

        DebugGridSettings &GetDebugGridSettings() { return m_DebugGridSettings; }
        const DebugGridSettings &GetDebugGridSettings() const { return m_DebugGridSettings; }
        void SetDebugGridSettings(const DebugGridSettings &settings) { m_DebugGridSettings = settings; }

        void OnEnvironmentTextureChanged();

        void ShadowPass(nvrhi::ICommandList *cmd, ICamera *camera);
        void ColorPass(nvrhi::ICommandList *cmd, ICamera *camera, nvrhi::IFramebuffer *framebuffer);
        void CompositePass(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer, Ref<Texture> sceneTexture, Ref<Texture> uiTexture);
        void DrawDebugGrid(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer, const DebugGridStyle &style, bool is2D);
        
    private:
        Ref<Environment> m_Environment;
		Ref<CascadedShadowMap> m_CascadedShadowMap;

        // Composite
        Ref<VertexBuffer> m_CompositeVertexBuffer;
		nvrhi::SamplerHandle m_CompositeSampler;

        Ref<Renderer2D> m_Renderer2D;
        Ref<UIRenderer> m_UIRenderer;

        std::vector<uint32_t> m_SelectedEntities;
        std::vector<AABB> m_EntityBounds;
        
        nvrhi::RasterFillMode m_FillMode = nvrhi::RasterFillMode::Solid;

        bool m_EnvironmentDirty = false;

        Ref<ConstantBuffer> m_DebugGridBuffer;
        DebugGridSettings m_DebugGridSettings;

        nvrhi::IDevice *m_Device = nullptr;
        Ref<Scene> m_Scene;
    };
}
