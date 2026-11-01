// Copyright (c) 2026 Evangelion Manuhutu

#pragma once

#include "ignite/graphics/render_target.hpp"

namespace nri
{
    struct CommandBuffer;
}

namespace ignite
{
    class FrameContext;
    class RenderTarget;

    struct DebugRendererSettings
    {

    };

    class DebugRenderer
    {
    public:
        void Draw(const DebugRendererSettings &settings);

    private:

        Ref<RenderTarget> m_RenderTarget;
    };
}
