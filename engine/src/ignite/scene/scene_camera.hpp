// Copyright (c) 2026 Evangelion Manuhutu

#pragma once

#include "icamera.hpp"

namespace ignite
{
    class SceneCamera : public ICamera
    {
    public:
        SceneCamera() = default;

        void SetTransform(const glm::mat4 &transform);
        virtual glm::mat4 GetView() override;

    private:
        glm::mat4 m_Transform = glm::mat4(1.0f);
    };
}
