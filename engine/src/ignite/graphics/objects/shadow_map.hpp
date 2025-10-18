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

#ifndef SHADOW_MAP_HPP
#define SHADOW_MAP_HPP

#include "ignite/graphics/buffers/constant_buffer.hpp"
#include "ignite/graphics/graphics_pipeline.hpp"
#include "ignite/graphics/render_target.hpp"

#include <glm/glm.hpp>

namespace ignite
{
	class ICamera;
	enum class ShadowMapQuality
	{
		LOW = 512,
		MEDIUM = 1024,
		HIGH = 2048,
		ULTRA = 4096
	};

	class CascadedShadowMap
	{
	public:
		CascadedShadowMap(ShadowMapQuality quality = ShadowMapQuality::MEDIUM);
		~CascadedShadowMap();

		void Resize(ShadowMapQuality quality);
		void BeginCascade(nvrhi::ICommandList *cmd, int cascadeIndex);
		void EndCascade();
		void CopyCascadeLayersForVisualization(nvrhi::ICommandList* cmd);

		Ref<RenderTarget> GetRenderTarget() const { return m_RenderTarget; }
		nvrhi::IFramebuffer* GetCascadeFramebuffer(int cascadeIndex) const;
		Ref<ConstantBuffer> GetGPUDataBuffer() const { return m_GPUDataBuffer; }
		Ref<ConstantBuffer> GetModelGPUDataBuffer() const { return m_ModelGPUDataBuffer; }
		nvrhi::BindingLayoutHandle GetBindingLayout() const { return m_BindingLayout;  }

		Ref<Texture> GetDepthTexture() const;
		nvrhi::TextureHandle GetCascadeLayerTexture(int cascadeIndex) const;

		Ref<GraphicsPipeline> GetPipeline() const { return m_Pipeline; }
		CascadedShadowMap_GPUData& GetGPUData() { return m_GPUData; }

		void ComputeMatrices(ICamera *camera, const glm::vec3& lightDir);
	private:
		void CreatePipeline(nvrhi::IFramebuffer *framebuffer);
		void CreateCascadeFramebuffers();
		void CreateCascadeLayerViews();
		void CreateDepthVisualizationPipeline(); // NEW: For compute shader

		Ref<RenderTarget> m_RenderTarget;
		std::array<nvrhi::FramebufferHandle, NUM_CASCADES> m_CascadeFramebuffers;
		std::array<nvrhi::TextureHandle, NUM_CASCADES> m_CascadeLayerViews;
		Ref<ConstantBuffer> m_GPUDataBuffer;
		Ref<ConstantBuffer> m_ModelGPUDataBuffer;
		Ref<GraphicsPipeline> m_Pipeline;

		// NEW: Compute pipeline for depth visualization
		nvrhi::ComputePipelineHandle m_DepthVisualizationPipeline;
		nvrhi::BindingLayoutHandle m_DepthVisualizationLayout;
		Ref<Shader> m_DepthVisualizationShader;

		nvrhi::BindingLayoutHandle m_BindingLayout;

		Ref<Shader> m_VS;
		Ref<Shader> m_PS;

		uint32_t m_DepthArray = 0;
		int m_Resolution = 0;
		ShadowMapQuality m_Quality;
		CascadedShadowMap_GPUData m_GPUData{};
	};
}

#endif