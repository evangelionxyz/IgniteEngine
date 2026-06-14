// Copyright (c) 2026 Evangelion Manuhutu
#pragma once
#ifndef IGN_GAME_UI_SYSTEM_HPP
#define IGN_GAME_UI_SYSTEM_HPP

#include "ignite/core/subsystem.hpp"

namespace ignite
{
	class Scene;

	class IGN_API GameUISystem : public Subsystem
	{
	public:
		virtual void Init() override;
		virtual void Shutdown() override;
		
		static void SetSceneContext(Scene *scene);
		static bool IsMouseOverUI(float mouseX, float mouseY);
		static void HandleMouseMotion(float mouseX, float mouseY);
	};
}

#endif