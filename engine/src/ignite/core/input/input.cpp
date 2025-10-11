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
    struct InputData
    {
        Input *input = nullptr;
        CursorMode cursorMode = CursorMode::Normal;
        SDL_Window *window;
    };

    static InputData s_InputData;

    Input::Input(void *window)
    {
        s_InputData.input = this;
        s_InputData.window = static_cast<SDL_Window *>(window);
    }

    bool Input::IsKeyPressed(KeyCode keycode)
    {
        const bool* keyboardState = SDL_GetKeyboardState(nullptr);
        SDL_Keymod modState;
        SDL_Scancode scancode = SDL_GetScancodeFromKey(keycode, &modState);
        return keyboardState[scancode];
    }

    bool Input::IsMouseButtonPressed(MouseCode button)
    {
        uint32_t mouseState = SDL_GetMouseState(nullptr, nullptr);
        uint32_t mask = SDL_BUTTON_MASK(button);
        bool result = (mouseState & mask) != 0;
        return result;
    }

    glm::vec2 Input::GetMousePosition()
    {
        float x, y;
        SDL_GetMouseState(&x, &y);
        return glm::vec2(x, y);
    }

    void Input::SetMousePosition(float x, float y)
    {
        SDL_WarpMouseInWindow(s_InputData.window, x, y);
    }

    void Input::SetCursorMode(CursorMode mode)
    {
        if (s_InputData.cursorMode == mode)
            return;

        s_InputData.cursorMode = mode;
        
        switch (mode)
        {
        case CursorMode::Normal:
        {
            SDL_SetWindowRelativeMouseMode(s_InputData.window, false);
            SDL_ShowCursor();
            break;
        }
        case CursorMode::Hidden:
        {
            SDL_SetWindowRelativeMouseMode(s_InputData.window, false);
            SDL_HideCursor();
            break;
        }
        case CursorMode::Disabled:
        {
            SDL_SetWindowRelativeMouseMode(s_InputData.window, true);
            break;
        }
        case CursorMode::Captured:
        {
            SDL_SetWindowRelativeMouseMode(s_InputData.window, false);
            SDL_CaptureMouse(true);
            SDL_ShowCursor();
            break;
        }
        default:
            break;
        }
    }
}
