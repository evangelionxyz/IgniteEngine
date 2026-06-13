// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_INPUT_SYSTEM_HPP
#define IGN_INPUT_SYSTEM_HPP

#include "input_layer.hpp"
#include "ignite/core/subsystem.hpp"
#include "SDL3/SDL_events.h"

namespace ignite
{
	class Scene;

	class IGN_API InputSystem : public Subsystem
	{
	public:
		virtual void ProcessEvent(SDL_Event *event) = 0;
	};

	class IGN_API EditorInputSystem : public InputSystem
	{
	public:
		virtual void ProcessEvent(SDL_Event* event);
	};

	class IGN_API GameInputSystem : public InputSystem
	{
	public:
		virtual void ProcessEvent(SDL_Event* event);
	};

	class IGN_API SceneInput : public Subsystem
	{
	public:
		static void SetSceneContext(Scene* scene);
		static void HandleMouseMotion(float mouseX, float mouseY);
		static void CancelWorldHover();
	};
}

#endif
