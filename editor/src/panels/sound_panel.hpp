//Copyright (c) 2026 Evangelion Manuhutu | IGNITE STUDIO

#pragma once

#include "ipanel.hpp"

namespace ignite
{
    class SoundPanel : public IPanel
    {
    public:
        SoundPanel(const char *name, EditorLayer *editor);
        virtual ~SoundPanel() override;
        
        virtual void OnGuiRender() override;
        virtual void OnUpdate(float deltaTime) override;
    };
}
