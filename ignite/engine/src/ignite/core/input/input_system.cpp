// Copyright (c) 2026 Evangelion Manuhutu

#include "input_system.hpp"
#include "ignite/graphics/ui/game_ui_system.hpp"
#include "ignite/graphics/window.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/core/logger.hpp"
#include <SDL3/SDL.h>

namespace ignite
{
	InputSystem* InputSystem::s_ActiveSystem = nullptr;
	Window* InputSystem::s_Window = nullptr;

	void InputSystem::Init()
	{
		if (!s_ActiveSystem)
		{
			s_ActiveSystem = this;
		}
	}

	void InputSystem::Shutdown()
	{
		if (s_ActiveSystem == this)
		{
			s_ActiveSystem = nullptr;
		}
	}

	bool InputSystem::IsKeyPressed(KeyCode keycode)
	{
		if (s_ActiveSystem)
		{
			return s_ActiveSystem->IsKeyPressedImpl(keycode);
		}
		return false;
	}

	bool InputSystem::IsModifierPressed(KeyModCode modcode)
	{
		if (s_ActiveSystem)
		{
			return s_ActiveSystem->IsModifierPressedImpl(modcode);
		}
		return false;
	}

	bool InputSystem::IsMouseButtonPressed(MouseCode button)
	{
		if (s_ActiveSystem)
		{
			return s_ActiveSystem->IsMouseButtonPressedImpl(button);
		}
		return false;
	}

	glm::ivec2 InputSystem::GetMousePosition()
	{
		if (s_ActiveSystem)
		{
			return s_ActiveSystem->GetMousePositionImpl();
		}
		return glm::ivec2(0);
	}

	glm::vec2 InputSystem::GetGameplayMousePosition()
	{
		if (auto* gameSystem = dynamic_cast<GameInputSystem*>(s_ActiveSystem))
		{
			return gameSystem->GetGameplayMousePosition();
		}
		return glm::vec2(0.0f);
	}

	void InputSystem::SetGameplayMousePosition(float x, float y, bool enabled)
	{
		if (auto* gameSystem = dynamic_cast<GameInputSystem*>(s_ActiveSystem))
		{
			gameSystem->SetGameplayMousePosition(x, y, enabled);
		}
	}

	bool InputSystem::IsGameplayMousePositionEnabled()
	{
		if (auto* gameSystem = dynamic_cast<GameInputSystem*>(s_ActiveSystem))
		{
			return gameSystem->IsGameplayMousePositionEnabled();
		}
		return false;
	}

	void InputSystem::SetActiveSystem(InputSystem* system)
	{
		s_ActiveSystem = system;
	}

	void InputSystem::SetWindow(Window* win)
	{
		s_Window = win;
	}

	Window* InputSystem::GetWindow()
	{
		return s_Window;
	}

	void InputSystem::SetMouseToCenter()
	{
		if (s_Window)
		{
			const auto size = s_Window->GetSize();
			SDL_WarpMouseInWindow(s_Window->GetWindowHandle(), size.x / 2.0f, size.y / 2.0f);
		}
	}

	void InputSystem::SetCursorMode(CursorMode mode)
	{
		if (s_ActiveSystem)
		{
			s_ActiveSystem->SetCursorModeImpl(mode);
		}
	}

	bool InputSystem::IsKeyPressedImpl(KeyCode keycode) const
	{
		auto it = m_KeyState.find(keycode);
		if (it != m_KeyState.end())
		{
			return it->second;
		}
		return false;
	}

	bool InputSystem::IsModifierPressedImpl(KeyModCode modcode) const
	{
		auto it = m_ModifierState.find(modcode);
		if (it != m_ModifierState.end())
		{
			return it->second;
		}
		return false;
	}

	bool InputSystem::IsMouseButtonPressedImpl(MouseCode button) const
	{
		auto it = m_MouseButtonState.find(button);
		if (it != m_MouseButtonState.end())
		{
			return it->second;
		}
		return false;
	}

	void InputSystem::SetKey(SDL_Keycode key, bool pressed)
	{
		m_KeyState[key] = pressed;
	}

	void InputSystem::SetModifier(SDL_Keymod mod, bool pressed)
	{
		m_ModifierState[mod] = pressed;
	}

	void InputSystem::SetMouseButton(MouseCode button, bool pressed)
	{
		m_MouseButtonState[button] = pressed;
	}

	void InputSystem::SetMousePosition(int x, int y)
	{
		m_MousePosition = glm::ivec2(x, y);
	}

	void InputSystem::SetCursorModeImpl(CursorMode mode)
	{
		if (m_CursorMode == mode)
			return;

		m_CursorMode = mode;

		if (!s_Window)
			return;

		switch (mode)
		{
		case CursorMode::Normal:
		{
			SDL_SetWindowRelativeMouseMode(s_Window->GetWindowHandle(), false);
			SDL_ShowCursor();
			break;
		}
		case CursorMode::Hidden:
		{
			SDL_SetWindowRelativeMouseMode(s_Window->GetWindowHandle(), false);
			SDL_HideCursor();
			break;
		}
		case CursorMode::Disabled:
		{
			SDL_SetWindowRelativeMouseMode(s_Window->GetWindowHandle(), true);
			break;
		}
		case CursorMode::Captured:
		{
			SDL_SetWindowRelativeMouseMode(s_Window->GetWindowHandle(), false);
			SDL_CaptureMouse(true);
			SDL_ShowCursor();
			break;
		}
		default:
			break;
		}
	}

	void EditorInputSystem::ProcessEvent(SDL_Event* event)
	{
		switch (event->type)
		{
		case SDL_EVENT_KEY_DOWN:
			SetModifier(KeyMod::Shift, event->key.mod & SDL_KMOD_SHIFT);
			SetModifier(KeyMod::Control, event->key.mod & SDL_KMOD_CTRL);
			SetModifier(KeyMod::LeftAlt, event->key.mod & SDL_KMOD_LALT);
			SetModifier(KeyMod::RightAlt, event->key.mod & SDL_KMOD_RALT);
			SetModifier(KeyMod::LeftShift, event->key.mod & SDL_KMOD_LSHIFT);
			SetModifier(KeyMod::RightShift, event->key.mod & SDL_KMOD_RSHIFT);
			SetModifier(KeyMod::LeftControl, event->key.mod & SDL_KMOD_LCTRL);
			SetModifier(KeyMod::RightControl, event->key.mod & SDL_KMOD_RCTRL);
			SetKey(event->key.key, true);
			break;
		case SDL_EVENT_KEY_UP:
			SetModifier(KeyMod::Shift, event->key.mod & SDL_KMOD_SHIFT);
			SetModifier(KeyMod::Control, event->key.mod & SDL_KMOD_CTRL);
			SetModifier(KeyMod::LeftAlt, event->key.mod & SDL_KMOD_LALT);
			SetModifier(KeyMod::RightAlt, event->key.mod & SDL_KMOD_RALT);
			SetModifier(KeyMod::LeftShift, event->key.mod & SDL_KMOD_LSHIFT);
			SetModifier(KeyMod::RightShift, event->key.mod & SDL_KMOD_RSHIFT);
			SetModifier(KeyMod::LeftControl, event->key.mod & SDL_KMOD_LCTRL);
			SetModifier(KeyMod::RightControl, event->key.mod & SDL_KMOD_RCTRL);
			SetKey(event->key.key, false);
			break;
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
			SetMouseButton(event->button.button, true);
			break;
		case SDL_EVENT_MOUSE_BUTTON_UP:
			SetMouseButton(event->button.button, false);
			break;
		case SDL_EVENT_MOUSE_MOTION:
			SetMousePosition((int)event->motion.x, (int)event->motion.y);
			break;
		}
	}

	void GameInputSystem::ProcessEvent(SDL_Event* event)
	{
		switch (event->type)
		{
		case SDL_EVENT_KEY_DOWN:
			SetModifier(KeyMod::Shift, event->key.mod & SDL_KMOD_SHIFT);
			SetModifier(KeyMod::Control, event->key.mod & SDL_KMOD_CTRL);
			SetModifier(KeyMod::LeftAlt, event->key.mod & SDL_KMOD_LALT);
			SetModifier(KeyMod::RightAlt, event->key.mod & SDL_KMOD_RALT);
			SetModifier(KeyMod::LeftShift, event->key.mod & SDL_KMOD_LSHIFT);
			SetModifier(KeyMod::RightShift, event->key.mod & SDL_KMOD_RSHIFT);
			SetModifier(KeyMod::LeftControl, event->key.mod & SDL_KMOD_LCTRL);
			SetModifier(KeyMod::RightControl, event->key.mod & SDL_KMOD_RCTRL);
			SetKey(event->key.key, true);
			break;
		case SDL_EVENT_KEY_UP:
			SetModifier(KeyMod::Shift, event->key.mod & SDL_KMOD_SHIFT);
			SetModifier(KeyMod::Control, event->key.mod & SDL_KMOD_CTRL);
			SetModifier(KeyMod::LeftAlt, event->key.mod & SDL_KMOD_LALT);
			SetModifier(KeyMod::RightAlt, event->key.mod & SDL_KMOD_RALT);
			SetModifier(KeyMod::LeftShift, event->key.mod & SDL_KMOD_LSHIFT);
			SetModifier(KeyMod::RightShift, event->key.mod & SDL_KMOD_RSHIFT);
			SetModifier(KeyMod::LeftControl, event->key.mod & SDL_KMOD_LCTRL);
			SetModifier(KeyMod::RightControl, event->key.mod & SDL_KMOD_RCTRL);
			SetKey(event->key.key, false);
			break;
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
			SetMouseButton(event->button.button, true);
			break;
		case SDL_EVENT_MOUSE_BUTTON_UP:
			SetMouseButton(event->button.button, false);
			break;
		case SDL_EVENT_MOUSE_MOTION:
			SetMousePosition((int)event->motion.x, (int)event->motion.y);
			break;
		}

		if (event->type == SDL_EVENT_MOUSE_MOTION)
		{
			float mouseX = event->motion.x;
			float mouseY = event->motion.y;

			if (m_GameplayMousePositionEnabled)
			{
				mouseX = m_GameplayMousePosition.x;
				mouseY = m_GameplayMousePosition.y;
			}

			if (GameUISystem::IsMouseOverUI(mouseX, mouseY))
			{
				GameUISystem::HandleMouseMotion(mouseX, mouseY);
				SceneInput::CancelWorldHover();
			}
			else
			{
				SceneInput::HandleMouseMotion(mouseX, mouseY);
			}
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
