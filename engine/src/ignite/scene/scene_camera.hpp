// Copyright (c) 2026 Evangelion Manuhutu

#pragma once

#include "icamera.hpp"

namespace ignite
{
    class SceneCamera : public ICamera
    {
    public:
        enum class AspectRatioPreset
        {
            Free = 0,
            Ratio16x9,
            Ratio16x10,
            Ratio4x3,
            Ratio21x9,
            Ratio1x1
        };

        SceneCamera() = default;

        void SetTransform(const glm::mat4 &transform);
        virtual glm::mat4 GetView() override;

        void SetAspectRatioPreset(AspectRatioPreset preset) { m_AspectRatioPreset = preset; }
        AspectRatioPreset GetAspectRatioPreset() const { return m_AspectRatioPreset; }
        bool IsFreeAspect() const { return m_AspectRatioPreset == AspectRatioPreset::Free; }
        float GetAspectRatioValue() const;

    private:
        glm::mat4 m_Transform = glm::mat4(1.0f);
        AspectRatioPreset m_AspectRatioPreset = AspectRatioPreset::Free;
    };
}
