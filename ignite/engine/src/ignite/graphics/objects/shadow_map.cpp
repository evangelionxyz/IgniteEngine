// Copyright (c) 2026 Evangelion Manuhutu

#include "pch.hpp"

#include "ignite/graphics/gpu_data.hpp"

#include "ignite/graphics/texture.hpp"
#include "ignite/graphics/renderer.hpp"

#include "shadow_map.hpp"
#include "ignite/scene/icamera.hpp"
#include "ignite/core/device/device_manager.hpp"
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
        m_GPUDataBuffer = ConstantBuffer::Create(sizeof(CascadedShadowMapBufferData), true, 256, "Cascadded ShadowMap");
        m_ModelGPUDataBuffer = ConstantBuffer::Create(sizeof(CascadedShadowMapModelBufferData), true, 256, "Cascadded Model ShadowMap");

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

		IGN_PROFILE_FUNCTION();

		m_Resolution = ShadowMapQuality::LOW == quality ? 512 :
					   ShadowMapQuality::MEDIUM == quality ? 1024 :
					   ShadowMapQuality::HIGH == quality ? 2048 :
					   ShadowMapQuality::ULTRA == quality ? 4096 : 1024;
		
		m_Quality = quality;

		// Linear clamp sampler for shadow map PCF — used with SampleLevel in the shader.
		// Linear filtering provides smooth interpolation between depth texels across Poisson taps.
		nvrhi::IDevice* device = DeviceManager::GetInstance()->GetDevice();

		nvrhi::SamplerDesc samplerDesc;
		samplerDesc.addressU = nvrhi::SamplerAddressMode::ClampToEdge;
		samplerDesc.addressV = nvrhi::SamplerAddressMode::ClampToEdge;
		samplerDesc.addressW = nvrhi::SamplerAddressMode::ClampToEdge;
		samplerDesc.borderColor = nvrhi::Color(1.0f, 1.0f, 1.0f, 1.0f);
		samplerDesc.minFilter = true;
		samplerDesc.magFilter = true;
		samplerDesc.mipFilter = false;
		// Standard sampler: depth comparison is done manually in the shader.
		// NVRHI binds this as the texture's sampler via Texture::GetSampler(), so
		// this m_DepthSampler is kept for reference but the binding uses the
		// depth texture's own internal sampler created in Texture::Create().

		m_DepthSampler = device->createSampler(samplerDesc);
		LOG_ASSERT(m_DepthSampler, "Failed to create depth comparison sampler");

		CreateCascadeFramebuffers();
		CreatePipeline(m_CascadeFramebuffers[0]);
	}

	void CascadedShadowMap::BeginCascade(nvrhi::ICommandList *cmd, int cascadeIndex)
	{
		IGN_PROFILE_FUNCTION();

		// Clear depth for the entire texture (all layers) on first cascade
		if (cascadeIndex == 0)
		{
			nvrhi::TextureSubresourceSet subresources = nvrhi::AllSubresources;
			cmd->clearDepthStencilTexture(m_DepthTexture->GetHandle(), subresources, true, 1.0f, false, 0);
		}
	}

	Ref<Texture> CascadedShadowMap::GetDepthTexture() const
	{
		return m_DepthTexture;
	}

	void CascadedShadowMap::ComputeMatrices(ICamera *camera, const glm::vec3 &lightPosition)
	{
		IGN_PROFILE_FUNCTION();

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

		glm::mat4 invView = glm::inverse(camera->GetView());

		const float aspect = static_cast<float>(camera->GetViewportSize().x) / static_cast<float>(camera->GetViewportSize().y);
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
				glm::vec3(nearX, -nearY, -nearDist),
				glm::vec3(nearX,  nearY, -nearDist),
				glm::vec3(-nearX,  nearY, -nearDist),
				glm::vec3(-farX,  -farY,  -farDist),
				glm::vec3(farX,  -farY,  -farDist),
				glm::vec3(farX,   farY,  -farDist),
				glm::vec3(-farX,   farY,  -farDist)
			};

			std::array<glm::vec3, 8> frustumCornersWS;
			for (size_t iCorner = 0; iCorner < frustumCornersVS.size(); ++iCorner)
			{
				glm::vec4 worldCorner = invView * glm::vec4(frustumCornersVS[iCorner], 1.0f);
				frustumCornersWS[iCorner] = glm::vec3(worldCorner);
			}

			glm::vec3 frustumCenter(0.0f);
			for (const auto &corner : frustumCornersWS)
				frustumCenter += corner;
			frustumCenter /= static_cast<float>(frustumCornersWS.size());

			float radius = 0.0f;
			for (const auto &corner : frustumCornersWS)
				radius = glm::max(radius, glm::length(corner - frustumCenter));
			radius = std::ceil(radius * 16.0f) / 16.0f;

			auto computeCascadeBounds = [&](const glm::mat4 &lightViewMatrix, glm::vec3 &outMin, glm::vec3 &outMax)
				{
					outMin = glm::vec3(std::numeric_limits<float>::max());
					outMax = glm::vec3(std::numeric_limits<float>::lowest());
					for (const auto &corner : frustumCornersWS)
					{
						glm::vec4 cornerLS = lightViewMatrix * glm::vec4(corner, 1.0f);
						outMin = glm::min(outMin, glm::vec3(cornerLS));
						outMax = glm::max(outMax, glm::vec3(cornerLS));
					}
				};

			// -----------------------------------------------------------------------
			// Step 1 – Snap the bounding sphere radius to a whole-texel multiple.
			// This prevents the ortho extent from changing sub-texel as the camera
			// rotates, which is the primary cause of shadow-map shimmering.
			// -----------------------------------------------------------------------
			const float texelWorldSize = (2.0f * radius) / static_cast<float>(m_Resolution);
			radius = std::ceil(radius / texelWorldSize) * texelWorldSize;

			// -----------------------------------------------------------------------
			// Step 2 – Build an initial light-view from the raw frustum center so we
			// can measure the sub-texel offset of that center.
			// -----------------------------------------------------------------------
			const float extent = radius; // symmetric ortho half-extent
			const float texelsPerUnit = static_cast<float>(m_Resolution) / (2.0f * extent);

			glm::vec3 cascadeCenter = frustumCenter;
			glm::vec3 lightPos = cascadeCenter - lightDir * radius * 2.0f;
			glm::mat4 lightView = glm::lookAt(lightPos, cascadeCenter, upDir);

			// -----------------------------------------------------------------------
			// Step 3 – Snap the light-eye position so the frustum center lands exactly
			// on a texel boundary. We move the camera rather than patching the
			// projection matrix — this avoids the NDC-scale-factor bug and is how
			// Unreal Engine 4/5 and Unity HDRP implement stable CSM.
			// -----------------------------------------------------------------------
			{
				// Project frustum center into light-view space.
				glm::vec3 centerLS = glm::vec3(lightView * glm::vec4(cascadeCenter, 1.0f));

				// Express in texel-space and find the fractional (sub-texel) part.
				glm::vec2 centerTex  = glm::vec2(centerLS.x, centerLS.y) * texelsPerUnit;
				glm::vec2 snapOffset = (glm::round(centerTex) - centerTex) / texelsPerUnit;

				// Shift the light position along the light's local X/Y axes.
				// Extracting the axes from the view matrix rows avoids an extra inverse.
				glm::vec3 lightRight = glm::vec3(lightView[0][0], lightView[1][0], lightView[2][0]);
				glm::vec3 lightUp    = glm::vec3(lightView[0][1], lightView[1][1], lightView[2][1]);
				lightPos += lightRight * snapOffset.x + lightUp * snapOffset.y;

				// Rebuild the view matrix with the snapped position.
				lightView = glm::lookAt(lightPos, lightPos + lightDir, upDir);
			}

			// -----------------------------------------------------------------------
			// Step 4 – Build symmetric ortho and depth bounds.
			// -----------------------------------------------------------------------
			glm::vec3 cascadeMin, cascadeMax;
			computeCascadeBounds(lightView, cascadeMin, cascadeMax);

			cascadeMin.x = -extent;
			cascadeMax.x =  extent;
			cascadeMin.y = -extent;
			cascadeMax.y =  extent;

			const float zPadding = 100.0f;
			cascadeMin.z -= zPadding;
			cascadeMax.z += zPadding;

			const float nearPlaneLS = glm::max(0.001f, -cascadeMax.z);
			const float farPlaneLS  = glm::max(nearPlaneLS + 1.0f, -cascadeMin.z);

			const glm::mat4 lightProj = glm::orthoZO(
				cascadeMin.x, cascadeMax.x,
				cascadeMin.y, cascadeMax.y,
				nearPlaneLS,  farPlaneLS);

			m_GPUData.lightViewProj[cascadeIdx] = lightProj * lightView;

			lastSplitDist = splitDist;
		}
	}

	void CascadedShadowMap::CreatePipeline(nvrhi::IFramebuffer *framebuffer)
	{
		IGN_PROFILE_FUNCTION();

        if (!m_VS) m_VS = Shader::Create("resources/shaders/cascaded_shadow_depth.vertex.hlsl", UMBRA_SHADER_TYPE_VERTEX, false);
		if (!m_PS) m_PS = Shader::Create("resources/shaders/cascaded_shadow_depth.pixel.hlsl", UMBRA_SHADER_TYPE_PIXEL, false);

		GraphicsPipelineParams params;
		params.enableDepthWrite = true;
		params.enableDepthTest = true;
		params.depthFunc = nvrhi::ComparisonFunc::Less;
		params.cullMode = nvrhi::RasterCullMode::Front;
		params.fillMode = nvrhi::RasterFillMode::Solid;
       m_BindingLayout = Renderer::GetBindingLayout(GLayoutMap::MESH_ANIM);

		m_Pipeline = GraphicsPipeline::Create();
		m_Pipeline->SetShaders({ m_VS, m_PS }).AddBindingLayout(m_BindingLayout).Build(framebuffer, params);
	}

	nvrhi::IFramebuffer *CascadedShadowMap::GetCascadeFramebuffer(int cascadeIndex) const
	{
		if (cascadeIndex >= 0 && cascadeIndex < NUM_CASCADES)
			return m_CascadeFramebuffers[cascadeIndex];
		return nullptr;
	}

	void CascadedShadowMap::CreateCascadeFramebuffers()
	{
		IGN_PROFILE_FUNCTION();

		nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();

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