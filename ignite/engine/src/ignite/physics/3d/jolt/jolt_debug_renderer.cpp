// Copyright (c) 2026 Evangelion Manuhutu

#include "jolt_debug_renderer.hpp"

#ifdef JPH_DEBUG_RENDERER

namespace ignite::physics
{
    JoltDebugRenderer::JoltDebugRenderer()
    {
    }

    void JoltDebugRenderer::BeginBatch(Renderer2D *renderer2D)
    {
        m_Renderer2D = renderer2D;
    }

    void JoltDebugRenderer::EndBatch()
    {
        m_Renderer2D = nullptr;
    }

    void JoltDebugRenderer::DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor)
    {
        if (!m_Renderer2D)
            return;

        glm::vec3 from(inFrom.GetX(), inFrom.GetY(), inFrom.GetZ());
        glm::vec3 to(inTo.GetX(), inTo.GetY(), inTo.GetZ());
        JPH::Color color = inColor;
        glm::vec4 col(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f);

        m_Renderer2D->DrawLine(from, to, col);
    }

    void JoltDebugRenderer::DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor, ECastShadow inCastShadow)
    {
        if (!m_Renderer2D)
            return;

        DrawLine(inV1, inV2, inColor);
        DrawLine(inV2, inV3, inColor);
        DrawLine(inV3, inV1, inColor);
    }

    void JoltDebugRenderer::DrawText3D(JPH::RVec3Arg inPosition, const JPH::string_view &inString, JPH::ColorArg inColor, float inHeight)
    {
    }
}

#endif // JPH_DEBUG_RENDERER
