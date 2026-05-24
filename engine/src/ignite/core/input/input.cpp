// Copyright (c) 2026 Evangelion Manuhutu

#include "input.hpp"

#include <SDL3/SDL.h>
#include "ignite/core/types.hpp"

namespace ignite
{
    std::unordered_map<KeyModCode, bool> Input::modifierState;
    std::unordered_map<KeyCode, bool> Input::keyState;
    std::unordered_map<MouseCode, bool> Input::mouseButtonState;
    glm::ivec2 Input::mousePosition = glm::ivec2(0);
    glm::vec2 Input::gameplayMousePosition = glm::vec2(0.0f);
    bool Input::gameplayMousePositionEnabled = false;
    CursorMode Input::cursorMode = CursorMode::Normal;
	Window* Input::window = nullptr;

    Input::Input(Window *window)
    {
        Input::window = window;
    }

    bool Input::IsKeyPressed(KeyCode keycode)
    {
		return keyState[keycode];
    }

    bool Input::IsModifierPressed(KeyModCode modcode)
    {
		return modifierState[modcode];
    }

    bool Input::IsMouseButtonPressed(MouseCode button)
    {
		return mouseButtonState[button];
    }

    glm::ivec2 Input::GetMousePosition()
    {
        return Input::mousePosition;
    }

    glm::vec2 Input::GetGameplayMousePosition()
    {
        return Input::gameplayMousePosition;
    }

    void Input::SetGameplayMousePosition(float x, float y, bool enabled)
    {
        Input::gameplayMousePosition = { x, y };
        Input::gameplayMousePositionEnabled = enabled;
    }

    bool Input::IsGameplayMousePositionEnabled()
    {
        return Input::gameplayMousePositionEnabled;
    }

    void Input::SetMouseToCenter()
    {
        const auto size = Input::window->GetSize();
		SDL_WarpMouseInWindow(Input::window->GetWindowHandle(), size.x / 2.0f, size.y / 2.0f);
    }

    void Input::SetCursorMode(CursorMode mode)
    {
        if (Input::cursorMode == mode)
            return;

        Input::cursorMode = mode;
        
        switch (mode)
        {
        case CursorMode::Normal:
        {
            SDL_SetWindowRelativeMouseMode(Input::window->GetWindowHandle(), false);
            SDL_ShowCursor();
            break;
        }
        case CursorMode::Hidden:
        {
            SDL_SetWindowRelativeMouseMode(Input::window->GetWindowHandle(), false);
            SDL_HideCursor();
            break;
        }
        case CursorMode::Disabled:
        {
            SDL_SetWindowRelativeMouseMode(Input::window->GetWindowHandle(), true);
            break;
        }
        case CursorMode::Captured:
        {
            SDL_SetWindowRelativeMouseMode(Input::window->GetWindowHandle(), false);
            SDL_CaptureMouse(true);
            SDL_ShowCursor();
            break;
        }
        default:
            break;
        }
    }

    void Input::SetKey(SDL_Keycode key, bool pressed)
    {
		keyState[key] = pressed;
    }

    void Input::SetModifier(SDL_Keymod mod, bool pressed)
    {
		modifierState[mod] = pressed;
    }

    void Input::SetMouseButton(MouseCode button, bool pressed)
    {
		mouseButtonState[button] = pressed;
    }

    void Input::SetMousePosition(i32 x, i32 y)
    {
		mousePosition = { x, y };
    }
}
