//Copyright (c) 2026 Evangelion Manuhutu | IGNITE STUDIO

#include "sound_panel.hpp"

namespace ignite
{
    SoundPanel::SoundPanel(const char *windowTitle)
        : IPanel(windowTitle)
    {
    }

	SoundPanel::~SoundPanel()
	{

	}

	void SoundPanel::OnGuiRender()
    {
        ImGui::Begin("Sound");

        ImGui::End();
    }

    void SoundPanel::OnUpdate(f32 deltaTime)
    {
    }
}
