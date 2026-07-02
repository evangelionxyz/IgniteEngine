/* MIT License
*
* Copyright (c) 2026 Evangelion Manuhutu
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

#ifndef IGN_GPU_DATA_HPP
#define IGN_GPU_DATA_HPP

#include <glm/glm.hpp>

namespace ignite
{
	static constexpr int NUM_CASCADES = 4;
	static constexpr int MAX_BONES = 100;
	static constexpr int VERTEX_MAX_BONES = 4;

	struct SkinnedMeshBufferData
	{
		glm::mat4 transformation;
		glm::mat4 normal;
        uint32_t objectID = 0xFFFFFFFFu;
		glm::vec3 _padding = glm::vec3(0.0f);
	};

	struct SceneBufferData
	{
		glm::vec4 sunColor = glm::vec4(0.87f, 0.87f, 0.87f, 1.1f); // w = light intensity
		glm::vec2 sungAngles = glm::vec2(0.0f, 1.0f);
		float sunAngularRadius = 0.5f;
		int renderMode = 0;
	    int debugShadow = 0;

		float exposure = 1.1f;
		float gamma = 2.2f;
		float ambient = 0.5f;
	};

	struct CascadedShadowMapBufferData
	{
		glm::mat4 lightViewProj[NUM_CASCADES];
		float cascadeSplits[NUM_CASCADES]; // view-space distances to end of each cascade
		float shadowStrength;
		float minBias = 0.001f;
		float maxBias = 0.05f;
		float pcfRadius = 0.3f; // in texels (multiplier)

		int cascadeIndex;
		float padding[3];
	};

	struct CascadedShadowMapModelBufferData
	{
		glm::mat4 transformation;
		glm::mat4 boneTransforms[MAX_BONES];
	};

	struct MaterialBufferData
	{
		glm::vec4 baseColorFactor = glm::vec4(1.0f);
		glm::vec4 emissiveFactor = glm::vec4(1.0f);
		float metallicFactor = 1.0f;
		float roughnessFactor = 1.0f;
		float occlusionStrength = 0.0f;
		int metallicChannel = 2;
		int roughnessChannel = 1;
		int blendMode = 0; // 0 = Opaque, 1 = Transparent
		glm::vec2 tilingFactor = glm::vec2(1.0f, 1.0f);
	};
}

#endif