/* MIT License
* 
* Copyright (c) 2025 Evangelion Manuhutu | IGNITE STUDIO
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#include "input.hpp"

#include <SDL3/SDL.h>
#include "ignite/core/types.hpp"

namespace ignite
{
    std::unordered_map<KeyModCode, bool> Input::modifierState;
    std::unordered_map<KeyCode, bool> Input::keyState;
    std::unordered_map<MouseCode, bool> Input::mouseButtonState;
    glm::ivec2 Input::mousePosition = glm::ivec2(0);
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
