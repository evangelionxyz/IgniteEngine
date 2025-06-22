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

#include <GLFW/glfw3.h>
#include "ignite/core/types.hpp"

namespace ignite
{
    struct InputData
    {
        Input *input = nullptr;
        GLFWwindow *window;
    };

    static InputData s_r2d;

    Input::Input(void *window)
    {
        s_r2d.input = this;
        s_r2d.window = static_cast<GLFWwindow *>(window);
    }

    bool Input::IsKeyPressed(KeyCode keycode)
    {
        return glfwGetKey(s_r2d.window, keycode) == GLFW_PRESS;
    }

    bool Input::IsMouseButtonPressed(MouseCode button)
    {
        return glfwGetMouseButton(s_r2d.window, button) == GLFW_PRESS;
    }

    glm::vec2 Input::GetMousePosition()
    {
        f64 x, y;
        glfwGetCursorPos(s_r2d.window, &x, &y);
        return glm::vec2(x, y);
    }
}
