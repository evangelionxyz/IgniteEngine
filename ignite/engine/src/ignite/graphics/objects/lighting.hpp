// Copyright (c) 2026 Evangelion Manuhutu
#pragma once
#ifndef IGN_LIGHTING_HPP
#define IGN_LIGHTING_HPP

#include <glm/glm.hpp>

namespace ignite
{

    struct DirLight
    {
        glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f }; // RGBA
        glm::vec4 direction = { 0.0f, 1.0f, 0.0f, 0.0f }; // Normalized direction
        float intensity = 0.5f;           // Brightness scalar
        float angularSize = 0.1f;         // Simulates soft shadows (sun size in degrees)
        float shadowStrength = 1.0f;      // Strength of cast shadows (0 = no shadow, 1 = full)

        DirLight() { ++count; }
        ~DirLight() { --count; }

        static uint32_t count;
    };

    struct PointLight
    {
        glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec4 position = { 0.0f, 0.0f, 0.0f, 1.0f };
        float intensity = 1.0f;
        float range = 10.0f;
        float constantAtt = 1.0f;
        float linearAtt = 0.09f;
        float quadraticAtt = 0.032f;

        PointLight() { ++count; }
        ~PointLight() { --count; }

        static uint32_t count;
    };

    struct SpotLight
    {
        glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec4 position = { 0.0f, 0.0f, 0.0f, 1.0f };
        glm::vec4 direction = { 0.0f, -1.0f, 0.0f, 0.0f };
        float intensity = 1.0f;
        float range = 10.0f;
        float constantAtt = 1.0f;
        float linearAtt = 0.09f;
        float quadraticAtt = 0.032f;
        float innerConeAngle = 12.5f; // degrees
        float outerConeAngle = 45.0f; // degrees

        SpotLight() { ++count; }
        ~SpotLight() { --count; }

        static uint32_t count;
    };
}

#endif
