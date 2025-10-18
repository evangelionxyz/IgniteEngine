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

#include "ignite/graphics/gpu_data.hpp"

#include "ignite/graphics/texture.hpp"

#include "shadow_map.hpp"
#include "ignite/scene/icamera.hpp"
#include "ignite/core/application.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/common.hpp>

#include <algorithm>
#include <cmath>
#include <limits>


namespace ignite
{
	CascadedShadowMap::CascadedShadowMap(ShadowMapQuality quality)
	{
        m_GPUDataBuffer = ConstantBuffer::Create(sizeof(CascadedShadowMap_GPUData), true, 256, "Cascadded ShadowMap");
        m_ModelGPUDataBuffer = ConstantBuffer::Create(sizeof(CascadedShadowMapModel_GPUData), true, 256, "Cascadded Model ShadowMap");

		m_Resolution = static_cast<uint32_t>(quality);
		
		// Initialize shadow parameters with reasonable defaults
		m_GPUData.shadowStrength = 0.8f;  // 80% shadow visibility
		m_GPUData.minBias = 0.0001f;       // Minimum depth bias to prevent acne
		m_GPUData.maxBias = 0.005f;        // Maximum depth bias for steep angles
		m_GPUData.pcfRadius = 1.5f;        // PCF filter radius in texels
		
		Resize(quality);
	}

	CascadedShadowMap::~CascadedShadowMap()
	{
	}

	void CascadedShadowMap::Resize(ShadowMapQuality quality)
{
        if (quality == m_Quality && m_RenderTarget)
            return;

        RenderTargetCreateInfo rtCreateInfo;
        rtCreateInfo.width = static_cast<uint32_t>(quality);
        rtCreateInfo.height = static_cast<uint32_t>(quality);
        rtCreateInfo.attachments =
        {
            { "Cascaded Shadow Map Depth", nvrhi::Format::D32, nvrhi::ResourceStates::DepthWrite, NUM_CASCADES }
        };

        m_Quality = quality;
		m_Resolution = static_cast<uint32_t>(quality);
        m_RenderTarget = RenderTarget::Create(rtCreateInfo, "Cascaded ShadowMap RT");

		CreateCascadeFramebuffers();
		CreateCascadeLayerViews();
		CreatePipeline(m_CascadeFramebuffers[0]);
	}

	void CascadedShadowMap::BeginCascade(nvrhi::ICommandList *cmd, int cascadeIndex)
	{
		// Clear depth for the entire texture (all layers) on first cascade
		if (cascadeIndex == 0)
		{
			nvrhi::TextureSubresourceSet subresources = nvrhi::AllSubresources;
			cmd->clearDepthStencilTexture(m_RenderTarget->GetDepthAttachment()->GetHandle(), subresources, true, 1.0f, false, 0);
		}
	}

	void CascadedShadowMap::EndCascade()
	{
		// Copy the current cascade layer to its individual visualization texture
		// This needs to be done in the command list during rendering
	}

	void CascadedShadowMap::CopyCascadeLayersForVisualization(nvrhi::ICommandList* cmd)
	{
		// TODO: Implement compute shader-based copy from depth to color
		// For now, remove the direct copy which causes Vulkan validation errors
		
		// The direct copyTexture doesn't work because:
		// - Source is D32 (depth format with VK_IMAGE_ASPECT_DEPTH_BIT)
		// - Destination is R32_FLOAT (color format with VK_IMAGE_ASPECT_COLOR_BIT)
		// - Vulkan doesn't allow copying between different aspect masks
		
		// Proper solution: Use a compute shader that reads from depth texture
		// and writes to color texture, or use a graphics pass with shader conversion
		
		// For debugging, you can temporarily display the main shadow map texture
		// which works but shows all cascades overlapped
	}

    Ref<Texture> CascadedShadowMap::GetDepthTexture() const
    {
		return m_RenderTarget->GetDepthAttachment();
    }

    void CascadedShadowMap::ComputeMatrices(ICamera *camera, const glm::vec3& lightDir)
	{
		const float lambda = 0.7f;
        const float nearPlane = camera->nearPlane;
        const float farPlane = camera->farPlane;
        const float clipRange = farPlane - nearPlane;

        const float minZ = nearPlane;
        const float maxZ = nearPlane + clipRange;
        const float ratio = maxZ / minZ;

        std::array<float, NUM_CASCADES> cascadeEnds{};
        for (int i = 0; i < NUM_CASCADES; ++i)
        {
            const float p = (i + 1) / static_cast<float>(NUM_CASCADES);
            const float logd = minZ * std::pow(ratio, p);
            const float lined = minZ + clipRange * p;
            cascadeEnds[i] = glm::mix(lined, logd, lambda);
        }

        m_GPUData.cascadeSplits = glm::vec4(cascadeEnds[0], cascadeEnds[1], cascadeEnds[2], cascadeEnds[3]);
        m_GPUData.cascadeIndex = -1;

        const glm::mat4 invViewProj = glm::inverse(camera->projection * camera->view);

        std::array<glm::vec4, 8> frustumCorners =
        {
            glm::vec4(-1.0f,  1.0f, 0.0f, 1.0f),
            glm::vec4( 1.0f,  1.0f, 0.0f, 1.0f),
            glm::vec4( 1.0f, -1.0f, 0.0f, 1.0f),
            glm::vec4(-1.0f, -1.0f, 0.0f, 1.0f),
            glm::vec4(-1.0f,  1.0f, 1.0f, 1.0f),
            glm::vec4( 1.0f,  1.0f, 1.0f, 1.0f),
            glm::vec4( 1.0f, -1.0f, 1.0f, 1.0f),
            glm::vec4(-1.0f, -1.0f, 1.0f, 1.0f)
        };

        for (auto& corner : frustumCorners)
        {
            glm::vec4 worldCorner = invViewProj * corner;
            corner = worldCorner / worldCorner.w;
        }

        float lastSplitDist = nearPlane;
        const glm::vec3 lightDirection = glm::normalize(lightDir);
        const glm::vec3 defaultUp(0.0f, 1.0f, 0.0f);
        const glm::vec3 alternateUp(0.0f, 0.0f, 1.0f);

        for (int cascadeIndex = 0; cascadeIndex < NUM_CASCADES; ++cascadeIndex)
        {
            const float splitDist = cascadeEnds[cascadeIndex];
            const float prevSplitNorm = (lastSplitDist - nearPlane) / clipRange;
            const float splitNorm = (splitDist - nearPlane) / clipRange;

            std::array<glm::vec4, 8> cascadeCorners;
            for (int i = 0; i < 4; ++i)
            {
                const glm::vec4 cornerRay = frustumCorners[i + 4] - frustumCorners[i];
                cascadeCorners[i] = frustumCorners[i] + cornerRay * prevSplitNorm;
                cascadeCorners[i + 4] = frustumCorners[i] + cornerRay * splitNorm;
            }

            glm::vec3 frustumCenter(0.0f);
            for (const auto& corner : cascadeCorners)
            {
                frustumCenter += glm::vec3(corner);
            }
            frustumCenter /= 8.0f;

            float radius = 0.0f;
            for (const auto& corner : cascadeCorners)
            {
                radius = std::max(radius, glm::length(glm::vec3(corner) - frustumCenter));
            }
            radius = std::ceil(radius * 16.0f) / 16.0f;

            const glm::vec3 upVector = (std::abs(lightDirection.y) > 0.95f) ? alternateUp : defaultUp;
            const glm::vec3 lightPosition = frustumCenter - lightDirection * (radius * 2.0f);
            const glm::mat4 lightView = glm::lookAt(lightPosition, frustumCenter, upVector);

            glm::vec3 cascadeMin(std::numeric_limits<float>::max());
            glm::vec3 cascadeMax(std::numeric_limits<float>::lowest());
            for (const auto& corner : cascadeCorners)
            {
                const glm::vec4 cornerLS = lightView * corner;
                cascadeMin = glm::min(cascadeMin, glm::vec3(cornerLS));
                cascadeMax = glm::max(cascadeMax, glm::vec3(cornerLS));
            }

            const float depthPadding = 200.0f;
            cascadeMin.z -= depthPadding;
            cascadeMax.z += depthPadding;

            float lightNear = std::max(0.1f, -cascadeMax.z);
            float lightFar = std::max(lightNear + 1.0f, -cascadeMin.z);

            glm::mat4 lightProj = glm::orthoRH_ZO(cascadeMin.x, cascadeMax.x, cascadeMin.y, cascadeMax.y, lightNear, lightFar);

            glm::mat4 lightViewProj = lightProj * lightView;
            glm::vec4 origin = lightViewProj * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            origin /= origin.w;
            origin = origin * 0.5f + 0.5f;

            glm::vec2 shadowPixel = glm::vec2(origin.x, origin.y) * static_cast<float>(m_Resolution);
            glm::vec2 rounded = glm::round(shadowPixel);
            glm::vec2 offset = (rounded - shadowPixel) / static_cast<float>(m_Resolution);
            offset *= 2.0f;

            glm::mat4 texelAdjust(1.0f);
            texelAdjust[3][0] += offset.x;
            texelAdjust[3][1] += offset.y;

            m_GPUData.lightViewProj[cascadeIndex] = texelAdjust * lightViewProj;
            lastSplitDist = splitDist;
        }
	}

    void CascadedShadowMap::CreatePipeline(nvrhi::IFramebuffer* framebuffer)
    {
        if (!m_VS) m_VS = Shader::Create("resources/shaders/cascaded_shadow_depth.vertex.hlsl", ShaderType::Vertex, true);
        if (!m_PS) m_PS = Shader::Create("resources/shaders/cascaded_shadow_depth.pixel.hlsl", ShaderType::Pixel, true);

		GraphicsPipelineParams params;
		params.enableDepthWrite = true;
		params.enableDepthTest = true;
		params.depthFunc = nvrhi::ComparisonFunc::LessOrEqual;
		params.cullMode = nvrhi::RasterCullMode::Front;
		params.fillMode = nvrhi::RasterFillMode::Solid;

		nvrhi::BindingLayoutDesc layoutDesc = {};
		layoutDesc.setVisibility(nvrhi::ShaderType::All);
		layoutDesc.addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(0));
		layoutDesc.addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(1));

        if (!m_BindingLayout)
        {
            auto device = Application::GetInstance()->GetGraphicsDevice();
            m_BindingLayout = device->createBindingLayout(layoutDesc);
        }
		
        m_Pipeline = GraphicsPipeline::Create();
		m_Pipeline->SetShaders({ m_VS, m_PS })
			.AddBindingLayout(m_BindingLayout)
			.Build(framebuffer, params);
    }

	nvrhi::IFramebuffer* CascadedShadowMap::GetCascadeFramebuffer(int cascadeIndex) const
	{
		if (cascadeIndex >= 0 && cascadeIndex < NUM_CASCADES)
			return m_CascadeFramebuffers[cascadeIndex];
		return nullptr;
	}

	nvrhi::TextureHandle CascadedShadowMap::GetCascadeLayerTexture(int cascadeIndex) const
	{
		if (cascadeIndex >= 0 && cascadeIndex < NUM_CASCADES)
			return m_CascadeLayerViews[cascadeIndex];
		return nullptr;
	}

	void CascadedShadowMap::CreateCascadeFramebuffers()
	{
		nvrhi::IDevice* device = Application::GetGraphicsDevice();
		
		// Create a framebuffer for each cascade layer
		for (int i = 0; i < NUM_CASCADES; ++i)
		{
			nvrhi::FramebufferDesc fbDesc;
			
			// Create a view into the specific array layer
			nvrhi::FramebufferAttachment depthAttachment;
			depthAttachment.texture = m_RenderTarget->GetDepthAttachment()->GetHandle();
			depthAttachment.subresources = nvrhi::TextureSubresourceSet(0, 1, i, 1); // mip 0, array layer i
			depthAttachment.format = nvrhi::Format::D32;
			
			fbDesc.setDepthAttachment(depthAttachment);
			
			m_CascadeFramebuffers[i] = device->createFramebuffer(fbDesc);
			LOG_ASSERT(m_CascadeFramebuffers[i], "Failed to create cascade framebuffer for layer {}", i);
		}
	}

	void CascadedShadowMap::CreateCascadeLayerViews()
	{
		nvrhi::IDevice* device = Application::GetGraphicsDevice();
		
		// Create color texture views for ImGui visualization
		// These will be filled using a compute shader that reads depth
		for (int i = 0; i < NUM_CASCADES; ++i)
		{
			nvrhi::TextureDesc viewDesc;
			viewDesc.width = m_RenderTarget->GetDepthAttachment()->GetHandle()->getDesc().width;
			viewDesc.height = m_RenderTarget->GetDepthAttachment()->GetHandle()->getDesc().height;
			viewDesc.mipLevels = 1;
			viewDesc.format = nvrhi::Format::R32_FLOAT; // Color format for ImGui
			viewDesc.dimension = nvrhi::TextureDimension::Texture2D;
			viewDesc.debugName = std::format("Cascade {} Layer View", i);
			viewDesc.isRenderTarget = false;
			viewDesc.isUAV = true; // Enable UAV for compute shader write
			viewDesc.isTypeless = false;
			viewDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
			viewDesc.keepInitialState = false;
			
			m_CascadeLayerViews[i] = device->createTexture(viewDesc);
			LOG_ASSERT(m_CascadeLayerViews[i], "Failed to create cascade layer view for layer {}", i);
		}
	}
}