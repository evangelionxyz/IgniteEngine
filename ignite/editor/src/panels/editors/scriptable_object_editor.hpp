// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef SCRIPTABLE_OBJECT_EDITOR_HPP
#define SCRIPTABLE_OBJECT_EDITOR_HPP

#include "ignite/asset/asset.hpp"

namespace ignite
{
    class EditorLayer;
    class ScriptableObject;

    class ScriptableObjectEditor
    {
    public:
        static bool DrawEditor(Ref<ScriptableObject> so, EditorLayer *editorLayer);
    };
}

#endif