//Copyright (c) 2026 Evangelion Manuhutu | IGNITE STUDIO

#pragma once

#include "ipanel.hpp"

namespace ignite
{
    class SoundPanel : public IPanel
    {
    public:
        SoundPanel(const char *windowTitle);
        virtual ~SoundPanel() override;
        
        virtual void OnGuiRender() override;
        virtual void OnUpdate(f32 deltaTime) override;
    };
}
