// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_ICAMERA_HPP
#define IGN_ICAMERA_HPP

#include "ignite/core/base.hpp"
#include "ignite/core/types.hpp"
#include "ignite/math/math.hpp"
#include <string>

namespace ignite
{
    enum class ProjectionType
    {
        Orthographic = 0, Perspective = 1
    };

    enum class TonemapMode
    {
        Reinhard = 0,
        Uncharted2 = 1,
        Filmic = 2,
    };

    struct CameraBufferData
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

    struct TAAProperties
    {
        bool enable = false;
        float blendFactor = 0.4825f; // Current-frame weight; lower is smoother, higher is more responsive
    };

    struct MSAAProperties
    {
        bool enable = false;
        int sampleCount = 4; // Requested MSAA sample count for compatible render paths
    };

    struct PostProcessing
    {
        // Toggles
        bool enableVignette = false;
        bool enableChromAb = false;
        bool enableBloom = false;
        bool enableSSAO = false; // HBAO-compatible ambient occlusion toggle
        bool debugSSAO = false; // Visualize raw AO buffer

        // Bloom
        float bloomIntensity = 1.5f;
        float bloomThreshold = 0.85f;    // HDR threshold
        float bloomKnee = 0.5f;         // Soft knee for smooth transition
        float bloomRadius = 1.0f;       // Blur radius multiplier
        int bloomIterations = 6;        // More levels for higher quality

        // Vignette params
        float vignetteRadius = 1.1f;
        float vignetteSoftness = 0.7f;
        float vignetteIntensity = 0.8f;
        glm::vec3 vignetteColor = glm::vec3(0.0f);

        // Chromatic aberration params
        float chromAbAmount = 0.001f;
        float chromAbRadial = 0.1f;

        // HBAO params
        float aoRadius = 1.2f;      // view-space horizon radius in world units
        float aoBias = 0.03f;       // tangent bias to reduce self-occlusion
        float aoIntensity = 1.0f;   // blend strength when applied in post (1.0 = natural)
        float aoPower = 1.35f;      // curve/power for contrast

		// Anti-aliasing 
		TAAProperties taaProperties;   // Current-frame weight; lower is smoother, higher is more responsive
		MSAAProperties msaaProperties; // Requested MSAA sample count for compatible render paths
		float renderScale = 1.0f;      // Internal scene resolution scale, composite remains native size

		// Tonemapping
		TonemapMode tonemapMode = TonemapMode::Reinhard;
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
        bool enabledDOF = false;
    };

    class IGN_API ICamera
    {
    public:
        ICamera();
        virtual ~ICamera() = default;

        virtual void UpdateView();
        virtual void UpdateProjection(uint32_t width, uint32_t height);

        // Directly set the view matrix (used for mirror/proxy cameras that copy another camera's view)
        void SetView(const glm::mat4 &view) { m_View = view; }
       
        virtual glm::vec3 GetUpDirection() const;
        virtual glm::vec3 GetRightDirection() const;
        virtual glm::vec3 GetForwardDirection() const;

        virtual glm::mat4 &GetProjection();
        virtual glm::mat4 GetView();

        inline bool IsPerspective() const { return projectionType == ProjectionType::Perspective; }
        inline const glm::uvec2 &GetViewportSize() const { return m_ViewportSize; }

        float pitch = 0.0f; // rotation around X axis
        float yaw = 0.0f; // rotation around Y axis

        float fov = 45.0f; // for perspective
        float nearPlane = 0.1f;
        float farPlane = 500.0f;
        float orthoSize = 10.0f;

        glm::vec3 position;

        // Control settings
        struct Controls
        {
            float mouseSensitivity = 0.003f;
            float zoomSensitivity = 2.0f;
            float panSensitivity = 0.001f;
            float minDistance = 0.5f;
            float maxDistance = 1500.0f;
            float minOrthoSize = 0.0001f;
            float maxOrthoSize = 1000.0f;
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

    protected:
        glm::mat4 m_View = glm::mat4(1.0f);
        glm::mat4 m_Projection = glm::mat4(1.0f);
        glm::uvec2 m_ViewportSize;
    };
}

#endif
