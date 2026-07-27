// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "ignite/graphics/gpu_data.hpp"

#include "ignite/graphics/texture.hpp"
#include "ignite/graphics/renderer.hpp"

#include "shadow_map.hpp"
#include "ignite/scene/icamera.hpp"
#include "ignite/core/device/device_manager.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/common.hpp>

namespace ignite
{
	CascadedShadowMap::CascadedShadowMap(ShadowMapQuality quality)
	{
        m_GPUDataBuffer = ConstantBuffer::Create(sizeof(CSM_GPUData), true, 256, "Cascadded ShadowMap");
        m_ModelGPUDataBuffer = ConstantBuffer::Create(sizeof(CSMModel_GPUData), true, 256, "Cascadded Model ShadowMap");

		// Initialize shadow parameters with reasonable defaults
		m_GPUData.shadowStrength = 0.8f;   // 80% shadow visibility
		m_GPUData.minBias = 0.0002f;        // bias for surfaces facing light directly
		m_GPUData.maxBias = 0.002f;         // bias for steep/grazing-angle surfaces
		m_GPUData.pcfRadius = 0.3f;         // PCF filter radius in texels for cascade 0
		
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
					   ShadowMapQuality::ULTRA == quality ? 4096 :
					   ShadowMapQuality::ULTIMATE == quality ? 8192 : 1024;
		
		m_Quality = quality;
		m_GPUData.shadowTexelSize = 1.0f / static_cast<float>(m_Resolution);

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

		m_DepthSampler = device->createSampler(samplerDesc);
		LOG_ASSERT(m_DepthSampler, "Failed to create depth comparison sampler");

		CreateCascadeFramebuffers();
	}

	void CascadedShadowMap::BeginCascade(nvrhi::ICommandList *cmd, int cascadeIndex, uint32_t frameIndex)
	{
		IGN_PROFILE_FUNCTION();

		// Clear depth for the entire texture (all layers) on first cascade
		if (cascadeIndex == 0)
		{
			if (frameIndex < m_DepthTextures.size() && m_DepthTextures[frameIndex])
			{
				nvrhi::TextureSubresourceSet subresources = nvrhi::AllSubresources;
				cmd->clearDepthStencilTexture(m_DepthTextures[frameIndex]->GetHandle(), subresources, true, 1.0f, false, 0);
			}
		}
	}

	Ref<Texture> CascadedShadowMap::GetDepthTexture(uint32_t frameIndex) const
	{
		if (frameIndex < m_DepthTextures.size())
			return m_DepthTextures[frameIndex];
		return nullptr;
	}

	void CascadedShadowMap::ComputeMatrices(ICamera *camera, const glm::vec3 &lightPosition, float shadowDistance)
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

		// Practical split scheme (Engel 2006).
		// λ=0.85 biases heavily toward logarithmic splits so the near cascade
		// gets the highest texel density (crispest shadows close to the camera).
		constexpr float splitLambda = 0.85f;

		// The CSM near plane must be large enough for the log/uniform ratio to
		// produce meaningful splits. With nearPlane=0.1 and farPlane=1000, the
		// ratio is 10000 and the log term becomes negligible — the splits
		// degenerate to nearly uniform, wasting resolution. Clamping to ≥0.5
		// keeps the ratio reasonable (≤400 for shadowDistance=200).
		const float csmNear = glm::max(camera->nearPlane, 0.5f);

		// Use shadowDistance instead of camera->farPlane for CSM coverage.
		// The camera needs a large far plane (1000) to render distant geometry,
		// but shadows only need to extend ~150–300 units from the camera.
		// Using the full far plane spreads the shadow budget over too much
		// depth, making every cascade blurry.
		m_ShadowDistance = glm::max(shadowDistance, csmNear + 1.0f);
		const float csmFar = m_ShadowDistance;

		const float range = csmFar - csmNear;
		const float ratio = csmFar / csmNear;

		std::array<float, NUM_CASCADES> cascadeSplits{};
		for (int i = 0; i < NUM_CASCADES; ++i)
		{
			const float p            = (i + 1) / static_cast<float>(NUM_CASCADES);
			const float logSplit     = csmNear * std::pow(ratio, p);
			const float uniformSplit = csmNear + range * p;
			const float dist         = splitLambda * (logSplit - uniformSplit) + uniformSplit;
			cascadeSplits[i]           = dist;
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

		float lastSplitDist = csmNear;

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

			// Bounding sphere around the frustum slice — rotation-invariant so
			// the ortho extent doesn't flicker when the camera rotates.
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
			// Step 1 – Build an initial light-view from the raw frustum center so we
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

			// Extra Z padding so tall casters (trees, buildings) behind the camera
			// sub-frustum are not clipped out of the shadow depth pass.
			const float zPadding = 150.0f;
			cascadeMin.z -= zPadding;
			cascadeMax.z += zPadding;

			const float nearPlaneLS = glm::max(0.001f, -cascadeMax.z);
			const float farPlaneLS  = glm::max(nearPlaneLS + 1.0f, -cascadeMin.z);

			// NOTE: The light-eye position was already snapped to a texel boundary
			// in Step 3 above. Do NOT apply a second snap here on lightProj — that
			// would fight the first snap and re-introduce sub-texel jitter.
			glm::mat4 lightProj = glm::orthoZO(cascadeMin.x, cascadeMax.x, cascadeMin.y, cascadeMax.y, nearPlaneLS, farPlaneLS);

			m_GPUData.lightViewProj[cascadeIdx] = lightProj * lightView;

			lastSplitDist = splitDist;
		}
	}

	nvrhi::IFramebuffer *CascadedShadowMap::GetCascadeFramebuffer(int cascadeIndex, uint32_t frameIndex) const
	{
		if (frameIndex < m_CascadeFramebuffers.size() && cascadeIndex >= 0 && cascadeIndex < NUM_CASCADES)
			return m_CascadeFramebuffers[frameIndex][cascadeIndex];
		return nullptr;
	}

	void CascadedShadowMap::CreateCascadeFramebuffers()
	{
		IGN_PROFILE_FUNCTION();

		nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();
		uint32_t maxFrames = DeviceManager::GetInstance()->GetDeviceParameters().maxFramesInFlight;

		m_DepthTextures.resize(maxFrames);
		m_CascadeFramebuffers.resize(maxFrames);

		for (uint32_t f = 0; f < maxFrames; ++f)
		{
			constexpr nvrhi::Format depthFormat = nvrhi::Format::D32;

			TextureCreateInfo depthCI;
			depthCI.width = m_Resolution;
			depthCI.height = m_Resolution;
			depthCI.isRenderTarget = true;
			depthCI.mipLevels = 1;
			depthCI.arraySize = NUM_CASCADES;
			depthCI.format = depthFormat;
			depthCI.dimension = nvrhi::TextureDimension::Texture2DArray;
			depthCI.initialState = nvrhi::ResourceStates::DepthWrite;
			depthCI.keepInitialState = true;

			m_DepthTextures[f] = Texture::Create(depthCI);

			// Create a framebuffer for each cascade layer
			for (int i = 0; i < NUM_CASCADES; ++i)
			{
				// Create framebuffer
				auto fbDesc = nvrhi::FramebufferDesc();
				nvrhi::FramebufferAttachment depthAttachment;
				depthAttachment.texture = m_DepthTextures[f]->GetHandle();
				depthAttachment.subresources = nvrhi::TextureSubresourceSet(0, 1, i, 1); // mip 0, array layer i
				depthAttachment.format = nvrhi::Format::D32;

				fbDesc.setDepthAttachment(depthAttachment);
				m_CascadeFramebuffers[f][i] = device->createFramebuffer(fbDesc);
				LOG_ASSERT(m_CascadeFramebuffers[f][i], "Failed to create cascade framebuffer for layer {}", i);
			}
		}
	}
}