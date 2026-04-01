//Copyright (c) 2026 Evangelion Manuhutu | IGNITE STUDIO

#pragma once

namespace ignite
{
    enum class State
    {
        SceneEdit,
        ScenePlay,
        SceneSimulate
    };

    enum class GizmoOperation
    {
        NONE = -1,
        TRANSLATE = 0,
        ROTATE = 1,
        SCALE = 2,
        BOUND_SIZING_2D = 3
    };

}
