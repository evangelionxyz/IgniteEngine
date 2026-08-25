// Copyright (c) 2026 Evangelion Manuhutu

#include "jolt_debug_renderer.hpp"

#ifdef JPH_DEBUG_RENDERER

namespace ignite::physics
{
    JoltDebugRenderer::JoltDebugRenderer()
    {
        Initialize();
    }

    void JoltDebugRenderer::DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor)
    {
        if (m_DrawLineCallback)
        {
            glm::vec3 from(inFrom.GetX(), inFrom.GetY(), inFrom.GetZ());
            glm::vec3 to(inTo.GetX(), inTo.GetY(), inTo.GetZ());
            JPH::Color c = inColor;
            glm::vec4 color(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f);
            m_DrawLineCallback(from, to, color);
        }
    }

    void JoltDebugRenderer::DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor, ECastShadow inCastShadow)
    {
        DrawLine(inV1, inV2, inColor);
        DrawLine(inV2, inV3, inColor);
        DrawLine(inV3, inV1, inColor);
    }

    void JoltDebugRenderer::DrawText3D(JPH::RVec3Arg inPosition, const JPH::string_view &inString, JPH::ColorArg inColor, float inHeight)
    {
    }
}

#endif
