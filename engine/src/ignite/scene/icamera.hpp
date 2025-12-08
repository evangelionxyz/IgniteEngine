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

#include "ignite/core/types.hpp"
#include <nvrhi/nvrhi.h>
#include <string>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

namespace ignite
{
	enum class ProjectionType
	{
		Orthographic = 0, Perspective = 1
	};

	struct CameraBuffer
	{
		glm::mat4 viewProjection;
		glm::mat4 view;
		glm::vec4 position;
	};

	struct CameraMouseState
	{
		glm::vec2 position;
		glm::vec2 lastPosition;
		glm::ivec2 scroll{ 0, 0 };
		bool leftButtonDown = false;
		bool middleButtonDown = false;
		bool rightButtonDown = false;
	};

	struct PostProcessing
	{
		// Toggles
		bool enableVignette = true;
		bool enableChromAb = true;
		bool enableBloom = true;
		bool enableSSAO = true; // Screen space ambient occlusion
		bool debugSSAO = false; // Visualize raw AO buffer

		// Vignette params
		float vignetteRadius = 1.1f;
		float vignetteSoftness = 0.7f;
		float vignetteIntensity = 0.8f;
		glm::vec3 vignetteColor = glm::vec3(0.0f);

		// Chromatic aberration params
		float chromAbAmount = 0.001f;
		float chromAbRadial = 0.1f;

		// SSAO params
		float aoRadius = 0.5f;
		float aoBias = 0.025f;
		float aoIntensity = 1.0f; // blend strength when applied in post
		float aoPower = 1.0f;     // curve/power for contrast
	};

	struct CameraLens
	{
		float focalLength = 120.0f;
		float focalDistance = 5.5f;
		float fStop = 1.4f;
		float focusRange = 5.0f;
		float blurAmount = 1.0f;
		float exposure = 1.1f;
		float gamma = 1.1f;
		bool enabledDOF = true;
	};

    class ICamera
    {
    public:

        ICamera();
        ~ICamera() { }

        void UpdateMatrices(float aspectRatio);
       
        glm::vec3 GetUpDirection() const;
        glm::vec3 GetRightDirection() const;
        glm::vec3 GetForwardDirection() const;

        glm::vec3 position;
		glm::vec3 target = { 0.0f, 0.0f, -1.0f };

		glm::mat4 view;
		glm::mat4 projection;

		float pitch = 0.0f; // rotation around X axis
		float yaw = 0.0f; // rotation around Y axis

		float distance = 1.0f;
		float fov = 45.0f; // for perspective
		float aspect = 1.778f;
		float nearPlane = 0.1f;
		float farPlane = 1000.0f;

		float orthoSize = 10.0f;

		glm::vec2 angularVelocity = glm::vec2(0.0f);
		glm::vec2 panVelocity = glm::vec2(0.0f);
		float zoomVelocity = 0.0f;

		// Control settings
		struct Controls
		{
			float mouseSensitivity = 0.003f;
			float zoomSensitivity = 2.0f;
			float panSensitivity = 0.001f;
			float minDistance = 0.5f;
			float maxDistance = 1500.0f;
			float minPitch = -glm::radians(89.0f);
			float maxPitch = glm::radians(89.0f);
			float inertiaDamping = 0.9f;
			float zoomDamping = 0.65f;
			bool enableInertia = true;
		} controls;

		CameraLens lens;
		CameraMouseState mouse;
		PostProcessing postProcessing;
        ProjectionType projectionType = ProjectionType::Perspective;
    };
}
