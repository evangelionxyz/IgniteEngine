/* MIT License
* 
* Copyright (c) 2025 Evangelion Manuhutu
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

#pragma once

#include <glm/vec2.hpp>

#include "key_codes.hpp"
#include "mouse_codes.hpp"

#include "ignite/graphics/window.hpp"

namespace ignite
{
    enum class CursorMode
    {
        Normal,
        Hidden,
        Disabled,
        Captured
    };

    class Input
    {
    public:
		Input(Window* window);

        static bool IsKeyPressed(KeyCode keycode);
        static bool IsModifierPressed(KeyModCode modcode);
        static bool IsMouseButtonPressed(MouseCode button);

        static glm::ivec2 GetMousePosition();
        
        static void SetMouseToCenter();
        static void SetCursorMode(CursorMode mode);
        static void SetKey(SDL_Keycode key, bool pressed);
		static void SetModifier(SDL_Keymod mod, bool pressed);
        static void SetMouseButton(MouseCode button, bool pressed);
		static void SetMousePosition(i32 x, i32 y);

    private:
		static std::unordered_map<KeyModCode, bool> modifierState;
        static std::unordered_map<KeyCode, bool> keyState;
		static std::unordered_map<MouseCode, bool> mouseButtonState;

        static glm::ivec2 mousePosition;
		static CursorMode cursorMode;

        static Window *window;
    };
}
