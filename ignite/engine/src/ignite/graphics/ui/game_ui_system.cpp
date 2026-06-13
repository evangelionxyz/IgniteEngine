// Copyright (c) 2026 Evangelion Manuhutu
#include "game_ui_system.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/core/subsystem.hpp"
#include "ignite/core/application.hpp"

namespace ignite
{
	struct GameUISystemData
	{
		Scene* scene = nullptr;
		Project* project = nullptr;
	};

	GameUISystemData s_GameUIData;

	void GameUISystem::Init() {}
	void GameUISystem::Shutdown()
	{
		s_GameUIData.scene = nullptr;
		s_GameUIData.project = nullptr;
	}

	void GameUISystem::SetSceneContext(Scene* scene)
	{
		s_GameUIData.scene = scene;
	}

	bool GameUISystem::IsMouseOverUI(float mouseX, float mouseY)
	{
		return false;
	}

	void GameUISystem::HandleMouseMotion(float mouseX, float mouseY)
	{

	}

}