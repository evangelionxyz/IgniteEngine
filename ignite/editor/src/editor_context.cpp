// Copyright (c) 2026 Evangelion Manuhutu
#include "pch.hpp"
#include "editor_context.hpp"

namespace ignite {

    static EditorContext *s_EditorContextInstance = nullptr;

    void EditorContext::Init()
    {
        if (!s_EditorContextInstance)
            return;

        s_EditorContextInstance = new EditorContext();
    }

    void EditorContext::Shutdown()
    {
        if (!s_EditorContextInstance)
        {
            delete s_EditorContextInstance;
            s_EditorContextInstance = nullptr;
        }
    }

    EditorContext *EditorContext::Get()
    {
        return s_EditorContextInstance;
    }
}
