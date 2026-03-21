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

#pragma once

#include <imgui.h>
#include <imgui_internal.h>
#include <string>
#include "ignite/core/logger.hpp"
#include "ignite/core/types.hpp"

#include "states.hpp"

namespace ignite
{
    class IPanel
    {
    public:
        IPanel() = default;
        explicit IPanel(const char *windowTitle)
            : m_WindowTitle(windowTitle)
        {
        }

        virtual ~IPanel() {};

        // from Layer class
        virtual void OnGuiRender() { }

        // to child class
        virtual bool IsOpen() { return m_IsOpen; }
        virtual bool IsFocused() { return m_IsFocused; }
        virtual bool IsHovered() { return m_IsHovered; }
        virtual void OnUpdate(float deltaTime) { }

        std::string &GetTitle() { return m_WindowTitle; }

    protected:
        std::string m_WindowTitle;
        bool m_IsOpen = true;
        bool m_IsFocused = false;
        bool m_IsHovered = false;
    };
}


