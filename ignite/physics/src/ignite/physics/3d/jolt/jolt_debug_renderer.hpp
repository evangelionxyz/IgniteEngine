// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_JOLT_DEBUG_RENDERER_HPP
#define IGN_JOLT_DEBUG_RENDERER_HPP

#include "ignite/core/base.hpp"
#include <functional>
#include <glm/glm.hpp>
#include <Jolt/Jolt.h>

#ifdef JPH_DEBUG_RENDERER

#include <Jolt/Renderer/DebugRendererSimple.h>

namespace ignite::physics
{
    using DrawLineFunc = std::function<void(const glm::vec3 &from, const glm::vec3 &to, const glm::vec4 &color)>;

    class IGN_API JoltDebugRenderer : public JPH::DebugRendererSimple
    {
    public:
        JoltDebugRenderer();
        virtual ~JoltDebugRenderer() override = default;

        void SetDrawLineCallback(const DrawLineFunc &callback) { m_DrawLineCallback = callback; }

        // Implement JPH::DebugRenderer / DebugRendererSimple interface
        virtual void DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor) override;
        virtual void DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor, ECastShadow inCastShadow = ECastShadow::Off) override;
        virtual void DrawText3D(JPH::RVec3Arg inPosition, const JPH::string_view &inString, JPH::ColorArg inColor, float inHeight = 0.5f) override;

    private:
        DrawLineFunc m_DrawLineCallback;
    };
}

#endif // JPH_DEBUG_RENDERER

#endif // IGN_JOLT_DEBUG_RENDERER_HPP
