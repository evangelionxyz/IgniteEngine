// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_CORE_RAY_HPP
#define IGN_CORE_RAY_HPP

#include <glm/glm.hpp>

namespace ignite
{
    struct Ray
    {
        glm::vec3 origin = glm::vec3(0.0f);
        glm::vec3 direction = glm::vec3(0.0f, 0.0f, -1.0f);
    };

    struct RaycastHit
    {
        float fraction = 1.0f;
        glm::vec3 hitPoint = {};
        glm::vec3 hitNormal = {};
        uint64_t userData = 0;
    };
}

#endif
