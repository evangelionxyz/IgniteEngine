//Copyright (c) 2026 Evangelion Manuhutu | IGNITE STUDIO

#include "sound_panel.hpp"

namespace ignite
{
    SoundPanel::SoundPanel(const char *name, EditorLayer *editor)
        : IPanel(name, editor)
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

    void SoundPanel::OnUpdate(float deltaTime)
    {

    }
}
