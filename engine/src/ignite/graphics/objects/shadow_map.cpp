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

#include <array>
#include <algorithm>
#include <cmath>
#include <limits>

namespace ignite
{
	CascadedShadowMap::CascadedShadowMap(ShadowMapQuality quality)
	{
        m_GPUDataBuffer = ConstantBuffer::Create(sizeof(CascadedShadowMap_GPUData), true, 256, "Cascadded ShadowMap");
        m_ModelGPUDataBuffer = ConstantBuffer::Create(sizeof(CascadedShadowMapModel_GPUData), true, 256, "Cascadded Model ShadowMap");

		// Initialize shadow parameters with reasonable defaults
		m_GPUData.shadowStrength = 0.8f;  // 80% shadow visibility
		m_GPUData.minBias = 0.0001f;       // Minimum depth bias to prevent acne
		m_GPUData.maxBias = 0.005f;        // Maximum depth bias for steep angles
		m_GPUData.pcfRadius = 0.3f;        // PCF filter radius in texels
		
		Resize(quality);
	}

	CascadedShadowMap::~CascadedShadowMap()
	{
		m_DepthSampler = nullptr;
	}

	void CascadedShadowMap::Resize(ShadowMapQuality quality)
{
        if (quality == m_Quality)
            return;

		m_Resolution = ShadowMapQuality::LOW == quality ? 512 :
					   ShadowMapQuality::MEDIUM == quality ? 1024 :
					   ShadowMapQuality::HIGH == quality ? 2048 :
					   ShadowMapQuality::ULTRA == quality ? 4096 : 1024;
		m_Quality = quality;

		// Configure depth texture sampler settings
		nvrhi::IDevice* device = Application::GetGraphicsDevice();
		
		nvrhi::SamplerDesc samplerDesc;
		samplerDesc.addressU = nvrhi::SamplerAddressMode::ClampToEdge;
		samplerDesc.addressV = nvrhi::SamplerAddressMode::ClampToEdge;
		samplerDesc.addressW = nvrhi::SamplerAddressMode::ClampToEdge;
		samplerDesc.borderColor = nvrhi::Color(1.0f, 1.0f, 1.0f, 1.0f); // White border = lit
        samplerDesc.setAllFilters(true);
		
		m_DepthSampler = device->createSampler(samplerDesc);
		LOG_ASSERT(m_DepthSampler, "Failed to create depth sampler");

		CreateCascadeFramebuffers();
		CreatePipeline(m_CascadeFramebuffers[0]);
	}

	void CascadedShadowMap::BeginCascade(nvrhi::ICommandList *cmd, int cascadeIndex)
	{
		// Clear depth for the entire texture (all layers) on first cascade
		if (cascadeIndex == 0)
		{
			nvrhi::TextureSubresourceSet subresources = nvrhi::AllSubresources;
			cmd->clearDepthStencilTexture(m_DepthTexture->GetHandle(), subresources, true, 1.0f, false, 0);
		}
	}

	void CascadedShadowMap::EndCascade(nvrhi::ICommandList *cmd)
	{
		// Copy the current cascade layer to its individual visualization texture
		// This needs to be done in the command list during rendering

		// for (int i = 0; i < NUM_CASCADES; ++i)
		// {
		// 	auto srcSlice = nvrhi::TextureSlice();
		// 	srcSlice.arraySlice = i;
		// 
		// 	cmd->copyTexture(m_CascadeLayerViews[i]->GetHandle(),
		// 		nvrhi::TextureSlice().resolve(m_CascadeLayerViews[i]->GetHandle()->getDesc()),
		// 		m_DepthTexture->GetHandle(), srcSlice
		// 	);
		// }
	}

    Ref<Texture> CascadedShadowMap::GetDepthTexture() const
    {
		return m_DepthTexture;
    }

	void CascadedShadowMap::ComputeMatrices(ICamera *camera, const glm::vec3& lightPosition)
	{
		if (!camera)
			return;

		glm::vec3 lightDir = lightPosition;
		if (glm::dot(lightDir, lightDir) < 1e-6f)
		{
			lightDir = glm::vec3(0.0f, -1.0f, 0.0f);
		}
		else
		{
			lightDir = -glm::normalize(lightDir);
		}

		constexpr float splitLambda = 0.7f;

		const float nearPlane = glm::max(camera->nearPlane, 0.001f);
		const float farPlane = glm::max(camera->farPlane, nearPlane + 1.0f);
		const float clipRange = farPlane - nearPlane;

		const float minZ = nearPlane;
		const float maxZ = nearPlane + clipRange;
		const float range = maxZ - minZ;
		const float ratio = maxZ / minZ;

		std::array<float, NUM_CASCADES> cascadeSplits{};
		for (int i = 0; i < NUM_CASCADES; ++i)
		{
			const float p = (i + 1) / static_cast<float>(NUM_CASCADES);
			const float logSplit = minZ * std::pow(ratio, p);
			const float uniformSplit = minZ + range * p;
			const float dist = splitLambda * (logSplit - uniformSplit) + uniformSplit;
			cascadeSplits[i] = dist;
			m_GPUData.cascadeSplits[i] = dist;
		}

		glm::mat4 invView = glm::inverse(camera->view);

		const float aspect = camera->width / camera->height;
		const float fovRadians = glm::radians(camera->fov);
		const float tanHalfFovY = std::tan(fovRadians * 0.5f);
		const float tanHalfFovX = tanHalfFovY * aspect;

		glm::vec3 upDir = camera->GetUpDirection();
		if (std::abs(glm::dot(upDir, lightDir)) > 0.95f)
			upDir = glm::vec3(0.0f, 0.0f, 1.0f);

		float lastSplitDist = nearPlane;

		for (int cascadeIdx = 0; cascadeIdx < NUM_CASCADES; ++cascadeIdx)
		{
			const float splitDist = cascadeSplits[cascadeIdx];
			const float nearDist = lastSplitDist;
			const float farDist = splitDist;

			const float nearX = tanHalfFovX * nearDist;
			const float nearY = tanHalfFovY * nearDist;
			const float farX = tanHalfFovX * farDist;
			const float farY = tanHalfFovY * farDist;

			std::array<glm::vec3, 8> frustumCornersVS{
				glm::vec3(-nearX, -nearY, -nearDist),
				glm::vec3( nearX, -nearY, -nearDist),
				glm::vec3( nearX,  nearY, -nearDist),
				glm::vec3(-nearX,  nearY, -nearDist),
				glm::vec3(-farX,  -farY,  -farDist),
				glm::vec3( farX,  -farY,  -farDist),
				glm::vec3( farX,   farY,  -farDist),
				glm::vec3(-farX,   farY,  -farDist)
			};

			std::array<glm::vec3, 8> frustumCornersWS;
			for (size_t iCorner = 0; iCorner < frustumCornersVS.size(); ++iCorner)
			{
				glm::vec4 worldCorner = invView * glm::vec4(frustumCornersVS[iCorner], 1.0f);
				frustumCornersWS[iCorner] = glm::vec3(worldCorner);
			}

			glm::vec3 frustumCenter(0.0f);
			for (const auto& corner : frustumCornersWS)
				frustumCenter += corner;
			frustumCenter /= static_cast<float>(frustumCornersWS.size());

			float radius = 0.0f;
			for (const auto& corner : frustumCornersWS)
				radius = glm::max(radius, glm::length(corner - frustumCenter));
			radius = std::ceil(radius * 16.0f) / 16.0f;

			auto computeCascadeBounds = [&](const glm::mat4& lightViewMatrix, glm::vec3& outMin, glm::vec3& outMax)
			{
				outMin = glm::vec3(std::numeric_limits<float>::max());
				outMax = glm::vec3(std::numeric_limits<float>::lowest());
				for (const auto& corner : frustumCornersWS)
				{
					glm::vec4 cornerLS = lightViewMatrix * glm::vec4(corner, 1.0f);
					outMin = glm::min(outMin, glm::vec3(cornerLS));
					outMax = glm::max(outMax, glm::vec3(cornerLS));
				}
			};

			glm::vec3 cascadeCenter = frustumCenter;
			glm::vec3 lightPos = cascadeCenter - lightDir * radius * 2.0f;
			glm::mat4 lightView = glm::lookAt(lightPos, cascadeCenter, upDir);

			glm::vec3 cascadeMin, cascadeMax;
			computeCascadeBounds(lightView, cascadeMin, cascadeMax);

			float extent = glm::max(cascadeMax.x - cascadeMin.x, cascadeMax.y - cascadeMin.y) * 0.5f;
			extent = glm::max(extent, radius);

			// float texelSize = (extent * 2.0f) / static_cast<float>(m_Resolution);
			// if (texelSize <= 0.0f)
			// 	texelSize = 1.0f / static_cast<float>(m_Resolution);

			// glm::vec3 centerLS = glm::vec3(lightView * glm::vec4(cascadeCenter, 1.0f));
			// centerLS.x = std::floor(centerLS.x / texelSize) * texelSize;
			// centerLS.y = std::floor(centerLS.y / texelSize) * texelSize;

			// glm::mat4 invLightView = glm::inverse(lightView);
			// cascadeCenter = glm::vec3(invLightView * glm::vec4(centerLS, 1.0f));
			// lightPos = cascadeCenter - lightDir * radius * 2.0f;
			// lightView = glm::lookAt(lightPos, cascadeCenter, upDir);

			computeCascadeBounds(lightView, cascadeMin, cascadeMax);
			extent = glm::max(cascadeMax.x - cascadeMin.x, cascadeMax.y - cascadeMin.y) * 0.5f;
			extent = glm::max(extent, radius);

			cascadeMin.x = -extent;
			cascadeMax.x = extent;
			cascadeMin.y = -extent;
			cascadeMax.y = extent;

			const float zPadding = 100.0f;
			cascadeMin.z -= zPadding;
			cascadeMax.z += zPadding;

			float nearPlaneLS = glm::max(0.001f, -cascadeMax.z);
			float farPlaneLS = glm::max(nearPlaneLS + 1.0f, -cascadeMin.z);

			glm::mat4 lightProj = glm::orthoZO(cascadeMin.x, cascadeMax.x, cascadeMin.y, cascadeMax.y, nearPlaneLS, farPlaneLS);

			glm::mat4 lightViewProj = lightProj * lightView;
			glm::vec4 shadowOrigin = lightViewProj * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
			shadowOrigin *= static_cast<float>(m_Resolution) / 2.0f;
			glm::vec4 roundedOrigin = glm::round(shadowOrigin);
			glm::vec4 roundOffset = (roundedOrigin - shadowOrigin) * 2.0f / static_cast<float>(m_Resolution);
			lightProj[3][0] += roundOffset.x;
			lightProj[3][1] += roundOffset.y;

			m_GPUData.lightViewProj[cascadeIdx] = lightProj * lightView;

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
		params.depthFunc = nvrhi::ComparisonFunc::Less;
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
		m_Pipeline->SetShaders({ m_VS, m_PS }).AddBindingLayout(m_BindingLayout).Build(framebuffer, params);
    }

	nvrhi::IFramebuffer* CascadedShadowMap::GetCascadeFramebuffer(int cascadeIndex) const
	{
		if (cascadeIndex >= 0 && cascadeIndex < NUM_CASCADES)
			return m_CascadeFramebuffers[cascadeIndex];
		return nullptr;
	}

	void CascadedShadowMap::CreateCascadeFramebuffers()
	{
		nvrhi::IDevice* device = Application::GetGraphicsDevice();

	    // Create All depth map layers

	    nvrhi::Format depthFormat = nvrhi::Format::D32;

	    TextureCreateInfo depthCI;
	    // depthCI.debugName = "Cascaded Shadow Map Depth";
	    depthCI.width = m_Resolution;
	    depthCI.height = m_Resolution;
		depthCI.isRenderTarget = true;
	    depthCI.mipLevels = 1;
	    depthCI.arraySize = NUM_CASCADES;
	    depthCI.format = depthFormat;
	    depthCI.dimension = nvrhi::TextureDimension::Texture2DArray;
		depthCI.initialState = nvrhi::ResourceStates::DepthWrite;
		depthCI.keepInitialState = true;

	    m_DepthTexture = Texture::Create(depthCI);

		// Create a framebuffer for each cascade layer
		for (int i = 0; i < NUM_CASCADES; ++i)
		{
#if 0
		    TextureCreateInfo viewCI;
		    viewCI.width = m_Resolution;
		    viewCI.height = m_Resolution;
		    viewCI.mipLevels = 1;
		    viewCI.arraySize = 1;
		    viewCI.format = nvrhi::Format::RGBA32_FLOAT; // Color format for ImGui
		    viewCI.dimension = nvrhi::TextureDimension::Texture2D;
		    viewCI.debugName = std::format("Cascade {} Layer View", i);
		    viewCI.isRenderTarget = false;
		    viewCI.isUAV = true; // Enable UAV for compute shader write
		    viewCI.isTypeless = false;
		    viewCI.initialState = nvrhi::ResourceStates::UnorderedAccess;
		    viewCI.keepInitialState = true;
		    m_CascadeLayerViews[i] = Texture::Create(viewCI);
#endif
		    // Create framebuffer
			auto fbDesc = nvrhi::FramebufferDesc();
			nvrhi::FramebufferAttachment depthAttachment;
			depthAttachment.texture = m_DepthTexture->GetHandle();
			depthAttachment.subresources = nvrhi::TextureSubresourceSet(0, 1, i, 1); // mip 0, array layer i
			depthAttachment.format = nvrhi::Format::D32;

			fbDesc.setDepthAttachment(depthAttachment);
			m_CascadeFramebuffers[i] = device->createFramebuffer(fbDesc);
			LOG_ASSERT(m_CascadeFramebuffers[i], "Failed to create cascade framebuffer for layer {}", i);
		}
	}
}