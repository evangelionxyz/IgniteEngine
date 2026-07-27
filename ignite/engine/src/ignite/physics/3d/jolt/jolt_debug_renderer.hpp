// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_JOLT_DEBUG_RENDERER_HPP
#define IGN_JOLT_DEBUG_RENDERER_HPP

#include "ignite/core/base.hpp"
#include "ignite/graphics/renderer/renderer_2d.hpp"

#include <Jolt/Jolt.h>

#ifdef JPH_DEBUG_RENDERER

#include <Jolt/Renderer/DebugRendererSimple.h>
#include <glm/glm.hpp>

namespace ignite::physics
{
    class IGN_API JoltDebugRenderer : public JPH::DebugRendererSimple
    {
    public:
        JoltDebugRenderer();
        virtual ~JoltDebugRenderer() override = default;

        void BeginBatch(Renderer2D *renderer2D);
        void EndBatch();

        // Implement JPH::DebugRenderer / DebugRendererSimple interface
        virtual void DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor) override;
        virtual void DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor, ECastShadow inCastShadow = ECastShadow::Off) override;
        virtual void DrawText3D(JPH::RVec3Arg inPosition, const JPH::string_view &inString, JPH::ColorArg inColor, float inHeight = 0.5f) override;

    private:
        Renderer2D *m_Renderer2D = nullptr;
    };
}

#endif // JPH_DEBUG_RENDERER

#endif // IGN_JOLT_DEBUG_RENDERER_HPP
