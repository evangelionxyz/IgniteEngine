// Copyright (c) 2026 Evangelion Manuhutu | IGNITE STUDIO

#ifndef STATES_HPP
#define STATES_HPP

namespace ignite
{

#define DND_PAYLOAD_SPRITE_SHEET_ITEM "sprite_sheet_item"
#define DND_PAYLOAD_CONTENT_BROWSER_ITEM "content_browser_item"
#define DND_PAYLOAD_ENTITY_SOURCE_ITEM "entity_source_item"

    enum class State
    {
        SceneEdit,
        ScenePlay,
        ScenePaused,
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

#endif