// Copyright (c) 2026 Evangelion Manuhutu
// Moved to Ignite.Engine to support native camera controls across all editor frontends.

#pragma once
#ifndef IGN_EDITOR_CAMERA_HPP
#define IGN_EDITOR_CAMERA_HPP

#include "ignite/scene/icamera.hpp"

namespace ignite
{
    class IGN_API EditorCamera : public ICamera
    {
    public:
        enum class NavigationMode
        {
            Orbit = 0,
            Fly = 1,
            Mode2D = 2
        };

        enum class EditorViewPreset : int
        {
            Perspective = 0,
            Orthographic2_5D = 1,
            Top = 2,
            Bottom = 3,
            Front = 4,
            Back = 5,
            Left = 6,
            Right = 7,
            Mode2D = 8
        };

        EditorCamera() = default;
        explicit EditorCamera(const std::string &name);

        virtual void SetView(const glm::mat4 &view) override;
        void SetOrientationFromView(const glm::mat4 &view);

        void UpdateMouseState();
        void UpdateSphericalPosition();
        void HandleOrbit(float deltaTime);
        void HandleFly(float deltaTime);
        void HandlePan(float deltaTime);
        void HandleZoom(float deltaTime);
        void ApplyInertia(float deltaTime);
        void UpdateCameraPosition(float deltaTime);

        void SetNavigationMode(NavigationMode mode);
        NavigationMode GetNavigationMode() const { return m_NavigationMode; }

        void SetViewPreset(EditorViewPreset preset);
        EditorViewPreset GetViewPreset() const { return m_ViewPreset; }
        bool IsCardinalView() const;

        void SetPanSnapValue(const float snap) { m_PanSnapValue = snap; }
        float GetPanSnapValue() const { return m_PanSnapValue; }

        void SetFlySpeed(float speed) { m_FlySpeed = glm::clamp(speed, 0.1f, 500.0f); }
        float GetFlySpeed() const { return m_FlySpeed; }

        void SetFocusSpeed(float speed) { m_FocusSpeed = glm::clamp(speed, 1.0f, 50.0f); }
        float GetFocusSpeed() const { return m_FocusSpeed; }

        void ResetControlSettings();

        virtual void UpdateView() override;

        void SetDistance(float distance) { m_Distance = distance; }
        const float &GetDistance() const { return m_Distance; }

        void SetTarget(const glm::vec3 &target) { m_Target = target; }
        void FocusTarget(const glm::vec3 &target, float distance);
        const glm::vec3 &GetTarget() const { return m_Target; }

        virtual glm::vec3 GetUpDirection() const override;
        virtual glm::vec3 GetRightDirection() const override;
        virtual glm::vec3 GetForwardDirection() const override;

        const std::string& GetName() { return m_Name; }

    private:
        glm::vec2 m_AngularVelocity = glm::vec2(0.0f);
        glm::vec2 m_PanVelocity = glm::vec2(0.0f);
        float m_ZoomVelocity = 0.0f;
        float m_Distance = 1.0f;
        float m_PanSnapValue = 0.0f;
        float m_FlySpeed = 6.0f;

        glm::vec3 m_Target = { 0.0f, 0.0f, -1.0f };
        glm::vec3 m_FocusTarget = { 0.0f, 0.0f, -1.0f };
        float m_FocusDistance = 1.0f;
        float m_FocusSpeed = 8.0f;
        bool m_FocusActive = false;
        NavigationMode m_NavigationMode = NavigationMode::Orbit;
        EditorViewPreset m_ViewPreset = EditorViewPreset::Perspective;

        std::string m_Name;
    };
}

#endif // IGN_EDITOR_CAMERA_HPP
