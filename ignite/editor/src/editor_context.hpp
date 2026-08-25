// Copyright (c) 2026 Evangelion Manuhutu
#pragma once
#ifndef IGN_EDITOR_CONTEXT_HPP
#define IGN_EDITOR_CONTEXT_HPP

namespace ignite
{
    class EditorContext
    {
    public:
        static void Init();
        static void Shutdown();

        static EditorContext *Get();
    };
}

#endif
