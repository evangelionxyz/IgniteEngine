// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef BLEND_SPACE_EDITOR_HPP
#define BLEND_SPACE_EDITOR_HPP

#include "ignite/animation/blend_space.hpp"
#include <imgui.h>

namespace ignite
{
    class AssetManager;

    struct BlendSpaceEditorState
    {
        int selectedSample = -1;
        glm::vec2 previewInput = glm::vec2(0.0f, 0.0f);
        bool isDraggingPreviewPoint = false;
        bool isDraggingSample = false;
        int draggingSampleIndex = -1;
        bool previewInScene = true;
    };

    class BlendSpaceEditor
    {
    public:
        static void DrawBlendSpaceEditor(const Ref<BlendSpace> &blendSpace, AssetManager *assetManager, BlendSpaceEditorState &state);
    };
}

#endif
