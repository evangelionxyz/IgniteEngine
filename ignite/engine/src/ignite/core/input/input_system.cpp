// Copyright (c) 2026 Evangelion Manuhutu

#include "input_system.hpp"
#include "ignite/graphics/ui/game_ui_system.hpp"
#include "ignite/scene/scene.hpp"

namespace ignite
{
	void EditorInputSystem::ProcessEvent(SDL_Event* event)
	{
	}

	void GameInputSystem::ProcessEvent(SDL_Event* event)
	{
		if (event->type == SDL_EVENT_MOUSE_MOTION)
		{
			float mouseX = event->motion.x;
			float mouseY = event->motion.y;

			// Check game UI Layer first
			if (GameUISystem::IsMouseOverUI(mouseX, mouseY))
			{
				GameUISystem::HandleMouseMotion(mouseX, mouseY);

				SceneInput::CancelWorldHover();
				return;
			}

			// If UI didn't take it, pass to World Layer
			SceneInput::HandleMouseMotion(mouseX, mouseY);
		}
	}

	void SceneInput::SetSceneContext(Scene* scene)
	{

	}

	void SceneInput::HandleMouseMotion(float mouseX, float mouseY)
	{

	}

	void SceneInput::CancelWorldHover()
	{

	}

}