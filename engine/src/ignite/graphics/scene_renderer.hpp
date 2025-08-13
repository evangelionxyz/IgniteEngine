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

#include "environment.hpp"
#include "edge_detection.hpp"
#include "graphics_pipeline.hpp"
#include "render_target.hpp"
#include "ignite/scene/entity.hpp"
#include "command_list.hpp"

#include "imgui.h"

#include <nvrhi/nvrhi.h>
#include <nvrhi/utils.h>

namespace ignite
{
    class Scene;
    class ICamera;
    class RenderTarget;
    class UIRenderer;
        
    class SceneRenderer
    {
    public:
        SceneRenderer();
        ~SceneRenderer();
        
        void Create();
        void SetActiveScene(Scene *scene);
        bool ShouldResize() const;
        void Resize(uint32_t width, uint32_t height);
        void CreatePipelines();
        void Render(ICamera *camera, bool renderEnvironment = true);
        void SetFillMode(nvrhi::RasterFillMode mode) const;

        void SetSelectedEntity(const Entity& entity);
        void UnselectEntity(const Entity& entity);
        void ClearSelectedEntities();

        // UI Input handling
        void UpdateUIInput(const glm::vec2& viewportMousePos, const glm::vec2& viewportPos, const glm::vec2& viewportSize, bool mousePressed);

        void OnGuiRender();

        static SceneRenderer *GetActive();

        Ref<GraphicsPipeline> &GetEnvironmentPipeline() { return m_EnvironmentPipeline; }
        Ref<GraphicsPipeline> &GetGeometryAnimPipeline() { return m_GeometryAnimPipeline; }
        
        Ref<Environment> &GetEnvironment() { return m_Environment; }
        Ref<RenderTarget> &GetRenderTarget() { return m_SceneRenderTarget; }
        Ref<RenderTarget> &GetCompositeRenderTarget() { return m_CompositeRenderTarget; }
        
        Ref<EdgeDetection> GetEdgeDetection() { return m_EdgeDetection; }

        Ref<UIRenderer> &GetUIRenderer() { return m_UIRenderer; }

    private:
        void CreateEnvironment();
        void CreateRenderTargets();
        void CreateDemoUI();

        void CompositeUpdateBindingSet();

        Ref<Environment> m_Environment;
        Ref<GraphicsPipeline> m_EnvironmentPipeline;
        Ref<GraphicsPipeline> m_GeometryAnimPipeline;
        Ref<RenderTarget> m_SceneRenderTarget;
        Ref<CommandList> m_CommandList;

        // Composite
        Ref<GraphicsPipeline> m_CompositePipeline;
        Ref<RenderTarget> m_CompositeRenderTarget;
        Ref<VertexBuffer> m_CompositeVertexBuffer;
        nvrhi::BindingSetHandle m_CompositeBindingSet;

        Ref<UIRenderer> m_UIRenderer;

        std::vector<uint32_t> m_SelectedEntities;
        std::vector<AABB> m_EntityBounds;

        Ref<EdgeDetection> m_EdgeDetection;
        EdgeDetectionParameter m_EdgeDetectionParams;

        nvrhi::IDevice *m_Device = nullptr;

        Scene *m_Scene = nullptr;
    };
}
