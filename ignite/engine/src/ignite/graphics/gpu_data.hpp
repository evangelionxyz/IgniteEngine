// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_GPU_DATA_HPP
#define IGN_GPU_DATA_HPP

#include <glm/glm.hpp>

namespace ignite
{
	static constexpr int NUM_CASCADES = 4;
	static constexpr int MAX_BONES = 100;
	static constexpr int VERTEX_MAX_BONES = 4;
	static constexpr int MAX_POINT_LIGHTS = 16;
	static constexpr int MAX_SPOT_LIGHTS = 16;

	struct Mesh_GPUData
	{
		glm::mat4 transformation;
		glm::mat4 normal;
        uint32_t objectID = 0xFFFFFFFFu;
        uint32_t boneOffset = 0;
		glm::vec2 _padding = glm::vec2(0.0f);
	};

	struct Scene_GPUData
	{
		glm::vec4 sunColor = glm::vec4(0.87f, 0.87f, 0.87f, 1.1f); // w = light intensity
		glm::vec2 sungAngles = glm::vec2(0.0f, 1.0f);
		float sunAngularRadius = 0.5f;
		int renderMode = 0;
		int debugShadow = 0;

		float exposure = 1.1f;
		float gamma = 2.2f;
		float ambient = 0.5f;

		int numPointLights = 0;
		int numSpotLights = 0;
		int skyType = 0; // 0 = HDRI, 1 = Procedural Sky
		float _pad2 = 0.0f;

		glm::vec4 sunDirection = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
	};

	struct CSM_GPUData
	{
		glm::mat4 lightViewProj[NUM_CASCADES];
		float cascadeSplits[NUM_CASCADES]; // view-space distances to end of each cascade
		float shadowStrength;
		// Slope-scaled bias bounds. The shader interpolates between these based on
		// surface angle (cosTheta) and then multiplies by a per-cascade scale.
		// Adjust via the Scene panel; these are sane defaults for ortho [0,1] depth.
		float minBias = 0.0002f;  // bias for surfaces facing the light directly
		float maxBias = 0.002f;   // bias for steep/grazing-angle surfaces
		float pcfRadius = 0.3f;   // PCF filter radius in texels for cascade 0

		int cascadeIndex;
		float shadowTexelSize = 1.0f / 2048.0f;
		float padding[2];
	};

	struct CSMModel_GPUData
	{
		glm::mat4 transformation;
		glm::mat4 boneTransforms[MAX_BONES];
	};

	struct Material_GPUData
	{
		glm::vec4 baseColorFactor = glm::vec4(1.0f);
		glm::vec4 emissiveFactor = glm::vec4(0.0f);
		float metallicFactor = 0.0f;
		float roughnessFactor = 0.0f;
		float occlusionStrength = 0.0f;
		int metallicChannel = 2;
		int roughnessChannel = 1;
		int blendMode = 0; // 0 = Opaque, 1 = Transparent
		glm::vec2 tilingFactor = glm::vec2(1.0f, 1.0f);
		int baseColorTextureIndex = 0;
		int emissiveTextureIndex = 0;
		int metallicTextureIndex = 0;
		int roughnessTextureIndex = 0;
		int normalTextureIndex = 0;
		int occlusionTextureIndex = 0;
		glm::vec2 _pad = glm::vec2(0.0f); // 16-byte alignment pad

		// OpenPBR Surface core. The existing metallic/roughness values map to
		// base_metalness and specular_roughness respectively.
		float baseWeight = 1.0f;
		float specularWeight = 1.0f;
		float specularIOR = 1.5f;
		float coatWeight = 0.0f;
		glm::vec4 specularColor = glm::vec4(1.0f);
		glm::vec4 coatColor = glm::vec4(1.0f); // squared normal-incidence transmittance
		float coatRoughness = 0.0f;
		float coatIOR = 1.6f;
		float coatDarkening = 1.0f;
		float specularAnisotropy = 0.0f;   // [0,1] stretch of specular highlight along tangent

		// OpenPBR §subsurface — diffuse subsurface scattering approximation
		float subsurfaceWeight = 0.0f;     // [0,1] blend from opaque-diffuse to subsurface
		float subsurfaceScale  = 1.0f;     // world-space mean-free-path scale
		glm::vec2 _padSS;                  // 16-byte alignment
		glm::vec4 subsurfaceColor  = glm::vec4(1.0f);           // tint of subsurface scattering
		glm::vec4 subsurfaceRadius = glm::vec4(1.0f, 0.2f, 0.1f, 0.0f); // per-channel MFP ratios (RGB + pad)

		// OpenPBR §translucent-base — thin-slab Beer's law transmission
		float transmissionWeight = 0.0f;   // [0,1] blend from opaque to translucent
		float transmissionDepth  = 0.0f;   // absorption depth
		glm::vec2 _padTR;                  // 16-byte alignment
		glm::vec4 transmissionColor = glm::vec4(1.0f); // volumetric absorption tint

		// OpenPBR §fuzz — microflake-based fuzz/sheen layer
		float fuzzWeight    = 0.0f;        // [0,1] fuzz coverage
		float fuzzRoughness = 0.5f;        // fuzz lobe roughness
		glm::vec2 _padFZ;                  // 16-byte alignment
		glm::vec4 fuzzColor = glm::vec4(1.0f); // fuzz albedo

		// Advanced
		float useMultiScatter   = 1.0f;    // [0,1] enable Kulla-Conty energy compensation
		float emissionLuminance = 1.0f;     // nits scale for emission
		glm::vec2 _padAdv;                 // 16-byte alignment
	};

	// GPU-side point light data (16-byte aligned for HLSL constant buffers)
	struct PointLight_GPUData
	{
		glm::vec4 positionAndRange;  // xyz = position, w = range
		glm::vec4 color;             // rgb = color, a = intensity
		glm::vec4 attenuation;       // x = constant, y = linear, z = quadratic, w = unused
	};

	// GPU-side spot light data (16-byte aligned for HLSL constant buffers)
	struct SpotLight_GPUData
	{
		glm::vec4 positionAndRange;  // xyz = position, w = range
		glm::vec4 directionAndAngle; // xyz = direction, w = cos(outerConeAngle)
		glm::vec4 color;             // rgb = color, a = intensity
		glm::vec4 attenuation;       // x = constant, y = linear, z = quadratic, w = cos(innerConeAngle)
	};

	struct PointLightBufferData
	{
		PointLight_GPUData lights[MAX_POINT_LIGHTS];
	};

	struct SpotLightBufferData
	{
		SpotLight_GPUData lights[MAX_SPOT_LIGHTS];
	};
}

#endif
